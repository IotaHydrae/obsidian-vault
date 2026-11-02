# 实验：把 kiosk 链换成 LVGL 直上 DRM，能快多少

> 本文只回答一个问题：**"cage + Chromium + 上位机"这条链换成"LVGL 直接画到 DRM"，
> 应用启动指标如何变化、怎么复现/回退**。历史数据来自开发板，内核 6.1.172；完整复位时间另见启动时间账。
> ⚠️ 这是**实验记录，不是产品方案**：LVGL 那条链没有上位机、没有 logo 之外的 UI、也没做 seat/权限那套。
> 全局启动账见 [boot-time.md](boot-time.md)；开机画面与 spinner 的退场时机见
> [plymouth-boot-splash.md](plymouth-boot-splash.md) §1.7。

## TL;DR

| 历史实验事件 | 板侧 monotonic | 判据 |
| --- | --- | --- |
| LVGL floor 版 | 3.96–4.21 s | 应用 `first_frame` 日志标记 |
| LVGL + Plymouth early 版 | 5.07 s | 应用 `first_frame` 日志标记 |
| kiosk 的 Cage 进程开始 | 5.21 s | 进程启动属性，未测底色提交 |
| 上位机后端完成启动 | 6.88 s | uvicorn 运行日志 |
| 浏览器开始导航 | 约 11.21 s | 浏览器请求事件 |

- 保留应用内部事件作为实验观察；终点不同，不能直接宣称“画面快 6–7 秒”。
  旧的上电换算用了 U-Boot `Total` 或估算前缀，已移除；完整复位时间要用串口锚定，见 [boot-time.md](boot-time.md)。
- **app 自身几乎不花时间**：DRM 就绪 +0.04~0.11 s、首帧再 +0.03~0.02 s（无竞争时**首帧距进程起点 61 ms**）。
  慢的从来不是 LVGL，是**引导 + unit 排序**。
- ⛔ **plymouth 与这条链的搭配要分场景**：LVGL 测试**不要**开 plymouth（顺序会反，见 §5）；
  Chrome/kiosk 保留它到 Cage 启动前；随后由合成器背景、Chromium buffer 和极简着陆页接管。
- 复现要点：`lv_port_linux`（lvgl 子模块）→ `drm.defconfig` 改成 **FBDEV(dumb buffer)** + widgets 演示 →
  板上原生 ninja（8 核 **59 s**）→ 三个临时 unit 里选一个（§3）→ 串口抓包量 `first_frame`（§4）。

## 1. 口径：比的是什么

- **同一天、同一块板**取两条链的数，避免跨版本比。
- kiosk 链用 journal 的 monotonic：`Started oem-host.service` / `Started lsp-kiosk.service` /
  `Uvicorn running` / uvicorn 的 access log（浏览器首个请求）。
- LVGL 链用 app 自己 `clock_gettime(CLOCK_MONOTONIC)` 打的 `[lsp-lvgl] first_frame`（§3 的插桩）。
- 应用日志标记不证明 HDMI 实际出画；完整复位测量应另建串口锚点，不能沿用旧的上电换算。

## 2. 怎么编（板上原生，59 秒）

```bash
# 依赖：libdrm-dev / libinput-dev / libxkbcommon-dev / libgbm-dev 本来就在镜像里；
#       Kconfig 工具（kconfiglib+pcpp）与 LVGL 版本无关（版本来自 lvgl/env_support/cmake/version.cmake）
python3 -m pip install --user --break-system-packages -i https://pypi.tuna.tsinghua.edu.cn/simple kconfiglib pcpp
# lvgl 是子模块：板上没有 GitHub 网络 ⇒ 在开发机取好（git submodule update --init --depth 1，
#   走 127.0.0.1:7897 代理）再 rsync 过去；或先 rsync 整个树，子模块内容一起带过去。
cd lv_port_linux
export PATH=$HOME/.local/bin:$PATH
defconfig lvgl/configs/defconfigs/drm.defconfig     # 现成的 DRM 配置（默认 EGL 后端）
sed -i 's/^CONFIG_LV_LINUX_DRM_BACKEND_EGL=y/# CONFIG_LV_LINUX_DRM_BACKEND_EGL is not set/' .config
cat >> .config <<'EOF'
CONFIG_LV_LINUX_DRM_BACKEND_FBDEV=y    # dumb buffer：不碰 GPU/EGL，最"纯净"
CONFIG_LV_BUILD_DEMOS=y
CONFIG_LV_USE_DEMO_WIDGETS=y
EOF
olddefconfig
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"
# 产物 build/bin/lvglsim（2.8 MB）；卡可用环境变量指定：LV_LINUX_DRM_CARD=/dev/dri/card0
```

**插桩**（为了量"上电→首帧"，改 `src/main.c`，临时改动、已标注"正式用请删"）：
进程起点记 `t0`，在 ① 后端初始化成功 ② UI 建完 ③ 第一次 `lv_timer_handler()` 返回（= 首帧刷出）
各打一行 `[lsp-lvgl] <名字> mono=<CLOCK_MONOTONIC> (+<相对 t0>)` 到 stdout。
用 stdout 而不是自己的日志文件，是为了让 journald 一起带上 monotonic 时间戳。

## 3. 怎么装（三个临时 unit，都在 `/etc/systemd/system/`，不进镜像）

