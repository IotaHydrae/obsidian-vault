# Linux `spin_lock_irqsave()`

> 当同一锁可能被本地硬中断上下文获取时，非中断上下文可用 `irqsave` 保存并屏蔽本地中断，同时获取锁。

```c
unsigned long flags;

spin_lock_irqsave(&lock, flags);
/* 只访问需要保护的共享状态。 */
spin_unlock_irqrestore(&lock, flags);
```

`flags` 恢复的是调用前的本地中断状态，不影响其他 CPU 的中断。若线程持锁后被同 CPU 的中断打断，而中断又获取同一锁，会发生自死锁；屏蔽本地中断是为避免这一竞争。所有访问同一共享状态的路径仍须使用一致的同步规则。

## 使用边界

- 临界区应尽量短；普通非 RT 自旋锁持有期间不能调用可能睡眠的操作。长时间屏蔽中断会增加延迟，具体设备可能发生事件合并或 FIFO 溢出。
- 不应把它当成所有场景的默认锁：仅进程上下文且需要睡眠时可考虑 mutex；与 softirq 共享时要依据实际上下文选择 `_bh` 等接口。
- 单核与多核、抢占配置和 `PREEMPT_RT` 下实现及中断语义存在差异；尤其不能把普通 `spinlock_t` 的非 RT 行为直接套到 RT 内核。需要不可睡眠的底层锁时应结合目标内核的锁类型规则评估 `raw_spinlock_t`。
- 不可屏蔽中断、具体控制器的 pending 行为和锁竞争开销依赖硬件及内核；不要承诺所有中断都会被保存或立即响应。

参考：[Linux 锁类型与 PREEMPT_RT 约束](https://docs.kernel.org/locking/locktypes.html)。
