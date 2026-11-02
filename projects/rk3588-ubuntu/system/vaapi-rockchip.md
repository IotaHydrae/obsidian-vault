# RK3588 的 VA-API 硬解驱动（rockchip-vaapi）

> 一句话：**这个驱动补的是"通用 VA-API 通路"**。Chromium 有自己的路（V4L2 VDA + libv4l-rkmpp），
> 用不到它；但在它装上之前，板上**所有走 VA-API 的软件都只能软解**，而且没有别的路可走。
>
> 集成形态：`packages/arm64/rockchip-vaapi_1.0.12-1+lsp1_arm64.deb`（约 39 KB，只含一个 `.so`）。
> 重新生成：`packages/rockchip-vaapi/build-rockchip-vaapi.sh`。上游：woodyst/rockchip-vaapi（LGPL-2.1）。

---

## TL;DR

- 它补的是**通用 VA-API 通路**；**Chromium 不走这条**（有自己的 V4L2 VDA + `libv4l-rkmpp`）。
- 谁受益别夸大：走 VA-API 的应用受益，**Chromium 与 ffmpeg 都不吃它**（§2）。
- 板上实测（2026-09-30 开发板 eMMC）见 §3；集成方式（deb + 镜像侧）见 §4。
- 边界见 §5；升级 / 复现见 §6。

## 1. 为什么要它

板子上`libva2`（**加载器**）本来就有，但 `/usr/lib/aarch64-linux-gnu/dri/` 里
**一个 `*_drv_video.so` 都没有** ⇒ VA-API 起不来 ⇒ 走这条路线的软件全部软解。

这个驱动把 libva 接到 **Rockchip MPP**（`librockchip-mpp1`），实现 H.264 / HEVC / VP9 的
`VAEntrypointVLD`，支持 DRM PRIME 2（NV12/DMABUF）零拷贝导出。本体只有 2 个 C 文件，
编出来 66 KB。

## 2. 谁受益（别夸大：Chromium 和 ffmpeg 都不吃它）

| 软件 | 装之前 | 装之后 |
| --- | --- | --- |
| Chromium | V4L2 VDA + libv4l-rkmpp + 伪节点 `/dev/video-dec0` | **不变** —— 它根本不走 libva |
| FFmpeg | `-hwaccel rkmpp` | **不变** —— 这版 ffmpeg 的 `-hwaccels` 只有 `drm rkmpp`，**没编 vaapi** |
| GStreamer | `mppvideodec`（`gstreamer1.0-rockchip1`） | 多一条 `va` 插件的路（`libgstva.so` 1.24.2，随 `gstreamer1.0-plugins-bad`） |
| **mpv** | **完全没有硬解通路** | `--hwdec=vaapi-copy` 走 VPU |
| **Firefox** | 必软解 | 开 pref + `MOZ_DISABLE_RDD_SANDBOX=1` 后硬解 |

mpv 那条最能说明问题：mpv 的后端只有 `vaapi / v4l2m2m / vdpau / vulkan`，**没有 rkmpp**；
而 `v4l2m2m` 在板上直接失败：

```
[ffmpeg/video] h264_v4l2m2m: Could not find a valid device
[ffmpeg/video] h264_v4l2m2m: can't configure decoder
```

（板上没有真的 V4L2 解码器，`/dev/video-dec0` 只是 `chromium-rkmpp-nodes.service` 用
`printf "dec" >` 造出来的伪节点，`v4l2-ctl` 认不出。）⇒ **装驱动前 mpv 是 100% 软解**。

## 3. 板上实测（2026-09-30，开发板 eMMC）

### 3.1 驱动能起来：`vainfo`

```bash
$ vainfo                     # ⚠️ 故意不设任何环境变量
libva info: Trying to open /usr/lib/aarch64-linux-gnu/dri/rockchip_drv_video.so
libva info: Found init function __vaDriverInit_1_20
libva info: va_openDriver() returns 0
vainfo: Driver version: Rockchip MPP VA-API Driver 0.1
      VAProfileH264ConstrainedBaseline: VAEntrypointVLD
      VAProfileH264Main / High / High10: VAEntrypointVLD
      VAProfileHEVCMain                : VAEntrypointVLD      ← Main10 已去掉，见下
      VAProfileVP8Version0_3           : VAEntrypointVLD
      VAProfileVP9Profile0 / Profile2  : VAEntrypointVLD
```

