# Chromium RKMPP 硬件加速浏览器

> 上游 `ubuntu-ebf-rk3588` 的 `e80577f5b`「add Chromium RKMPP hardware acceleration」，
> 用 `libv4l-rkmpp` 把 Chromium 的 V4L2 解码器接到 RK3588 的 MPP 硬解。
> 本文记录**这套东西由哪些部件组成、工程机实测数据、踩到的点、集成进本镜像要动哪里**。
> 相关：`notes/build/deb-packages.md`（deb 清单）、`notes/system/system-integration.md`（集成总账）、
> `notes/build/image-size-trimming.md`（体积，chromium 自带 300 MB 的二进制）。

## TL;DR

- 三个部件：`libv4l-rkmpp`（把 V4L2 解码请求转给 MPP）+ 伪节点 `/dev/video-dec*` + Chromium 的 V4L2 VDA（§1）。
- ⛔ **最容易踩的一条**：Chromium **必须走 `/usr/bin/chromium`**，硬解开关在 §2 那个文件里，改错地方等于没生效。
- 工程机实测（2026-09-12）：生效后 CPU 占用明显下降，数据见 §3；不生效时按 §4 的清单查。
- 上位机自带素材的硬解实测（2026-09-30 开发板 eMMC）见 §6。
- 集成进本镜像要动哪几处，见 §5。

## 0. 结论速览（2026-09-12 工程机实测）

- 浏览器本体：**Chromium 151.0.7922.169**（Debian 风格打包：`/usr/bin/chromium` 包装脚本 +
  `/usr/lib/chromium/chromium` 真实二进制 300 MB）。
- H.264 / HEVC / VP9 1080p30 **都走 MPP 硬解**（插件日志 `mpp initializing for H.264 / HEVC / VP9`，
  稳定 `DEC FPS: 30~35`）；桌面上打开网页正常。
- ✅ **可复现的功能测试：`bash tests/browser-codec.sh --baseline`**（在板子的桌面会话里跑，
  自己硬编测试流 → 播放 → 抓插件日志/页面解码帧数/CPU/VPU 占用，`--baseline` 再做一遍软解对照）。
  分辨率可选（`--resolution=720p|1080p|4k|宽x高`）——**1080p 对 RK3588 太轻松**：
  实测 H.264 1080p30 的 VPU 硬件占用只有 **2.4~3.8 %**，换 4K30 才有 **9.7~14.7 %**（都稳在 35 fps）。
  实测参考值、判定项与量测口径见 `tests/README.md`。
- **VPU 占用率怎么看**：内核 MPP 走 **procfs**（不是 debugfs）——
  `cat /proc/mpp_service/load`，每行 `load: A% utilization: B%`；
  **`utilization` 才是硬件真正忙的比例**（`load` 是 MPP 任务占用，含排队）。
  镜像里由 **`lsp-mpp-load.service`**（`overlay/usr/local/sbin/lsp-mpp-load.sh`）开机设好
  `load_interval=1000` —— 因为这个 procfs 设置**重启就恢复 0**，而写它需要 root。
  没设时用 `/proc/mpp_service/<核>/task_count`（≈ 解码帧率）与 `/proc/interrupts` 里的
  rkvdec/vdpu 中断速率兜底（这两个不需要 root）。
- **打开统计的开销 ≈ 0**（2026-09-12 工程机实测，4K H.264 300 帧 × 3 轮交错 A/B，`mpi_dec_test`）：
  关 `load_interval=0` **5452.3 ms** ／ 开 `1000` **5446.3 ms** ⇒ **-0.11%**（噪声级）。
  驱动侧只是每任务多几次 `ktime_get()` 与每窗口一次整数除法；`schedule_delayed_work(timeout_work)`
  那个超时看门狗跟这个开关无关（无条件执行）。同一轮里 4K30 流实测解码 **55 fps**（≈1.85× 实时）。
