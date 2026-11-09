# Linux Tasklet：旧式 softirq 下半部

> Tasklet 把短小不可睡眠的处理推迟到 softirq 路径；同一 tasklet 不会同时在多个 CPU 上执行，但这不等于与硬中断或其他任务互斥。

## TL;DR

- **6.18.62 已明确将 tasklet API 标为弃用**；新驱动优先考虑 [threaded IRQ](../irq/threaded-irq.md)，其他异步需求按上下文选择 workqueue。
- 回调不能睡眠，即使某次实际由 `ksoftirqd` 执行，也不能据此使用 mutex、睡眠式总线 I/O 或等待。
- 重复 schedule 可以合并，回调应检查真实设备状态、FIFO 或软件队列，不能把调度次数当事件数。
- 先停止调度源，再在合适上下文执行 `tasklet_kill()`，等待结束后才能释放包含 tasklet 的对象。

源码基准：Linux 6.18 LTS / 6.18.62。以下用法用于维护已有代码。

## 调度与执行路径

```text
硬中断或其他允许的路径 tasklet_schedule()
  → 设置 TASKLET_STATE_SCHED；已设置则合并请求
  → 加入 per-CPU tasklet 链表，触发 TASKLET_SOFTIRQ
  → softirq 处理路径 / ksoftirqd
  → tasklet_action_common() 取执行权并检查 disable 计数
  → 清调度状态、执行回调、释放执行权
```

一个 tasklet 在执行期间又被 schedule，可被安排稍后再执行；不同 tasklet 可以在不同 CPU 上并行。它们与进程、IRQ、其他 tasklet 共享数据时仍需要正确的同步协议。不要依赖 CPU 永远不变，也不要承诺调度后立即执行。

## 接口与示例

| 接口 | 含义 |
| --- | --- |
| `tasklet_setup(&t, callback)` | 初始化，回调参数为 `struct tasklet_struct *` |
| `tasklet_schedule(&t)` | 请求普通 tasklet 执行，重复请求可能合并 |
| `tasklet_hi_schedule(&t)` | 高优先级 softirq 路径，不构成实时保证 |
| `tasklet_disable()` / `tasklet_enable()` | 用计数禁止/允许执行，必须正确配对 |
| `tasklet_disable_nosync()` | 不等待当前回调结束，不是完成同步 |
| `tasklet_kill()` | 等待待处理/运行状态被清理，不自动禁止外部再次调度 |

```c
#include <linux/atomic.h>
#include <linux/container_of.h>
#include <linux/interrupt.h>

struct legacy_bottom_half {
    struct tasklet_struct tasklet;
    atomic_t batches;
};

static void legacy_tasklet_fn(struct tasklet_struct *tasklet)
{
    struct legacy_bottom_half *ctx;

    ctx = container_of_const(tasklet, struct legacy_bottom_half, tasklet);
    atomic_inc(&ctx->batches);
}

static void legacy_bottom_half_init(struct legacy_bottom_half *ctx)
{
    atomic_set(&ctx->batches, 0);
    tasklet_setup(&ctx->tasklet, legacy_tasklet_fn);
}

static void legacy_bottom_half_schedule(struct legacy_bottom_half *ctx)
{
    tasklet_schedule(&ctx->tasklet);
}

static void legacy_bottom_half_stop(struct legacy_bottom_half *ctx)
{
    tasklet_kill(&ctx->tasklet);
}
```

示例计数只表达回调运行次数。调用 stop 前，控制路径必须已停止并同步 IRQ、定时器等所有调度源；stop 不能与仍可能访问 `ctx` 的生产者并发后立即释放对象。实际回调只做有界、不可睡眠的工作，不在此处打印大量日志。

## 禁用、取消与迁移

- `tasklet_disable()` 不移除全部待处理请求，enable 后可能继续处理；不能把 disable 当作销毁。
- 若 tasklet 已 pending 却长期 disabled，kill 可能等待它无法完成的状态；卸载须按真实状态设计，不机械套用“disable 后 kill”。
- `tasklet_kill()` 可能等待，不能从硬中断、当前 tasklet 回调或持有其需要的锁时调用。
- 老式 `tasklet_init()` 使用 `unsigned long` 数据回调，新旧回调类型不同，不能强制转换函数指针代替迁移。
- 替换成线程化 IRQ 后可把睡眠式设备访问放到 `thread_fn`，仍须审查共享锁、触发源清除、事件合并和释放顺序。
- 6.18.62 还提供 `WQ_BH` workqueue；它仍在 softirq 上下文执行，不能因接口叫 workqueue 就允许睡眠。

排查延迟和丢事件时同时观察调度合并、设备队列深度与回调耗时；排查死锁时确认回调调用链中没有睡眠操作。PREEMPT_RT 的 softirq 调度方式改变，也不应由驱动擅自放宽 tasklet API 的可睡眠约束。

## 源码与参考

- [6.18.62 interrupt.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/interrupt.h)：弃用说明、串行与调度性质、接口。
- [kernel/softirq.c](https://github.com/gregkh/linux/blob/v6.18.62/kernel/softirq.c)：tasklet 队列、执行与 kill。
- 相关：[workqueue](Kernel%20API%20-%20workqueue.md)、[spin_lock_irqsave](Kernel%20API%20-%20spin_lock_irqsave.md)。
