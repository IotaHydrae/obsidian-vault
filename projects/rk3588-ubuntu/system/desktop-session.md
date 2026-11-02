# 维护桌面会话（D-Bus / GDM / 轻量桌面评估）

> 当前通过 `lsp-maintenance on/off` 临时运行 GDM + GNOME Wayland，GDM 不开机自启。
> 构建显式安装 `dbus-user-session rtkit`，为桌面与 PipeWire 提供会话总线和实时调度服务。
> GNOME 默认终端为 GNOME Terminal；首次登录默认开启 NumLock，之后记住用户切换状态。
> labwc 可作为替代方案研究，目前继续使用 GNOME 维护桌面。

## TL;DR

- ⛔ 根因与修法见 §1；判据：`systemctl --user show-environment | grep -i dbus` 应有
  `DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus`。
- 音频（判"没声音"三步、`pulseaudio` 为什么 inactive、音量 >100% 的真相）→
  [audio-pipewire.md](audio-pipewire.md)。
- 开机自启条目与常驻服务审计（我们屏蔽了哪些、怎么屏蔽）→
  [desktop-autostart-audit.md](desktop-autostart-audit.md)。
- GDM 自动登录的落点（`/etc/gdm3/custom.conf` + AccountsService + `.dmrc`）见 §2。
- SSH 中检查 user manager 时，以目标账户设置其 `XDG_RUNTIME_DIR` / 总线地址；journal 读取权限按实际组与 ACL 判断。
- 「能不能用 wlroots + labwc 换掉维护桌面」的**可行性结论**：可行但不采用（验证范围与集成要求见
  §「试过但不采用」那节）。

> 下文 D-Bus 故障输出保留自 2026-09-20 的 Ubuntu 24.04 arm64 / Rockchip BSP 6.1.172 记录，
> 不代表当前机器状态。2026-10-13 的核对使用开发板 systemd 255.4-1ubuntu8.17 的包元数据、
> 既有实验 journal 与当前工程脚本；没有重新播放音频、切换桌面或测量性能。

---

## 0. 维护桌面检查入口

| 组件 | 单元 / 文件 | 检查目标 | 说明 |
| --- | --- | --- | --- |
| 会话总线 | `dbus.socket` + `dbus.service`（**user**） | 目标账户总线可连接，路径 `/run/user/<UID>/bus` | 由 `dbus-user-session` 包提供，**必须显式装**（见 §1） |
| 会话总线地址 | user manager 环境里的 `DBUS_SESSION_BUS_ADDRESS` | `unix:path=/run/user/1000/bus` | 判据：`systemctl --user show-environment \| grep -i dbus` |
| 会话管理器 | `wireplumber.service`（user） | 维护会话中 active | 失败会影响音频设备管理，见 [audio-pipewire.md](audio-pipewire.md) |
| 音频服务 | `pipewire.service` / `pipewire-pulse.service`（user） | active | pulse socket 的 owner 是 `pipewire-pulse` |
| 实时优先级 | `rtkit-daemon.service`（system） | D-Bus 按需激活，不要求开机自启 | 由 `rtkit` 包提供，客户端请求结果需另查 |
| 声卡 | `es8388` / `hdmi0` / `hdmi1` / `dp0` | 核对实际板型提供的设备 | `pactl list cards short` 只证明设备被发现，不证明播放 |
| 声音服务选择 | `pulseaudio.socket` / `pulseaudio.service`（user） | 使用 `pipewire-pulse` 时可为 inactive | 服务关系见 [audio-pipewire.md](audio-pipewire.md) |
| 文件索引 | tracker3 / localsearch | 已主动拆除 | 见 [desktop-autostart-audit.md](desktop-autostart-audit.md) |

### 默认终端与 NumLock

GNOME Terminal 的三个入口在构建时保持一致；这里配置终端程序，账户的登录 shell 不变。

| 入口 | 默认值与来源 |
| --- | --- |
| 兼容 schema | `org.gnome.desktop.default-applications.terminal` 的 `exec='gnome-terminal'`、`exec-arg='--'`，由 `99-lsp-desktop.gschema.override` 提供 |
| `Ctrl+Alt+T` | `etc/dconf/db/local.d/03-default-terminal` 定义自定义快捷键，执行 `gnome-terminal` |
| `x-terminal-emulator` | `rootfs` 阶段通过 alternatives 选择 `/usr/bin/gnome-terminal.wrapper` |

