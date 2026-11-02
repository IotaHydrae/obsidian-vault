# 系统时间：离线恢复、联网校准与时间存档

> 开机依次使用 RTC、fake-hwclock 存档，联网后由 chrony 校准；校准后的首次存档在后台完成，不增加开机等待。

## TL;DR

- RTC 由内核读取；无电池且掉电后，hym8563 驱动会初始化为 **2021-01-01 12:00 UTC**。
- fake-hwclock 恢复时间下限，不能补算断电时长；保持默认 `FAKE_HWCLOCK=Y`，兼容有无 RTC 电池。
- chrony 使用国内 NTP，`makestep 1 -1` 允许运行期间跳时，`rtcsync` 维护 RTC。
- `lsp-time-save.service` 在后台等待首次同步收敛，立即保存一次；原有每小时和正常关机保存继续生效。
- `sudo lsp-time-sync status` 查看各时间来源；`status --json` 提供结构化观测。未联网不是诊断失败。

## 当前配置与安装

| 对象 | 镜像内路径 / 行为 |
| --- | --- |
| chrony 主配置 | `/etc/chrony/chrony.conf`，0644 root:root；国内源、`iburst`、`makestep 1 -1`、`rtcsync` |
| 现场配置入口 | `/etc/chrony/conf.d/*.conf`、`/etc/chrony/sources.d/*.sources`；目录 0755 root:root |
| 时间工具 | `/usr/local/sbin/lsp-time-sync`，0755 root:root；只读诊断或写时间存档，不设置系统时钟 / RTC |
| 首次同步保存 | `/usr/lib/systemd/system/lsp-time-save.service`，0644 root:root；`Type=simple`，不排序到 `time-sync.target` |
| 存档策略 | `/etc/default/lsp-time-sync`，0644 root:root；构建按 `FAKE_HWCLOCK=Y/N` 生成，N 时不启用新保存服务 |
| 出厂基准记录 | `/etc/lsp-time-seed.json`，0644 root:root；记录封装基准 UTC 与来源 `chrony` / `explicit` |
| 本次启动保存记录 | `/run/lsp-time-sync/saved.json`，0644 root:root；记录 boot ID、UTC、NTP 源及剩余校正量 |

构建在 `rootfs` 阶段安装文件并启用保存服务，在 `image` 阶段封装时刷新出厂基准。
`rtc-hym8563.service`、`chrony-wait.service` 保持不启用；系统校时只由 chrony 负责。
时区为 `Asia/Shanghai`，RTC 与存档按 UTC 解释。

## 开机与联网时发生什么

1. 内核 `rtc_hctosys()` 从 RTC 设置系统时间。hym8563 的 VL 置位或日期无效时，驱动先写默认日期。
2. `fake-hwclock-load` 只在系统时间不晚于存档时恢复，通常不会把正常走时的 RTC 往回调。
3. chrony 后台取样。误差大于 1 秒可直接跳时，小误差渐进调整；`-1` 表示不限时钟更新次数。
4. 保存服务每 10 秒查询一次 chrony；已同步且剩余校正量绝对值不超过 **0.1 秒**后，
   原子替换 `/etc/fake-hwclock.data`，记录本次启动已保存，随后结束等待。

0.1 秒沿用[服务有界等待示例](services-wait-for-clock.md)的剩余校正量门限，用于避免保存尚在大幅调整的系统时间，
不是绝对 UTC 精度保证。服务用 `Type=simple` 提前完成 systemd 启动握手，无网络时只在后台等待；
不让本地显示和采集等待它。每次启动只额外保存一次，原包的 hourly timer 与关机保存不变。

`rtcsync` 启用内核 RTC 回写机制，进入同步状态后可安排首次回写，之后约每 11 分钟维护；
不能假定首次必须等满 11 分钟，也不再添加一个定时 `hwclock --systohc`。

有电池并不意味着 fake-hwclock 永远没有影响：错误的未来存档仍可能把有效 RTC 推向未来。
诊断会报告存档领先系统 / RTC 的情况，供核对来源；不以任意年份阈值自动丢弃存档。
首次 NTP 校准后的保存会用当前已校准时间替换这类旧存档。

## 配置现场内网 NTP

在 `/etc/chrony/sources.d/site.sources` 配置具备可靠时间源的内网服务器，例如：

```conf
# 示例地址，部署时替换为现场 NTP 服务器 IP。
server 192.0.2.123 iburst
```

修改 `.sources` 后执行 `sudo chronyc reload sources`，再用 `chronyc -n sources` 核对。
固定 IP 可避免内网 DNS 尚未就绪时的域名解析依赖；保留公网源作为其它可用来源。
修改主配置或 `conf.d/*.conf` 时需重新启动 chrony。

默认不自动接纳 DHCP 下发的 NTP。如现场明确由 DHCP 管理时间源，可在
`/etc/chrony/conf.d/site-dhcp.conf` 加入 `sourcedir /run/chrony-dhcp`，再重新启动 chrony。
Ubuntu chrony 包已提供 NetworkManager 的 DHCP 更新与 online/offline 钩子；
需验证当前包钩子存在及实际生效，不再叠加常驻联网检测或定时重启。

