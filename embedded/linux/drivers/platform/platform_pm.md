# Linux Platform 驱动的电源管理回调

> Platform 驱动通过 `dev_pm_ops` 参与设备运行时电源管理和系统休眠；回调必须与设备 I/O、IRQ 和依赖资源的生命周期配合。

## TL;DR

- Runtime PM 面向单个设备的运行期状态；system sleep 面向整机睡眠过程，两者不能简单视为同一个状态机。
- 访问硬件前取得设备活动引用，异步 DMA / IRQ / work 使用设备时引用需覆盖实际使用期。
- system suspend 要先阻止新 I/O、处理正在进行的操作并按设备要求配置唤醒；resume 后恢复硬件状态。
- `noirq` 阶段涉及设备 IRQ 的处理顺序，不表示回调天然运行在硬中断上下文。

范围：Linux 6.18 LTS，具体电源、时钟和复位时序以硬件为准。

## 回调与责任

| 回调族 | 用途 | 驱动侧问题 |
| --- | --- | --- |
| `runtime_suspend` / `runtime_resume` | 运行期降低功耗和恢复 | I/O 与 usage count 的配对、硬件访问是否已经结束 |
| `suspend` / `resume` | 系统睡眠前停止设备、恢复后重启 | 用户请求、DMA、IRQ 和状态保存 |
| `suspend_late` / `resume_early` | 常规阶段之后 / 之前的进一步处理 | 父子设备、供应者及总线的阶段约束 |
| `suspend_noirq` / `resume_noirq` | 设备 IRQ 相关的后续 / 前置阶段 | 此阶段不能依赖已经被暂停的普通设备 IRQ 完成操作 |
| `freeze` / `thaw`、`poweroff` / `restore` | hibernation 等路径的设备状态 | 不应把所有状态迁移机械等同于 suspend-to-RAM |

设备 PM core、PM domain、bus、type、class 和 driver 的协作由核心代码决定；具体回调是否执行、由哪一层调用以及顺序取决于设备配置。查看当前设备的实际绑定，不假定 `dev_pm_ops` 的每个函数都会无条件运行。

## 设备访问与退出

常见访问模式是先检查 `pm_runtime_resume_and_get()` 的结果，设备可用后启动 I/O；完成后根据设备策略调用相应 `pm_runtime_put*()`。失败、超时和取消也要按实际获得的引用配对。运行期 PM 的接口细节见 [[Kernel API - pm_runtime]]。

进入系统睡眠前，驱动需要按硬件协议停止或冻结新请求，并协调现有 DMA、工作项和 IRQ。恢复后先确保供电、时钟、复位和寄存器状态正确，再恢复请求；能否保持寄存器由硬件及电源域决定。

## 排查

1. 从日志和 PM tracepoint 确定停在 prepare、suspend、noirq、resume 还是 runtime 阶段。
2. 检查设备活动引用、异步任务、锁等待和设备依赖；不要在持有回调需要的锁时等待回调结束。
3. 无法唤醒时分开检查设备唤醒配置、IRQ wake、pinctrl sleep、供电域和物理信号。
4. 恢复失败时检查硬件是否掉电、状态是否需要重建，以及相关供应者是否已恢复。

系统层流程和唤醒链路见 [[Linux 系统休眠与中断唤醒]]；时钟和电源框架见 [[Linux 时钟与电源管理]]。

## 参考

- Linux 6.18.62：[设备电源管理](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/pm/devices.rst)、[Runtime PM](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/power/runtime_pm.rst)、[Suspend 与中断](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/power/suspend-and-interrupts.rst)。
- 蜗窝科技：[设备驱动电源管理](https://www.wowotech.net/pm_subsystem/device_driver_pm.html)、[PM 接口](https://www.wowotech.net/pm_subsystem/pm_interface.html)。
