# kiosk 会话的启动与运行契约

> 仅在 HDMI 已连接时启动 kiosk 的 Cage、Chromium 和上位机；拔掉全部 HDMI 后停止整套服务，维护模式不被热插拔切回 kiosk。

## TL;DR

- `User=kiosk`、`PAMName=login` 建立 seat 会话，Cage 接管 tty1 与 DRM。
- 开机与 DRM 热插拔由 `lsp-kiosk-display.service` 一次性检查；无常驻轮询进程。
- 无 HDMI 时不启动 Cage、Chromium、上位机或其 socket，手动 start 也受门控。
- `Conflicts` 与 `After=getty@tty1.service` 配合，避免 agetty 抢终端；GDM 默认不开机启动。
- Chromium 先打开极简 `loading.html#${LSP_KIOSK_URL}`；后端与浏览器并行启动，连接失败在页内重试。
- 按键过滤和 NumLock 在 Cage 中执行；Chromium 策略仅在 kiosk 会话期间装入。
- `RestartSec=0` 且关闭启动限流，持续恢复显示；停止直接 SIGKILL，退出预算 20 秒。

## HDMI 生命周期

用户要求：正常使用不接屏幕，不应为 kiosk 和上位机占用内存；插上 HDMI 才启动，拔掉就停止。

| 状态 | 行为 |
| --- | --- |
| 开机无 HDMI / 所有 HDMI 断开 | 停止 `lsp-kiosk.service`、`oem-host.service`、`oem-host.socket`，退出 Plymouth |
| 任意 HDMI-A 输出为 `connected` | 启动 socket、上位机和 kiosk；后端初始化与 Chromium 冷启动并行 |
| 维护标记在 / GDM 已启动或正在切换 | 保留维护桌面，HDMI 事件不启动 kiosk；断开 HDMI 仍停止 HMI 后端与 socket |
| 退出维护模式 | 清除标记并重新检查 HDMI；无屏保持停止，有屏回 kiosk |

只启用 `lsp-kiosk-display.service` 的开机检查，不独立 enable 三个图形栈单元。
`99-lsp-kiosk-hdmi.rules` 接收 DRM card/connector 的 add/change/remove，使用
`systemctl --no-block restart` 排队检查，不在 udev worker 中启动图形进程。
每次重新读取 `/sys/class/drm/card*-HDMI-A-*/status`，不写死 card0，也不把 Writeback/DSI 当成 HDMI。
`Type=oneshot` 检查结束即退出；进程崩溃仍由原服务的 `Restart=` 恢复。

`/run/lsp-kiosk/hdmi-connected` 仅由检查脚本发布/撤回，三个单元均有条件门控。
Cage 与后端启动前还通过 `ExecCondition` 复核实际 HDMI 状态；Cage 同时复核维护模式。
标记目录由 tmpfiles 创建，`/run/lsp-kiosk/maintenance` 只持续到本次开机结束。
维护脚本先写标记再停 Cage，避免 DRM 事件在停 Cage、起 GDM 的间隙抢回会话。
后端允许写这个运行目录，让经受限 sudo 调用的维护入口在私有 namespace 内也能设置标记。

`tests/kiosk-hdmi.sh` 在已接 HDMI 的板上用 DRM `off`/`detect` 和 udev change 验证真实服务：
启动、全部进程退出、8080 关闭、无屏手动启动被挡、重新连接与维护标记。
该测试会短暂熄屏并重启图形栈；退出恢复自动检测，**不等同于物理拔线或冷启动验证**。

板上验证（2026-10-10）：回归 **11 通过 / 0 失败**；另做物理拔插，驱动发出
`HOTPLUG=1`，断开后三个单元均 `inactive`、8080 关闭，插回后均 `active`、HTTP 200。
一次停止对照中 kiosk 账户 PSS 合计 **726,184 → 35,051 KiB**（约减少 675 MiB）；
这只是该次负载下的观察，不作为性能阈值。后端 mount namespace 中维护目录可写也已验证。

## 账户与会话

kiosk 密码锁定，具有 video/audio/render/plugdev/board 等所需组权限，
不属于 sudo、adm、systemd-journal 组。sudo 仅授权固定命令，见 [sudo-policy.md](sudo-policy.md)。
`PAMName=login` 运行 PAM session，logind 提供 seat0 会话与设备权限；锁定密码的账号可建立该会话。
显式 `XDG_RUNTIME_DIR=/run/user/1001` 必须与 kiosk UID 对齐，镜像体检会核对。

单元与 `gdm.service`、`getty@tty1.service` 互斥，并在 getty 后排序。
两条 getty 约束须同时保留：单独占 tty1，可能被稍后打开终端的 agetty 挂断。
GDM 由维护模式临时启动，重启后按 HDMI 连接状态启动 kiosk，见 [maintenance-mode.md](maintenance-mode.md)。
GDM 的 Wayland、board 自动登录配置由 `overlay/etc/gdm3/custom.conf` 提供。

## 浏览器、后端与显示交接

