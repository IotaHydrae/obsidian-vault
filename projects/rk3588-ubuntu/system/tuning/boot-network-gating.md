# 本地启动与有界网络等待如何分工

> 本地 HMI、会话和采集直接启动；MQTT 上报与更新器各自有界等待 NetworkManager startup，避免联网过程门控本地显示。

## TL;DR

- `oem-host`、`collect_system` 只需 loopback，用前置 `ip link set lo up` 保证。
- `rc-local`、`systemd-user-sessions` 去掉不必要的网络排序；发行版副本在升级时重新核对。
- NetworkManager 使用 `Type=simple`，started 仅代表进程启动。
- reporter、updater 使用 `-nm-online -s -q --timeout=15`，超时仍放行；业务自行重试或本地回退。
- `nm-online -s` 等 NM startup 完成，不保证 broker、发布服务或互联网可达。

## 当前配置分工

| 单元 | 网络处理 |
| --- | --- |
| `oem-host.service`、`collect_system.service` | 不排任何网络目标；前置 `-+ip link set lo up` |
| `lsp-kiosk.service` | 只排本地 HMI 进程启动顺序；着陆页重试本机连接，不等外网或 HTTP 就绪 |
| `rc-local.service`、`systemd-user-sessions.service` | 同名覆盖去掉 `network.target` |
| `rc-local.service.d/debian.conf` | 同名覆盖去掉 `network-online.target` |
| `NetworkManager.service.d/10-lsp-ready-early.conf` | `Type=simple`，启动事务不等待连接激活 |
| `device_reporter.service` | `After/Wants=network.target`，前置有界 nm-online；失败重试 |
| `data-collection-boot.service` | `After/Wants=network.target`，drop-in 有界 nm-online；更新器处理网络失败与本地版本 |

`nm-online -s` 检查启动阶段是否完成，`-q` 关闭进度输出，`--timeout=15` 为秒数。
`ExecStartPre` 的 `-` 前缀允许超时后继续。网络服务仍须处理 DNS、路由、连接和远端故障。
`network.target` 自身也不承诺网络可达，不能用 After 它作为业务连接成功的判据。

采集单元默认 disabled，现场启用后的 reporter/updater 等待会影响其各自启动完成时间。
本地会话无需等待这两条业务链。离线时间由 RTC 等机制维持，时钟精度要求见
[services-wait-for-clock.md](services-wait-for-clock.md)。

## 为何使用同名覆盖

systemd 的依赖列表在 drop-in 中追加，空 `After=` 不能移除主单元已有的排序。
需要删除依赖时，本工程使用：

- `/etc/systemd/system/rc-local.service` 与 `systemd-user-sessions.service` 完整覆盖发行版同名单元；
- `/etc/systemd/system/rc-local.service.d/debian.conf` 同名覆盖 `/usr` 下 Debian drop-in。

改一个不同名称的 drop-in 会合并，原有的 `After=network-online.target` 仍在。
升级 systemd 后须比较副本与发行版新版本；`tools/verify-image.sh` 核对除注释和特定 After 外的配置一致性。

NetworkManager 的提前就绪仅改变 systemd 的 started 时机；Wi-Fi 关联、DHCP 仍由 NM 完成。
新增联网服务须明确其自身的等待与故障处理，不能把连接激活等待重新放回本地显示链。

## 协调器的独立约束

`data-collection-boot.service` 会启动、停止业务服务并做健康检查。
`collect_system`、`device_reporter` 不能反向 `After/Requires` 协调器，否则协调器等业务起来，
业务又等协调器完成，形成等待环。

协调器为 oneshot，`TimeoutStartSec=infinity` 保护正在进行的版本切换，
网络、下载、staging 阶段各自有预算；不能用全局启动超时在切换中途 SIGKILL。
只有 reporter 与更新器需要有界等网，collect_system 的本地 DDS 直接启动。

## 核对方法

```sh
systemctl show rc-local systemd-user-sessions oem-host collect_system -p After -p Wants
systemctl show NetworkManager -p Type
systemctl cat device_reporter data-collection-boot
systemd-analyze critical-chain lsp-kiosk.service
```

检查前两条本地兼容单元没有网络排序，oem-host/collect_system 没有网络目标；
NM 为 simple，两个网络业务的前置等待带 15 秒预算和忽略失败前缀。
critical-chain 用于检查依赖关系，不代表屏幕首帧时间。
`NetworkManager-wait-online` 可能由其它服务请求，不能把它 inactive 或成功当作本地显示的验收条件。

新增 overlay 文件还须确认构建脚本实际安装了它；源码存在不等于镜像已集成。
运行板上的应用方式见 [overlay-apply.md](../../build/overlay-apply.md)，启动测量见 [boot-time.md](../boot-time.md)。
