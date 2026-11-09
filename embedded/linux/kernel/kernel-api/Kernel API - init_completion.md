# Linux Completion：等待事件完成

> Completion 表达“某项活动已完成”：完成通知可以先于等待发生；超时或信号只结束等待，不会取消仍在运行的活动。

## TL;DR

- 先初始化，再把对象交给异步执行者；`complete()` 发布一次可消费的完成通知。
- 等待接口只能用于可睡眠上下文，超时参数是 jiffies，返回值须按具体接口判断。
- 栈上使用 `DECLARE_COMPLETION_ONSTACK()`，且函数返回前必须保证所有访问者已经结束。
- 下一轮使用 `reinit_completion()`，前提是旧等待者、通知者和复用操作之间没有竞态。

源码基准：Linux 6.18 LTS / 6.18.62。

## 初始化与返回值

| 接口 | 用途 |
| --- | --- |
| `init_completion(&op->done)` | 初始化动态对象中的 completion，内部通知数为 0 |
| `DECLARE_COMPLETION(done)` | 静态/文件作用域声明并初始化 |
| `DECLARE_COMPLETION_ONSTACK(done)` | 函数自动变量的声明与初始化，明确栈生命周期 |
| `complete()` | 增加一次通知并唤醒一个等待者；通知可先于等待 |
| `complete_all()` | 让当前及后续等待者均可通过，直到受控重置 |
| `wait_for_completion()` | 不可中断、无限期等待，无返回值 |
| `wait_for_completion_timeout()` | 超时为 0，成功为至少 1 的正剩余 jiffies |
| `wait_for_completion_interruptible()` | 成功为 0，信号中断为 `-ERESTARTSYS` |
| `wait_for_completion_interruptible_timeout()` | 成功为正数、超时为 0、信号中断为负数；使用有符号类型接收 |
| `try_wait_for_completion()` | 不阻塞，成功消费通知返回 true |
| `reinit_completion()` | 仅将完成状态重置为 0，不重建等待队列 |

## 实现原理

`struct completion` 包含 `done` 计数和 simple wait queue。`complete()` 在内部锁保护下增加计数并唤醒等待者；成功等待通常消费一个计数。因此先完成、后等待不会仅因没有等待者而丢失通知。

`complete_all()` 将计数置为饱和值，后来的等待也不会将它耗尽；它不是只对“调用瞬间已经排队的人”生效。复用之前必须确保上一轮所有等待者均已结束，不能用 `completion_done()` 判断是否还有旧等待者。

等待方在睡眠前后都检查完成状态，并在临时释放内部锁后调度；调用者不能持自旋锁、关中断或占有完成方必须获取的资源再等待。`complete()` 可用于硬中断通知；`complete_all()` 在 PREEMPT_RT 下还有线程上下文约束，不能把两者的上下文边界完全等同。

## 示例：一次操作发布一个结果

```c
#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/jiffies.h>

struct operation {
    struct completion done;
    int result;
};

static void operation_init(struct operation *op)
{
    init_completion(&op->done);
    op->result = 0;
}

static void operation_finish(struct operation *op, int result)
{
    op->result = result;
    complete(&op->done);
}

static int operation_wait(struct operation *op, int *result)
{
    if (!wait_for_completion_timeout(&op->done, msecs_to_jiffies(1000)))
        return -ETIMEDOUT;

    *result = op->result;
    return 0;
}
```

示例假设只有一个完成方和一个结果消费者，完成通知前写入结果，之后不再修改该轮结果。对象由更长生命周期的驱动上下文持有；不展示启动和取消设备操作的协议。若等待超时，调用者仍须保留对象并协调停止异步活动，不能立刻释放 `op` 或重用它。

## 复用与退出顺序

```text
初始化 → 发布异步操作 → 完成方写结果并 complete → 等待方取得结果
超时/信号退出 → 停止或取消异步活动 → 等待所有访问者退出 → 才能释放
下一轮 → 确认旧活动和旧等待者均已结束 → reinit_completion → 再发布
```

- 不要再次 `init_completion()` 一个仍有等待者的对象，这会重建队列并破坏等待关系。
- 不能先提交下一轮操作、再清零完成状态，否则快速完成的通知可能被抹掉。
- completion 不携带请求编号，上一轮迟到的完成可能被下一轮误消费；多轮并发操作应使用独立上下文或明确的代际协议。
- 完成等待不自动证明整个回调已返回；如果回调在 `complete()` 后仍访问对象，还需相应的同步退出保证。

排查时分别确认：通知是否发生、是否被其他等待者消费、是否被错误重置，以及超时后是否仍有回调访问对象。等待任意持续条件时考虑 [wait queue](Kernel%20API%20-%20wait_queue.md)，不要用反复清零 completion 模拟数据队列。

## 源码与参考

- [6.18.62 Completion 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/scheduler/completion.rst)：初始化、等待返回值和超时生命周期。
- [completion.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/completion.h)、[completion.c](https://github.com/gregkh/linux/blob/v6.18.62/kernel/sched/completion.c)：计数、等待队列与 RT 约束。