`Wants=oem-host.service` 启动后端，`After=` 只排序进程启动。
Chromium 冷启动与后端初始化并行；极简引导页仅保留同色背景，5 秒后仍未跳转才显示启动提示，
不复制 HMI 表单、CSS 或时钟。它也需要浏览器开始绘制，不能覆盖 Chromium 自身的冷启动空窗。
`oem-host.socket` 使 HEAD 请求排队等待后端接管；引导页收到响应头后导航至目标 URL，避免重复下载主页正文。
连接失败后等 1 秒再试，同时只有一个在途请求；请求挂起时没有应用层截止时间，与原实现相同。
`no-cors` 返回 opaque response，无法读取状态码；GET-only 路由的 HEAD 405 也会触发导航，
因此它是连接恢复入口，不是 HTTP 200 或业务就绪检查。HMI 显示后的后端故障不由引导页处理。
实现以 `/usr/share/lsp-kiosk/loading.html` 为准；直连取舍与板测见 [chromium-cold-start.md](../chromium-cold-start.md)。
`LSP_KIOSK_WAIT_S` 是保留的兼容值，当前单元没有使用它的等待命令。

必须经过 `/usr/bin/chromium` 包装脚本，以加载包自带的 `/etc/chromium.d/default-flags`
和 RKMPP 硬解开关。镜像追加配置 `99-lsp-flags` 使用 basic password store
避免无密码会话的 keyring 弹窗，并关闭启动期后台联网与首次运行向导。

`/etc/default/lsp-kiosk` 只接受环境赋值：

| 参数 | 当前作用 |
| --- | --- |
| `LSP_KIOSK_URL` | 默认本地 `http://127.0.0.1:8080/`，作为着陆页 fragment 传入 |
| `LSP_KIOSK_BACKGROUND` | Cage 底色 `#f3f6fa`，需与实际页面背景一致 |
| `LSP_KIOSK_WAIT_S` | 兼容保留，当前服务不使用 |

改 URL 须核对 socket 和 `/usr/share/lsp-kiosk/10-lsp-kiosk.json` 的 allowlist。
底色仅影响合成器画面，Chromium 自己提交的 buffer 可覆盖它；细节见
[compositor-customization.md](compositor-customization.md)。

Plymouth 的退出由 kiosk 最后一条前置动作执行，然后 Cage 接管 DRM/VT：

| 覆盖文件 | 当前合并结果 |
| --- | --- |
| `plymouth-quit.service.d/10-lsp-keep-splash.conf` | 将退动画动作替换为 `/bin/true`，保留排序 |
| `plymouth-quit-wait.service.d/check-hdmi.conf` | 保留无显示输出时的提前退出检查 |
| `plymouth-quit-wait.service.d/zz-lsp-keep-splash.conf` | 字典序覆盖 check-hdmi 的等待命令，最终 ExecStart 为 `/bin/true` |
| `lsp-kiosk.service.d/10-lsp-quit-plymouth.conf` | 主单元前置动作执行完后 `-+plymouth quit`，无屏或已退出也放行 |

## 策略与恢复

Chromium 强制策略是系统级配置。策略源放在 `/usr/share/lsp-kiosk/`，
kiosk 启动时 `+install` 到 `/etc/chromium/policies/managed/`，停止后 `+rm` 撤回；
`+` 保证以完整权限写入。GDM 前置动作再次删除残留，避免桌面浏览器加载 kiosk 策略。
allowlist 须同时放行着陆页和 HMI；具体策略的生效范围见 [escape-hardening.md](escape-hardening.md)。

`PasswordManagerEnabled=false` 用于关闭维护密码提交后的 Chromium 保存密码提示。
[官方策略定义](https://chromeenterprise.google/policies/#PasswordManagerEnabled)说明：禁用后不能保存新密码，
已有密码仍可使用；此项不删除历史凭据。镜像体检与 `kiosk-lockdown.sh` 检查配置，
界面验收需在真实 kiosk 中提交维护密码，确认右上角不再弹出保存密码提示。
板上验证（2026-10-13，LubanCat-5 V2、新 SD 镜像）：运行策略值为 false，
现场提交维护密码后确认不再弹出保存密码提示。

Cage 使用独立客户端 XKB 状态，只转发放行的按键及修饰状态，见 [keyfilter.md](keyfilter.md)。
Cage 运行时 `KD_GRAPHICS`、`K_OFF` 阻断内核 VT 切换键；`NAutoVTs=0` 与
`ctrl-alt-del.target` mask 兜住会话未建立的窗口。

`Restart=always`、`RestartSec=0` 缩短显示中断，`StartLimitIntervalSec=0` 避免永久停止重试。
停止使用 `KillSignal=SIGKILL`，因该可丢弃会话曾在交还 DRM/VT 时不响应 SIGTERM；
`TimeoutStopSec=20` 限制其它停止步骤。已知问题与验证见 [maintenance-mode.md](maintenance-mode.md)。
`SuccessExitStatus=SIGKILL` 使主动停止成为正常结束；`Restart=always` 仍恢复意外退出。
HDMI 检查单元接受重排检查时的 SIGTERM，避免密集 DRM 事件产生无意义的 signal 告警。

NumLock 由 Cage 在会话启动及键盘热插拔时设置，不在 ExecStartPost 创建辅助终端进程。
有关终端挂断的约束见 [cage-sighup.md](cage-sighup.md)。

## 验证依据

```sh
systemctl show lsp-kiosk -p Conflicts -p After -p NRestarts -p StartLimitIntervalUSec -p TimeoutStopUSec
loginctl list-sessions
journalctl -b -u lsp-kiosk -u oem-host
```

`tools/verify-image.sh` 核对 UID、URL、策略与会话配套；
`tests/kiosk-hdmi.sh` 验证 HDMI 门控；`tests/kiosk-lockdown.sh`、`tests/kiosk-policy-probe.sh`、
`tests/cage-keyfilter.sh` 验证运行中的会话。
这些测试涉及真实会话和输入注入，按各测试声明的环境执行。
