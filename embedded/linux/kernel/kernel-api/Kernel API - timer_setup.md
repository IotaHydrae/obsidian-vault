# Linux Timer：定时回调与可靠退出

> `timer_list` 在期限到达后安排不可睡眠的回调；最终退出用 `timer_shutdown_sync()` 等待回调并阻止重新启动，再释放包含 timer 的对象。

## TL;DR

- `timer_setup()` 初始化，`mod_timer(timer, jiffies + delta)` 启动或修改期限。
- 期限是绝对 jiffies，实际回调可能晚于期限，不适合精确硬件时序。
- 普通 timer 回调不能睡眠；需要睡眠的处理交给普通 workqueue。
- `timer_delete_sync()` 只删除并等待；`timer_shutdown_sync()` 还禁止后续重新 arm，适合最终销毁。

源码基准：Linux 6.18 LTS / 6.18.62。

## 接口与返回值

| 接口 | 行为 |
| --- | --- |
| `timer_setup(timer, callback, flags)` | 初始化嵌入对象中的定时器，不会自动启动 |
| `mod_timer(timer, expires)` | 0：原先未挂起而启动，或 timer 已 shutdown；1：修改原有挂起项 |
| `timer_pending()` | 观察是否挂在 timer 队列中，不证明回调没有运行 |
| `timer_delete_sync()` | 删除挂起项并等待回调结束；调用者必须防止之后重新 arm |
| `timer_shutdown_sync()` | 删除、等待，并使后续启动请求被忽略，直到重新初始化 |

`mod_timer()` 的 0/1 不是通常的成功/失败约定。回调由定时器子系统在 softirq 处理路径执行；即使 PREEMPT_RT 改变执行载体，也不要据此把可移植回调写成可睡眠函数。

## 示例：一次超时标志

```c
#include <linux/atomic.h>
#include <linux/container_of.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

struct timeout_guard {
    struct timer_list timer;
    atomic_t expired;
};

static void timeout_expire(struct timer_list *timer)
{
    struct timeout_guard *guard;

    guard = container_of(timer, struct timeout_guard, timer);
    atomic_set(&guard->expired, 1);
}

static void timeout_guard_start(struct timeout_guard *guard)
{
    atomic_set(&guard->expired, 0);
    timer_setup(&guard->timer, timeout_expire, 0);
    mod_timer(&guard->timer, jiffies + msecs_to_jiffies(500));
}

static void timeout_guard_stop(struct timeout_guard *guard)
{
    timer_shutdown_sync(&guard->timer);
}
```

示例用于新建、尚未发布的对象：启动一次，其他路径用 `atomic_read()` 读取标志，最终同步停止后再释放。不能对正在使用的 timer 再次调用 start 来“重新初始化”。500 ms 是示例超时；该标志不负责发布其他共享数据，也不取消设备操作。6.18.62 还提供 `timer_container_of()` 便捷宏。

## 与 workqueue 共同退出

若 timer 调度 work，而 work 又重启 timer，应先停止外部生产者，再执行：

```text
timer_shutdown_sync() → cancel_work_sync() 或销毁专用 workqueue → 释放对象
```

这样正在运行的 work 即使尝试重新 arm，timer 也不会复活。系统工作队列只能取消自己的 work，不能销毁系统工作队列；专用队列参见 [workqueue](Kernel%20API%20-%20workqueue.md)。

- 不能持有回调需要获取的锁调用同步停止，否则可能死锁；通常在可睡眠的退出路径调用。
- shutdown 不让外部访问已释放指针变得合法，所有 timer/work 使用者仍须结束。
- 栈上 timer 需要对应的 on-stack 初始化和销毁接口，并保证函数返回前全部活动结束。
- 高精度期限评估 hrtimer；任务上下文中的普通短睡眠可用 `usleep_range()` 等，不能让忙等替代同步协议。

## 源码与参考

- [timer.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/timer.h)：初始化、状态和停止接口。
- [kernel/time/timer.c](https://github.com/gregkh/linux/blob/v6.18.62/kernel/time/timer.c)：mod_timer 返回值、shutdown 和 timer/work 退出顺序。