**不需要 `LIBVA_DRIVER_NAME`**：libva 会自动选中目录里唯一的 `*_drv_video.so`。

> ⚠️ **2026-10-02 起 `VAProfileHEVCMain10` 不再 advertise**（`packages/rockchip-vaapi/`
> 的 `0001-no-hevc-main10-advertise.patch`，版本 `1.0.12-1+lsp1`）。原因：MPP 解得出 10-bit HEVC
> （`ffmpeg -c:v hevc_rkmpp` 正常），但**VA-API 的 surface 导出这条路径在这块板上直接失败**
> —— `AVHWFramesContext: Failed to sync surface … 23 (internal decoding error)` 且 rkvdec 全程 0%。
> advertise 一个履约不了的能力会让应用选中后**报错**而不是退回软解。
> 打 patch 后应用会干净退回软解（只剩一行 `hevc: Failed setup for format vaapi`），
> 4K30 H.264 硬解不受影响（验收 5 通过 / 0 失败）。
> 完整三路对照见 `notes/system/rock-5b-issue4-repro.md` §7.2，边界与恢复前提见
> `packages/rockchip-vaapi/README.md`。

### 3.2 Firefox：A/B（同一次会话，都播那个 4K H.264 素材）

| | `rkvdec` 峰值 | Firefox 进程树 CPU |
| --- | --- | --- |
| 不给 VA-API 环境 | **0%** | 140% |
| `LIBVA_DRIVER_NAME=rockchip` + `MOZ_DISABLE_RDD_SANDBOX=1` | **10%** | **64%** |

驱动日志 `/tmp/rkvaapi.log`（`RK_VAAPI_LOG`）：`assign_mpp_frame` 292 次、
`ExportSurfaceHandle` 582 次 ⇒ 零拷贝 dmabuf 导出在跑。截图为 4K 画面正常播放。

> ⚠️ **更正 §6.5 的结论**：`notes/system/browser-choice-hardware-decode.md` §6.5 判过"换 Firefox 必掉软解"，
> 当时的前提是"板上没有任何 `*_drv_video.so`" —— 那个前提**已经不成立**。
> 但换浏览器仍不划算：启动耗时 Firefox **2.39/2.39/2.43 s** vs Chromium **0.95/0.99/0.93 s**
> （判据：`Started lsp-kiosk` → 上位机访问日志第一条页面请求，热缓存；方法见
> `chromium-rkmpp.md` §6.6）。

### 3.3 mpv

```
$ LIBVA_DRIVER_NAME=rockchip mpv --no-audio --vo=null --hwdec=vaapi-copy <4K mp4>
Using hardware decoding (vaapi-copy).      ← 同时 /proc/mpp_service/load 里 rkvdec 有负载
```

⛔ **验证时的坑**：`--vo=null` 配 `--hwdec=vaapi`（**不带 `-copy`**）会**静默回退软解** ——
硬件帧交不到 null VO 上，mpv 只在日志里打一版 `VO: [null] … yuv420p`，rkvdec 全程 0%。
所以判据必须是「驱动自报走了硬解」+「VPU 有负载」两条，跟
`chromium-rkmpp.md` §6.3 那条"别把日志当唯一判据"是同一个道理。

### 3.4 顺带修掉一个既有 bug：`/etc/mpv/mpv.conf` 里的 `hwdec=rkmpp`

用户反馈"mpv 之前能硬解，是不是被改坏了？" —— 查完：**不是我们改的，而且它在这个镜像上
从来没生效过。**

镜像里 `overlay/etc/mpv/mpv.conf` 从**仓库第一个 commit** 起就只有一行 `hwdec=rkmpp`；
而板上 mpv 是 Ubuntu noble 的 `mpv 0.37.0-1ubuntu4`，链的是 Ubuntu 的
`libavcodec60 7:6.1.1-3ubuntu5`。**裸跑**（不带任何参数、走 /etc/mpv/mpv.conf）实测：