- 装它**只需要 6 个 deb**（4 个 chromium + `libv4l-rkmpp` + 新版 `librockchip-mpp1`）。
  ⚠️ 上游那套 `libv4l-0t64 1.32.0-2ubuntu1` / `libv4lconvert0t64` **实测不需要**，
  而且要它会把 `v4l-utils`（`v4l2-ctl`）、`qv4l2`、`libv4l-dev` 顶掉。
- ⚠️ **必须通过 `/usr/bin/chromium` 启动**：硬解开关在 deb 自带的
  `/etc/chromium.d/default-flags` 里，直接跑 `/usr/lib/chromium/chromium` 不会启用硬解。
- **换浏览器的事**（2026-09-30 实测）**已单独成篇** → [browser-choice-hardware-decode.md](browser-choice-hardware-decode.md)：
  Firefox 156 **不能硬解**、启动比 Chromium **慢约 1.4 s** ⇒ 继续用 Chromium；
  "换个更轻的 Chromium 但保留硬解"**可行但要自己编**（现成轻量版都不带 H.264），
  收益上限只有 ~0.3–0.5 s，**暂不做**。

## 1. 三个部件

| 部件 | 我们装的版本 | 作用 |
| --- | --- | --- |
| Chromium（3 个 deb） | `151.0.7922.169-1`（chromium / -common / -sandbox；**不装 `-l10n`**） | 浏览器本体，构建时带 V4L2 VDA/VEA |
| `libv4l-rkmpp` | `1.8.0-1` | libv4l2 插件，把"假 V4L2 设备"的 ioctl 转成 MPP 调用 |
| 内核 MPP 栈 | RK3588 BSP 内核**内建**（`mpp_service`/`mpp_rkvdec2`/`mpp_vdpu2`/`mpp_rkvenc2`/`mpp_vepu2`） | 真正的编解码硬件驱动，不是模块 |

### 1.1 这套东西的"官方来源"（2026-09-15 查证）

**每个部件的上游**（都是 deb 元数据/源码里查到的，不是猜的）：

