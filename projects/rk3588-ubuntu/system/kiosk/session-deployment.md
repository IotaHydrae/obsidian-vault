# kiosk 落地：坑清单与步骤

> 从"设计稿"到"镜像里真能跑"要过的坑，按症状→原因→做法列在这里；每条都给判据。

## TL;DR

- ⛔ **`--kiosk` 不是安全边界**：能锁住的是"会话外观"，不是"用户权限"。真正的边界是**谁物理接触这台机器**。
- ⛔ **必须 `Conflicts=getty@tty1.service` + `After=getty@tty1.service`**（冷启动才暴露）：
  否则两边互顶、每 ~2.2 s 一轮，开机多等 ~11 s；判据是 `journalctl -b -u getty@tty1` **一条都没有**。
- `Ctrl+W` 由 Cage 按键过滤阻断；`Restart=always` 只负责异常退出恢复。
- 默认只在 HDMI 已连接时启用图形栈，拔屏即停，见 [session-design.md](session-design.md)。
- 策略目录要我们建（`/etc/chromium/policies`），且**必须验**（kiosk 里看不到 `chrome://policy`）。
- HMI 用 `127.0.0.1` + 页面自带重连；显示固定用 `wlr-randr`；工厂机时间要准 ⇒ **接 RTC 电池**。
- 运维出口必须留：SSH + `sudo lsp-maintenance on|off`（见 [maintenance-mode.md](maintenance-mode.md)）+ 长按电源键 2 s。

## 坑与注意事项

1. ⛔ **`--kiosk` 不是安全边界**：能锁住的是"会话外观"，不是"用户权限"。真正的边界是：**这台机器谁物理接触**
   （ATM 也是靠机柜）。别把它当防攻击手段。
2. **`Ctrl+W` 由按键过滤挡住**，不应靠重启和闪屏判定通过；验证见 [keyfilter.md](keyfilter.md)。
3. **策略目录在启动时建**：`/etc/chromium/policies` 由 kiosk 前置动作创建；策略是否被加载**必须验**（正常 kiosk 里看不到
   `chrome://policy`，验证时临时开一次带地址栏的窗口，或 `--enable-logging=stderr` 看策略日志）。
4. **HMI 用本地服务**（`127.0.0.1`）+ 页面自带重连/离线页。远程 URL 一断网就是白屏，kiosk 里没有任何提示手段。
5. **显示固定**：HDMI 热插拔会改模式 ⇒ 用 `wlr-randr` 固定分辨率/刷新率；需要时 `--force-device-scale-factor` 锁缩放。
   Wayland 下 `--window-position/--window-size`（多屏定位）只在 XWayland 会话里认。
6. **时间**：工厂机显示/记录时间 ⇒ **接 RTC 电池**（否则冷启动回到 2021，见 [time-sync.md](../tuning/time-sync.md)）。
7. **中文输入**：镜像没有 ibus；HMI 若需要中文输入要先加输入法框架（否则"只能看不能打"）。
8. **`cage 0.1.5` 偏老**（2024-01 build）：若与 Chromium/RKMPP 有兼容问题，退到方案 C（`sway -c`）。
9. **运维出口必须留**：SSH 保留 + **`sudo lsp-maintenance on|off`**（[maintenance-mode.md](maintenance-mode.md)，2026-09-23 落地；
   原设想给 `lsp-config` 加 `kiosk on|off` 子命令，用户明确"不需要通过 lsp-config 来切换"）；
   RAM 里的"退出方式"若不写清楚，工厂现场出问题只能断电 —— 而长按电源键 2 s 正是为此保留的。
