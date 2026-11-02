# 合成器在不在用 GPU：wlroots 渲染器 A/B（GPU 合成 vs CPU 合成）

> **一句话**：把 `WLR_RENDERER` 在 `gles2` 与 `pixman` 之间切，用 devfreq 的 GPU 负载、
> `/proc/<pid>/maps` 里的 GL 栈、`/dev/dri/renderD*` 三样做**硬判别式**。同一块板、同一个 4K 负载实测：
> **GPU 合成 = 7% CPU / GPU 99% / 1098 FPS；CPU 合成 = 90% CPU / GPU 0% / 4 FPS**。
> 配套回归：`tests/wlroots-renderer-ab.sh`（板上 14 通过 / 0 失败）。

## TL;DR

- 开关只对 **wlroots 系合成器**（cage / weston / sway）有效，**GNOME/mutter 不认**。
- 必须 `PAMName=login` + `TTYPath=/dev/ttyN`（瞬态 systemd 单元最省事），否则拿不到真 DRM master。
- 硬判别式：maps 里有没有 **`libEGL_mesa` + `libgallium`**、有没有打开 **`/dev/dri/renderD*`**。
  ⛔ 别找 `dri/*.so` —— Mesa 25.2 的 Mali 驱动内建在 `libgallium-25.2.8…` 里。
- 量 CPU 代价必须**同时看帧率**：`weston-simple-shm` 这类帧回调客户端，pixman 下只是"画得更少"，
  单看 CPU% 会得出"两者差不多"的错结论。
- 四个坑（取错 pid / 清场顺序 / 输出不挂单元 / cage 不响应 SIGTERM）在本篇末尾。

---

**问题**：怎么**单独**验证"合成器在不在用 GPU"，以及"CPU 合成"到底贵多少。
合成器是 wlroots 系的（cage）；GNOME/mutter **不认**下面这些开关。

**做法**（只改一个变量）：

1. 用 **cage** 在**空闲 VT** 上起 —— 必须走 `PAMName=login` + `TTYPath=/dev/ttyN`
   （最省事的是瞬态 systemd 单元 `systemd-run --property=PAMName=login --property=TTYPath=…`）；
   直接在 ssh 里跑 cage 拿不到 logind 会话 ⇒ 也就拿不到真 DRM master。
2. 开关：`WLR_RENDERER=gles2` ↔ `WLR_RENDERER=pixman WLR_RENDERER_ALLOW_SOFTWARE=1`
   （⛔ 漏掉 `ALLOW_SOFTWARE` 时 wlroots 会**拒绝** pixman 并退出）。
3. 负载：
   - 想看"**合成器**用不用 GPU" ⇒ 纯 shm 客户端 **`weston-simple-shm`**（客户端自己不碰 GPU）；
   - 想看吞吐/代价 ⇒ `es2gears_wayland` + `vblank_mode=0` + **`stdbuf -oL`**（否则 stdout 被缓冲、
     抓不到 FPS）。
4. 指标：GPU 用 `/sys/class/devfreq/fb000000.gpu-panthor/load`（`负载%@频率`，本板是真百分比，
   见 [rk3588-hw-monitor.md](rk3588-hw-monitor.md)）；CPU 用 `top -b -d 1 -p <cage MainPID> -n N`
   的 `%CPU` 中位；**"到底走没走 GPU"的硬判别式** = `/proc/<pid>/maps` 里有没有
   **`libEGL_mesa` + `libgallium-*`**，以及有没有打开 **`/dev/dri/renderD*`**
   （⛔ 别找 `dri/*.so` —— Mesa 25.2 的 Mali 驱动内建在 `libgallium-25.2.8…` 里，那个查法看不到）。

**结果（LubanCat-5 v2，HDMI 3840×2160，cage 0.1.5 + wlroots 0.17.1 + Mesa 25.2.8）**：

| 负载 | `WLR_RENDERER` | GPU 中位 | cage CPU 中位 | 客户端吞吐 |
| --- | --- | --- | --- | --- |
| `es2gears_wayland`（`vblank_mode=0`） | `gles2` | **99%** | **7%** | **1098 FPS** |
| 同上 | `pixman` | **0%** | **90%** | **4 FPS** |
| `weston-simple-shm` | `gles2` | 32%（峰 44%） | 28% | —— |
| `weston-simple-shm` | `pixman` | 0% | 25% | —— |

- ⇒ 同一负载下 **CPU 7% → 90%**、**GPU 99% → 0%**、**FPS 1098 → 4**：GPU 合成 vs CPU 合成的量级差。
- ⚠️ `weston-simple-shm` 那两行**不能用来比 CPU 代价**：它是帧回调驱动，pixman 下只是"画得更少"
  ⇒ CPU% 看起来差不多；要下结论必须同时看**帧率**。
- ⚠️ pixman 那 4 FPS 是**最惨路径**：GLES 客户端交的是 GPU 渲染的 buffer，CPU 合成器只能
  每帧读回内存 ✗；shm 客户端的代价没这么极端。
- 证据（`gles2` 组）：maps 里有 `libEGL_mesa.so.0` + `libgallium-25.2.8-…`，并打开
  `card0` + `renderD128` + `renderD129`；`pixman` 组两者都没有、只开 `card0`（只做 modeset）。

**坑（本次实测踩到的）**：

- ⛔ `pgrep -x cage | head -1` 会取到**上一个刚停的单元**的 cage ⇒ 一律用
  `systemctl show <unit> -p MainPID`；
- ⛔ 清场（等 cage 归零 / `pkill -9 -x cage`）必须放在**启动新单元之前**；
- ⛔ `PAMName=login` 的子进程输出**不挂在单元上**（`journalctl -u <unit>` 看不到客户端的 FPS）
  ⇒ 抓全量 journal 的**时间窗**；
- cage 不响应 SIGTERM（见 [../system/kiosk/maintenance-mode.md](../system/kiosk/maintenance-mode.md)）
  ⇒ 每个单元都给短 `TimeoutStopSec`，否则测试末尾白等。

固化脚本：`tests/wlroots-renderer-ab.sh`（同一套做法 + 判据 + 退出码）。

---
