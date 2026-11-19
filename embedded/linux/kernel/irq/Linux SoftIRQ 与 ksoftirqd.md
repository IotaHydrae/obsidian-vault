# Linux SoftIRQ 与 ksoftirqd

> SoftIRQ 是内核延迟处理机制；它可以在 IRQ 退出路径或 `ksoftirqd` 中被执行，但回调仍须遵守 softirq 上下文约束。

## TL;DR

- SoftIRQ 有按 CPU 维护的 pending 状态；重复触发可能合并，pending 位不是业务事件计数器。
- 普通非 RT 配置中，IRQ 退出时可能处理待执行的 softirq；工作较多或不适合继续处理时可唤醒 `ksoftirqd`。
- softirq / tasklet / `WQ_BH` 回调不能按普通可睡眠线程编写；`ksoftirqd` 是执行载体，不改变该约束。
- `local_bh_disable()` 影响本 CPU 的 bottom-half 处理，不单独提供跨 CPU 互斥或设备数据队列。

范围：Linux 6.18.62 LTS 的普通非 PREEMPT_RT 配置；PREEMPT_RT 的执行和锁语义应按其专门文档分析。

## 触发与执行

```text
事件产生 / raise_softirq() → 当前 CPU 的 pending 位
→ 合适的处理点执行 __do_softirq()
→ 按 softirq 类型调用已注册的处理函数
→ 若仍有工作或达到处理限制，交由后续处理 / ksoftirqd
```

具体路径取决于触发上下文和配置，不是每个 softirq 都来自设备硬 IRQ。Linux 6.18 的 `kernel/softirq.c` 中可从 `raise_softirq()`、`__do_softirq()` 和 `run_ksoftirqd()` 定位；`MAX_SOFTIRQ_TIME` / `MAX_SOFTIRQ_RESTART` 是该实现中限制连续处理的因素，不是业务处理的实时期限保证。

| 类型示例 | 用途边界 |
| --- | --- |
| `NET_RX_SOFTIRQ` | 网络接收相关处理；具体预算和后续调度依网络子系统 |
| `TIMER_SOFTIRQ` | 普通定时器相关处理，不等同于所有时钟事件 |
| `HRTIMER_SOFTIRQ` | 适用的 hrtimer softirq 路径；不是所有 hrtimer 回调必然在此运行 |
| `RCU_SOFTIRQ` | RCU 相关延迟处理，具体行为取决于配置 |

## 与其他延迟机制的选择

| 机制 | 上下文 / 常用场景 | 重要约束 |
| --- | --- | --- |
| softirq | 内核子系统的延迟处理 | 不可睡眠，多个 CPU 可并发执行相同类型 |
| tasklet | 建立在 softirq 上的延迟机制 | 已弃用方向的接口；维护现有代码时注意合并和退出 |
| `WQ_BH` | softirq 上下文的 workqueue 工作 | 回调不可睡眠，不等于普通 kworker |
| 普通 workqueue | kworker 线程处理独立工作 | 通常可睡眠，但需要处理并发与取消 |
| threaded IRQ | 设备 IRQ 的线程处理 | primary 与线程分工、ONESHOT 和共享 IRQ 需正确设计 |

普通驱动通常不需要新建 softirq 类型；慢速 I²C/SPI 操作可评估 threaded IRQ 或普通 workqueue。具体接口和生命周期见 [[threaded-irq]]、[[Kernel API - workqueue]]、[[Kernel API - tasklet]]。

## 诊断

1. 查看 `/proc/softirqs` 的各 CPU 计数增量，并结合任务和 IRQ 负载，不能只看累计总量。
2. 用 [[ftrace]] 或 `trace-cmd` 观察可用的 `irq:softirq_raise`、`irq:softirq_entry`、`irq:softirq_exit` 和 sched 事件。
3. 高 `ksoftirqd` CPU 使用只说明延迟处理消耗了时间；继续定位 softirq 类型、具体驱动/网络负载、预算和锁竞争。
4. 卸载时先停止事件源并同步相关回调，再释放数据；pending 位合并不代表硬件事件已经被完整消费。

## 参考

- Linux 6.18.62：[`kernel/softirq.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/softirq.c)、[`include/linux/interrupt.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/interrupt.h)、[PREEMPT_RT 上下文差异](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/real-time/differences.rst)。
- 蜗窝科技：[softirq](https://www.wowotech.net/irq_subsystem/soft-irq.html)、[workqueue](https://www.wowotech.net/irq_subsystem/workqueue.html)。
