# CM4 超频的受控试验与恢复

> 本板保留 2200 MHz/OV6，结论仅覆盖已执行的有限负载与固件组合。

## TL;DR

- 先建立默认频率基线和 tryboot/看门狗恢复，再逐档提高；不改通用镜像默认上限。
- 正确性同时检查退出码、校验日志和实际内存覆盖，不能只看压力命令成功。
- 固件更新后重跑相同判据；本板短测通过不代表其他 CM4 或长期稳定。

## 方法与范围

本篇归纳同一 CM4 在两套固定固件下的有限负载试验，参数是该设备历史选择。
先证明恢复路径，再按预定计算、内存覆盖和健康判据逐档比较；固件变更后重跑，不能只沿用旧结果。
`initial_turbo=0` 属于本轮历史输入，后续早期启动配置见[labwc 启动等待](../boot/cm4-early-desktop.md)。

## 一次性配置与启动顺序

**实施依据：** `tryboot.txt` 替代整份 `config.txt`，因此先完整复制正常启动配置，
再增加本次 `[cm4]` 覆盖项；只写超频片段会丢失原有板级配置。
一次性试验期间保留正常 `config.txt` 与 `cmdline.txt`，单独使用 `cmdline-oc.txt`。

```ini
[cm4]
arm_freq=2000
arm_freq_min=600
over_voltage=6
force_turbo=0
initial_turbo=0
cmdline=/cmdline-oc.txt
```

试验命令行追加 `cpufreq.default_governor=powersave`。
进入系统后先确认 PID 1 的 `RuntimeWatchdogSec=15s` 已接管、看门狗处于 active，
再将 governor 提高到 `performance` 并启动负载。
这减少了看门狗接管前的高频运行，但不能保证启动早期不会挂起。

本次已经验证以下命令会进入一次性启动，设备树 `/chosen/bootloader/tryboot` 为 1：

```sh
systemctl --reboot-argument='0 tryboot' reboot
```

2.2 GHz 阶段仅把 `arm_freq` 改为 `2200`，继续使用 `over_voltage=6`。
不启用 `force_turbo`，不提高 `temp_limit`。

## 升级前已验证的恢复路径

**设备观察：** 在默认频率下实际打开 `/dev/watchdog0` 并设为 10 秒，随后退出而不发送 magic close 的 `V`。
设备发生复位，下一次启动回到 `tryboot=0`，CPU 范围为 600–1500 MHz，固件标志为 `0x0`。
这证明了本次“运行中看门狗超时 → 复位 → 正常配置”的路径。

**边界：** 升级前的上述固件没有新版 `kernel_watchdog_timeout` 支持。
该轮试验未升级 EEPROM 或固件，也未证明进入 PID 1 以前卡死可以自动恢复。
一次性配置保留了正常启动入口；它本身不构成启动早期挂起时的复位源。

## 通过与停止判据

- **计算正确性：** stress-ng 命令退出码必须为 0，且日志无 `fail:` 校验错误行。
  V0.17.06 的部分 CPU 校验错误只调用 `pr_fail()`；少量错算仍可能伴随成功退出，不能仅看退出码。
- **内存覆盖：** memtester 必须确认实际分配并锁定 256 MiB，完成 `Loop 1/1` 和 `Done.`，且无 `FAILURE`。
  退出码为 0 但分配或覆盖量不足，不能算作完成预定负载。
- **系统健康：** 同步检查内核错误、实际 ARM 频率、温度与固件欠压/节流标志。
  设置频率只表示请求值；本次同时记录固件测得频率。
- **停止策略：** 78 °C 是本次主动停止阈值，低于官方约 80 °C 的节流起点。
  它是实验策略，不是芯片温度上限或新的稳定性指标。

## 升级前三档试验对象

| 项目 | 本次设备观察 |
| --- | --- |
| 核心板 | CM4 Rev 1.1，revision `c03141`，4 GB RAM |
| 系统 | Ubuntu Base 24.04.5，无桌面 |
| 内核 | `6.18.57-v8-uconsole` |
| 存储 | SD 卡根分区 `mmcblk0p2` |
| 默认频率 | ARM 最高 1500 MHz |
| 固件 | 2024-02-29，提交前缀 `f4e2138c` |
| EEPROM | 2023-01-11 |

## 升级前三档结果

| 配置 | CPU 单独阶段 | int64（bogo ops/s） | 最高温度 | int64 对基线 |
| --- | --- | --- | --- | --- |
| 默认 1500 MHz | 60 s | 4058.69 | 42.355 °C | 基线 |
| 2000 MHz，OV6 | 120 s | 5417.89 | 54.043 °C | +33.5% |
| 2200 MHz，OV6 | 120 s | 5966.24 | 61.835 °C | +47.0% |