```
$ mpv --no-audio --vo=null <4K H.264>
Unsupported hwdec: rkmpp        ← 只打一行错，**不退出**
VO: [null] 2160x3840 yuv420p    ← 接着静默走软解，rkvdec 全程 0%
```

为什么这个值不认：

- mpv 能接受哪些 hwdec 名字，取决于**那份 mpv 二进制里有没有对应的后端**。upstream 文档里
  确实列了 `:rkmpp: requires --vo=gpu (some RockChip devices only)`，但**板上这份没有**：
  `mpv --hwdec=help` 给出的合法值是 `auto / cuda / nvdec / v4l2m2m-copy / vaapi / vdpau / vulkan`
  （+ 各自的 `-copy`）。翻 mpv 0.37.0 的源码树也对得上 —— `video/out/hwdec/` 下只有
  `aimagereader / cuda / drmprime / mac_gl / vaapi / vt / vulkan`，没有 rkmpp 的实现。
- 而且就算名字认了，也还需要 **libavcodec** 提供 `h264_rkmpp`：板上 `libavcodec.so.60` 是
  Ubuntu 的 6.1.1，**没有**。板上的 `rkmpp` 只存在于那个**自包含**的 `/usr/bin/ffmpeg` 里
  （它的上游是 `nyanmisaka/ffmpeg-rockchip`，但野火是**静态**打包的、没有 `libav*.so` ——
  所以别的程序拿不到。详见 `notes/build/deb-packages.md` §1.1）。

⚠️ **别被 `/usr/bin/ffmpeg -hwaccels` 的输出误导**（它打 `drm rkmpp`）：那个 ffmpeg 是
**厂商自包含**的（`ldd /usr/bin/ffmpeg | grep -c libavcodec` = **0**），rkmpp 在它自己肚子里，
**不在系统的 libavcodec 里**。所以"ffmpeg 能 `-hwaccel rkmpp`"和"mpv 能用 `hwdec=rkmpp`"
是两件事。

**那"原厂镜像里的 mpv"去哪了？——它从来不存在**（2026-09-30 事后核对）：

- 上游 `ubuntu-ebf-rk3588`（去品牌化的来源）里，mpv 也是**从 apt 装的**：
  `scripts/build-rootfs.sh`（**2026-09-24 已从仓库删除**，见 [../build/build-internals.md](../build/build-internals.md) §5）的安装列表里就是 `mpv`；`packages/` 下**没有任何 mpv 的 deb**；
  overlay 里只有那份 `etc/mpv/mpv.conf`。
- 我们仓库同样是 apt 的 `DESKTOP_APPS="mpv"`（`scripts/build-lsp-image.sh:514`），
  `packages/arm64/` 里也没有 mpv。
- 两边的 `overlay/etc/mpv/mpv.conf` **逐字节一样**，而且两个仓库的第一个 commit
  SHA 都是 `e01918e5c`（我们是从上游 fork 的、历史被 squash 过）。
- 在两个仓库里全量搜 `rkmpp`：除了那一行配置，只剩 Chromium / ffmpeg / libv4l 的东西，
  **没有任何 mpv 的补丁、deb 或源码**。

⇒ 所以这行不是"某个被打补丁的 mpv 被换掉之后留下的残迹"，而是**从别处抄来的一个
"想要的值"**（仓库里查不到出处，第一个 commit 就是快照）。用户记忆里的"mpv 能硬解"
大概是记混了——同期的 Chromium（V4L2 VDA）和厂商版 ffmpeg（`-hwaccel rkmpp`）**都能硬解**，
而且都会在日志里打 `mpp[...]`，很容易记成 mpv。

**修法**：`overlay/etc/mpv/mpv.conf` 改成 `hwdec=auto-safe`。它在本板（有 `rockchip-vaapi`）会
自动选 vaapi / vaapi-copy，没有驱动就回退软解 —— 换机器、或者哪天驱动没装，都不会变成
"报错/打不开"。修完实测（裸跑、4K）：`Using hardware decoding (vaapi-copy).` + rkvdec ≈ 3%。

