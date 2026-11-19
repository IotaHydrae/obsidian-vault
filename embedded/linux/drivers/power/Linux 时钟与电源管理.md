# Linux 时钟与电源管理

> 时钟、供电、复位和设备活动状态需要一起管理；只有寄存器地址正确并不能证明硬件可访问。

## 框架分工

| 框架 | 管理什么 | 驱动侧关注 |
| --- | --- | --- |
| Common Clock Framework | 时钟树、父时钟、速率和启停 | 获取正确的 consumer clock；按契约配对 prepare / enable 与 disable / unprepare |
| regulator / reset | 电源轨和复位控制 | 依硬件顺序使能、释放复位；失败时回退已启用资源 |
| Runtime PM | 单个设备在运行期间的活动与休眠 | I/O 及异步访问期间持有正确的活动引用，结束后配对释放 |
| System sleep / wakeup | 整机睡眠、恢复及唤醒来源 | 按设备和中断控制器的唤醒能力配置，并验证恢复顺序 |
| cpufreq / thermal | CPU 频率策略与温度约束 | 负载、策略、散热和频率限制共同影响性能 |

`clk_prepare_enable()` 组合了准备和使能步骤，可能睡眠；不要从硬中断上下文调用。具体时钟能否改速率、共享父时钟或自动门控由平台与 provider 实现决定。

System suspend 与 runtime suspend 是不同层次：设备可能平时已 runtime suspend，系统仍需处理整机睡眠和唤醒。设置 wakeup 标志也不能代替确认电源域、引脚状态、中断路径和硬件能力。

## 排查顺序

1. 查设备当前 runtime PM 状态、调用配对及驱动日志；访问前确认设备确实已恢复。
2. 核对设备树中的时钟、供电、复位和 pinctrl 依赖；检查资源缺失或 `-EPROBE_DEFER`。
3. 对休眠唤醒问题，分别验证唤醒源、IRQ 配置、设备电源、恢复回调和主机可观察到的事件。
4. 对降频或卡顿，结合 cpufreq 策略、温度、thermal 限制和工作负载，避免只看当前频率一次采样。

Runtime PM 的具体引用与错误处理见 [[Kernel API - pm_runtime]]；设备树资源见 [[platform|设备树与 Platform 设备]]。

## 参考

- 蜗窝科技：[Common Clock Framework](https://www.wowotech.net/pm_subsystem/ccf-arch.html)、[系统休眠与中断](https://www.wowotech.net/pm_subsystem/suspend-irq.html)、[thermal 概述](https://www.wowotech.net/pm_subsystem/519.html)。
- Linux 6.18.62：[时钟框架](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/clk.rst)、[Runtime PM](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/power/runtime_pm.rst)、[系统睡眠](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/pm/devices.rst)。