## 镜像封装时刷新出厂基准

`image` 阶段在复制 rootfs 前执行 `lsp-time-sync seed --root <rootfs>`；
旧 tar 回退路径则在解包后的目标根分区执行，均在发布镜像前完成。
工具只写目标 rootfs 的存档与 JSON，不操作宿主时钟；拒绝目标 `/` 和逃逸目标根目录的 `etc` 软链。

默认要求构建宿主 chrony 已同步并满足上述剩余校正量门限，再采纳当前 UTC。
自动查询只使用 chrony；缺少 `chronyc` 或无法读取其状态时，报 `build chrony monitoring unavailable`
并附查询错误，不能据此断言系统时钟未同步。使用其他授时服务的宿主也可在独立确认时间可靠后显式提供基准。
离线构建可显式提供 `LSP_TIME_SEED_UTC`，格式严格为 `YYYY-MM-DDTHH:MM:SSZ`，由操作者确认基准可靠。
缺少可信时间或写入失败时，`image` 阶段失败，不能继续发布镜像；`FAKE_HWCLOCK=N` 时跳过这一步。
复用旧 rootfs 也会刷新基准，不依赖 fake-hwclock 包首次安装时的存档。
已经完成并被缓存跳过的阶段不会因环境变量变化自动重跑，仍须显式选择 `image` 阶段 / 重建。

JSON 的 `schema_version=1`，`utc` 为封装基准，`source` 为 `chrony` 或 `explicit`。
运行后 fake-hwclock 存档会继续更新，不能要求它始终等于出厂 JSON。
通过 overlay 更新运行系统时不补造出厂时间记录；live 检查对旧镜像缺该记录只作警告。

## 如何诊断与验证

```bash
sudo lsp-time-sync status
sudo lsp-time-sync status --json
chronyc tracking
chronyc -n sources
systemctl status lsp-time-save.service
journalctl -b -u lsp-time-save.service --no-pager
```

诊断分别报告系统 UTC、chrony 同步状态 / 来源 / 剩余校正量、RTC 当前值与开机有效性、
时间存档及两份保存记录。RTC 开机有效性来自本次启动内核日志，当前 RTC 值则来自 sysfs；
NTP 回写后的 RTC 当前值不能证明开机时电池有效。缺 journal 权限会报告未知，不猜测结果。
系统时间看起来合理、chrony 为 active、网络已连接，都不能单独证明已经同步。

`tools/verify-image.sh` 增加时间专项配置检查，区分配置缺失与离线未同步。
隔离回归：`python3 tests/time-sync.py`、`python3 tests/time-seed-build.py`；
只使用临时目录及受控观测，不设置宿主时钟，不需要拔 RTC 电池。
真正的电池保持能力仍需[受控断电验证](../../hardware/rtc-hym8563-battery.md)，
隔离回归或手工设置默认日期不能代替电气断电测试。

## 验证范围

2026-10-22 在 LubanCat-5 V2、Rockchip BSP 6.1.172 上定向部署并验证：
工具隔离回归 32 项、构建路径回归 12 项、真实联动检查 19 项均通过。
真实 chronyd 能读取两类现场配置；同次启动重复保存不改存档；
隔离网络的 6 秒观察期间，服务完成启动握手、后台等待且未写存档；
已同步的板上时钟能刷新临时 rootfs 的存档与出厂记录，不改运行系统存档。

重启后保存服务自动完成首次保存，状态为 `enabled / active (exited)`、`Result=success`；
时间专项检查 26 项通过、0 项失败，仅旧镜像缺出厂记录产生 1 项警告。
三个采集服务仍为 disabled / inactive；同日完成保留 RTC 电池、主供电断开至少约 14 分钟的
[冷启动保持验证](../../hardware/rtc-hym8563-battery.md)，首次 RTC 读数有效且随实际经过时间推进。
上述定向部署证据用于说明各机制的验证范围；完整镜像构建通过仍不等于已验证烧录后的冷启动行为。
真实内网 NTP 或 DHCP NTP 授时、长期 RTC 漂移与电池寿命尚未实测。

## 依据与相关文档

- `kernel-6.1/drivers/rtc/rtc-hym8563.c`：默认日期与 VL 处理；`drivers/rtc/class.c`：`rtc_hctosys()`。
- `kernel-6.1/kernel/time/ntp.c`：同步后的 RTC 回写调度。
- Ubuntu 24.04 chrony 4.5 的 `chronyc -c tracking`：14 列 CSV，含同步状态与剩余系统校正量。
- fake-hwclock 包的 load / save 脚本与 systemd timer：只向前恢复、每小时及关机保存；save 本身不判断 NTP 状态。
- [服务等待时钟](services-wait-for-clock.md)、[本地启动与网络等待](boot-network-gating.md)。