`etc/dconf/db/local.d/04-numlock` 与 schema override 同时设置
`org.gnome.desktop.peripherals.keyboard` 的 `remember-numlock-state=true`、`numlock-state=true`。
这些是不加锁的默认值：新 GNOME 会话先开启 NumLock，用户之后按键切换的状态可被记忆；已有用户配置优先。
2026-10-23 开发板运行内核 `#13` 的 GNOME Wayland 会话中，GNOME Terminal 启动成功，用户确认 `Ctrl+Alt+T` 打开该终端及小键盘可直接输入 `123`。
停止 GDM 后重置上述两个 NumLock 键，Gio 的 `get_user_value()` 均为 `None`、有效值均为 `true`；重新进入 GNOME Wayland 后，有效值仍为 `true`，`input6::numlock/brightness=1`。
这验证了无用户覆盖时的会话初始化；尚未验证用户手动切换 NumLock 后下次登录的记忆行为，也未以新镜像启动重测。
同会话中 MCAP 的 `app.should_show()` 为 `false`，默认 MIME 处理器仍为 `lsp-mcap.desktop`。

---

## 1. D-Bus 会话总线缺失的故障表现（2026-09-20）

### 1.1 当时的症状与检查范围

1. **整机没声音**：`wireplumber.service` 开机即死，PipeWire 只剩 `Dummy Output`，4 张声卡一张都不挂。
2. **gnome-session 回落到非 systemd 启动流程**：开机 8 条
   `WARNING: Could not check if unit gnome-session-wayland@ubuntu.target is active: … Name "org.freedesktop.systemd1" does not exist`
   → `Falling back to non-systemd startup procedure`。
3. **`RTKit error: org.freedesktop.DBus.Error.ServiceUnknown`**（pipewire / wireplumber 都报），拿不到实时优先级。

前两项与缺失 user bus 有关；RTKit 使用系统总线，`rtkit` 包和服务是否可用是独立条件。
当时同时补齐了 `dbus-user-session rtkit`，不能据此把所有实时调度错误归因于会话总线。

### 1.2 证据链

```
$ systemctl --user status wireplumber
× wireplumber.service - Multimedia Service Session Manager
     Active: failed (Result: exit-code) since 2026-09-20 09:56:40 CST; 9h ago
    Process: 1272 ExecStart=/usr/bin/wireplumber (code=exited, status=70)
Sep 20 09:56:40 systemd[1131]: wireplumber.service: Start request repeated too quickly.
Sep 20 09:56:40 systemd[1131]: Failed to start wireplumber.service - Multimedia Service Session Manager.

$ journalctl --user -b | grep autolaunch
wireplumber[1146]: Failed to connect to session bus: Unable to autolaunch a dbus-daemon without a $DISPLAY for X11
pipewire[1144]:   mod.rt: Failed to connect to session bus: Unable to autolaunch a dbus-daemon without a $DISPLAY for X11
pipewire-pulse[1147]: mod.rt: Failed to connect to session bus: Unable to autolaunch a dbus-daemon without a $DISPLAY for X11
```

```
$ dpkg -l dbus-user-session
un  dbus-user-session   <none>            ← 没装
$ ls /usr/lib/systemd/user/ | grep -i dbus
at-spi-dbus-bus.service                  ← 没有 dbus.socket / dbus.service
gnome-session-restart-dbus.service
$ systemctl --user show-environment | grep -i dbus    ← 空：user manager 环境里没有总线地址

$ ss -xlp | grep dbus
u_str LISTEN ... /tmp/dbus-vzoSehHDys   users:(("dbus-daemon",pid=1167))   ← 总线在 /tmp，gdm 自己起的私有总线
```

**链条**：`dbus-user-session` 没装 ⇒ 没有 systemd 管的会话总线（`dbus.socket` → `/run/user/1000/bus`），
只剩 `dbus-x11` 的 autolaunch 路径，而那条路需要 `$DISPLAY` ⇒ `systemd --user` 启动的那批单元
（pipewire / wireplumber / pipewire-pulse）拿不到总线地址 ⇒ wireplumber 直接 `exit 70`
（pipewire/pipewire-pulse 只降级不退出）⇒ 没有会话管理器 ⇒ **PipeWire 没有 ALSA 设备节点**。
`gnome-session` 找不到 `org.freedesktop.systemd1` 是同一件事的另一面：**user manager 没挂在会话总线上**。

### 1.3 为什么 `--no-install-recommends` 会漏掉它

- 本板 noble 的 `pipewire-bin 1.0.5-1ubuntu3.3` 推荐 `dbus-user-session`、`wireplumber` 和 `rtkit`；
  严格安装模式不能依赖这些 Recommends 自动带入。`systemd` 的 Recommends 没有 `dbus-user-session`。
