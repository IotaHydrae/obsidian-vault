# CM5 IO Board 超频：短测、错误判据与恢复边界

## TL;DR

- 该台 4GB CM5 的 2.8 GHz、不额外加压完成了计算与内存分阶段短测；用户选定此档后已写正常启动配置。
- 2.875 GHz 的更长混合校验失败，改善风扇后仍有内存校验错误；增加 25 mV 又触发欠压/节流历史位。3.0 GHz 各次尝试均未完成验收。
- 设备已断开，2.8 GHz 最终普通启动复核和临时看门狗清理尚未完成；不能将配置写入成功表述为最终启动通过。
- 仓库通用镜像保持默认 2.4 GHz；单板短测不代表长期稳定、全内存覆盖或其他 CM5 的余量。

## 适用对象与已保存配置

2026-10-28，官方 CM5 IO Board、4GB RAM / 32GB eMMC 的单台 CM5；
Ubuntu Base 24.04.5，无桌面，内核 `6.18.57-v8-16k-uconsole`，16 KiB 页。
散热是接板载风扇座的第三方 Pi 5 样式主动散热器；型号和电源输出规格未知。
固件/EEPROM 自报日期均为 2025-05-08，版本前缀 `69471177`。

用户要求停止继续探高频并使用 2.8 GHz。正常 `config.txt` 已在保留板型与
[设备专属风扇曲线](cm5-io-board-fan-control.md)的基础上保存以下 CPU 段：

```ini
[cm5]
arm_freq=2800
over_voltage_delta=0
force_turbo=0
initial_turbo=0
[all]
```

写入后 fsync、读回 SHA-256 一致并触发了正常重启。最终配置摘要为
`e3ebddbfe8ffce031117ecb385cfd4100d7f979ee8108ae44cbd36356e9b58c2`。
正常 `cmdline.txt` 原字节保留；本轮 `tryboot.txt`、`cmdline-oc.txt` 已删除。
这是实机覆盖，不属于导出镜像或仓库的默认超频设置。

## 已完成的短测与未通过档位

分阶段短测依次执行：四核 int64 校验 20 秒、四核 CPU `all --verify` 90 秒、
请求 1 GiB 的单 VM worker `all --verify` 45 秒，以及锁定 256 MiB 的 memtester 一轮。
另设 180 秒四核 CPU + 单 VM 混合校验；分阶段通过不代替混合结果。

| 条件 | 结果与观测 |
| --- | --- |
| 2.4 GHz，原风扇曲线 | 分阶段通过；int64 7117.09 bogo ops/s，最高 61.15°C，采样保护位全 0 |
| 2.8 GHz，额外电压 0，原风扇曲线 | 分阶段通过；int64 8296.24 bogo ops/s，最高 68.85°C，采样保护位全 0；未做该档混合验收 |
| 2.875 GHz，额外电压 0，原风扇曲线 | 分阶段曾通过，180 秒混合却报告 11 个内存位错误，最高 71.05°C；拒绝持久化 |
| 2.4 GHz，新风扇曲线 | 180 秒混合通过，最高 55.1°C，采样保护位全 0 |
| 2.875 GHz，额外电压 0，新风扇曲线 | CPU 阶段通过，VM `swap bytes` 报 1 个位错误、退出 2；最高 60.05°C，未进入 memtester |
| 2.875 GHz，+25 mV，新风扇曲线 | 约 22 秒出现 `get_throttled=0x50000`，自动停止；最高 52.35°C |
| 3.0 GHz，额外电压 0 | `vcgencmd measure_clock arm` SIGSEGV，内核调用栈包含 `get_vfs_caps_from_disk`，短测失败 |
| 3.0 GHz，+25 / +50 mV | 网络失联，没有完整通过回执；+50 mV 仅持久化到约 21 秒的 int64 尾采样 |

int64 的一次观测增幅约 16.6%，只对应该方法；不是整机性能承诺。
`0x50000` 表示该次启动期间曾出现欠压和节流，不能据采样时刻的 5V 电压反推
瞬态最低供电，也不能把电源线、供电或芯片余量中的某一项断言为唯一原因。

**推断：** 降温后 2.875 GHz 仍有错误，说明仅改善散热不足以满足本次正确性判据。
这不是物理 RAM 已损坏的证据，也没有隔离到特定 CPU、互连或内存硬件路径。

## 判据与频率报告

- 正确性以退出 0、无 `fail:` / `FAILURE`、预定阶段完成为准；最终 `passed=true` 且 `cleanup_errors=[]` 才计通过。
- memtester 需实际分配并锁定 256 MiB、完成 `Loop 1/1` 和 `Done.`；它不覆盖全部 4GB。
- 每阶段核对内核新增故障、保护位和尾采样；78°C 是主动停止策略，不是芯片规格上限。
- CPU 单独与混合阶段至少 95% 的负载采样须在目标硬件频率 ±0.1% 内；温度低或没有欠压不能代替校验。

固定驱动 raspberrypi-cpufreq.c（工程源码：`upstream/linux/drivers/cpufreq/raspberrypi-cpufreq.c`）
以 `RASPBERRYPI_FREQ_INTERVAL=100000000` 向上取整 OPP 上限。
因此配置 2875 MHz 时 cpufreq 可报告 `2900000` kHz；固件硬件测频约 2.875 GHz，
不能据此声称测试了 2.9 GHz。频率验收同时核对固件设置和 `measure_clock arm`。

Pi 5/CM5 使用 `over_voltage_delta`，单位 µV；这里的 25000 表示额外 25 mV。
未沿用 [CM4 的 OV6](cm4-overclock-testing.md)，没有提高 `temp_limit` 或启用 `force_turbo=1`。

## 恢复与未完成收尾

实验使用完整正常配置复制成一次性 `tryboot.txt`，另加 powersave 启动命令行；
进入用户态、确认 PID1 看门狗 active 后才提高频率并加负载。
默认频率下曾实际验证看门狗复位后回到 tryboot=0 与 2.4 GHz，但 3.0 GHz 挂起
没有自动恢复联网，最终仍需完全断电：电源与连接电脑的 USB 都拔掉、灯全灭后再上电。
这条恢复路径不能保证启动各阶段或所有超频故障都自动恢复。

**设备已断开，待下次连接：** 核对正常启动的配置摘要、tryboot=0、固件 2800 MHz、
`ondemand`、新风扇运行树和保护位，并完成有限的重启后校验；2.8 GHz 新曲线下的
180 秒混合验收没有执行，不计通过。临时
`/etc/systemd/system.conf.d/90-cm5io-oc-watchdog.conf`
仍需在复核后删除并执行 `systemctl daemon-reexec`，确认 RuntimeWatchdogUSec=0、
watchdog inactive。默认 RebootWatchdog 不要求为 0。

原始正常配置、风扇基线、候选配置及测试目录保留在设备
`/var/lib/cm5io-overclock/20261028/` 和 `20261028-fan/`；
本机已获取的日志与选择回执保留在忽略的 `image/work/cm5-io-board-20261027/`。
设备断开前未完成两目录的完整归档，不声称全部原始日志已镜像到本机。

关键本机证据：`oc-fan-stock-before-select.log`、`oc-2875-mixed-failure-detail.log`、
`oc-fan-2875-failure.log`、`oc-fan-2875v25-failure.log`、`oc-fan-selection.log`。
配置写入与测试结果分别记录；长期运行、重复冷启动和不同供电条件均未知。
