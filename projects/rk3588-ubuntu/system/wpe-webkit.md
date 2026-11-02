# WPE WebKit（替代 kiosk 浏览器栈的候选）—— 调研与更正后的构建方案

> 目标：评估"**不用合成器 + 保留硬解**"的浏览器栈。我们现在的 kiosk 是
> `cage`(合成器) + Chromium（走 V4L2 VDA + libv4l-rkmpp）。
> WPE 是 WebKit 的嵌入式端口，**媒体栈是 GStreamer**（板上 `mppvideodec` 已经能跑，
> 见 `notes/system/vaapi-rockchip.md` §2），而且它的平台层自带"直接出屏"的实现。
>
> ⚠️ **本文取代之前口头提过的"WPE WebKit + Cog"方案**：那条是 **legacy 路线**，见 §1。

## TL;DR

- ⛔ **先纠正**：`Cog` / `libwpe` / `WPEBackend-fdo` 都是 **legacy（pre-2.54）** 路线；
  新方案用 WebKit **内置的 `WPEPlatform`**（**直接出屏、不要合成器**），
  而**我们停着的 2.52.6 源码里就有它**（不用升 2.54）。
- ⛔ **选 display 的变量是 `WPE_DISPLAY`（不是 `WPE_PLATFORM`）**，值用**扩展名**：
  `WPE_DISPLAY=wpe-display-drm`。写成 `WPE_PLATFORM=drm` 不报错、也不生效 —— 会静默落到
  **headless** display，于是 view 是 `WPEViewHeadless`、`render_buffer` 是空实现，
  **屏幕全黑**（2026-10-02 的黑屏就是这么来的，见 §3.1）。
- 更正后的构建开关见 §2；当前状态（2026-10-02 已开工）见 §3。
- 它跟其它"换浏览器"候选（Firefox / 轻量 Chromium / QtWebEngine）的关系见 §4。
- 计划 TODO 见 §5；⚠️ **在板上编必须限内存**（WebKit 的 JSC 单编译单元峰值 2 GB，
  `-j8` 会把 kiosk 会话的进程一起 OOM 掉）见 §5.1。

## 1. ⛔ 先纠正：Cog / libwpe / WPEBackend-fdo 都是 **legacy（pre-2.54）**

