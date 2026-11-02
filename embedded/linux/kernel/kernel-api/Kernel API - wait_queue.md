# Linux Wait Queue

> 等待队列用于睡眠等待条件变化；先发布条件，再唤醒等待者，并确保条件访问具有正确同步。

```c
DECLARE_WAIT_QUEUE_HEAD(queue);

/* 等待方，必须处于可睡眠上下文。 */
int ret = wait_event_interruptible(queue, READ_ONCE(ready));
if (ret)
    return ret;

/* 事件方：实际共享数据还需合适的锁或内存顺序。 */
WRITE_ONCE(ready, true);
wake_up_interruptible(&queue);
```

`wait_event_interruptible()` 在条件已成立时直接返回，否则加入等待并反复检查；信号中断时返回负错误码。`wake_up_interruptible()` 针对可中断等待，普通和 exclusive 等待者的唤醒数量不同，不能简单理解为总是只唤醒一个线程。

示例中的 `READ_ONCE` / `WRITE_ONCE` 仅限制编译器对标志访问的处理，并不自动保护其他共享数据，也不替代锁或 acquire/release 同步。要检查超时可用 `wait_event_interruptible_timeout()`；手工构造等待队列时必须处理状态、竞态、退出和对象生命周期。实际驱动用法可查 `drivers/net/usb/lan78xx.c` 中的 `lan78xx_terminate_urbs()`，以目标内核版本为准。
