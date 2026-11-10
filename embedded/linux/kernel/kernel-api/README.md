# Linux Kernel API 笔记索引

> 按执行上下文、等待语义和对象生命周期选择接口；文档以 Linux 6.18 LTS / 6.18.62 源码为基准，示例中的设备协议须按实际硬件实现。

## 内存、资源与对象

| 需求 | 笔记 | 首要边界 |
| --- | --- | --- |
| 分配对象、数组或大块内存 | [kmalloc / kzalloc / kcalloc](Kernel%20API%20-%20kmalloc.md) | GFP 上下文、尺寸溢出、连续性和释放配对 |
| 随设备绑定回收资源 | [devres](Kernel%20API%20-%20devres.md) | 逆序清理不替代停止设备和同步回调 |
| 从嵌入成员找回外层对象 | [container_of](Kernel%20API%20-%20container_of.md) | 来源、生命周期和 const；新代码优先保留 const |
| 共享对象并在最后一次使用后释放 | [kref](Kernel%20API%20-%20kref.md) | 发布前取引用，lookup 与 get 之间不能失去保护 |
| 加载并校验设备固件 | [request_firmware](Kernel%20API%20-%20request_firmware.md) | 数据长度、异步传输与释放时机 |

## 互斥与等待

| 需求 | 笔记 | 首要边界 |
| --- | --- | --- |
| 串行化可睡眠配置流程 | [mutex](Kernel%20API%20-%20mutex.md) | 仅适用任务上下文，信号失败后不能 unlock |
| 任务与硬中断保护短共享状态 | [spin_lock_irqsave](Kernel%20API%20-%20spin_lock_irqsave.md) | 本地 IRQ 与跨 CPU 互斥；RT 普通锁语义不同 |
| 表达 helper 必须已持锁 | [lockdep_assert_held](Kernel%20API%20-%20lockdep_assert_held.md) | 调试断言不会代替加锁 |
| 等待一次完成通知 | [completion](Kernel%20API%20-%20init_completion.md) | 超时不是取消，复用必须协调旧活动 |
| 等待可重复检查的条件 | [wait queue](Kernel%20API%20-%20wait_queue.md) | 先发布条件再唤醒，醒来后重新判断 |

## 定时与异步执行

| 需求 | 笔记 | 首要边界 |
| --- | --- | --- |
| 可睡眠的延时或调度等待 | [schedule_timeout](Kernel%20API%20-%20schedule_timeout.md) | 单位、任务状态和实际调度延迟 |
| 在未来期限触发回调 | [timer_setup / mod_timer](Kernel%20API%20-%20timer_setup.md) | 绝对 jiffies、不可睡眠回调与 shutdown |
| 运行专用事件循环 | [kthread_run](Kernel%20API%20-%20kthread_run.md) | 线程须协作停止并保证数据和 task 生命周期 |
| 把工作交给后台执行池 | [workqueue](Kernel%20API%20-%20workqueue.md) | 普通与 BH 上下文不同，停止生产者后再取消 |
| 维护旧式 softirq 下半部 | [tasklet](Kernel%20API%20-%20tasklet.md) | 已弃用、不可睡眠，重复请求可合并 |

## 容器与编号

| 需求 | 笔记 | 首要边界 |
| --- | --- | --- |
| 组织对象队列或双向链表 | [list_head](Kernel%20API%20-%20list_head.md) | safe 遍历不代表并发安全，摘链不等于释放 |
| 保存生产者与消费者之间的事件 | [kfifo](Kernel%20API%20-%20kfifo.md) | 元素/字节单位、单生产者单消费者与满队列策略 |
| 分配 ID 并索引对象 | [XArray / xa_alloc](Kernel%20API%20-%20xa_alloc.md) | 包含上界、编码错误与对象引用 |
| 维护现有整数编号映射 | [idr_alloc](Kernel%20API%20-%20idr_alloc.md) | 写者同步和回收；新设计优先评估 XArray/IDA |

## 硬件访问与电源管理

| 需求 | 笔记 | 首要边界 |
| --- | --- | --- |
| 映射 MMIO 并等待寄存器条件 | [ioremap / readl_poll_timeout](Kernel%20API%20-%20ioremap.md) | __iomem、访问副作用、超时和 posted write |
| 统一 I²C/SPI 寄存器操作 | [regmap](Kernel%20API%20-%20regmap.md) | 读改写、W1C、缓存与总线睡眠 |
| 准备 DMA 缓冲区和映射 | [dma_map_single](Kernel%20API%20-%20dma_map_single.md) | DMA 地址、缓存所有权和停止后 unmap |
| 按功能获取和操作 GPIO | [gpiod](Kernel%20API%20-%20gpiod.md) | active-low、optional、cansleep 与错误传播 |
| 访问期间保持设备可用 | [Runtime PM](Kernel%20API%20-%20pm_runtime.md) | usage 配对、异步访问完成和真实硬件状态 |

## 调试与用户程序

| 需求 | 笔记 | 首要边界 |
| --- | --- | --- |
| 由内核启动用户程序 | [call_usermodehelper](Kernel%20API%20-%20call_usermodehelper.md) | 等待模式、参数持有和编码后的退出状态 |
| 观察二进制数据 | [print_hex_dump](Kernel%20API%20-%20print_hex_dump.md) | 有效长度、快照一致性、日志开关和频率 |

相关入口：[错误指针 ERR_PTR / IS_ERR / PTR_ERR](../../drivers/base/%E5%A4%84%E7%90%86%E7%A9%BA%E6%8C%87%E9%92%88%E9%94%99%E8%AF%AF.md)、[线程化中断 request_threaded_irq](../irq/threaded-irq.md)。

## 常见选择

- 等“完成一次”用 completion，等“有数据/停止等条件”用 wait queue，固定睡眠不能代替设备完成协议。
- 与设备中断直接相关且需要睡眠时，先评估 [threaded IRQ](../irq/threaded-irq.md)；独立异步工作可考虑普通 workqueue，持续专用循环再考虑 kthread。
- 回调可能合并并不等于允许丢业务数据；设备 FIFO、软件队列或计数协议负责保存实际事件。
- 锁保护共享状态，引用保护对象内存，Runtime PM usage 保护设备使用期间的电源状态；三者不能互相代替。
- 退出顺序先阻止新访问，再同步所有回调，最后释放资源；唤醒、超时和异步取消不单独构成生命周期保证。
