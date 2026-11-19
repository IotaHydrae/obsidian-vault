# Linux RCU 与内存顺序

> RCU 主要让读者在并发更新期间安全访问对象；锁、引用计数和内存屏障各解决不同的问题。

## TL;DR

- `rcu_read_lock()` 保护受 RCU 管理的读侧临界区；发布与读取指针使用相应的 RCU 接口。
- 删除对象先让新读者无法找到它，再等待宽限期或安排 RCU 回调，最后释放；普通 RCU 不替写者串行化。
- 自旋锁 / mutex 保护共享状态，引用计数延长对象寿命，RCU 管理读侧并发与回收；不能互相替代。
- 编译器重排、CPU 重排、DMA 缓存同步和 MMIO 顺序是不同问题，选择各自的接口。

## 常见模式

| 目标 | 常用机制 | 边界 |
| --- | --- | --- |
| 短临界区、不可睡眠上下文共享状态 | 自旋锁 | 注意中断上下文、锁顺序及 PREEMPT_RT 语义 |
| 可睡眠的互斥过程 | mutex | 不能在硬中断或禁止睡眠的上下文中使用 |
| 读多写少且允许延迟回收 | RCU | 写侧仍需同步；读侧取出的指针不能无条件带出保护范围 |
| 等待一次完成或可重复检查的条件 | completion / wait queue | 超时、取消和对象释放是独立步骤 |

典型 RCU 生命周期：写者初始化新对象后发布指针；读者在读侧临界区取得并使用对象；写者移除旧对象后，使用适合上下文的宽限期等待或回调回收。对象若需在读侧临界区外继续使用，还需在受保护时安全获取有效引用。

内存屏障约束的是访问顺序，不自动提供互斥、等待或对象寿命。`READ_ONCE()` / `WRITE_ONCE()` 不等同于完整锁；设备寄存器使用 MMIO 访问器，设备 DMA 缓冲区按 DMA API 的所有权规则同步。锁和 DMA 细节分别见 [[Kernel API - spin_lock_irqsave]] 与 [[Kernel API - dma_map_single]]。

## 参考

- 蜗窝科技：[RCU 概述](https://www.wowotech.net/kernel_synchronization/461.html)、[RCU 使用](https://www.wowotech.net/kernel_synchronization/462.html)、[同步机制栏目](https://www.wowotech.net/sort/kernel_synchronization/)。
- Linux 6.18.62：[RCU 入门](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/RCU/whatisRCU.rst)、[内存屏障](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/memory-barriers.txt)。
