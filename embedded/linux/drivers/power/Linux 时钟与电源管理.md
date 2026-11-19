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
| Common Clock Framework | 时钟树、父时钟、速率和启停 | 获取正确的 consumer clock；按契约配对 prepare / enable 与 disable / unprepare |
| regulator / reset | 电源轨和复位控制 | 依硬件顺序使能、释放复位；失败时回退已启用资源 |
| Runtime PM | 单个设备在运行期间的活动与休眠 | I/O 及异步访问期间持有正确的活动引用，结束后配对释放 |
| System sleep / wakeup | 整机睡眠、恢复及唤醒来源 | 按设备和中断控制器的唤醒能力配置，并验证恢复顺序 |
| cpufreq / thermal | CPU 频率策略与温度约束 | 负载、策略、散热和频率限制共同影响性能 |

不要从硬中断上下文调用 `clk_prepare_enable()`。具体时钟能否改速率、共享父时钟或自动门控由平台与 provider 实现决定。

System suspend 与 runtime suspend 是不同层次：设备可能平时已 runtime suspend，系统仍需处理整机睡眠和唤醒。设置 wakeup 标志也不能代替确认电源域、引脚状态、中断路径和硬件能力。

## 设备访问期间的资源关系

```text
获取设备活动引用 → runtime resume / 设备可访问
→ 配置和启动 I/O → 等待或同步完成 → 释放活动引用
→ 满足条件时 runtime suspend
```

一次 I/O 若由 DMA、IRQ 线程或 workqueue 异步完成，活动引用需要覆盖**实际硬件使用期**，不能在提交请求后立即释放。驱动还需要按硬件要求协调 regulator、时钟、复位和寄存器访问；不同设备的使能顺序并不相同。

时钟树可能有多个 consumer。驱动应通过 CCF 取得和释放自己的引用，不假设独占父时钟；`clk_set_rate()` 改变共享时钟时还要考虑其他设备和 provider 的限制。查找、prepare、enable 与设备生命周期的配对应在 probe 失败和 remove 路径中保持一致。

### Common Clock Framework 的两端

| 层次 | 接口 / 对象 | 主要职责 |
| --- | --- | --- |
| Consumer | `struct clk`、`clk_get()`、`clk_prepare_enable()` | 获取设备所需时钟并按生命周期使用 |
| 框架核心 | 时钟拓扑、父时钟、引用与速率 | 协调共享时钟的状态和操作 |
| Provider | `struct clk_hw`、`struct clk_ops` | 实现硬件相关的门控、父时钟或速率操作 |

CCF 将 prepare 与 enable 分为不同的操作组：prepare 组使用可睡眠锁，enable 组使用自旋锁。消费方通常用 `clk_prepare_enable()` 配对 `clk_disable_unprepare()`；但函数是否适用于当前上下文还取决于整个调用路径，不能只观察某一次硬件寄存器写入很快就假设不会睡眠。

`clk_get_rate()` 提供框架报告的速率；它不能单独证明引脚上存在有效波形或外设已经解除复位。查看时钟树、父时钟和计数时，还要结合电源与设备运行状态。

## 系统休眠与唤醒

system sleep 期间，驱动参与设备挂起和恢复；runtime PM 管理运行时设备状态，二者有交互但作用范围不同。一个可唤醒设备还依赖可用的电源域、pinctrl sleep 状态、真实 IRQ 路径和唤醒控制配置。

IRQ 作为系统唤醒源与普通运行时处理中断不是同一件事：即使驱动正常收到中断，也要单独验证系统休眠期间的唤醒能力与配置。恢复后还应确认设备状态、挂起前未完成的请求以及 IRQ 是否能够继续工作。

## 性能和温度

cpufreq 负责 CPU 频率策略，OPP 描述可用工作点，thermal 框架可依据温度和策略限制性能；cpuidle 处理空闲状态选择。它们共同影响功耗和延迟，但不能用某一次频率读数解释整个工作负载。

对于间歇性性能下降，记录负载、频率、温度和限制状态的时间序列；同时检查供电与散热，区分任务未运行、频率受限和设备 I/O 等待。

## 常见故障判据

| 现象 | 优先验证 |
| --- | --- |
| probe 读取寄存器超时 | 对应电源、时钟、复位和寄存器访问宽度是否满足设备手册 |
| 空闲一段时间后首次 I/O 失败 | runtime resume 结果、引用配对和 autosuspend 竞态 |
| 休眠后无法唤醒 | 唤醒源权限、IRQ 唤醒配置、供电域和引脚 sleep 状态 |
| 恢复后寄存器内容不对 | 硬件是否丢失上下文，驱动是否按需要恢复状态和缓存 |
| 持续负载时性能下降 | 频率、温度、thermal 冷却状态、功耗限制和实际 CPU 运行时间 |

`/sys/devices/.../power/` 可用于观察部分设备的 runtime PM 状态；时钟与电源框架的 debugfs 信息依配置和权限而定。这些信息是软件状态快照，不能单独证明电源轨或时钟波形正确。

## 排查顺序

1. 查设备当前 runtime PM 状态、调用配对及驱动日志；访问前确认设备确实已恢复。
2. 核对设备树中的时钟、供电、复位和 pinctrl 依赖；检查资源缺失或 `-EPROBE_DEFER`。
3. 对休眠唤醒问题，分别验证唤醒源、IRQ 配置、设备电源、恢复回调和主机可观察到的事件。
4. 对降频或卡顿，结合 cpufreq 策略、温度、thermal 限制和工作负载，避免只看当前频率一次采样。

Runtime PM 的具体引用与错误处理见 [[Kernel API - pm_runtime]]；设备树资源见 [[platform|设备树与 Platform 设备]]。

## 参考

- 蜗窝科技：[Common Clock Framework](https://www.wowotech.net/pm_subsystem/ccf-arch.html)、[系统休眠与中断](https://www.wowotech.net/pm_subsystem/suspend-irq.html)、[thermal 概述](https://www.wowotech.net/pm_subsystem/519.html)。
- Linux 6.18.62：[时钟框架](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/clk.rst)、[Runtime PM](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/power/runtime_pm.rst)、[系统睡眠](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/pm/devices.rst)。