三档均完成 4 worker int64 20 s、CPU `all --verify`、1 GiB VM `all --verify` 60 s，以及锁定 256 MiB 的 memtester 1 轮。
2.2 GHz 额外同时运行 4 worker CPU 与 1 GiB VM 共 600 s；所有命令退出为 0，日志判据通过，固件标志全为 0，所查日志未匹配指定内核故障模式。
全套时长分别为 372.3、413.5、1016.4 s；int64 指标只比较同一方法，不代表整体系统提速。

2.0 GHz 实测约 2.00048–2.00053 GHz，2.2 GHz 实测约 2.20050–2.20055 GHz。
两档虽同为 OV6，固件报告核心电压分别约 1.000 V、0.990 V；设置值不能代替测量，也不能外推为所有 CM4 的电压规格。

## 升级前常规启动状态

**设备观察：** 用户选择的 2200 MHz / OV6 已持久化并完成常规重启，`tryboot=0`。
CPU 范围为 600–2200 MHz，governor 为 `ondemand`，保留 `force_turbo=0`、`initial_turbo=0`。
正常配置使用 `cmdline=/cmdline.txt`，内核命令行保留 `fbcon=rotate:1`。

重启后 4 核 int64 `--verify` 20 秒的日志和退出码均通过，最高温度 50.634 °C。
实际高档频率约 2.20050–2.20055 GHz，固件标志全为 0，所查日志未匹配指定内核故障模式，failed units 为空。

临时 `tryboot.txt`、`cmdline-oc.txt` 和看门狗配置已移除，PID 1 的 `RuntimeWatchdogUSec=0`。
原默认频率配置备份保留。

## 固件升级后的 2.2 GHz 回归

**设备观察：** VideoCore 已更新为 `a86983925695a7e63166327d7c002d64040ed31d`（2026-09-14，tag `1.20260915`），
EEPROM 为 `default` 2026-09-23、版本前缀 `456428dc`。

复用原 runner，负载期间使用 `performance`，完成四核 int64 20 s、CPU `all --verify` 120 s、1 GiB VM `all --verify` 60 s、
实际分配并锁定 256 MiB 的 memtester 1 轮，以及四核 CPU＋VM 混合负载 600 s。
各命令退出码、校验日志和内存覆盖判据均通过。

| 指标 | 升级后回归观察 |
| --- | --- |
| 全套时长与遥测 | 1014.136 s，982 条 |
| 实际 ARM 频率 | 2.200499968–2.200552704 GHz |
| 固件报告核心电压 | 1.000 V |
| 最高温度 | 62.809 °C |
| 固件保护标志 | 全部采样为 0 |
| int64 | 5968.04 bogo ops/s |

所查日志未匹配 BUG、Oops、panic、OOM 或存储 I/O 错误；启动日志仍有 Wi-Fi `chanspec` 错误 `-52`，不能称为日志完全无错误。
int64 相对旧固件的 5966.24 仅约 +0.03%；单次差异没有提供固件升级带来性能增益的证据。
日常保留 2200 MHz / OV6 / `ondemand`，临时看门狗已移除；最终常规启动状态见[固件更新记录](../boot/cm4-firmware-update.md)及[开发状态](../../project/status.md)。

## 证据与适用边界

升级前三档汇总见本地 `~/.cache/uconsole-oc-20261018/results.json`，该轮常规启动记录为同目录下的
`device-evidence/device/final-state.json`。设备原始记录位于 `/var/lib/uconsole-overclock/20261018/`。
完整本地归档为同缓存目录下的 `device-evidence.tar.gz`。
升级后回归的设备证据位于 `/var/lib/uconsole-firmware/20261018/regression-validation.json` 与同目录的 `regression/`；
完整本地归档为 `~/.cache/uconsole-fw-20261018/device-evidence.tar.gz`。
两轮 memtester 各仅覆盖其锁定的 256 MiB，VM 负载仅请求 1 GiB，不代表完整覆盖 4 GB 物理内存。
短时通过不能排除长期错算、温度变化或不同供电条件下的不稳定，也不外推到其他核心板。
两轮使用不同的固件输入；后续固件或 EEPROM 再次变化时，应重新执行相同判据的测试。

- [树莓派官方超频选项](https://www.raspberrypi.com/documentation/computers/config_txt.html#overclocking-options)；[温度与频率管理源码](https://github.com/raspberrypi/documentation/blob/master/documentation/asciidoc/computers/raspberry-pi/frequency-management.adoc)
- [树莓派官方 tryboot 说明](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#fail-safe-os-updates-tryboot)
- [stress-ng V0.17.06 CPU 校验实现](https://github.com/ColinIanKing/stress-ng/blob/V0.17.06/stress-cpu.c)
- [stress-ng V0.17.06 手册与退出码](https://github.com/ColinIanKing/stress-ng/blob/V0.17.06/stress-ng.1)
