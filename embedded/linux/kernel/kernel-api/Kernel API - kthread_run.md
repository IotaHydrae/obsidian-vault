# Linux `kthread_run()`

> `kthread_run()` 创建并唤醒一个内核线程；线程必须通过可退出的等待和 `kthread_should_stop()` 配合 `kthread_stop()` 完成停止。

## TL;DR

- 创建失败返回 `ERR_PTR()`，用 `IS_ERR()` / `PTR_ERR()` 检查，不能只检查 `NULL`。
- 所有回调依赖的数据先初始化，再启动；线程可能在创建函数返回前已经运行。
- 等待条件纳入 `kthread_should_stop()`；停止不是强杀，也不能中断任意 I/O 或任意等待协议。
- 先停止生产者，再停止并等待线程，最后释放 `data`；停止时不能持有线程需要的锁。

源码基准：Linux 6.18 LTS / 6.18.62。

## 创建、运行与停止路径

```text
kthread_run(fn, data, name)
  → kthread_create() → 请求 kthreadd 创建 → 等待创建完成
  → wake_up_process() → 调度器安排线程执行 fn(data)

kthread_stop(task)
  → 设置 SHOULD_STOP → 解除 park 并唤醒
  → 等待线程退出 completion → 返回线程函数的结果
```

`kthread_create()` 创建后先保持未开始执行 `threadfn` 的状态，可在唤醒前设置所需属性；`kthread_run()` 是创建加唤醒的便捷宏。它们可能睡眠，不能从硬中断或持自旋锁的上下文调用。

| 接口 | 作用 |
| --- | --- |
| `kthread_run()` | 创建并启动，返回 task 指针或错误指针 |
| `kthread_should_stop()` | 在线程自身中检查停止请求 |
| `kthread_stop()` | 请求停止并同步等待，返回线程函数结果 |
| `kthread_park()` / `kthread_unpark()` | 暂停/恢复协议，线程需配合对应 park 接口；不同于永久退出 |
| `kthread_stop_put()` | 停止并释放调用者额外持有的 task 引用，不能对没有额外引用的裸指针滥用 |

## 示例：可停止的事件线程

示例用一个原子标志合并通知，处理次数不代表硬件事件数。创建、停止由控制路径串行执行，线程只在收到 stop 后正常返回。

```c
#include <linux/atomic.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/wait.h>

struct event_thread {
    wait_queue_head_t wait;
    atomic_t pending;
    struct task_struct *task;
    unsigned long batches;
};

static int event_thread_fn(void *data)
{
    struct event_thread *ctx = data;

    for (;;) {
        wait_event(ctx->wait,
                   kthread_should_stop() || atomic_read(&ctx->pending));
        if (kthread_should_stop())
            break;
        if (atomic_xchg(&ctx->pending, 0))
            ctx->batches++;
    }
    return 0;
}

static int event_thread_start(struct event_thread *ctx)
{
    struct task_struct *task;

    init_waitqueue_head(&ctx->wait);
    atomic_set(&ctx->pending, 0);
    ctx->batches = 0;
    ctx->task = NULL;
    task = kthread_run(event_thread_fn, ctx, "demo-event");
    if (IS_ERR(task))
        return PTR_ERR(task);
    ctx->task = task;
    return 0;
}

static void event_thread_notify(struct event_thread *ctx)
{
    atomic_set(&ctx->pending, 1);
    wake_up(&ctx->wait);
}

static void event_thread_stop(struct event_thread *ctx)
{
    if (ctx->task) {
        kthread_stop(ctx->task);
        ctx->task = NULL;
    }
}
```

业务处理可替换计数更新，但必须有有界执行或可退出协议。`batches` 仅由线程更新，控制路径在 stop 返回后才能无额外同步地读取；真实共享数据不能仅靠 `pending` 自动获得互斥。开启通知源要放在 start 成功之后，停止通知源并同步其回调要放在 stop 之前。

## 生命周期与易错点

- 若等待条件不含 stop，线程可能被唤醒后又因业务条件不满足再次睡下，导致 `kthread_stop()` 长期等待。
- 无超时的设备 I/O、不可取消的 completion 等不会因 stop 自动结束；必须分别设计终止机制。
- 不从线程自身调用 `kthread_stop(current)`，也不在外部持有线程需要的锁时等待它退出。
- 若线程允许在 stop 前自行返回或调用 `kthread_exit()`，调用者须保证 task 指针仍有效。需要额外引用时，可在 `kthread_create()` 后、首次唤醒前 `get_task_struct()`，随后与 `kthread_stop_put()` 配对；不能在线程可能已退出后才试图补引用。
- 需要参与系统冻结的线程还要实现 freezer 协议，例如核对 `kthread_freezable_should_stop()` 等接口；创建普通线程不自动完成设备的休眠/恢复协调。

排查停止卡住时查看线程栈，区分未检查 stop、等待无法退出、持锁相互等待和设备访问阻塞。按需采用 [workqueue](Kernel%20API%20-%20workqueue.md)，避免为每个短异步任务都创建专用线程；时间等待语义见 [schedule_timeout](Kernel%20API%20-%20schedule_timeout.md)。

## 源码与参考

- [6.18.62 kthread.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/kthread.h)：创建宏与公开接口。
- [kernel/kthread.c](https://github.com/gregkh/linux/blob/v6.18.62/kernel/kthread.c)：`kthread()`、创建完成协议、stop 与 task 引用。
