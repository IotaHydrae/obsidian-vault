# 换个浏览器还能硬解吗（Chromium vs Firefox 的实测与取舍）

> 本文回答**"能不能换个更轻的浏览器、还能不能保留 RKMPP 硬解"**：
> Firefox 的实测结论（不能硬解）、Chromium vs Firefox 的启动耗时对照、
> 以及"换个更轻量的 Chromium 但保留硬解"的评估（可行但收益太低）。
> Chromium RKMPP 本身的集成/开关/实测见 [chromium-rkmpp.md](chromium-rkmpp.md)（§0~§6.4）。
> ⚠️ 正文**原样搬自该篇的 §6.5~§6.7**（节号保留，便于对照引用）。

## TL;DR

- ⛔ **换 Firefox 会掉硬解**（2026-09-30 实测）—— 与我们另一条结论完全一致
  （见 [rock-5b-ysp-crossref.md](rock-5b-ysp-crossref.md) §1.4/§6.5 的印证）。
- "换个更轻的 Chromium 但保留硬解"：**可行，但收益太低**，暂不做（§6.7 有量化）。


### 6.5 换浏览器（Firefox）还能硬解吗？——**实测不能**（2026-09-30）

> ⚠️ **本节结论已被推翻（2026-09-30 更晚）**：当天后面装上了 `rockchip-vaapi`
> （VA-API → MPP 的驱动，见 [vaapi-rockchip.md](vaapi-rockchip.md)），Firefox 是**能硬解的**
> （rkvdec 0%→10%、CPU 140%→64%）。下面这节的判据没变（"板上一开始没有任何
> `*_drv_video.so`"），只是那个前提被补上了。**换浏览器仍不划算的是启动耗时**：
> Firefox 2.39/2.39/2.43 s vs Chromium 0.95/0.99/0.93 s（§6.6）。本节其余内容作为
> "当时的证据链"保留。

用户问："如果我换一个浏览器，比如 `firefox-156.0.1.tar.xz`（aarch64 官方构建），还能从
上位机网页里播放素材硬解吗？" 答案：**不能，会掉到软解**。实测（cage 探针里跑这份 Firefox，
播 4K 素材，同一套判据）：

| | 画面在播？ | `rkvdec` 负载 | 解码 CPU |
| --- | --- | --- | --- |
| Chromium（现状） | ✓ | **~18~20%**（两核合计） | ~0.4~0.5 核 |
| Firefox 156（这份 tar.xz） | ✓（抓帧均值在变） | **全程 0%** | **RDD 进程 ~195%，全家 325~420%** |

⇒ Firefox 走的是**软解**（RDD 就是它的解码进程）。

**为什么**（三条，都不是"调个开关"能解决的）：

1. **Firefox 的 Linux 硬解路径是 VA-API**（它的二进制里就有 `libva.so.2`、
   `media.ffmpeg.vaapi.enabled`、`media.hardware-video-decoding.*` 这些字符串）。
   而本镜像里只有 libva 的**加载器**（`libva2 2.20.0`），**没有任何 VA-API 驱动**
   —— `/usr/lib/aarch64-linux-gnu/dri/` 里 `*_drv_video.so` 一个都没有
   （`rockchip_dri.so` 是 Mesa 的 GPU 驱动，不是 VA-API）。⇒ VA-API 起不来。
2. Firefox **确实会去探 `/dev/video-dec0`**（日志里 `Sandbox: Couldn't query capabilities of
   video device /dev/video-dec0`），但一是内容沙箱不让它查这条节点，二是**板上这条 V4L2 节点
   是靠 `libv4l-rkmpp` 插件把请求翻译成 MPP 的**，而 libv4l 插件只对**走 libv4l 的程序**生效
   （Chromium 就是），Firefox/FFmpeg 走原始 ioctl ⇒ 谈判不起来。
   实测强行 `LD_PRELOAD=/usr/lib/aarch64-linux-gnu/libv4l2.so.0` 也没有用（rkvdec 仍 0%，
   CPU 反而更高）。
3. Chromium 能硬解，是因为**专门为它做了三件事**：Chromium 151（带 V4L2 VDA）+
   `/etc/chromium.d/default-flags` 打开开关 + `chromium-rkmpp-nodes.service` / `libv4l-rkmpp`
   插件。这套是 Chromium 专属的，换浏览器整套失效。

**要做 Firefox 硬解得先补什么**（都超出"改配置"的范围，可单独立项）：
给这块板提供 **VA-API 后端**（厂商/社区的 mpp→VA-API 驱动；上游 Mesa 不给 RK3588 的 VPU 提供
VA-API），或者让 Firefox 能驱动那条 MPP 节点（现状不行）。**结论：继续用 Chromium** ——
素材已验证硬解，4K30 只占 VPU ~18%。

### 6.6 "换个更轻的浏览器会不会更快"：Chromium vs Firefox 启动实测（2026-09-30）