**对比 VLC：换 VLC 只会更差**（用户提过"删了换 VLC"）。Ubuntu noble 的 `vlc 3.0.20` 里
`--avcodec-hw` 的合法值只有 **`{any, vdpau_avcodec, none}`** —— **连 vaapi 都没有**
（同样因为那份 libavcodec 没编 vaapi）。实测 `--avcodec-hw=vaapi` 播同一个 4K：
rkvdec **全程 0%**，日志里还一串 `h264 decode_slice_header error`。
⇒ **不换**：mpv 是镜像 `DESKTOP_APPS` 里刻意装的默认视频播放器（还替掉了 Totem 的
mime 关联、进了 GNOME 收藏夹），修配置就够。

### 3.5 4K60 掉帧：不是解码、不是合成器，是 mpv 默认那条老渲染路径

用户先报"`~/Videos` 下那个 4K 播起来 Dropped 非常高"，随后质疑"RK3588 应该能 4K60，
是不是 vaapi 的问题？" —— **用户是对的，我第一轮"平台能力不足"的结论下早了。**

素材：`4K_4_Thetestdata.mp4`，**3840×2160@59.94**，H.264 High L5.2，16.3 Mbps，
1511 帧 / 25.2 s（比我们上位机素材的 4K30 重一倍）。

| 条件 | `frame-drop-count` / 1511 |
| --- | --- |
| `vo=gpu`（mpv 默认）+ 硬解，走 cage | **635~659（42%）** |
| `vo=gpu` + **纯软解** | **634（42%）** |
| `vo=gpu` + 硬解，**停掉 cage、直接吃 DRM** | 558（37%） |
| `vo=gpu` + **合成 4K60 信号**（`av://lavfi:testsrc2`，**完全不经过解码**） | 87 / 240（36%） |
| **`vo=gpu-next`** + 硬解（走 cage） | **16 ~ 75（1~5%）** |
| `vo=dmabuf-wayland` + 硬解 | **1** |
| `vo=null`（只解码不出画） | 0 |
| 对照：4K30 素材 + `vo=gpu` | 1 / 634 |

被逐一排除的嫌疑：

- **解码**：所有情况下 `decoder-frame-drop-count` **全是 0**；VPU 只用到 ~25%。
- **VA-API / 驱动**：纯软解掉一样多；**合成信号（完全没有解码）掉一样多**。
- **合成器**：停掉 cage、mpv 直接 DRM 仍掉 37%。
- **刷新率**：不看 cage/mpv 的自报，直接数 VOP 中断 —— **60 Hz**（`184585→184765 / 3 s`，
  `/proc/interrupts` 的 `fdd90000.vop`），不是"跑在 4K30 模式"。
- **时钟/带宽**：播放中 DDR `dmc` **1848 MHz（满频，负载 7%）**、VOP 750 MHz、
  GPU 200 MHz 且负载仅 6~15% —— 没有任何一个时钟域被打满（把 GPU 锁到 1 GHz 也无改善）。

⇒ **瓶颈是 mpv 0.37 默认的 `vo=gpu` 那条老 GL 渲染路径。**

**修法（已落地）**：`overlay/etc/mpv/mpv.conf` 里加 `vo=gpu-next`。
为什么不用更漂亮的 `vo=dmabuf-wayland`（DROP=1）：它只接受**硬解出来的 dmabuf**、
只适用 Wayland；`gpu-next`（libplacebo）对硬解/软解都适用，适合当默认。
⚠️ 软解 4K60 仍然跟不上（DROP=1054）—— 那是 **CPU** 的账，所以硬解必须走
`hwdec=auto-safe`（§3.4）。

## 4. 集成方式（镜像侧）

- 产物放 **`packages/arm64/`** ⇒ 被 `scripts/build-lsp-image.sh` 的「安装 arm64 专用 deb 包」
  统一 `dpkg -i`，并参与后面的属主/权限归一（本包本来就是 `root:root 0644`，归一不改动任何东西）。
