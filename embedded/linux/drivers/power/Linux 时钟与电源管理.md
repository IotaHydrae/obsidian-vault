# Linux 时钟与电源管理

> 时钟、供电、复位和设备活动状态需要一起管理；只有寄存器地址正确并不能证明硬件可访问。

## TL;DR

- 在硬件 I/O 和异步访问期间保持 Runtime PM 活动引用，并按使用契约配对释放。
- `clk_prepare_enable()` 可能睡眠；时钟、电源和复位顺序由设备硬件决定。
- 排查休眠唤醒时分别验证设备电源、引脚、中断路径和恢复回调。

适用范围：Linux 6.18 LTS。

## 框架分工

| 框架 | 管理什么 | 驱动侧关注 |
| --- | --- | --- |
| [[Linux CCF 时钟框架]] | 时钟树、父时钟、速率和启停 | 不把共享的父时钟当作本设备独占资源 |
| [[Linux Regulator 与电源域]] | 电源轨、复位控制、供电依赖 | 按硬件顺序使能、释放复位；失败时撤销已完成的动作 |
| [[Kernel API - pm_runtime]] | 单个设备在运行期间的活动与休眠 | 持有活动引用直到实际硬件访问结束 |
| [[Linux 系统休眠与中断唤醒]] | 整机休眠、恢复及唤醒来源 | 运行时有 IRQ 不表示可以唤醒整机；恢复后检查寄存器与未完成 I/O |
| [[Linux CPUFreq OPP Thermal 与 CPUIdle]] | 频率、允许工作点、温度约束及空闲状态 | 区分任务未运行、频率受限和设备 I/O 等待 |

不要从硬中断上下文调用 `clk_prepare_enable()`。具体时钟能否改速率、共享父时钟或自动门控由平台与 provider 实现决定。

System suspend 与 runtime suspend 是不同层次：设备可能平时已 runtime suspend，系统仍需处理整机睡眠和唤醒。设置 wakeup 标志也不能代替确认电源域、引脚状态、中断路径和硬件能力。

## 设备访问期间的资源关系

```text
获取设备活动引用 → runtime resume / 设备可访问
→ 配置和启动 I/O → 等待或同步完成 → 释放活动引用
→ 满足条件时 runtime suspend
```

一次 I/O 若由 DMA、IRQ 线程或 workqueue 异步完成，活动引用需要覆盖**实际硬件使用期**，不能在提交请求后立即释放。驱动还需要按硬件要求协调 regulator、时钟、复位和寄存器访问；不同设备的使能顺序并不相同。

从驱动的两条逆向路径验证引用是否配对：probe 失败时只回滚已成功的步骤；remove 停止新请求并同步在途访问后，才关闭相应的时钟和供电。CCF 的对象、prepare / enable 锁及速率检查见上表专题。

## 常见故障判据

| 现象 | 优先验证 |
| --- | --- |
| probe 读取寄存器超时 | 对应电源、时钟、复位和寄存器访问宽度是否满足设备手册 |
| 空闲一段时间后首次 I/O 失败 | runtime resume 结果、引用配对和 autosuspend 竞态 |
| 休眠后无法唤醒 | 唤醒源权限、IRQ 唤醒配置、供电域和引脚 sleep 状态 |
| 恢复后寄存器内容不对 | 硬件是否丢失上下文，驱动是否按需要恢复状态和缓存 |
| 持续负载时性能下降 | 频率、温度、thermal 冷却状态、功耗限制和实际 CPU 运行时间 |

`/sys/devices/.../power/` 可用于观察部分设备的 runtime PM 状态；时钟与电源框架的 debugfs 信息依配置和权限而定。这些信息是软件状态快照，不能单独证明电源轨或时钟波形正确。

遇到资源获取失败或 `-EPROBE_DEFER`，按 [[Linux 设备树 Binding 与依赖排查]] 核对实际 DTB、供应者与引脚配置。

## 参考

- 蜗窝科技：[Common Clock Framework](https://www.wowotech.net/pm_subsystem/ccf-arch.html)、[系统休眠与中断](https://www.wowotech.net/pm_subsystem/suspend-irq.html)、[thermal 概述](https://www.wowotech.net/pm_subsystem/519.html)。
- Linux 6.18.62：[时钟框架](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/clk.rst)、[Runtime PM](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/power/runtime_pm.rst)、[系统睡眠](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/pm/devices.rst)。