用户："就算是软件的 Firefox 对比 Chromium，哪个启动上位机更快？"
结论：**Firefox 反而慢约 1.4 s**，而且它已经掉软解（§6.5）⇒ **继续用 Chromium**。

**怎么测的**（复用生产会话、只换 `ExecStart`，比另起 `systemd-run` 探针干净）：
临时 drop-in `/etc/systemd/system/lsp-kiosk.service.d/99-*.conf` 覆盖 `ExecStart`，
`systemctl restart lsp-kiosk`，判据 = journal 单调时钟上
`Started lsp-kiosk.service` → oem-host 访问日志里第一条 `GET / HTTP`。
访问日志靠临时 drop-in `/etc/systemd/system/oem-host.service.d/99-tmp-accesslog.conf`
（把 `ExecStart=` 重置成不带 `--no-access-log` 的那条）打开，测完删掉并 `daemon-reload`。

| 路径 | #1 | #2 | #3 |
| --- | --- | --- | --- |
| Chromium 旧生产路径（本地着陆页 → 上位机） | 1.00 | 0.99 | 0.96 s |
| Chromium 直连 `http://127.0.0.1:8080/` | 0.98 | 0.92 | 0.97 s |
| Firefox 直连（同一 URL，profile 预热过） | 2.36 | 2.44 | 2.41 s |

⚠️ 三条口径**不完全相同**：旧生产路径的第一个 `GET /` 来自本地着陆页的 fetch 探活，
另两条是文档请求。三者只表示浏览器已请求后端，不能据此比较 FCP 或物理首帧。
当前极简着陆页改用 HEAD 探测，不再适用这批 GET 数字；现行流程见
[session-design.md](kiosk/session-design.md)。
⚠️ 这批是**热缓存**数字（没 `drop_caches`）；开机冷缓存下的空窗见 `boot-time-optimizations.md` 第 6 条。

**那 ~1.4 s 花在哪**（用 `/proc/<pid>/stat` 的 starttime 拆，和 journal 单调时钟同源；
`t0` = `Started lsp-kiosk`）：

| Chromium | 时刻 | Firefox | 时刻 |
| --- | --- | --- | --- |
| cage | −0.01 | cage | −0.01 |
| `chromium` 主进程 | +0.25 | `firefox` 包装脚本 | +0.26 |
| zygote ×3 | +0.41 | `crashhelper` | +0.32 |
| gpu-process | +0.65 | `firefox-bin … forkserver` | +1.20 |
| network service | +0.66 | | |
| 第一个 renderer | +0.79 | | |
| **第一条 `GET /`** | **+0.97** | **第一条 `GET /`** | **+2.41** |

⇒ **两边浏览器进程几乎同时在 +0.25 s 起来**，差的 ~1.5 s 全在"进程已存在 → 发出导航请求"之间，
跟磁盘/网络/上位机都无关。原因：Firefox 是 **JS/XUL 启动栈**（解 `omni.ja`、跑 `browser-init`
那一串 `.sys.mjs`），还要做 **profile 初始化**（prefs 迁移、sqlite、session-restore、
telemetry/TOU 策略评估），启动期又多 fork 几个进程；Chromium 是原生 C++、zygote 预 fork。
**"Firefox 更轻量"说的是内存占用/常驻体积，不是启动路径。**

**顺带：Firefox 首启的弹窗怎么关**（用户报"紫色 splash + Welcome to Firefox 弹窗 + 引导页"）：
`browser.aboutwelcome.enabled=false` 只能关 `about:welcome` 那个**标签页**，关不掉新的
"Terms of Use" **模态弹窗**。真正的开关在 `omni.ja/modules/TelemetryReportingPolicy.sys.mjs`：
**`browser.termsofuse.bypassNotification = true`**（`_shouldShowTOU()` 见它直接 return false）。
profile 里最好一起写：`browser.shell.checkDefaultBrowser=false`、
`browser.startup.homepage_override.mstone="ignore"`、
`datareporting.policy.dataSubmissionPolicyBypassNotification=true`。

**两个坑**（测这个时踩的，下次直接照做）：
1. ⛔ 临时单元**一定要给 `TimeoutStopSec`**（`systemd-run --property=TimeoutStopSec=5`）：
   `cage` 收到 SIGTERM 不立即退出，默认 **90 s** 才 SIGKILL ⇒ 每次 stop 卡一分半，屏幕掉到内核控制台
   （现场表现就是"屏幕上报错了"，全是内核日志）。
2. ⛔ `journalctl -o short-monotonic` 的时间戳在 **`$2`**（格式 `[ 2859.903468] msg`）不是 `$1`；
   解析用 `sed -n 's/^\[ *\([0-9.]*\)\].*GET \/ HTTP.*/\1/p'` 最稳。

### 6.7 "换个更轻量的 Chromium 但保留硬解"可行吗 —— 可行，但收益太低（2026-09-30 评估，暂不做）

**先说好消息：硬解不绑定任何厂商魔改版 Chromium。** 板上是**普通 Ubuntu deb**：
`dpkg -S /usr/lib/chromium/chromium` → `chromium`，版本 `151.0.7922.169-1`
（`--version` 自报 `built on Ubuntu 24.04.4 LTS`），**没有 Rockchip 补丁**。硬解是三个独立部件拼的：

