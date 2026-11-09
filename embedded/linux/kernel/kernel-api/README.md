# Linux Kernel API 笔记索引

> 按执行上下文、等待语义和对象生命周期选择接口；文档以 Linux 6.18 LTS / 6.18.62 源码为基准，示例中的设备协议须按实际硬件实现。

## 接口入口

| 需求 | 笔记 | 首要边界 |
| --- | --- | --- |
| 从嵌入成员找回外层对象 | [container_of](Kernel%20API%20-%20container_of.md) | 来源、生命周期和 const；新代码优先保留 const |
| 给对象分配整数编号 | [idr_alloc](Kernel%20API%20-%20idr_alloc.md) | 写者同步和回收；新设计优先评估 XArray/IDA |
| 任务与硬中断保护短共享状态 | [spin_lock_irqsave](Kernel%20API%20-%20spin_lock_irqsave.md) | 本地 IRQ 与跨 CPU 互斥；RT 普通锁语义不同 |
| 表达 helper 必须已持锁 | [lockdep_assert_held](Kernel%20API%20-%20lockdep_assert_held.md) | 调试断言不会代替加锁 |
| 等待一次完成通知 | [completion](Kernel%20API%20-%20init_completion.md) | 超时不是取消，复用必须协调旧活动 |
| 等待可重复检查的条件 | [wait queue](Kernel%20API%20-%20wait_queue.md) | 先发布条件再唤醒，醒来后重新判断 |
| 可睡眠的延时或调度等待 | [schedule_timeout](Kernel%20API%20-%20schedule_timeout.md) | 单位、任务状态和实际调度延迟 |
| 运行专用事件循环 | [kthread_run](Kernel%20API%20-%20kthread_run.md) | 线程须协作停止并保证数据和 task 生命周期 |
| 把工作交给后台执行池 | [workqueue](Kernel%20API%20-%20workqueue.md) | 普通与 BH 上下文不同，停止生产者后再取消 |
| 维护旧式 softirq 下半部 | [tasklet](Kernel%20API%20-%20tasklet.md) | 已弃用、不可睡眠，重复请求可合并 |
| 由内核启动用户程序 | [call_usermodehelper](Kernel%20API%20-%20call_usermodehelper.md) | 等待模式、参数持有和编码后的退出状态 |
| 观察二进制数据 | [print_hex_dump](Kernel%20API%20-%20print_hex_dump.md) | 有效长度、快照一致性、日志开关和频率 |

## 常见选择

- 等“完成一次”用 completion，等“有数据/停止等条件”用 wait queue，固定睡眠不能代替设备完成协议。
- 与设备中断直接相关且需要睡眠时，先评估 [threaded IRQ](../irq/threaded-irq.md)；独立异步工作可考虑普通 workqueue，持续专用循环再考虑 kthread。
- 回调可能合并并不等于允许丢业务数据；设备 FIFO、软件队列或计数协议负责保存实际事件。
- 退出顺序先阻止新访问，再同步所有回调，最后释放资源；唤醒、超时和异步取消不单独构成生命周期保证。
