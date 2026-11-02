# Linux Completion：等待事件完成

> `completion` 用于等待其他执行上下文通知完成；初始化一次，复用前必须协调旧的等待者和通知者。

```c
/* completion 必须活到所有可能的 complete() 和等待操作结束。 */
DECLARE_COMPLETION(done);

/* 工作方完成后： */
complete(&done);

/* 可睡眠的进程上下文： */
if (!wait_for_completion_timeout(&done, msecs_to_jiffies(1000)))
    pr_warn("operation timed out\n");
```

| 接口 | 用途 |
| --- | --- |
| `init_completion(&done)` / `DECLARE_COMPLETION(done)` | 首次初始化；栈上声明按目标版本使用相应 on-stack 接口 |
| `complete()` / `complete_all()` | 通知一个等待者 / 通知所有等待者 |
| `wait_for_completion()` | 在可睡眠上下文中等待 |
| `wait_for_completion_timeout()` | 超时返回 0，完成时返回正的剩余 jiffies |
| `wait_for_completion_interruptible()` | 完成返回 0；信号中断返回负错误码 |
| `try_wait_for_completion()` | 非阻塞尝试消耗一次完成通知 |
| `reinit_completion()` | 将完成状态重置，用于受控的下一轮操作 |

`complete()` 可以先于等待发生；后来的等待仍可观察通知。不要对存在等待者的对象再次调用 `init_completion()`；`reinit_completion()` 也必须与通知者及等待者同步，否则可能丢失通知。超时或信号返回不代表异步工作已停止，不能立即释放其可能访问的 completion 或上下文。

参考：[Linux Completion 文档](https://docs.kernel.org/scheduler/completion.html)、[Linux v6.1 completion.h](https://github.com/torvalds/linux/blob/v6.1/include/linux/completion.h)。