| 部件 | 官方上游 | 依据 |
| --- | --- | --- |
| `libv4l-rkmpp` | [JeffyCN/libv4l-rkmpp](https://github.com/JeffyCN/libv4l-rkmpp)（Rockchip 的 Jeffy Chen）| deb 的 `Maintainer: Jeffy Chen <developer@example.com>`、`Homepage:` 指这个仓库、changelog 也是他签的（`1.8.0-1`）|
| `librockchip-mpp*` | [rockchip-linux/mpp](https://github.com/rockchip-linux/mpp)（Rockchip 官方 MPP）| deb 的 `Source: mpp`、`Maintainer: Randy Li <developer@example.com>`（Debian/Ubuntu 侧的打包者）；我们这份的 changelog 写着 "Build upstream commit `c08762eb`（upstream tag 1.1.0）for Ubuntu Noble —— **Local Builder <builder@localhost>**" ⇒ **是本地给 noble 重编的**，不是官方仓库里的现成包 |
| Chromium 本体 | **Debian 的 chromium 源码包**（Debian Chromium Team）| deb 的 `Maintainer: Debian Chromium Team`、`Homepage: chromium.org`，changelog 就是 Debian 的 `151.0.7922.169-1 unstable` 那篇（含 CVE 列表）|

**把这三样拼成"装上就能用"的那份发行版集成** = JianFeng Liu 的 Launchpad PPA
[`ppa:liujianfeng1994/rockchip-multimedia`](https://launchpad.net/~liujianfeng1994/+archive/ubuntu/rockchip-multimedia)
（"rockchip legacy multimedia packages"，Noble + Jammy）。这个 PPA 里有：
`chromium …+rkmpp`（Noble 上是 `132.0.6834.159-1~deb12u1+rkmpp`）、`chromium-browser …+rkmpp8`、
`mpp …~noble`、`libv4l-rkmpp`、`v4l-utils 1.26.1-4build3+rkmpp1`、`rockchip-multimedia-config`、
`librga`、`widevine` 等。**本镜像的 apt 源里就配了这个 PPA**
（`/etc/apt/sources.list.d/` 里是 `ppa.launchpadcontent.net/liujianfeng1994/rockchip-multimedia/ubuntu/`），
板上 `v4l-utils 1.26.1-4build3+rkmpp1` 与 PPA 的版本号**逐字一致** —— 这也是"PPA 版"这个说法的出处。

⚠️ **`packages/Chromium-rkmpp/` 里那三份 deb 不是这个 PPA 的现成二进制**：
- 版本号是 Debian 自己的 `151.0.7922.169-1`（**没有** `+rkmpp` 后缀），changelog 也是 Debian 原文；
- 但 control 里被加了 `Depends: libv4l-rkmpp, librockchip-mpp1`，包里多了
  `usr/lib/systemd/system/chromium-rkmpp-nodes.service`（创建假 `/dev/video-dec0`、`/dev/video-enc0`），
  `default-flags` 也带了 arm64 的 Wayland + `AcceleratedVideo*` 段。
  ⇒ 它是**"Debian chromium 源码 + rkmpp 补丁"的第三方重打包**，deb 里没有任何构建者信息
  （没有 `.buildinfo`、changelog 也没加条目），**从包本身追不到是谁编的**。
- 那个"假 V4L2 节点"的做法在 Rockchip 生态里是通用的（Yocto meta-rockchip 有同名改动
  [`chromium-ozone-wayland: Create dummy V4L2 device for V4L2 VEA`](https://code.example.com/nova/meta-rockchip/commit/095beaa81381d55f309fb2e232970aefc08db2d2)）。
- 想要**可溯源/可复现**：用 PPA 的源码（在带 PPA 的机器上 `apt-get source chromium`，或直接看
  `+rkmpp` 那版源码包）对照，把上面三处改动（Depends / nodes.service / default-flags）整理成 patch；
  或者干脆改成直接装 PPA 的 `chromium` 包（代价：版本会跟 PPA 走，且要联网装）。

工作链路：Chromium 的 `V4L2VideoDecoder` 打开 `/dev/video-dec0` → libv4l2 预处理层把请求交给
`/usr/lib/aarch64-linux-gnu/libv4l/plugins/libv4l-rkmpp.so` → 插件调 `mpp_create/mpp_init/…` 走硬解。
编码侧对应 `/dev/video-enc0`。

插件 1.8.0 里只有三个配置键（`strings` 实测）：`type=`、`codecs=`、`max-width=`。
假设备文件是**文本文件**，内容是能力描述，例如工程机开机后（装 chromium 之前）看到的：

```
$ cat /dev/video-dec0            # /dev/video-enc0 里只有 "enc"
type=dec
codecs=VP8:VP9:H.264:H.265:AV1
max-width=7680
max-height=4320
```

## 2. 硬解开关在哪（最容易踩的一条）

deb 自带 `/etc/chromium.d/default-flags`，aarch64 上追加：

```sh
if [ "$(uname -m)" = "aarch64" ]; then
  export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --ozone-platform=wayland"
  export CHROMIUM_FLAGS="$CHROMIUM_FLAGS --enable-features=AcceleratedVideoDecoder,AcceleratedVideoDecodeLinuxGL,AcceleratedVideoEncoder"
fi
```

这个文件由 `/usr/bin/chromium`（包装脚本）`source`，而包装脚本最后 `exec /usr/lib/chromium/chromium
$CHROMIUM_FLAGS "$@"`。**直接调真实二进制 = 丢掉这三个 feature → 一定走软解**。
实测对照：同一段 1080p30 H.264，直接调真实二进制时 `RKMPP` 日志 **0 行**、渲染进程 48.5% CPU；
走包装脚本后 `RKMPP` 日志 **5794 行**、渲染进程 4.2%。

⇒ 集成时**不要**覆盖 `/etc/chromium.d/default-flags`；桌面 `.desktop` 也要用 deb 自带的
`/usr/share/applications/chromium.desktop`（`Exec=/usr/bin/chromium %U`）。

## 3. 工程机实测（2026-09-12，SD 卡系统，eMMC 未动）

环境：工程机 `<HOST_09>` 从 SD 卡启动（`findmnt -no SOURCE /` = `/dev/mmcblk1p2`），
在**桌面会话里**通过 ssh 起 Chromium（`XDG_RUNTIME_DIR=/run/user/1000`、`WAYLAND_DISPLAY=wayland-0`），
`LIBV4L_RKMPP_LOG_LEVEL=3 LIBV4L_RKMPP_LOG_FPS=1` 打开插件日志。

装了：`chromium/chromium-common/chromium-sandbox/chromium-l10n 151.0.7922.169-1`、
`libv4l-rkmpp 1.8.0-1`、`librockchip-mpp1|mpp-dev|vpu0/demos 1.5.0+git20260811.c08762eb-1~noble1`。
**`libv4l-0t64` 保持镜像里的 PPA 版 `1.26.1-4build3+rkmpp1`**（不升 1.32.0）。

测试素材：`ffmpeg -f lavfi -i testsrc2=size=1920x1080:rate=30 -t 15 -pix_fmt nv12 -c:v h264_rkmpp`
（14.7 MB，顺便验证了 MPP **编码**通路），以及从 test-videos.co.uk 下的 VP9 1080p webm。

| 用例 | 渲染进程 CPU | GPU 进程 CPU | RKMPP 日志 | 其余证据 |
| --- | --- | --- | --- | --- |
| H.264 1080p30（**硬解**） | **4.2 %** | 17.1 % | 5794 行 | `mpp initializing for H.264`；`DEC FPS: 30.0 \|\| Frames: 1440` |
| 同上（**软解**对照：`--disable-features=AcceleratedVideoDecoder,AcceleratedVideoDecodeLinuxGL,AcceleratedVideoEncoder`） | **47.7 %** | 19.9 % | 0 行 | 页面 `decoded=` 帧数照常增长 |
| VP9 1080p30（**硬解**） | 4.5 % | 21.1 % | 5259 行 | `mpp initializing for VP9`；`DEC FPS: 30.0`；日志里 0 条 libvpx 痕迹 |

页面侧（`<video>` + `document.title` 上报，从 CDP `http://127.0.0.1:<port>/json` 读回）：
`t=… decoded=… dropped=3~17 err=0 vw=1920x1080` —— 播放、解码正常，无取流错误。

`navigator.mediaCapabilities.decodingInfo({type:'file', …1080p30…})` 一条命令的探测结果：

```
H.264: supported=true smooth=true powerEfficient=true | VP9: … | AV1: … | HEVC: … | VP8: …
```

五个编码格式都报 `powerEfficient=true`（即走平台解码器）。

后续用 `tests/browser-codec.sh` 做了**实际码流**验证：H.264 与 HEVC 1080p30 都 `mpp initializing for …`
（35 fps 上下，渲染进程 CPU 5~6%、合计 29~33%）。⚠️ **HEVC 只有硬解一条路**：关掉
`AcceleratedVideo*` 之后 `<video>` 直接 `MediaError=4`（`MEDIA_ERR_SRC_NOT_SUPPORTED`）——
这个 Chromium 构建里没有 HEVC 软解器。**AV1 仍只有"广告支持"，没有码流实测**（ffmpeg 里没有
libvpx/av1 编码器，本地造不出测试文件）。

网页加载：`https://www.baidu.com` → CDP 读回标题 `百度一下，你就知道`（TLS/渲染/网络都正常）。

回滚点（工程机上）：`/root/state-before-chromium-rkmpp-20260912.{selections,versions}`，
记录了安装前 `dpkg --get-selections` 与会被升级的 11 个包的版本。

## 4. 踩到的点

1. **不走包装脚本就没硬解** —— 见 §2，这是最容易误判成"这套东西不work"的原因。
2. **deb 的 postinst 会自动 `enable` 并 `start` `chromium-rkmpp-nodes.service`**（不需要手工加
   `systemctl enable`，上游构建脚本那句是幂等的）。该服务内容是
   `printf "dec\n" > /dev/video-dec0; printf "enc\n" > /dev/video-enc0; chown root:video …; chmod 0660 …`，
   会**截断**节点文件里原有的能力描述（§1 那个 `codecs=…` 列表 → 只剩 `dec`）。
   实测截断后 H.264/VP9 照样硬解（插件从**节点名**推 `type`，日志 `rkmpp_parse_options: parsing option: dec`），
   但这行覆盖行为值得留意：将来若发现某些格式（VP9/AV1/HEVC）不被认，先怀疑这里。
3. **从 ssh 起浏览器会缺会话总线**：`/run/user/1000/bus` 不存在 → Chromium 打一片
   `Failed to connect to the bus`（不影响解码与渲染，但钥匙串/通知/桌面集成没有）。
   桌面上点图标启动会继承正确会话环境，不是镜像问题。
4. **上游那套 `libv4l-0t64_1.32.0-2ubuntu1` / `libv4lconvert0t64` 是个坑**：`apt-get install` 模拟显示
   它会 `Remv v4l-utils libv4l-dev librockchip-mpp-dev rockchip-mpp-demos`（同一版本号互相依赖）。
   实测**不装它 Chromium 照样硬解**，所以本工程不采纳这两个 deb。
5. Chromium 要按**固定路径** `dlopen` `/usr/lib/libv4l2.so`（多架构路径它不找）——
   `rootfs` 阶段将该路径链接到 `aarch64-linux-gnu/libv4l2.so.0`，不依赖 `libv4l-dev`
   提供的无版本开发链接；**不能删除这个运行别名**。
   上游新提交把这一行去掉了，但那是因为他们的 libv4l 版本不同；实测保留是对的。
6. 工程机 `/dev/video-dec0` 在**开机后**（17:42:31，boot+4s）就有内容，创建者不明
   （用户说属于 RK 闭源驱动，不再深究）——记下来只是解释 §4.2 那个"被覆盖"的对照基线。

## 5. 集成进本镜像（2026-09-12 已落地）

| # | 改动 | 现状 |
| --- | --- | --- |
| 1 | `packages/Chromium-rkmpp/`：`chromium` / `chromium-common` / `chromium-sandbox` `151.0.7922.169-1`（共 **126 MB**，`.gitattributes` 已配 `*.deb` 走 LFS） | ✅ 已放进仓库（**不含 `chromium-l10n`**：用户明确不要中文界面包） |
| 2 | `packages/arm64/`：新增 `libv4l-rkmpp 1.8.0-1`；`librockchip-mpp{1,-dev,-vpu0}` / `rockchip-mpp-demos` 换成 `1.5.0+git20260811.c08762eb-1~noble1`（旧 `1.5.0-1` 四个已删） | ✅ |
| 3 | `scripts/build-lsp-image.sh`：桌面阶段前把 deb 暂存到 `${chroot}/tmp/Chromium-rkmpp/`，桌面会话里 `apt-get -y install --no-install-recommends /tmp/Chromium-rkmpp/*.deb`（用 apt 才能补齐 `libgtk-3-0t64`/`libdav1d7`/`libopenh264-7` 这类仓库依赖）；再 `systemctl --root= enable chromium-rkmpp-nodes.service`；固定路径 `/usr/lib/libv4l2.so` 链接到 multiarch 运行库 `.so.0` | ✅ |
| 4 | `overlay/etc/dconf/db/local.d/00-favorite-apps` 加 `chromium.desktop`；删掉过时的 `overlay/etc/chromium-browser/default`、`overlay/usr/lib/chromium-browser/initial_preferences`、`overlay/usr/share/applications/chromium-browser.desktop` | ✅ |
| 5 | `tools/verify-image.sh` 新增分组「Chromium（RKMPP 硬件加速）」（13 项：包装脚本/flags/固定路径 libv4l2/插件/服务 enable/桌面项/收藏夹，live 再加服务状态与节点权限）；`notes/system/system-integration.md` §6.1 登记 | ✅ 用假 rootfs harness（从真 deb 解出 `/usr/bin/chromium` + `/etc/chromium.d/default-flags`）验过 **13/13 通过** |

⚠️ 写体检项时踩到一个坑：`systemctl enable` 生成的软链是**绝对路径**
（`/etc/systemd/system/multi-user.target.wants/chromium-rkmpp-nodes.service → /usr/lib/systemd/system/…`），
而 `verify-image.sh` 的 `exists()` 用 `[ -e ]` 判定 —— 离线 `--root build/rootfs` 时它会拿**宿主机**
的文件系统去解析那个绝对路径，构建机上并没有 chromium，于是报**假失败**。这一条改成
`[ -L ] || [ -e ]`（软链在就算 enable）。

**没有采纳的上游部分**（别照抄上游脚本）：

- `libv4l-0t64_1.32.0-2ubuntu1` + `libv4lconvert0t64_1.32.0-2ubuntu1`：实测会
  `Remv v4l-utils libv4l-dev librockchip-mpp-dev rockchip-mpp-demos`，而本镜像用的 PPA 版
  `libv4l-0t64 1.26.1-4build3+rkmpp1` 就够（工程机上硬解照常）。
- 上游那批 **gstreamer 1.18.5 deb**（`libgstreamer1.0-0` 等）：本镜像用 noble 的 1.24 + PPA 的
  `gstreamer1.0-rockchip1`，装 1.18.5 是一次全栈降级，且与 Chromium 无关，不采纳。
- 上游删掉的 Chromium 固定路径 libv4l2 别名与 `ln -Tsf lib /usr/lib64`：**保留**（见 §4.5）。

仍未验证：**AV1** 的实际码流硬解（只有 `mediaCapabilities` 的"支持"声明），
以及 YouTube 那类 VP9/AV1 **在线播放**的长时间稳定性（本地文件播放没问题，但那是另一条路）。

**从上游 `build-rootfs.sh` 的改动里借鉴的两点**（该上游脚本 **2026-09-24 已从仓库删除**；
此处记录的是当年借鉴的来源。其余按上面的理由不采纳）：

- `dpkg -i /tmp/*.deb` 之后补一次 `apt --fix-broken install`：`dpkg -i` 不做依赖解析，失败会把包留在
  "unpacked 未配置"状态，而 `set -e` 会在这一步直接打断整个 `rootfs` 阶段、报错点还离原因很远。
  本工程照做，但**加了守卫**：`dpkg -i` 失败只打 `[warn]` 让 apt 去补，随后逐个核对
  `ffmpeg` / `fire-config-special` / `librockchip-mpp1` / `libv4l-rkmpp` / `librockchip-vpu0`
  仍是 `install ok installed`（`--fix-broken` 在"修不好"时会把包**删掉**，那是静默降级），缺了直接 fail。
- 浏览器 deb **单独暂存到一个目录**再装（而不是混进 `/tmp/*.deb` 一起 `dpkg -i`）：
  它的依赖要靠 apt 从源里补。

### 5.1~5.2 首次启动的两个弹窗 / 书签栏三个默认收藏 → 见 [chromium-first-run-defaults.md](chromium-first-run-defaults.md)

首启弹窗与默认收藏分别是怎么在镜像里关掉的**已搬成单独一篇** →
**[chromium-first-run-defaults.md](chromium-first-run-defaults.md)**。


## 6. 上位机自带素材的硬解实测（2026-09-30，开发板 eMMC）

用户："上位机上有几个视频素材，你试一下这几个素材可以硬件解码吗？"
—— 素材在 `oem-host/media/`（镜像里 `/opt/oem-host/media/`），**5 个 mp4**：

| 文件 | 视频 | 音频 |
| --- | --- | --- |
| `camera-bind-chest-plug.mp4` / `camera-bind-left-2-plug.mp4` / `camera-bind-right-plug.mp4` / `camera-usb-unplug.mp4` | **H.264 High @L5.1，3840×2160@30，19.2~19.9 Mbps** | AAC-LC（48 kbps） |
| `camera-usb-unplug-720p.mp4` | H.264 High @L4.0，720×1280@30，1.1 Mbps（竖屏） | 无 |

### 6.1 结论：5 个全部硬解 ✓（两条路径都验了）

**① 调用链层**（`ffmpeg -c:v h264_rkmpp` 解整个文件 vs 原生软解；rkvdec 峰值取两核之和）：

| 文件 | 硬解 | 软解 | rkvdec 峰值（全速解） |
| --- | --- | --- | --- |
| camera-bind-chest-plug | 5.88 s | 8.67 s | 85% |
| camera-bind-left-2-plug | 6.34 s | 10.29 s | 100% |
| camera-bind-right-plug | 7.86 s | 12.93 s | 106% |
| camera-usb-unplug | 4.41 s | 6.90 s | 92% |
| camera-usb-unplug-720p | 0.41 s | 0.58 s | 172%（连解 8 遍才采到，见下） |

**② 真实路径层**（cage 探针会话 + `/usr/bin/chromium` 播 `file://` 素材）：
Chromium 进程里 **`libv4l-rkmpp` 已映射**，播放 4K 素材时 **`rkvdec-core` 两核合计 ~18~20%**
⇒ 浏览器侧走的就是 VPU ✓。

### 6.2 两个数字别混：全速解 vs 实时播

`ffmpeg -f null` 是**全速**解（比实时快很多）⇒ 那 85~172% 是它的负载；
**实时播 4K30 只要 ~18~20%** ⇒ 硬件余量很大（同时播几路都轻松）。
rkvdec 单个 core 报 0~100%，所以两核之和会 >100%，这是正常的。

### 6.3 踩到的坑（下次直接照这个来）

1. ⛔ **插件日志 `mpp initializing for H264` 要靠 `LIBV4L_RKMPP_LOG_LEVEL=3` 才打**，而且
   `systemd-run --property=Environment=...` 传多个变量这次**没打通**（Chromium 环境里没看到）。
   ⇒ **别把这条日志当唯一判据**，用「Chromium 进程里 `libv4l-rkmpp` 已映射」+
   「`/proc/mpp_service/load` 里 `rkvdec-core` 有负载」这两条 —— 它们直接说明走没走 VPU。
   （`tests/browser-codec.sh` 能拿到日志是因为它是**自己启动** Chromium 并带上了环境变量。）
2. ⛔ 探针单元带 `PAMName=login` ⇒ Chromium 的 stderr **不挂在单元上**（`journalctl -u` 查不到），
   要按内容查全量 journal —— 和 `lsp-kiosk` 那边同一个坑。
3. ⛔ `/proc/mpp_service/load` 是 **1 秒窗口**：`camera-usb-unplug-720p` 一遍只跑 0.41 s，
   采样必然看到 0；**连解 8 遍**才采到 172%。
4. `ffprobe -of csv=p=0` 对有音轨的文件会输出 `h264,`（多一个逗号），拿它做 `case` 匹配会落空。

### 6.4 可复用工具

`tests/media-hardware-decode.sh`（新增）：把上面两层证据一次跑完 ——
`sudo bash tests/media-hardware-decode.sh <视频...>`（`--no-browser` 只跑第一层）。
板上实测 5 个素材 **7 通过 / 0 失败**。

## 6.5~6.7 换浏览器还能硬解吗 → 见 [browser-choice-hardware-decode.md](browser-choice-hardware-decode.md)

"换个浏览器 / 换个更轻的 Chromium 还能不能硬解、值不值得"这三节（Firefox 实测、启动耗时对照、
轻量 Chromium 评估）**已搬成单独一篇** → **[browser-choice-hardware-decode.md](browser-choice-hardware-decode.md)**。