- `dbus` 建议 `default-dbus-session-bus | dbus-session-bus`；`dbus-user-session` 提供这两个虚拟包，
  `dbus-x11` 只提供 `dbus-session-bus`，不能替代 systemd 管理的 user bus。
- 两包可以共存；构建保留 `dbus-x11` 提供的 `dbus-launch`，并显式安装 `dbus-user-session rtkit`。

### 1.4 修法（已进构建脚本）

`scripts/build-lsp-image.sh` 的 `BASE_PACKAGES` 增加 `dbus-user-session rtkit`
（`dbus-x11` 保留：它提供 `dbus-launch`，两者可共存）。

### 1.5 板上验证（2026-09-20，重启后）

```
$ systemctl --user show-environment | grep -i dbus
DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
$ ls -l /run/user/1000/bus
srw-rw-rw- 1 board board 0 Sep 20 19:07 /run/user/1000/bus

$ systemctl --user is-active pipewire wireplumber pipewire-pulse
active / active / active
$ pactl list cards short
42 alsa_card.platform-dp0-sound      alsa
43 alsa_card.platform-es8388-sound   alsa
44 alsa_card.platform-hdmi0-sound    alsa
45 alsa_card.platform-hdmi1-sound    alsa
$ pactl list sinks short
50 alsa_output.platform-es8388-sound.HiFi__hw_rockchipes8388__sink  PipeWire  s24-32le 2ch 48000Hz
58 alsa_output.platform-hdmi0-sound.HDMI__hw_rockchiphdmi0__sink    PipeWire  s32le    2ch 48000Hz

$ systemctl --user --failed          → 0 loaded units listed
$ journalctl -b | grep -E "Falling back to non-systemd|session bus|RTKit"   → 无输出
$ journalctl -b | grep rtkit
systemd[1]: Started rtkit-daemon.service - RealtimeKit Scheduling Policy Service.
rtkit-daemon[1083]: Successfully called chroot.
```

顺带：`pulseaudio.desktop` 的 `WARNING: App 'pulseaudio.desktop' exited with code 1` **也自己消失了** ——
那个 desktop 文件带 `X-GNOME-HiddenUnderSystemd=true`，之前只有因为会话回落到非 systemd 路径才被真拉起。
**所以"会话能走 systemd"这件事本身就是好几类 autostart 噪音的开关。**

⚠️ **只验证到"sink 存在"，没有实际播放测试**（`wpctl status` 里默认 sink 已自动切到
`ES8388 Headphones + Speaker`）。

---


## 2. GDM 自动登录与会话（构建开关落在哪）

构建脚本的 `AUTO_LOGIN_USER` / `AUTO_LOGIN_SESSION` 对应三处文件：

| 文件 | 内容 |
| --- | --- |
| `/etc/gdm3/custom.conf` | `AutomaticLoginEnable=true` + `AutomaticLogin=<用户>`；保持 `WaylandEnable=true` |
| `/var/lib/AccountsService/users/<用户>` | `Session=<会话>` / `XSession=<会话>`（GDM 以 AccountsService 为权威来源），权限 0600 |
| `~<用户>/.dmrc` | 会话回退配置 |

会话名可选 `ubuntu-wayland`（Wayland）或 `ubuntu-xorg`（Xorg），由 `ubuntu-session` 包提供。
LightDM / SDDM 分支暂不生成自动登录与会话配置，换桌面时需自行配置。

### 桌面会话默认 Wayland

构建默认同时配置下面三处；`lsp-maintenance on` 负责会话切换和互锁，不修改会话选择值：

| 位置 | 构建默认值 | 说明 |
| --- | --- | --- |
| `/var/lib/AccountsService/users/board` | `Session=ubuntu-wayland` / `XSession=ubuntu-wayland` | GDM 会话选择来源；0600，登录器可能回写 |
| `/etc/gdm3/custom.conf` | `WaylandEnable=true` | 允许 GDM 使用 Wayland；设 `false` 禁用 GDM 的 Wayland 会话 |
| `/home/board/.dmrc` | `Session=ubuntu-wayland` | 回退（非 GDM 的登录器/工具会读它） |

`tools/verify-image.sh` 检查 AccountsService 的 `Session`、GDM 的 `WaylandEnable`、`.dmrc`
与会话 desktop 文件；当前未单独断言 `XSession`。实际会话类型以 logind 为准：

