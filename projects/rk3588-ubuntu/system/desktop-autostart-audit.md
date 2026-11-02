# 桌面自启条目与常驻服务审计

> 屏蔽自启项的做法：**在包自带的 `.desktop` 末尾追加 `Hidden=true`**（幂等、必须在装完桌面包之后做，
> 否则会被包安装重写）。下面同时给出"开机 9 小时稳定态"的常驻服务审计清单。

## TL;DR

- 机制：GNOME 按 XDG 规范读 `/etc/xdg/autostart/*.desktop`，**`Hidden=true` = 这条不存在**；
  构建脚本用 `grep -q '^Hidden=true'` 守卫后追加。
- ⚠️ `X-GNOME-HiddenUnderSystemd=true` **只在会话由 systemd 管理时生效**（`pulseaudio.desktop` 自带这个键）。
- 审计结论（§"常驻服务审计"）：哪些常驻是**多余的**、哪些是必需的，都在那张表里；
  改桌面策略时先对照它。
- `tracker3` / `localsearch` 已主动拆除。

## 开机自启条目（`/etc/xdg/autostart`）：我们屏蔽了哪些、怎么屏蔽

### 机制

- GNOME 按 XDG autostart 规范读 `/etc/xdg/autostart/*.desktop`；**`Hidden=true` 表示"这条不存在"**。
- 我们的做法（`scripts/build-lsp-image.sh` 桌面段）：**在包自带的文件末尾追加 `Hidden=true`**，
  幂等（`grep -q '^Hidden=true'` 守卫）。必须放在桌面包装完之后 —— 包安装会重写这些文件。
- ⚠️ 另一个键 `X-GNOME-HiddenUnderSystemd=true`（`pulseaudio.desktop` 自带）**只在会话由 systemd 管理时生效**；
  会话一旦回落（见 [desktop-session.md](desktop-session.md) §1），这些条目会被真的拉起。**别拿它当保险**。

### 当前被我们追加 `Hidden=true` 的条目

| 条目 | 为什么 |
| --- | --- |
| `update-notifier.desktop` | 关闭自动更新检测的一部分 |
| `ubuntu-advantage-notification.desktop` | 同上（Ubuntu Pro 通知） |
| `ubuntu-report-on-upgrade.desktop` | 同上（遥测/上报） |
| `gnome-software-service.desktop` | 同上 |
| `pulseaudio.desktop` | 声音服务是 **pipewire-pulse**，这个条目会让 `pulseaudio` 去抢同一个 socket（实测 `exited with code 1`）。**屏蔽条目而不是删包**：`gnome-settings-daemon` 的依赖是 `pulseaudio \| pipewire-audio`，且 `/usr/share/pulseaudio/alsa-mixer/` 下还有我们的 overlay 文件 |

### tracker3 / localsearch 的完整拆除（2026-09-20 补完）

GNOME 文件索引是我们主动关掉的，但**拆得不彻底会留三类痕迹**：

| 层次 | 文件 | 处理 |
| --- | --- | --- |
| user 单元 | `/usr/lib/systemd/user/tracker-*`、`localsearch-*` | `rm -f` |
| 二进制 | `/usr/libexec/tracker-*`、`/usr/libexec/tracker3/*`、`/usr/bin/tracker3`、`/usr/bin/localsearch*` | `chmod -x`（不删，保留包完整性） |
| autostart 条目 | `/etc/xdg/autostart/tracker-miner-fs-3.desktop` | `rm -f` ← **2026-09-20 补**：光去掉执行位，gnome-session 每次开机仍报 `references a missing TryExec binary` |
| D-Bus 激活文件 | `/usr/share/dbus-1/services/org.freedesktop.Tracker3.Miner.Files{,.Control}.service`、`…Tracker3.Writeback.service` | `rm -f` ← **2026-09-20 补**：清掉 user 单元却留着它们，桌面图标那个 `ding` 扩展（= Nautilus）每次登录会去激活，产生 3 行报错：`Activation via systemd failed for unit 'tracker-miner-fs-3.service': Unit not found.` / `nautilus: Unable to create connection for session-wide Tracker indexer` |