| unit | 排序 | 单元起点(mono) | 用途 |
| --- | --- | --- | --- |
| `lsp-lvgl-demo.service` | 照抄 kiosk（`Conflicts=getty@tty1` + `After=getty@tty1` + `After=systemd-user-sessions` + `WantedBy=graphical.target`） | 5.241 s | A/B 对照"按 kiosk 那套排序要等多久" |
| `lsp-lvgl-early.service` | `DefaultDependencies=no` + `After=plymouth-quit.service` + `WantedBy=sysinit.target` | 4.824 s | **要 logo 就用这版**：logo 先出，再由它 quit plymouth |
| `lsp-lvgl-floor.service` | `DefaultDependencies=no` + `After=systemd-journald.socket` + `WantedBy=sysinit.target`（**不等 plymouth**） | 3.775~3.879 s | 纯 LVGL 测速下限（配合 §5 的 plymouth 关掉） |

三个 unit 的共同点（都是踩过才这么写的）：

- `ExecStartPre=-/usr/bin/plymouth quit` —— 不退 plymouth 就抢不到 DRM master（`-` 前缀=失败也继续）。
- `User=root` + `TTYPath=/dev/tty1` —— 实验图省事；**产品化必须像 kiosk 那样走 `PAMName=login` + seat**，
  否则拿不到 logind 会话（LVGL 自己 `drmSetMaster`，root 能拿到，但那是特权路径）。
- `Environment=LV_LINUX_DRM_CARD=/dev/dri/card0` —— drm.c 里是 `getenv_default("LV_LINUX_DRM_CARD", 自动找)`。
- `Restart=no` —— 实验期间崩了就要看得见，别自愈掩盖。
- `Conflicts=getty@tty1.service` 必须留（这个镜像 `getty@tty1` 是 enabled 的，不加会互相顶掉，
  原因见 [kiosk/session-design.md](kiosk/session-design.md) 的「账户与会话」
  那张表（`getty@tty1` / `autovt@` 几行），以及 `lsp-kiosk.service` 里的注释）。

切换与回退：

```bash
sudo systemctl disable --now lsp-lvgl-floor && sudo systemctl enable --now lsp-lvgl-early   # 换变体
sudo systemctl disable --now lsp-lvgl-<变体>
sudo systemctl enable lsp-kiosk-display.service
sudo systemctl restart lsp-kiosk-display.service   # 按 HDMI 状态恢复 kiosk
```

## 4. 怎么量（串口逐行，两种口径互证）

```bash
# 开发机上：先开抓包，再触发重启（否则丢掉前半段）
tools/serial-exec capture start --output /tmp/lvgl-boot-record --duration 60 --json
ssh board@<板子> 'sudo systemctl reboot'
```

判据（同一份日志里三样都对得上）：

1. **串口前缀差**：`reboot: Restarting system` → `[lsp-lvgl] first_frame` 那一行（端到端，含 SPL/U-Boot）。
2. **app 自报 mono**：`first_frame mono=` —— 不受关机路径波动影响，**比快慢看这个**。
3. **换算**：`上电后 ≈ mono + (Starting kernel − Restarting system) + (内核首行时刻 − Starting kernel)`。
   本次两次实测这两个偏移分别 ≈1.00 s 与 0.48~0.74 s。

实测样例（early 版 + plymouth，串口逐行）：

```
+0.000  mono  —     reboot: Restarting system
+1.004  mono  —     Starting kernel ...          （SPL 110 ms + U-Boot 686 ms）
+5.749  mono 4.165  Started plymouth-start.service      ← 自定义 logo
+6.300  mono 4.734  Finished plymouth-quit.service      ← 由我们请求退场
+6.402  mono 4.824  Started lsp-lvgl-early.service
+6.815  mono 5.228  [lsp-lvgl] first_frame (mono=5.070)
```

## 5. plymouth 与这条链的关系（别搞反）

- **LVGL 测试不要开 plymouth**：floor 版不等 plymouth，结果 **splash 跑在 app 之后**
  （实测：app `first_frame` mono 3.959 已画完，`plymouth-start` 才在 mono 4.138 起）⇒ 屏上会闪一下。
  要纯 LVGL 测速就在 cmdline 加 `plymouth.enable=0`（或 `systemctl mask plymouth-start plymouth-quit`）。
- **Chrome/kiosk 的交接独立维护**：Plymouth 在 Cage 启动前退出，不能遮住整个 Chromium 冷启动过程；
  极简着陆页也要等浏览器能绘制后才出现。当前顺序见
  [plymouth-boot-splash.md](plymouth-boot-splash.md) §1.7。
- 本次 LVGL floor 实验中，开 Plymouth 为 5.930 s、关闭为 5.665 s，差 0.265 s；
  floor 版本身不等它，不能把这组差值推广到 kiosk 或其它排序。

## 6. 这个实验回答了什么、没回答什么

回答了：

- LVGL 应用内部 `first_frame` 和浏览器主页请求可用于定位各自路径；两者终点不同，不能换算成上电物理首帧对比。
- LVGL 的 DRM 直出本身极便宜（**首帧 61 ms**，无竞争时），瓶颈全在引导与 unit 排序。

没回答（要产品化还得做）：

1. **seat/权限**：现在跑 root；正式方案得像 kiosk 一样走 logind 会话。
2. **evdev**：现在自动枚举所有 `/dev/input/*`（开机时约 +0.11~0.25 s）；指定设备可再省 ~0.1 s。
3. **EGL 后端对照**：本文只用 dumb buffer；EGL/GPU 路径（`CONFIG_LV_LINUX_DRM_BACKEND_EGL`）没量。
4. **与 Plymouth 的正式交接**：需要独立验证 DRM 接管、显示输出与失败退出，不能套用 kiosk 的现有排序。
5. **去掉 initramfs**：唯一的大项（文档口径 −1.7 s，见 [boot-time-optimizations.md](boot-time-optimizations.md) 第 2 条），
   与 LVGL 正交，需要单独做实验版。
