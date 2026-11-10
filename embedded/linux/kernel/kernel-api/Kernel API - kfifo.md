# Linux Kfifo：事件缓存与单生产者单消费者

> Kfifo 保存实际数据，唤醒或调度只负责通知；无锁并发要求严格的单生产者、单消费者，且不能并发 reset。

## TL;DR

- 固定容量类型化 FIFO 使用 `DECLARE_KFIFO()` 和 `INIT_KFIFO()`，容量是 2 的幂。
- 类型化 FIFO 的长度是元素数；普通 `struct kfifo` 通常按字节计数。
- `kfifo_put/get()` 返回是否完成一个元素，`kfifo_in/out()` 返回实际复制的元素数，可能少于请求数。
- 多生产者、多消费者或并发重置需要额外同步；FIFO 满时必须明确丢弃、背压或报错策略。

源码基准：Linux 6.18 LTS / 6.18.62。

## 数据结构与接口

内部用递增的 `in`、`out` 计数和环形数组保存数据，通过容量掩码计算位置；发布和消费使用相应内存屏障。它不会替调用者保存被覆盖的设备状态，也不会自动唤醒消费者。

| 需求 | 接口 |
| --- | --- |
| 对象内嵌固定容量队列 | `DECLARE_KFIFO(fifo, type, size)` + `INIT_KFIFO()` |
| 动态队列 | `DECLARE_KFIFO_PTR()` 或 `struct kfifo` + `kfifo_alloc()`，配对 `kfifo_free()` |
| 单元素入队/出队 | `kfifo_put()` / `kfifo_get()` |
| 批量复制 | `kfifo_in()` / `kfifo_out()` |
| 查询状态 | `kfifo_len()`、`kfifo_avail()`、`kfifo_is_empty()` |

动态分配容量会向上取整为 2 的幂；类型化动态队列以元素为单位指定容量。状态查询只是并发中的瞬时观察，不能代替后续操作的返回值检查。

## 示例：缓存事件编号

```c
#include <linux/errno.h>
#include <linux/kfifo.h>
#include <linux/types.h>

struct event_buffer {
    DECLARE_KFIFO(events, u32, 64);
};

static void event_buffer_init(struct event_buffer *buffer)
{
    INIT_KFIFO(buffer->events);
}

/* 恰好一个生产者负责调用此入口。 */
static int event_push(struct event_buffer *buffer, u32 event)
{
    if (!kfifo_put(&buffer->events, event))
        return -ENOSPC;

    return 0;
}

/* 恰好一个消费者负责调用此入口。 */
static int event_pop(struct event_buffer *buffer, u32 *event)
{
    if (!kfifo_get(&buffer->events, event))
        return -EAGAIN;

    return 0;
}
```

同一个入口不能同时由 IRQ、work 或多个进程调用，否则不再满足单生产者/单消费者条件。若生产者来自多个上下文，按访问上下文选择锁；`kfifo_in_spinlocked()` 等接口可以封装相应临界区，但所有同类访问仍要遵守同一协议。

## 通知与退出

```text
生产者保存事件 → 检查实际入队结果 → 唤醒/调度消费者
消费者循环取出已有事件 → 按等待条件决定是否睡眠
```

- workqueue 的重复调度可能合并；实际事件必须保存在 FIFO 中，不能把一次 work 回调等同于一个事件。
- 用 [wait queue](Kernel%20API%20-%20wait_queue.md) 等待时，把“非空或停止”作为条件，并遵守先发布数据再唤醒的顺序。
- `kfifo_reset()` 要求独占访问；不能为了清空数据而与生产/消费并发调用。`kfifo_reset_out()` 也仅在其文档限定的单消费者条件下适用。
- 停止生产者、结束消费者并同步回调之后，才能释放或重新初始化 FIFO。

## 源码与参考

- [kfifo.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/kfifo.h)：类型、计数单位和无锁条件。
- [lib/kfifo.c](https://github.com/gregkh/linux/blob/v6.18.62/lib/kfifo.c)：容量取整、环形复制和内存屏障。