⛔ **故意不动**的两类：
- `org.freedesktop.portal.Tracker.service`（xdg-desktop-portal 的文件搜索后端）：它配套还有
  `/usr/share/xdg-desktop-portal/portals/*.portal`，**只删一半**会变成"门户被广告但起不来"，比现在（用到才报错）更糟。
  真要拆得连 `.portal` 一起拆，目前没这个必要。
- 两个 `org.gnome.Nautilus.Tracker3.Miner.*.service`（**snap 版**，`Exec=/app/libexec/…`）：本机没 snapd，永久惰性。

判据：

```bash
ls /etc/xdg/autostart/ | wc -l                                  # 条目数
grep -L '^Hidden=true' /etc/xdg/autostart/*.desktop             # 没被屏蔽的
journalctl -b | grep -iE "autostart|tracker-miner|references a missing TryExec"
```

---

## 常驻服务审计（2026-09-20，开机 9 小时后的稳定态）

前提：`systemctl --failed` 为空，没有进程在烧 CPU（gnome-shell 1.0%，几个 devfreq/panthor kworker 0.1~0.2%）。

### 判断为"多余"的

| 东西 | 实测代价 | 为什么多余 | 状态 |
| --- | --- | --- | --- |
| `lttng-sessiond.service` | RSS **9,540 kB**，0% CPU，`lttng list` → `no available recording session` | 是 `ros-jazzy-tracetools` 依赖 `lttng-tools` 被拖进来的；ROS 真做 LTTng 跟踪时再 `systemctl start` 即可 | 未处理（等你定） |
| `rkaiq_3A.service` | 每次开机跑 **6 ms**、往 journal 打 8 行 `ERR: Bad media topology for: /dev/media8..15` 后退出，**不留进程** | Rockchip ISP 3A 守护，服务 MIPI CSI 相机；本板用 USB(UVC) 相机 | 未处理（等你定） |
| `gsd-wacom` | RSS **23.1 MB** | 没有数位板 | 未处理 |
| `gsd-printer` + `gsd-print-notif` | **13.2 + 10.2 MB** | `cups.service` 是 `not-found`，打印栈根本没装 | 未处理 |
| `gsd-sharing` / `gsd-wwan` / `gsd-smartcard` / `gsd-housekeeping` / `gsd-usb-protect` / `gsd-a11y-settings` | 10.8 / 8.5 / 7.2 / 6.9 / 6.6 / 6.3 MB | 对应硬件/功能都不存在 | 未处理（合计 ≈93 MB） |
| `avahi-daemon` | RSS 2,908 kB | 在客户网段广播 `example-project.local`；留着的唯一价值是 `.local` 解析 | 未处理 |
| `colord` + `gsd-color` | 13.5 + 23.4 MB | ICC 色彩管理，固定 LCD 上未必需要 | 未处理（会影响显示配置，动前要问） |

⛔ **GNOME 45+ 没有"插件开关"了**：`gsettings get org.gnome.settings-daemon.plugins.wacom active`
→ `No such schema`（`wwan`/`housekeeping`/`color` 是 `No such key "active"`）。每个插件是**独立进程**，
由 gnome-session 按 `/etc/xdg/autostart/org.gnome.SettingsDaemon.*.desktop` 拉起
⇒ 要禁用只能覆盖这些 `.desktop`（`Hidden=true`），不能靠 gsettings。

### 判断为"缺失"的（比多余更值得管）

| 缺什么 | 症状 | 说明 |
| --- | --- | --- |
| `ibus` + `ibus-libpinyin` | gnome-shell 每次开机两次 `Failed to launch ibus-daemon: No such file or directory` | 镜像装了 `language-pack-zh-hans` + `fonts-wqy-zenhei`，但**没有输入法框架** ⇒ 中文只能看不能打 |
| `dbus-user-session`、`rtkit` | 见 [desktop-session.md](desktop-session.md) §1 | **2026-09-20 已修** |

### 运行中的系统级常驻（对照，都是正常的）

`systemd-journald` 20.9 MB / `NetworkManager` 20.1 MB / `systemd-resolved` 12.4 MB / `udisksd` 12.2 MB /
`polkitd` 11.4 MB / `wpa_supplicant` 11.0 MB / `accounts-daemon` 7.5 MB / `upowerd` 8.8 MB / `colord` 13.5 MB。
`serial-getty@ttyFIQ0` 保留（调试串口要能登录）。

---