```bash
sudo grep -E 'Session|XSession' /var/lib/AccountsService/users/board   # 0600，要 sudo
grep WaylandEnable /etc/gdm3/custom.conf; cat /home/board/.dmrc
loginctl list-sessions
# 先从列表确认图形会话 ID，避免选到同账户的 SSH 会话。
loginctl show-session "${SESSION_ID:?Set graphical session ID}" -p Type -p Active -p Seat
```

手动选择 Xorg、会话文件缺失或 Wayland 被禁用/不可用，都需要独立排查；预置 Wayland
不等于保证实际一定启动 Wayland。`ps` 中的 `gdm-x-session` / `Xorg` 也可能属于 greeter，
不能用它代替目标账户的 `loginctl Type`。

**`gdm-hack.sh` 的隐患与收紧**（2026-10-09）：这个 hack（由 `99-gdm-hack.rules` 在 `fb0` add 时触发）
原先是"只要看到 `gdm-x-session` 就 `systemctl restart gdm`" ⇒ 会把手动选的 X11 会话顶掉，重启后
重新按默认值选择会话。现在加了一道闸门：**当前存在任何非 gdm 的登录会话就直接退出**，
包括 SSH 会话，并非只判断活动图形会话。脚本仅在 GDM 已 active 且配置允许 Wayland 时继续；安装权限为 0755。判据：
存在非 gdm 会话时脚本应打印 `gdm-hack: skipped (a non-gdm session is active)` 且 rc=0。

**既有微基准（2026-10-09，同一块板）**：`es2gears`（Mali-G610 / Mesa Panfrost、
Mesa 25.2.8；`vblank_mode=0`，es2gears 每 5 s 打一行 FPS）：

| 会话 | 工具 | 样本 | 中位 FPS |
| --- | --- | --- | --- |
| X11（Xorg，`DISPLAY=:1`） | `es2gears_x11` | 1（**样本太少**） | 1656 |
| Wayland（GNOME） | `es2gears_wayland` | 4 | **6395**（谷 5922 / 峰 6430） |

口径：这里只记录该负载下的 FPS，不能单独隔离客户端提交或合成器开销，也未验证显示帧率、
用户可见延迟或一般 UI 性能。X11 只有 1 个样本，**不足以支撑"快几倍"的结论**；本轮没有重测。
会话环境应从**目标会话进程的 `/proc/<pid>/environ`** 核对：该次 X11 使用 `DISPLAY=:1` 和
`XAUTHORITY=/run/user/1000/gdm/Xauthority`。该次 `gnome-shell` 的 environ 未提供 `WAYLAND_DISPLAY`，
Wayland 测试使用了 `wayland-0` 并 `env -u DISPLAY`；重测时须核对实际 socket，不能沿用固定名称。

## 3. 试过但不采用：wlroots + labwc 当维护桌面

结论：labwc 可以承载维护桌面的基本会话，当前继续使用 GDM + GNOME。
完整替代还需组装面板、启动器、输入法、缩放、授权代理、音频与网络 UI，
并验证现有运维工具。减少组件有潜在内存收益，现有记录不足以给出可靠的容量或性能结论。

### 已核对的实验范围

2026-10-10 的板级实验未纳入镜像构建。2026-10-13 只读复核确认：

| 项 | 依据与范围 |
| --- | --- |
| 组件 | `labwc 0.7.1-1build1`、`libwlroots12t64 0.17.1-2.1build1`、`sfwbar 1.0~beta13-1build2` 的板上包元数据 |
| 会话入口 | 板上实验单元 `lsp-desktop-test.service` 为 disabled，配置 `Type=simple`、`User=board`、`PAMName=login`、`TTYPath=/dev/tty3`、`ExecStart=/usr/bin/labwc`；不是镜像正式入口 |
| 启动与互锁 | 10 月 10 日 journal 显示 kiosk 抢占时 labwc 报 `unable to start the wlroots backend`；维护标记使调度保留维护桌面后，实验单元成功启动 |
| 测量限制 | 现有记录缺完整性能采样方法与原始数据，不给出内存、CPU 或启动时延结论；`Type=simple` 的 start 返回不代表首帧或桌面就绪 |

最小候选组合是 labwc + sfwbar + 文件管理器 + 终端 + lxpolkit + 所需 desktop portal。
各组件能否开窗、提权、输入中文和稳定运行，不能仅靠包安装或单元 active 判定。

### 会话、cgroup 与 journal 的检查方法

- 自建会话可能没有切到活动 VT：先用 `loginctl show-session` 查 `Active` / `Seat` / `Type`，
  需要时使用 `loginctl activate`；启动任务成功不证明屏幕已显示。
