# Linux Wait Queue

> Wait queue 让任务睡眠等待一个可检查的条件；真正决定能否继续的是条件，`wake_up()` 只是让等待者重新检查。

## TL;DR

- 先初始化队列，等待使用 `wait_event*()`，生产者先发布条件及数据，再唤醒匹配状态的等待者。
- 不手写“检查条件，再 `schedule()`”来代替宏，否则容易在检查与入睡之间丢失唤醒。
- 条件需能安全重复求值；唤醒不等于获得资源，多个消费者仍需锁保护的资源获取协议。
- 等待仅用于可睡眠上下文；退出时发布停止条件并唤醒，再等待使用者离开后才能释放对象。

源码基准：Linux 6.18 LTS / 6.18.62。

## 等待与唤醒的实现路径

```text
wait_event*：先检查条件
  → 未满足：prepare_to_wait_event() 加入队列并设置任务状态
  → 再检查条件：满足则退出，否则 schedule()/schedule_timeout()
  → 唤醒后重新检查，直到条件成立、信号或超时
  → finish_wait() 移除等待项并恢复任务状态

生产者：发布状态/数据 → wake_up*() → 匹配等待者变为可运行
```

重复检查用于覆盖生产者恰好在等待者准备睡眠时更新条件的竞争。不要在条件表达式里做不可重复的出队或破坏性读寄存器操作，除非接口和锁协议明确允许。

## 接口与返回值

| 等待接口 | 结果 | 常见唤醒接口 |
| --- | --- | --- |
| `wait_event(wq, condition)` | 条件成立后返回，无返回值 | `wake_up()` |
| `wait_event_interruptible(wq, condition)` | 成功为 0，信号为 `-ERESTARTSYS` | `wake_up_interruptible()` 或 `wake_up()` |
| `wait_event_timeout(wq, condition, timeout)` | 超时且条件不成立为 0，成功为至少 1 的正数 | `wake_up()` |
| `wait_event_interruptible_timeout(...)` | 成功为正数、超时为 0、信号为负数 | `wake_up_interruptible()` 或 `wake_up()` |

超时单位是 jiffies，使用 `msecs_to_jiffies()` 等转换；有信号错误码的形式使用 `long` 等有符号类型接收。边界上超时到达但条件成立也可返回 `1`，不能把正数直接解释成精确剩余毫秒。

`wake_up_interruptible()` 不唤醒普通不可中断等待。默认唤醒路径可唤醒多个非 exclusive 等待者，对 exclusive 等待者另有配额规则；不能一概认为 `wake_up()` 只唤醒一个，也不能假设所有等待者都会运行。`wake_up_all()` 可用于需要广播的协议，但不替代条件检查。

## 示例：只发布一次的结果

```c
#include <linux/compiler.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/wait.h>

struct result_wait {
    wait_queue_head_t queue;
    bool ready;
    int value;
};

static void result_wait_init(struct result_wait *result)
{
    init_waitqueue_head(&result->queue);
    result->ready = false;
    result->value = 0;
}

static void result_publish(struct result_wait *result, int value)
{
    result->value = value;
    smp_store_release(&result->ready, true);
    wake_up_interruptible(&result->queue);
}

static int result_wait_read(struct result_wait *result, int *value)
{
    int ret;

    ret = wait_event_interruptible(result->queue,
                                  smp_load_acquire(&result->ready));
    if (ret)
        return ret;

    *value = result->value;
    return 0;
}
```

示例限定为一个生产者、单次发布，发布后结果保持不变，可有多个只读等待者。release/acquire 配对保证观察到 `ready` 的读者也能观察之前写入的 `value`；对象须在初始化后再发布给其他执行者，且活到所有调用者退出。

仅将条件改为 `READ_ONCE(ready)` / `WRITE_ONCE(ready, true)`，不会自动获得关联数据的发布顺序或互斥。持续读写的数据队列应使用锁或经证明的并发队列协议；消费者醒来后要在保护范围内重新确认并取得实际资源。

## 退出、手工等待与常见错误

- 停止路径通常将 `stopping` 纳入等待条件，发布停止状态并唤醒相关等待者；`wake_up_all()` 本身不意味着等待者已经返回，还需生命周期同步。
- 等待期间不能持有生产者更新条件所需的 mutex，也不能持自旋锁或关中断睡眠。
- 多次 wake 不会积攒为事件计数；没有等待者时的一次 wake 也不会单独被“记住”，需要由持续可见的条件保存事实。
- 避免随意使用 `waitqueue_active()` 优化掉 wake：无锁判断需要额外内存屏障和严格协议，普通代码直接唤醒更容易审查。
- 手工使用 `prepare_to_wait*()` 时，每个成功、信号、超时分支都须配对 `finish_wait()`；应优先采用封装宏。

排查“永远睡眠”时依次检查条件是否真正变更、先发布后唤醒的顺序、等待与唤醒是否使用同一个队列，以及停止条件和生产者是否还可能执行。需要保留完成通知而非任意条件时，参见 [completion](Kernel%20API%20-%20init_completion.md)。

## 源码与参考

- [6.18.62 wait.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/wait.h)、[wait.c](https://github.com/gregkh/linux/blob/v6.18.62/kernel/sched/wait.c)：等待宏、任务状态及唤醒匹配。
- 实际驱动实例可查 [lan78xx.c](https://github.com/gregkh/linux/blob/v6.18.62/drivers/net/usb/lan78xx.c) 的 `lan78xx_terminate_urbs()`，注意其条件、超时和 URB 生命周期协议。
- 相关：[schedule_timeout](Kernel%20API%20-%20schedule_timeout.md)、[kthread_run](Kernel%20API%20-%20kthread_run.md)。
