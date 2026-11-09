# Linux `schedule_timeout()` 与睡眠

> `schedule_timeout()` 用 jiffies 表示等待预算，并依据调用前的任务状态决定是否睡眠；它是调度等待接口，不是精确定时器或事件完成证明。

## TL;DR

- 普通延时优先使用 `msleep()`、`usleep_range()` 或 `fsleep()` 等封装；等条件用 `wait_event*_timeout()`。
- 直接调用前设置任务状态；保持 `TASK_RUNNING` 会调用调度器，但不会按超时阻塞。
- 返回值为非负的剩余 jiffies，超时为 0；正数只表明提前返回，不能据此确定原因。
- 只能用于可睡眠上下文，不能在自旋锁临界区、硬中断或关中断状态使用。

源码基准：Linux 6.18 LTS / 6.18.62；实现位于 `kernel/time/sleep_timeout.c`，不要沿用旧版本文件位置。

## 接口与任务状态

```c
signed long schedule_timeout(signed long timeout);
```

| 调用前状态 | 行为 |
| --- | --- |
| `TASK_RUNNING` | 任务仍可运行，不会作为睡眠任务等待定时超时 |
| `TASK_INTERRUPTIBLE` | 可被信号或显式唤醒提前结束睡眠 |
| `TASK_UNINTERRUPTIBLE` | 不因普通信号结束等待，但仍可被显式唤醒 |
| `TASK_KILLABLE` | 可被致命信号打断，通常通过对应封装使用 |

函数返回时任务状态恢复为 `TASK_RUNNING`。`MAX_SCHEDULE_TIMEOUT` 表示不安装定时超时，调度后返回同一特殊值；这不表示任务永远无法被唤醒。负超时会走错误诊断路径，不属于正常输入。

## 实现路径

```text
设置当前任务等待状态
  → schedule_timeout(timeout)
  → 定时器记录 current，期限为 jiffies + timeout
  → schedule() 让出 CPU
  → 定时器 process_timeout() 或其他唤醒路径使任务可运行
  → 任务重新得到调度，同步删除本地定时器
  → 返回 max(期限 - 当前 jiffies, 0)
```

任务可运行不等于立刻执行。返回时刻还受时钟粒度、定时器实现、CPU 负载和调度延迟影响；不可把 jiffies 换算后的毫秒值当作实际测量结果。

## 示例：封装式可中断等待

```c
#include <linux/jiffies.h>
#include <linux/sched.h>

static long pause_interruptible(void)
{
    return schedule_timeout_interruptible(msecs_to_jiffies(250));
}
```

该函数返回的是剩余 jiffies；调用者按自己的退出、信号或条件协议继续处理。若直接使用低层接口，等价的基本顺序是先 `set_current_state(TASK_INTERRUPTIBLE)`，再调用 `schedule_timeout()`。但完整条件等待还需要正确入队和重复检查，不应把这两行扩展为无同步的自制等待协议。

## 延时接口如何选择

| 需求 | 接口 | 单位与限制 |
| --- | --- | --- |
| 普通较长睡眠 | `msleep()` / `ssleep()` | 毫秒 / 秒，不可中断封装 |
| 允许信号中断的延时 | `msleep_interruptible()` | 参数和剩余时间返回值均为毫秒 |
| 任务状态可控的调度等待 | `schedule_timeout_*()` | jiffies；interruptible、uninterruptible、killable 等形式 |
| 较短且允许调度的区间睡眠 | `usleep_range(min, max)` | 微秒，给定允许范围，不能承诺严格执行上界 |
| 通用可睡眠延时 | `fsleep()` | 微秒，按实现选择延时方式，仍要求可睡眠上下文 |
| 等待设备或软件条件 | `wait_event*_timeout()` / completion timeout | jiffies；返回值另按相应接口解释 |
| 原子上下文极短忙等 | `udelay()` 等 | 不调度，必须有硬件需求并严格控制耗时 |

不要把传给 `msleep()` 的毫秒直接传给 `schedule_timeout()`；使用 `msecs_to_jiffies()`。`usleep_range()` 也不是纳秒精度接口，允许范围和任务实际恢复执行时刻是不同概念。

## 常见错误与验证

- 用定时睡眠猜设备已经完成：时间到了仍需检查真实状态；完成事件应使用 [completion](Kernel%20API%20-%20init_completion.md) 或 [wait queue](Kernel%20API%20-%20wait_queue.md)。
- 被提前唤醒后直接认为成功：重新检查业务条件及信号/停止请求。
- 每次重新给满额超时：多次重试可能使总等待无限增长；用明确的总期限或剩余预算。
- 在循环中“处理耗时 + 固定睡眠”：周期会累计处理和调度延迟，不等于固定频率执行。
- 卸载等待被阻塞：确认停止请求能使等待退出，且等待时没有占有唤醒者所需的锁。

验证延迟应记录单调时钟上的请求、唤醒和实际执行时间，并区分定时等待与调度等待；不要仅观察 timeout 参数就宣称设备满足实时期限。

## 源码与参考

- [6.18.62 sleep_timeout.c](https://github.com/gregkh/linux/blob/v6.18.62/kernel/time/sleep_timeout.c)：状态、栈上定时器、特殊值及封装。
- [延迟与睡眠接口](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/timers/delay_sleep_functions.rst)：接口选型、粒度和允许误差。
