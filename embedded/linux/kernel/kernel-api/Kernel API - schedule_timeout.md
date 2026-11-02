# Linux `schedule_timeout()` 与睡眠

> `schedule_timeout()` 的参数是 jiffies，并依据调用前的任务状态睡眠；简单延时优先使用封装接口。

```c
set_current_state(TASK_INTERRUPTIBLE);
remaining = schedule_timeout(msecs_to_jiffies(250));
```

超时通常返回 0，提前唤醒可返回剩余 jiffies；返回值本身不说明提前唤醒的原因。调用前若保持 `TASK_RUNNING`，不会按预期阻塞。`MAX_SCHEDULE_TIMEOUT` 表示没有定时超时的等待，但仍可被适当方式唤醒。

| 接口 | 单位 | 用途 |
| --- | --- | --- |
| `msleep()` / `ssleep()` | 毫秒 / 秒 | 普通不可中断睡眠 |
| `msleep_interruptible()` | 毫秒 | 可被信号中断，检查返回的剩余时间 |
| `schedule_timeout_interruptible()` | jiffies | 设置可中断状态后调度 |
| `schedule_timeout_uninterruptible()` | jiffies | 设置不可中断状态后调度 |
| `wait_event*_timeout()` | jiffies | 等待条件，避免手写易丢失唤醒的协议 |

不得在不能睡眠的上下文中调用。负超时不属于正常用法；时间精度、头文件和实际调度延迟以目标内核版本为准，`usleep_range()` 也不能保证纳秒级精度。

参考：[Linux 延迟与睡眠接口](https://docs.kernel.org/timers/delay_sleep_functions.html)、[Linux v6.1 timer.c](https://github.com/torvalds/linux/blob/v6.1/kernel/time/timer.c)。
