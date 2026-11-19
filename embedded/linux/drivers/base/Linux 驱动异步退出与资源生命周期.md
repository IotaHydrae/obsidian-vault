# Linux 驱动异步退出与资源生命周期

> 安全退出要求先阻止新的生产者，再停止硬件并同步所有异步访问，最后释放它们可能引用的对象；`devm_*` 不会自动完成设备停机。

## TL;DR

- 先画出 IRQ、timer、work、DMA、用户请求和 PM 回调之间的生产关系，确认谁能重新排队谁。
- `cancel_work_sync()`、`timer_shutdown_sync()`、`synchronize_irq()`、DMA terminate 解决不同路径，不能互相替代。
- 共享 IRQ 不应为了停一个设备而无条件禁用整条线路；按设备协议先屏蔽自己的中断源。
- 不能持有回调需要的锁并等待它结束；超时或取消请求不自动证明硬件和回调已经停止。

范围：Linux 6.18 LTS 的常见驱动退出和 probe 失败路径；具体顺序由硬件及依赖图决定。

## 先识别生产者和消费者

```text
用户请求 ─┐
设备 IRQ ─┼→ 发布数据 / 排队 work → DMA / 寄存器访问 → 完成回调
timer ────┘             ↑                           │
                         └──── 某些回调可重新启动 ───┘
```

退出的目标不是只让某个函数返回，而是证明**没有任何路径还会访问设备或私有数据**。生产者可能是另一个 CPU 的 IRQ、回调、用户入口或尚未取消的 timer。

## 典型退出阶段

1. **停止入口**：将设备标记为停止，阻止新的用户请求和外部生产者；同步共享状态和对象引用。
2. **控制事件源**：按设备协议屏蔽自身中断源、停止周期采样或禁止 timer 重启；共享 IRQ 的其他设备不能被无故关闭。
3. **同步在途工作**：等待 IRQ handler / IRQ 线程、work、delayed work、timer、DMA 和其他回调分别结束。
4. **关闭资源**：在所有访问停止后关闭 DMA、时钟、电源和映射，释放 buffer、队列及私有对象；具体资源顺序按硬件要求决定。

这不是可直接粘贴到所有驱动的固定函数序列：某些硬件必须先结束 DMA 才能停止 IRQ，另一些设备需要 IRQ 才能报告停止完成。明确依赖和失败回退后再决定次序。

## 同步接口各自保证什么

| 接口 | 等待 / 阻止什么 | 不自动保证什么 |
| --- | --- | --- |
| `synchronize_irq()` | 等待指定 IRQ 的在途处理完成，包括适用的线程处理 | 设备不会再次产生中断；不能持有 handler 需要的锁等待 |
| `free_irq()` | 移除指定 action 并同步其执行 | 替驱动安全关闭外设中断源或停止 DMA |
| `cancel_work_sync()` | 取消 / 等待该 work 的运行 | 其他生产者以后不会重新提交 |
| `cancel_delayed_work_sync()` | 取消 / 等待 delayed work | 外部仍在使用其容器是安全的 |
| `timer_shutdown_sync()` | 停止、等待 timer 并阻止后续重新 arm | work、IRQ 和 DMA 一并停止 |
| `dmaengine_terminate_sync()` | 按通道接口终止并同步 DMA / 回调 | 替代设备侧停机、缓冲区映射释放和错误处理 |

`disable_irq()` 可能等待并睡眠，不能持有 handler 需要的锁调用；`disable_irq_nosync()` 只发起禁用，不等待当前处理完成。共享线路应区分设备侧 mask 与对整个 Linux IRQ 的禁用。

## 常见竞态

- **IRQ 在 cancel 后重新排队 work**：先关闭设备侧事件源，再撤销或阻断该 action 的提交路径，最后取消 work。
- **timer 与 work 互相重启**：阻止外部入口，用适当的 timer shutdown 和 work 同步拆除循环。
- **DMA 超时后立刻 free**：超时不是终止成功；确认硬件和回调不会再访问 buffer 后再 unmap/free。
- **`devm_kfree` / 自动回收被当作停机**：受管释放按设备资源生命周期执行，但不会自动理解硬件和异步依赖。

具体 API 契约见 [[Kernel API - workqueue]]、[[Kernel API - timer_setup]]、[[Linux DMAEngine 数据传输]] 和 [[threaded-irq]]。

### 共享 IRQ、timer 和 work 的退出例子

假设共享 IRQ 和 timer 都能排队 work，work 也能重新 arm timer；没有必须依赖 IRQ 才能完成的停机握手。相应的 `remove()` 顺序是：

1. 在提交者使用的锁或等价串行化机制下设置 `stopping`，让“检查仍可提交”和“实际排队”没有撤销间隙；退出这把锁后再等待回调。正在运行的 work 不应在停机后恢复设备中断。
2. 在设备仍可访问时屏蔽并按器件要求清除自己的中断源。用 `free_irq(irq, dev_id)` 撤销本设备的 action；若本来由 `devm_request_threaded_irq()` 注册且需要在 `remove()` 中提前撤销，改用 `devm_free_irq()`，不要混用两套释放接口。
3. `timer_shutdown_sync()` 阻止 work 将 timer 重新启动，然后 `cancel_work_sync()` 等 work 完成。`synchronize_irq()` 本身不会注销 action：只要还挂在共享线上，其他设备的中断仍可能进入本设备的 handler。
4. 停用时钟、供电和映射，释放对象。已打开的用户接口若仍持有引用，应在解绑后拒绝新的 I/O，按该接口的生命周期等引用释放；注销节点不等于关闭现有 FD。

如果停机必须等待设备 IRQ 或 DMA 完成，保留所需的回调到握手结束，再撤销；不要机械照搬屏蔽顺序。

## 验证

重复测试设备绑定、I/O、失败回退、解绑与重新绑定；使用可用的 KASAN / KCSAN、lockdep 和 tracepoint 检查 use-after-free、竞态和等待依赖。硬件验证需覆盖真实中断、总线错误和断开，编译通过不能证明退出顺序正确。

## 参考

- Linux 6.18.62：[`kernel/irq/manage.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/irq/manage.c)、[workqueue](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/workqueue.rst)、[DMAEngine client](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/dmaengine/client.rst)、[devres](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/devres.rst)。
- 蜗窝科技：[workqueue 概念](https://www.wowotech.net/irq_subsystem/workqueue.html)、[设备资源管理](https://www.wowotech.net/device_model/device_resource_management.html)。