- 此板使用 hybrid cgroup：cgroup2 在 `/sys/fs/cgroup/unified`，但 memory controller 仍是 v1。
  按 `/proc/<pid>/cgroup` 和实际挂载定位，v1 读 `memory.usage_in_bytes`，v2 读 `memory.current`；
  不能在 unified 下盲拼会话路径。`systemd-cgtop` 是另一种观测入口，需说明聚合范围。
- `PAMName=login` 配合 `pam_systemd` 可能将进程关联到 session scope。
  `journalctl -u` 未找到客户端输出时，用 `journalctl -o verbose` 核对 `_SYSTEMD_UNIT`、
  `_SYSTEMD_USER_UNIT`、`_SYSTEMD_CGROUP`，再按 scope、PID 或时间窗查询；不能断言子进程 stdout 必然不属于原单元。

### Noble 上复用 Budgie 控制中心

[Budgie 10.10 官方发布说明](https://buddiesofbudgie.org/blog/budgie-10-10-released)
确认其桌面转向 Wayland 并推荐 labwc。Noble 的独立控制中心实验只是复用一个应用，
不等于安装或验证了 Budgie 10.10 的完整桌面。

板上 `budgie-control-center 1.4.0-2build3` 对 `budgie-desktop` 是 Recommends，
实验用 `--no-install-recommends` 避免引入该桌面包；当前 `budgie-desktop` 未安装。
`budgie-desktop-environment 0.20.13` 则 Depends `budgie-desktop`，会带入 Noble 的 X11 桌面栈，
但安装元包不等于自动把已有会话切成 X11。
Nemo、swaybg 等可独立组装；总包数与安装体积应按实际依赖解析统计。

### HDMI 生命周期互锁

替代会话必须参与 [HDMI 会话契约](kiosk/session-design.md)：
`99-lsp-kiosk-hdmi.rules` 在 DRM card/connector 的 add/change/remove 时排队运行一次检查，
实际 HDMI 连接状态、维护标记和 GDM 状态决定是否启动 kiosk。
只有匹配的 DRM 事件才触发，不能泛指任何 udev 事件或保证每次改分辨率都会触发。

自建 labwc 单元不会自动被识别成 GDM。进入替代会话前须建立维护互锁、停止冲突会话，
退出或失败时再恢复 HDMI 调度；实现应接入 [维护模式](kiosk/maintenance-mode.md) 的生命周期。
现有 `lsp-maintenance on` 启动的是 GDM，不能直接用它启动 labwc；也不能只建标记就当作完成切换。

---

## 4. 速查命令

```bash
# 在目标账户中执行（本项目维护账户为 board）。
export XDG_RUNTIME_DIR="/run/user/$(id -u)"
export DBUS_SESSION_BUS_ADDRESS="unix:path=${XDG_RUNTIME_DIR}/bus"

systemctl --user show-environment | grep -iE "dbus|user="   # 会话总线地址/USER
systemctl --user --failed                                    # 用户会话里的失败单元
systemctl --user list-units --type=service --state=running   # 用户侧常驻
systemctl --user show "${UNIT:?Set user unit}" -p ConditionResult -p AssertResult -p ConditionTimestamp
systemctl list-units --type=service --state=running          # 系统侧常驻
systemctl list-timers --all                                  # 定时器（apt-daily/ua-timer 等应已 mask）
ps -eo pid,rss,pcpu,etimes,comm --sort=-rss | head -30       # 谁占内存
grep -H '^Exec=' /etc/xdg/autostart/*.desktop              # 仅列入口，参数与 PATH 还需核对
```

⚠️ **审计 autostart 时别只看 `[ -x "$exec" ]`**：`Exec=update-notifier` 这种没有绝对路径的项要过 PATH，
否则会把 `start-pulseaudio-x11`、`xdg-user-dirs-update` 之类**误报成"缺二进制"**（踩过）。

---

## 5. 相关文档

- 系统集成总账（哪些文件要同步改）：[system-integration.md](system-integration.md)
- 桌面/系统策略（熄屏、自动挂载、串口 getty）：tuning/（镜像源码：`notes/system/tuning`）、
  [removable-media-policy.md](removable-media-policy.md)
- 镜像里装了哪些包、为什么装：[../build/deb-packages.md](../build/deb-packages.md)
- 构建期"少装了什么"的同类坑（`Recommends` 被 `--no-install-recommends` 丢掉）：
  [system-integration.md](system-integration.md) 里的 `systemd-resolved`/`plymouth`/`wireless-regdb`
