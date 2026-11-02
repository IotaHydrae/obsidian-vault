# Linux `kthread_run()`

> `kthread_run()` 创建并唤醒内核线程；失败时返回错误指针，退出时需要协作停止并等待。

```c
static int worker_fn(void *data)
{
    while (!kthread_should_stop()) {
        /* 处理工作；使用可被唤醒的等待，不要长时间占用 CPU。 */
        schedule_timeout_interruptible(msecs_to_jiffies(1000));
    }
    return 0;
}

/* 初始化中： */
worker = kthread_run(worker_fn, data, "my-worker");
if (IS_ERR(worker))
    return PTR_ERR(worker);

/* 清理中： */
kthread_stop(worker);
```

- `kthread_run()` 相当于创建后唤醒；`kthread_create()` 只创建，需另行 `wake_up_process()`。
- `kthread_stop()` 设置停止请求、唤醒线程并等待其退出；线程应检查 `kthread_should_stop()`。它不是向线程发送普通信号。
- 示例要求调用方保证 `worker` 和 `data` 的生命周期及清理顺序；若线程可能自行提前退出，还须按目标内核接口处理 task 引用和退出竞态。
- 睡眠 API 的中断语义见 [[Kernel API - schedule_timeout]]；不要用无同步的共享标志替代正确的停止协议。