1. Chromium 编进了 **V4L2 视频解码器** —— 运行时开关在**包自带的** `/etc/chromium.d/default-flags`
   （仅 aarch64 加）：`--enable-features=AcceleratedVideoDecoder,AcceleratedVideoDecodeLinuxGL,AcceleratedVideoEncoder`
2. **`libv4l-rkmpp 1.8.0-1`**（`/usr/lib/aarch64-linux-gnu/libv4l/plugins/libv4l-rkmpp.so`）把 V4L2 ioctl 翻成 MPP
3. `chromium-rkmpp-nodes.service` 造出伪节点 `/dev/video-dec0`

判据（板上实测）：GPU 进程的 `/proc/<gpu-pid>/maps` 里有 `libv4l2.so` + `libv4lconvert.so`。

⇒ 所以**理论上**换一个更轻的 Chromium 可以保留硬解，前提是新 build 满足：
`use_v4l2_codec=true`、`proprietary_codecs=true` + `ffmpeg_branding="Chrome"`（H.264）、
`ozone_platform="wayland"`，并且**把 flags 接上**（见下面那条集成坑）。

**但现成的"轻量 Chromium"基本都不满足**：ungoogled-chromium / Thorium / Debian chromium 的官方二进制
默认 `ffmpeg_branding=Chromium` ⇒ **不含 H.264**（我们素材全是 H.264 High，直接解不了），
而且多半不开 `use_v4l2_codec`；`chromium-headless-shell` 没有显示、GPU 路径被砍。
⇒ **唯一可靠路径是自己编。**

**自己编的账**：

- 源码 40+ GB、构建产物 60–100 GB，24 核服务器上一次 full build 约 **3–8 小时**（还要 arm64 交叉 sysroot）。
- 能砍的开关：`enable_nacl=false enable_widevine=false enable_remoting=false enable_pdf=false
  use_cups=false enable_swiftshader=false use_kerberos=false symbol_level=0 is_official_build=true`
  ＋上面那三条**必须保留**的。
- 体积构成（`du -a --max-depth=2 /usr/lib/chromium`，共 **368 MB**）：

  | 文件 | 大小 | 说明 |
  | --- | --- | --- |
  | `chromium` | **286 MB** | **已 strip**（`readelf -S` 里没有 debug/symtab），占 76% |
  | `libvk_swiftshader.so` | 26 MB | 软件 Vulkan |
  | `libVkLayer_khronos_validation.so` | 20 MB | Vulkan 校验层 |
  | `resources.pak` | 17 MB | |
  | `icudtl.dat` | 10 MB | |
  | `locales/` | 576 KB | **只有 en-US 一个**，没什么可砍 |

  砍完大概到 150–200 MB —— **Chromium 没有"小而全"的版本**。
- ⛔ **集成坑**：Ubuntu 的 `/usr/bin/chromium` 是个**包装脚本**，会 source `/etc/chromium.d/*`，
  `default-flags`（硬解开关）和我们自己的 `99-lsp-flags` 全靠它，`lsp-kiosk.service` 也是调
  `/usr/bin/chromium`。vanilla 自编**没有这层** ⇒ 得把 flags 直接写进 unit。

**收益上限很小**：稳态热启动已经 **~1.0 s**（§6.6），没多少可省的；能省的只有**开机冷读 I/O**，
而 `chromium-cold-start.md` 实测"把 Chromium 自己那 317 MB 全预热"**只省 0.52 s** ⇒ 二进制砍一半，
量级也就 **0.3–0.5 s**。

**不重编就能做的减法（已查，但省不到启动时间）**：上面那两块大文件
**没有任何 Chromium 进程 map**（用 root 读当时 11 个 `/proc/<pid>/maps` 全查过，命中 0）——
`libvk_swiftshader.so`（26 MB，但它是 **GPU 初始化失败时的软件兜底**，删了要接受这个后果）、
`libVkLayer_khronos_validation.so`（20 MB，只有显式开启校验层才加载）。合计 46 MB（占安装体积 12%）。
⚠️ 它们既然没被 mmap 就**没被读** ⇒ 删了**只省磁盘、不省启动时间**（除非内核 readahead 扫到）。

**结论：这部分先这样，不重编。** 真要试就单独立项：定 GN args → 服务器上一次 full build →
用 `tests/media-hardware-decode.sh`（板上 7 通过/0 失败）和 `/proc/mpp_service/load` 回归硬解。

## 7. 外部资料

- 上游工程与提交：`ubuntu-ebf-rk3588` 的 `e80577f5b`「add Chromium RKMPP hardware acceleration」。
- 插件上游：[JeffyCN/libv4l-rkmpp](https://github.com/JeffyCN/libv4l-rkmpp)（"A rockchip-mpp V4L2
  wrapper plugin for chromium V4L2 VDA/VEA"）。