10. ⛔ **必须 `Conflicts=getty@tty1.service`（+ `After=`），这是冷启动才暴露的坑**（2026-09-22 真机抓到）：
    镜像里 `getty@tty1` 是 **enabled**，kiosk 又要占 tty1 ⇒ 缺这条时两边**互相把对方顶掉**：
    - 现场（`journalctl -b -o short-monotonic`）：
      `[9.48] Started lsp-kiosk` → `[9.59] Started getty@tty1` → `[9.95] kiosk Deactivated successfully`
      → `[9.96] getty Deactivated successfully` → 每隔 ~2.2 s 重复一轮，**kiosk 每次只活 18~470 ms**
      （连 `pam_unix(login:session)` 都没来得及打）；
    - `getty` 先撞上自己的默认启动限流（5 次/10 s）→ `Failed with result 'start-limit-hit'`，
      kiosk 第 6 次（~21 s）才拿到 tty1 ⇒ **开机多等 ~11 s、`NRestarts=5`**；
    - 加 `Conflicts=getty@tty1.service` + `After=getty@tty1.service` 后冷启动：`NRestarts=0`、
      `journalctl -b -u getty@tty1` **一条都没有**（它根本没被拉起）；
    - 依据：GDM 自己那份 `/usr/lib/systemd/system/gdm.service` 就是 `Conflicts=getty@tty1.service` +
      `After=getty@tty1.service`（它也要占一个 VT）。**照抄它，别自己发明。**
    - ⛔ 别用 `systemctl mask getty@tty1` 来"修"：那是全局的，回桌面时还得记得解开；`Conflicts` 随单元生效。
    - ⛔ 排查时别被 `Deactivated successfully` 骗了：**"干净退出"不等于"程序跑完自己退出"** ——
      这里是 agetty 打开 tty1 时把 cage 的会话挂断（HUP）。判据是**看 PAM 行在不在**
      （18 ms 的启动不可能跑完 PAM + chromium）。

## 落地步骤（每步都有判据）

1. ✅ **可行性验证（已做，2026-09-22）**：板上临时装 `cage` + 手写 unit，`systemctl start lsp-kiosk` 后确认
   —— gdm 被停、kiosk 拿到 `seat0/tty1`、屏幕上正常显示页面（用户目视确认）、
   `wlr-randr` 显示输出 `Enabled: yes / 3840x2160@60`。⏳ 硬解口径要用 `tests/browser-codec.sh` 另测（没做）。
2. ✅ **逃逸面验证（已做，2026-09-22 晚）**：不靠人按键 —— 用 `/dev/uinput` **真注入**按键，
   加上本地 HTTP 服务的浏览器策略 A/B。写成了 **`tests/kiosk-lockdown.sh`**
   （板上 **27 通过 / 0 失败**；默认不做会短暂停 kiosk 的策略行为验证，加 `--with-policy-probe` 才做）。
   机制与实测见 [escape-hardening.md](escape-hardening.md)（cage 的 `KD_GRAPHICS`+`K_OFF`）与 [escape-hardening.md](escape-hardening.md)（兜底 `NAutoVTs=0`、CAD mask、策略、`/*` 的坑）。
   静态侧：`tools/verify-image.sh` 的「kiosk 会话」+「kiosk 逃逸面加固」两组共 20+ 条断言。
3. ✅ **固化进镜像（已做）**：`lsp-kiosk.service` + `/etc/default/lsp-kiosk` +
   `logind.conf.d/99-lsp-kiosk.conf` + `usr/share/lsp-kiosk/10-lsp-kiosk.json` 进 `overlay/`、
   `cage` 进 `BASE_PACKAGES`；启用 `lsp-kiosk-display`、关闭 GDM 自启并 mask CAD 进构建脚本。
   Cage、Chromium、上位机与 socket 由 HDMI 状态控制，不独立 enable。
   按 [system-integration.md](../system-integration.md) 第 0 节四处同步。
4. ❌ **不做 `lsp-config kiosk on|off` 开关**（2026-09-23 用户明确："不需要通过 lsp-config 来切换"）。
   回桌面的正式办法是 **`lsp-maintenance on`**（见 [maintenance-mode.md](maintenance-mode.md)；⛔ 只 `start gdm3`、**不 enable**，
   否则重启也进桌面）。运维手段 = SSH（镜像里保留了）+ 长按电源键 2 s 关机。