- 同一段的 `for _pkg in …` 白名单里加了 `rockchip-vaapi`：**装不上或被 `--fix-broken` 静默删掉
  就直接 fail**。
- **本包对系统唯一的改动就是那一个 `.so`**：
  - 不设环境变量（libva 自动发现，理由见 §3.1）；
  - ⛔ **不写** `/etc/profile.d/rockchip-vaapi.sh` 和 `/etc/environment.d/60-rockchip-vaapi.conf`。
    ⚠️ **这条要靠我们覆盖 postinst 才成立**：上游的 debconf 模板
    `rockchip-vaapi/configure-environment` 是 **`Default: true`**，非交互 `dpkg -i`
    会走 `install_env` 写出这两个文件，内容含 **`MOZ_DISABLE_RDD_SANDBOX=1`**。
    （我一开始以为 `debian/templates` 是空的、所以不会写 —— **装完发现文件在**，实测纠正。）
    `packages/rockchip-vaapi/build-rockchip-vaapi.sh` 因此在打包前把 `debian/postinst`
    覆盖成"幂等清理"（只 `rm -f` 那两个文件）。板上实测：装我们的包后两个文件不存在、
    驱动与 `vainfo` 正常。
    为什么不要：libva 根本不需要这些变量；而"关 Firefox 的 RDD 沙箱"是**安全性折扣**，
    谁要用谁在自己的启动环境里显式加，不该由镜像替所有人默认接受。
    补充口径（外部仓 `yisding/rock-5b-ysp` 的原话）：**关沙箱只是诊断手段，不是出货配置**；
    正规做法是给 RDD 进程 broker 访问 `mpp_service`/`rga`/`dma_heap` + 一份窄 ioctl 白名单。
    见 `notes/system/rock-5b-ysp-crossref.md` §1.4。
- `tools/verify-image.sh` 新增一组断言（驱动在不在、以及上面那两个文件**必须不存在**）。
- ⚠️ 顺带记一条**对 system 单元无效**的坑：`/etc/environment.d/*.conf` 只有 `systemd --user` 会读；
  `PAMName=login` 的 `pam_env` 读的是 `/etc/environment`；`/etc/profile.d/` 只有登录 shell 会 source。
  所以就算将来要给 kiosk 会话加这类变量，也得写进 unit 的 `Environment=` 或
  `/etc/default/lsp-kiosk`（那个 `EnvironmentFile` 已经在用）。

## 5. 边界

- **只补解码**：`vainfo` 里全是 `VAEntrypointVLD`，**没有编码**入口（编码仍走 `mpph264enc`）。
- **不支持 AV1**（上游说明：MPP 要完整 OBU，VA-API 给不了）。浏览器会回落到 VP9，那个是硬解。
- **Firefox 要 `MOZ_DISABLE_RDD_SANDBOX=1`**：RDD 进程在沙箱里打不开 MPP 设备。
- 上游很年轻（2026-04 建仓，作者自述 3~4 小时 AI 辅助开发），但做了逐帧比对
  （YouTube 720p60：2107/2107 帧与软解逐字节一致）。**当候选能力可用，仍要自己回归**。

## 6. 升级 / 复现

```bash
# 在 arm64（开发板）上；整个包几秒钟
sudo apt-get install -y --no-install-recommends debhelper po-debconf \
    libva-dev librockchip-mpp-dev pkg-config
packages/rockchip-vaapi/build-rockchip-vaapi.sh
```

脚本**钉死上游 commit**（改 `UPSTREAM_COMMIT` 才升级），产物覆盖到 `packages/arm64/`。
当前版本：`1.0.12-1`，上游 commit `08559e8a00e81b6bc8713943485310a88fd2b2b2`（2026-09-24），
deb sha256 `6338f2dbe6c6d80939900cc25fc9e64ee897a0bb709a68ba99c5c3a0537de4c4`
（本地 39,516 B；与上游原包的唯一差别就是那个被覆盖的 `debian/postinst`）。