出处：[WPE 官方 FAQ](https://wpewebkit.org/about/faq.html)，用户 2026-10-02 指出。原文要点：

- **`WPEPlatform` 是随 WPE WebKit 一起发布的平台集成层**：`WPEDisplay` 管 `WPEView`，
  帧走 `WPEBuffer`（`WPEBufferDMABuf` / shm…），WebKit **内置 Wayland / DRM-KMS / headless
  三种实现**，用环境变量选（或自动）。
  ⚠️ **别照抄 FAQ 里的变量名**：2.52.6 的代码读的是 **`WPE_DISPLAY`**，值取 GIO 扩展名
  （`wpe-display-drm` / `wpe-display-wayland` / `wpe-display-headless`）—— 见
  `WPEPlatform/wpe/WPEDisplay.cpp` 的 `wpe_display_get_default()`；留空则遍历已注册实现、
  **谁先 connect 成功用谁**（本板实测会落到 headless，这正是 §3.1 黑屏的根因）。
- **"Since 2.54, WPEPlatform is the default and recommended API."**
- **"Do I still need libwpe and a WPE backend? — No."**：常见目标（Wayland / DRM-KMS / headless）
  全都由内置实现覆盖。`libwpe`、`WPEBackend-fdo`、`Cog` 属于 **pre-2.54 的 legacy API**，
  只为已有部署保留。
- **"What is (and isn't) Cog? (legacy)"** —— Cog 就是 legacy API 上的那个 launcher。
  新项目**不需要**它：用 WPEPlatform，launcher 只有几行。
- **Wayland 不是必需的**：`WPEDisplayDRM` **直接把网页内容渲染到屏幕上，完全没有合成器**
  —— 原文说这在 **kiosk / appliance 产品上非常常见**（正是我们的场景）。

⇒ 所以之前的方案（`libwpe` 1.16.3 + `wpebackend-fdo` 1.16.1 + `wpewebkit` + **Cog 0.18.5**）
**选错了组件**：多背三个包，而且用的是要被淘汰的那条 API。

## 2. 更正后的方案（更小、更对）

### 2.1 不用升到 2.54 —— 我们停在板上的 2.52.6 里**已经有 WPEPlatform**

2026-10-02 在板上的源码树里核对过（`/home/board/wpe-build/wpewebkit-2.52.6`）：

- `Source/WebKit/WPEPlatform/wpe/{drm,headless,wayland}/...` **都在**，
  包括 `wpe-platform-drm.pc.in`；
- `Source/cmake/OptionsWPE.cmake` 里的开关：

| 开关 | 默认 | 说明 |
| --- | --- | --- |
| `ENABLE_WPE_PLATFORM` | `= ENABLE_DEVELOPER_MODE`（**默认关**） | 必须**显式打开** |
| `ENABLE_WPE_PLATFORM_DRM` | **ON** | 依赖 `USE_GBM` |
| `ENABLE_WPE_PLATFORM_HEADLESS` | **ON** | CI/测试用 |
| `ENABLE_WPE_PLATFORM_WAYLAND` | ON | 我们不需要（要合成器） |

且 `WEBKIT_OPTION_CONFLICT(ENABLE_WPE_PLATFORM, ENABLE_WPE_1_1_API)` ⇒
**WPEPlatform 与 legacy API 互斥**，可以**只开 WPEPlatform**；配置里还有一句
`FATAL_ERROR "At least one of ENABLE_WPE_PLATFORM or ENABLE_WPE_LEGACY_API needs to be enabled"`。

⇒ **构建配置改成**（其余保持我们原先那套"尽量少编"的开关）：

```bash
cmake -S wpewebkit-2.52.6 -B build -G Ninja \
    -DPORT=WPE -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_WPE_PLATFORM=ON \
    -DENABLE_WPE_LEGACY_API=OFF \
    -DENABLE_WPE_PLATFORM_DRM=ON \
    -DENABLE_WPE_PLATFORM_HEADLESS=ON \
    -DENABLE_WPE_PLATFORM_WAYLAND=OFF \
    …（`packages/…`/本仓笔记里那批精简开关：`USE_SYSTEM_MALLOC=ON`、关 introspection/doc/
      minibrowser/webdriver/speech/gamepad/media-session/webrtc/avif/jpegxl、
      `USE_LIBBACKTRACE=OFF` —— 后者是实测必须的，noble 没有 libbacktrace）
```

⇒ **依赖也少**：不再需要 `libwpe` / `wpebackend-fdo`（板上前一版构建装过的
`/usr/local/lib/aarch64-linux-gnu/libwpe-1.0.so*` 与 `libWPEBackend-fdo-1.0.so*` 就成了废件，
清理即可）。

### 2.2 launcher 就 9 行（官方 FAQ 给的原文）

```c
#include <wpe/webkit.h>
int main(int argc, const char *argv[]) {
    g_autoptr(GMainLoop) loop = g_main_loop_new(nullptr, false);
    g_autoptr(WebKitWebView) view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, nullptr));
    webkit_web_view_load_uri(view, (argc > 1) ? argv[1] : "https://wpewebkit.org");
    g_main_loop_run(loop);
    return EXIT_SUCCESS;
}
```

编译：`pkg-config --cflags --libs wpe-webkit-2.0 wpe-platform-2.0`；
运行：**`WPE_DISPLAY=wpe-display-drm WPE_DRM_DEVICE=/dev/dri/card0 ./my-launcher http://127.0.0.1:8080/`**
（DRM/KMS，无合成器）。⛔ 变量名是 **`WPE_DISPLAY`**、值是**扩展名**（`wpe-display-drm`）；
`WPE_PLATFORM=` 不是这个栈里的变量（写了不报错、也不生效，会落到 headless ⇒ 黑屏，见 §3.1）。
（`WPE_DISPLAY=wpe-display-wayland` 才需要 compositor，例如 Weston；`wpe-display-headless` 用于 CI。）

### 2.3 验收要测什么

1. **能出画**：`WPE_PLATFORM=drm` 起 launcher，屏幕上直接出现上位机页面（抓帧确认），
   且**没有**任何合成器进程（`cage`/`weston` 都不在）。
2. **能不能硬解**：WPE 的媒体后端是 GStreamer ⇒ 播上位机的 4K 素材，看
   `/proc/mpp_service/load` 的 `rkvdec` 是否有负载（这正是我们的判据口径，
   见 `tests/media-hardware-decode.sh` 的第①②层思路）。
3. **跟现状比**：进程树少一个合成器、启动时间、以及 §2.3.2 那条硬解是否成立。
   ⚠️ 参考：Chromium 那条链是 `libv4l-rkmpp` + 伪节点；WPE 这条是 GStreamer `mppvideodec`
   —— 两条不同的通路，结论要分开记。

## 3. 当前状态（2026-10-02）

- 板上 **`/home/board/wpe-build`（664 MB）**：`wpewebkit-2.52.6` 源码 + 一个**用旧配置
  （没开 `ENABLE_WPE_PLATFORM`、带了 Cog 那套）编到一半的 build 目录**。
  ⇒ 恢复时要**重配**（上面那套新开关），不要直接用旧 build 目录续编。
- 已装的废件：`/usr/local` 下的 `libwpe-1.0.so*`、`libWPEBackend-fdo-1.0.so*`
  （前一版方案为 Cog 装的）。新方案用不到。
- **2026-10-02 已开工（用户："编 wpe"）**：按 §2.1 的新开关重配完成（`ENABLE_WPE_PLATFORM=ON`、
  `ENABLE_WPE_LEGACY_API=OFF`、`..._DRM=ON`、`..._HEADLESS=ON`、`..._WAYLAND=OFF`，
  另加精简开关 `USE_AVIF=OFF` / `USE_JPEGXL=OFF` / `ENABLE_WEB_RTC=OFF` 等），配置 rc=0；
  随后 `ninja -j8` 后台开编（**8662 个目标**）。
  · 旧 build 改名保留为 `build.old-legacy-<时间戳>`（114 MB），没有直接删。
  · 配置脚本 `/home/board/wpe-build/wk-configure-platform.sh`（幂等：build 里已经是新配置就原地重配）。
  · 构建日志 `/home/board/wpe-build/wk.platform.build.log`；配置日志 `/home/board/wpe-build/wk.platform.configure.log`。
  · ⚠️ `wpe-build/` 整个目录是 **root 属主**，配置/构建都要 sudo。
  · 规模：8 核上初测 ~150 目标/分钟（前面的都是头文件生成这类轻活），全程按 1~2.5 h 估。

### 3.1 ✅ 2026-10-02 实测：编译/安装/加载页面都成功；**DRM 直出已出画**

**一句话结论：黑屏跟 DMA-BUF / 格式 / GPU 都无关，是"平台选错了"。**
指定 DRM display 要用 **`WPE_DISPLAY=wpe-display-drm`**（值是扩展名）；我们之前一直传
`WPE_PLATFORM=drm` —— 那个变量没人读，于是 `wpe_display_get_default()` 去遍历已注册的 display，
**先连上了 headless**，WebKit 的 view 就成了 `WPEViewHeadless`（它的 `render_buffer` 是空实现，
返回 TRUE 但什么都不提交）⇒ 屏幕全黑。

```bash
# 正确的跑法（两个变量都要）：
WPE_DISPLAY=wpe-display-drm WPE_DRM_DEVICE=/dev/dri/card0 ./wpe-launcher file:///tmp/red.html
```

判据（2026-10-02）：`/sys/kernel/debug/dri/0/summary` 里 `Esmart0-win0` 的 `buf[0].addr`
由 `0x0` 变成 `0x1fa4000`（同 cage 运行时的值）；`ffmpeg -f kmsgrab` 抓帧平均 RGB = **(254,2,2)**
（屏幕上就是那张纯红测试页）。链路逐环日志（临时 `WTFLogAlways` 插桩）：
`ABS::create(type=WPEViewDRM)` → `setupBufferFormat chosen XR24` → `created EGLImage target 3840x2160`
→ `WEB didRenderFrame` → `ABS::frame` → `renderBuffer ENTER` → `drmBufferCreate OK (imported for scanout)`
→ `WPEViewDRM::requestUpdate` → `atomic commit rc=0` → `didPageFlip`。

- **编译**：服务器（24 核交叉）约 20 分钟编完 8662 个目标、0 报错；`ninja install` 到 `DESTDIR` 暂存 165 MB
  → rsync 推到板子 `/usr/local`（172 MB、11 s）→ `ldconfig`。`pkg-config --modversion wpe-webkit-2.0 wpe-platform-2.0` = **2.52.6** ✓。
- **launcher**：§2.2 那 9 行在 C 下要写成 `g_main_loop_new(NULL, FALSE)`；用
  `pkg-config --cflags --libs wpe-webkit-2.0 wpe-platform-2.0` 编过、链接到 `/usr/local/lib/libWPEWebKit-2.0.so.1` ✓。
- **运行**（当时的写法，⛔ 变量名是错的，看上面结论）：`WPE_PLATFORM=drm WPE_DRM_DEVICE=/dev/dri/card0 ./wpe-launcher http://127.0.0.1:8080/`：
  - 进程起得来、`WPEWebProcess` 起来了、**与 127.0.0.1:8080 的 TCP 处于 ESTABLISHED**（页面确实加载了）；
  - ⛔ **屏幕是黑的**（当时）：kmsgrab 抓帧平均像素 `(0,0,0)`；换成**纯红本地页**（`file:///tmp/red.html`）**同样全黑**
    ⇒ 不是页面的问题，是**平台/渲染出屏**这条路径 —— 后来查明就是 `WPE_PLATFORM` 这个变量名写错了。
  - ✅ 抓帧方法本身可靠：同一条命令在 cage 运行时抓到 `(242,245,247)`（页面底色）。
  - ⚠️ 不指定 `WPE_DRM_DEVICE` 时它还**自己挑错设备**（警告 "Infered DRM device (/dev/dri/card2)"，card2 是 RKNPU）。
- ✅ **2026-10-02 追加定位：渲染器正常，问题在"提交到屏幕"这一步**：
  - 用 `webkit_web_view_get_snapshot()`（注意这个版本 `..._finish()` 返回的是 **`WebKitImage`** 而不是
    cairo surface）在 DRM 平台下做快照 → **`LOAD FINISHED` + `1024x768`，中心/左上像素 BGRA=(0,0,255,255)**
    = 纯红（就是那个测试页）⇒ **渲染管线完全正常**。
  - 而 `/sys/kernel/debug/dri/0/summary` 里 plane `Esmart0-win0` 是 `ACTIVE` 但
    **`buf[0].addr = 0x0`**（cage 下是 `0x1fa4000`）⇒ **一个缓冲都没提交上去**，所以是黑的。
  - **缓冲类型不匹配**（源码）：`WPEViewDRM.cpp` 的 `drmBufferCreate()` 只处理
    `WPE_IS_BUFFER_DMA_BUF`，SHM 缓冲直接 `g_set_error(..."unsupported buffer")` —— 那里还写着
    `// FIXME: implement.`。运行时**没有 GPU 进程**（只有 launcher / WPENetworkProcess / WPEWebProcess），
    即 web 进程走的是非 DMA-BUF 路径 ⇒ DRM 平台拿不到能扫出的缓冲。
  - ⚠️ 两个被**推翻**的假设（省得以后再查）：①「显示设备 card0 没有 GL 驱动」——错，
    `card0`/`renderD128`/`renderD129` 上 EGL 都能初始化，而且 `GL_RENDERER` 都是
    **Mali-G610 (Panfrost)** 硬件 GL；②「EGL 初始化失败」——错，全都成功。
  - ⚠️ 本构建 **`ENABLE_WPE_PLATFORM_WAYLAND=OFF`**（当初按"我们不需要合成器"关的）⇒
    `WPE_PLATFORM=wayland` 不可用、会静默回落到 DRM（日志里那条 "Infered DRM device (/dev/dri/card2)"
    就是回落后的 DRM 平台打的）。要验 Wayland 路径得先重编。
**黑屏是怎么定位的（可复用的排查顺序）**：

1. 先确认渲染本身没问题：`webkit_web_view_get_snapshot()` 在 DRM 平台下能拿到 1024x768、纯红像素
   ⇒ 渲染管线 OK，问题在"上屏"这条路（当时还不知道平台选错了，这条结论依然成立）。
2. 在 UI 进程看**缓冲有没有回到 view**：`AcceleratedBackingStore::frame()`（web 进程发来的帧）
   → `renderPendingBuffer()`（`if (!wpe_view_get_toplevel(...)) return;` 是第一个可疑的静默返回点）
   → `wpe_view_render_buffer()`。用 `WTFLogAlways` 逐层打，发现链路**一路都到**
   `wpe_view_render_buffer()`，只是 `render_buffer` 的 vfunc 不是 DRM 的。
3. 决定性的一条：把 view 的类型打出来 —— `G_OBJECT_TYPE_NAME(view)` = **`WPEViewHeadless`**。
   于是回头读 `wpe_display_get_default()`（`WPEPlatform/wpe/WPEDisplay.cpp:265`）：
   它读的是 **`WPE_DISPLAY`**（不是 `WPE_PLATFORM`），留空则遍历已注册的 display、
   **谁先 `connect()` 成功就用谁**；本构建里 DRM 与 headless 都是优先级 -100，
   遍历时 DRM 先被构造（那条 "Infered DRM device (/dev/dri/card2)" 警告就是这样来的）
   但 `connect()` 失败（card2 是 RKNPU），接着 headless 连上 ⇒ view 变成 headless。

**顺手记下的环境细节**：本板三对节点是 **card0=rockchip-drm(显示) / card1=panthor(Mali) / card2=RKNPU**，
render 分别是 **renderD128/129/130**；不指定 `WPE_DRM_DEVICE` 时 DRM 后端会去推 card2（RKNPU）而失败，
所以**两个变量都要显式给**。本构建 `ENABLE_WPE_PLATFORM_WAYLAND=OFF`（`WPE_DISPLAY=wpe-display-wayland`
不可用，要验 Wayland 路径得重编）。

**插桩位置（临时 `WTFLogAlways`，只改在服务器构建树 `/srv/lsp-cross/src/wpewebkit-2.52.6/`，
未进 git；补丁脚本 `/srv/lsp-cross/patch{3,4,5}.py`）**：
`UIProcess/wpe/AcceleratedBackingStore.cpp`（create / frame / renderPendingBuffer / 调用点前后）、
`WPEPlatform/wpe/WPEView.cpp`（`wpe_view_render_buffer` 入口 + vfunc 指针）、
`WPEPlatform/wpe/drm/WPEViewDRM.cpp`（renderBuffer / requestUpdate / didPageFlip / atomic commit）、
`WebProcess/WebPage/CoordinatedGraphics/AcceleratedSurface.cpp`（createTarget / 建缓冲 / didRenderFrame）。

### 3.2 ✅ 2026-10-02 硬解验收通过（4K30 H.264，走 GStreamer `mppvideodec`）

被测素材：板上 `/opt/oem-host/media/camera-bind-right-plug.mp4`（**3840x2160 H.264 High L5.1 + AAC**，31.6 s）。

**两个前置条件**（都踩过）：

1. **镜像缺 `gstreamer1.0-gl`**：`libgstreamer-gl1.0-0` 在，但 `glimagesink` / `glupload` / `glcolorconvert`
   全都没有 ⇒ WebKit 打 `WebKit wasn't able to find the GL video sink dependencies`（零拷贝渲染不可用）。
   `apt-get install -y gstreamer1.0-gl` 后该警告消失。**走 WPE 路线的话镜像必须加这个包**。
2. **launcher 要放开"需要用户手势才自动播放"**：`webkit_settings_set_media_playback_requires_user_gesture(settings, FALSE)`
   （+ `..._allows_inline`）。WebKit 默认要求用户手势，`muted autoplay` 也不会播 —— 现象就是只有页面背景、无媒体错误。

**实测（`file://` 页面、视频在同目录）**：

```
mpp-fd=1                                  # web 进程持有 /dev/mpp_service（确实在用 MPP 硬解）
rkvdec 负载 9.7% / 10.35% / 9.65%          # 三秒采样（4K30 硬解）
web 进程 CPU 17~20%
272 帧 / 9.08 s = 29.9 fps                 # 满帧
抓帧 6514 B（不播时纯色只有 922 B）
```

⛔ **WebKit 的媒体播放器要求 HTTP 支持 Range**：用 `python3 -m http.server`（对 Range 返回 **200**）时
媒体元素直接报错：
`R2: Received unexpected 200 HTTP status code for range request (url=…)` ⇒ 不播、也没有别的线索。
**生产路径没问题**：上位机素材走 Starlette `StaticFiles`（`/static/media/…`），
实测 `curl -r 0-99 http://127.0.0.1:8080/static/media/…mp4` → **206** ✓。
所以自测时要么用 `file://`，要么用一个支持 Range 的服务器（别用 `python -m http.server`）。

## 4. 与其它"换浏览器"候选的关系

| 候选 | 结论 | 出处 |
| --- | --- | --- |
| Firefox | 能硬解（装了 `rockchip-vaapi` 之后），但**启动慢 ~1.4 s** | `notes/system/chromium-rkmpp.md` §6.5/§6.6 |
| 更轻量的 Chromium | 可行但要自己编，收益上限 ~0.5 s ⇒ 暂不做 | 同上 §6.7 |
| Framebuffer-browser / QtWebEngine | Qt5.15(Chromium 87)+QtWebEngine，无 VA-API 后端 ⇒ 不划算 | （调研结论，未单独成篇） |
| **WPE WebKit + WPEPlatform(DRM)** | **本文**：组件更少、无合成器、媒体走 GStreamer | 本页 |

## 5. 计划（TODO，2026-10-02 用户要求"先记录一下"，**未开工**）

> 用户口径：**先记录，等有空再编**。WebKit 是几小时级的构建（实测 ~85 目标/分钟、
> 7100 个目标量级），所以不要随手起编。下面按顺序做，每步都有判据。

| # | 步骤 | 判据 |
| --- | --- | --- |
| 1 | `[ ]` 重配 `/home/board/wpe-build/wpewebkit-2.52.6`（**不能续用旧 build 目录**，它没开 `ENABLE_WPE_PLATFORM`）→ 后台编 | 配置通过（§2.1 的开关全绿）+ 产物里有 `wpe-webkit-2.0.pc` 与 `wpe-platform-2.0.pc` |
| 2 | `[ ]` `ninja install` 到 `/usr/local`；顺手清掉 legacy 废件（`libwpe-1.0.so*`、`libWPEBackend-fdo-1.0.so*`） | `pkg-config --modversion wpe-webkit-2.0 wpe-platform-2.0` 都有值 |
| 3 | `[x]` 那 9 行 launcher（§2.2）已写（板上 `/tmp/wpe-launcher`，源码 `/tmp/wpe-launcher.c`）；**systemd 单元还没做**（要用 `WPE_DISPLAY=wpe-display-drm` + `WPE_DRM_DEVICE=/dev/dri/card0`，`User=kiosk`、`PAMName=login`、`TTYPath=/dev/tty1`，与既有 `lsp-kiosk.service` 同款骨架） | 编译通过、能起 ✓ |
| 4 | `[x]` **出画验收**：`WPE_DISPLAY=wpe-display-drm WPE_DRM_DEVICE=/dev/dri/card0 ./wpe-launcher file:///tmp/red.html` | ✓ 2026-10-02 通过：plane `buf[0].addr` 0x0 → 0x1fa4000、kmsgrab 抓帧平均 RGB **(254,2,2)**；**进程表里没有 cage/weston**（真·无合成器） |
| 5 | `[x]` **硬解验收**：播 `/opt/oem-host/media/camera-bind-right-plug.mp4`（4K30 H.264） | ✓ 2026-10-02 通过：`mpp-fd=1`、`rkvdec` 9.7~10.3%、web CPU 17~20%、**29.9 fps 满帧**；两个前置条件见 §3.2（缺 `gstreamer1.0-gl`、要关"需用户手势"；HTTP 必须支持 Range） |
| 6 | `[ ]` 与现状对比：启动时间、进程树、以及"按键过滤/维护模式"这类定制在 WPE 下怎么落 | 结论写回本文 §2.3；定制缺口单独立项 |
| 7 | `[ ]` 决定要不要进镜像（**镜像级决策**：多一套自编 WebKit vs 现在 cage+Chromium） | 用户拍板 |

### 5.1 已知风险 / 前置条件（动手前先看）

- ⛔ **在板上编必须限制内存，而且并行度只能 2~3**（2026-10-02 实测，踩过一次）：
  - 单个 JavaScriptCore unified-source 编译单元（`UnifiedSource-*.cpp.o`）**峰值 RSS 2033 MiB、耗时 144 s**
    （量法：`ninja -C build -t commands <目标>` 取命令 → 单跑 → 轮询 `/proc/<cc1plus>/status` 的 `VmRSS`）。
  - `ninja -j8` 直接把 7.9 GB 吃干：earlyoom 在"可用 51 MiB(0.70%)"时开始 SIGKILL，
    **先杀掉的是 kiosk 会话里的 `gvfsd` / `at-spi-bus-launcher` / `xdg-permission-store` /
    `at-spi2-registryd`**（现场表现就是用户报的"**上位机网页突然崩溃**"），另一个被杀的 `cc1plus`
    让 ninja 直接中止。
  - ⇒ 正确做法：**放在带内存上限的 systemd 临时单元里编**，让 OOM 只打在构建自己的 cgroup：

    ```bash
    sudo systemd-run --unit=wpe-build \
      -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=1G \
      -p CPUWeight=20 -p IOSchedulingClass=idle -p Nice=10 \
      --working-directory=/home/board/wpe-build/wpewebkit-2.52.6 \
      /usr/bin/ninja -C build -j3
    ```
    （`-j4` + `MemoryMax=4G` 试过：一到 3.5~4 G 就顶到上限，`cc1plus` 被 **cgroup 内的 OOM** 杀掉、
    构建失败 —— 报错长相是 `c++: fatal error: Killed signal terminated program cc1plus`，
    别误判成编译错误。）
  - ⚠️ JSC 编译是"**一个 TU 一个线程**"⇒ 并行度受**内存**（3 × ~1.8 G）而不是 CPU 限制：
    8 核最多也就跑 3 个，所以**板上整编是数小时级**（4~8 h 量级）。kiosk 会话本身只占 ~0.4 GB RSS。
- **依赖**：2.52.6 是为 noble 选的版本（noble 自带 webkit2gtk 2.52.6 ⇒ 依赖能对上）。
  构建需要 `libsoup-3.0-dev`、`libgstreamer*-dev`、`libegl1-mesa-dev`、`libgles2-mesa-dev`、
  `libgbm-dev`、`libdrm-dev`、`libharfbuzz-dev`、`libicu-dev`、`libwebp-dev`、`libwpe-1.0-dev`(?)…
  —— 板上前一版方案已经把大部分装过（见那次构建日志）。
- **`USE_LIBBACKTRACE=OFF` 必须加**（noble 没有 libbacktrace，实测配置阶段直接报错）。
- **网络**：板子到 GitHub 有时会卡死（实测 `wget` 挂 5 分钟不动）⇒ 源码已在板上，**别重新下载**。
- **旧 build 目录**只能删掉重配，不能续编（配置项不同）。
- 板上磁盘：`/home/board/wpe-build` 已占 664 MB，重配后构建产物还会涨；动手前看 `df -h /`。
