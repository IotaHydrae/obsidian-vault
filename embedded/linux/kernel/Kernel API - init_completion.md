`init_completion` 是 Linux 内核中用于**初始化 completion 结构**的函数/宏，主要用于线程/任务间“完成事件”同步（类似于一个轻量级的、单次使用的 semaphore 或 barrier）。

它定义在 `<linux/completion.h>` 中。

### 核心类型

```c
struct completion {
    unsigned int done;
    wait_queue_head_t wait;
};
```

### 主要初始化接口对比

|接口|作用|使用时机|是否清空等待队列|现代内核推荐度|备注|
|---|---|---|---|---|---|
|`init_completion(&done)`|完整初始化（done=0 + 初始化等待队列）|**第一次使用** 或 静态变量声明后|是（新建空队列）|★★★★★|最常用|
|`DECLARE_COMPLETION(done)`|声明 + 初始化（栈上或文件作用域）|局部变量或模块静态变量|是|★★★★☆|方便|
|`INIT_COMPLETION(done)`|只把 `done` 置 0（**不**动等待队列）|**复用同一个 completion** 时|否|★★☆☆☆|已 deprecated|
|`reinit_completion(&done)`|只把 `done` 置 0（**不**动等待队列）|**复用**（推荐替代 INIT_COMPLETION）|否|★★★★★|现代首选复用方式|

**重要结论（现代内核 4.x/5.x/6.x）**：

- **第一次初始化** → 用 `init_completion()` 或 `DECLARE_COMPLETION()`
- **重复使用同一个 completion 结构**（最常见场景） → 用 `reinit_completion()`
- 永远不要在已经有人在 `wait_for_completion()` 的 completion 上调用 `init_completion()`（会破坏等待队列，导致等待者丢失或死锁）

### 典型完整用法示例（最常见模式）

```c
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/delay.h>

static struct completion init_done;           // 全局或模块静态
static struct task_struct *worker_task;

static int worker_func(void *arg)
{
    printk(KERN_INFO "worker: starting initialization...\n");
    msleep(3000);   // 模拟耗时初始化

    printk(KERN_INFO "worker: initialization complete!\n");
    complete(&init_done);          // 通知等待者：完成了！

    return 0;
}

static int __init my_mod_init(void)
{
    printk(KERN_INFO "module loading...\n");

    // 方式1：推荐（动态初始化）
    init_completion(&init_done);

    // 方式2：如果在函数内声明，可以用宏（栈上）
    // DECLARE_COMPLETION_ONSTACK(init_done);   // 栈上专用，函数结束自动销毁

    worker_task = kthread_run(worker_func, NULL, "init-worker");

    // 主线程等待 worker 完成初始化（最常见用法）
    printk(KERN_INFO "main: waiting for initialization...\n");

    // 阻塞等待，最多等 10 秒（可选超时版本）
    if (!wait_for_completion_timeout(&init_done, 10 * HZ)) {
        printk(KERN_WARNING "init timeout!\n");
        // 可选择 kthread_stop(worker_task);
    } else {
        printk(KERN_INFO "init successfully completed.\n");
    }

    return 0;
}

static void __exit my_mod_exit(void)
{
    if (worker_task)
        kthread_stop(worker_task);
}
```

### 常用 API 一览表

|API|作用|参数说明|是否阻塞|典型返回值意义|
|---|---|---|---|---|
|`init_completion()`|完整初始化|&completion|—|—|
|`reinit_completion()`|复用时重置 done=0|&completion|—|—|
|`complete()`|唤醒**一个**等待者|&completion|—|—|
|`complete_all()`|唤醒**所有**等待者|&completion|—|—|
|`wait_for_completion()`|无限期等待完成|&completion|是|—|
|`wait_for_completion_timeout()`|带超时等待（jiffies）|&c, timeout|是|剩余时间（0=超时）|
|`wait_for_completion_interruptible()`|可被信号打断等待|&c|是|0=完成，-ERESTARTSYS=信号|
|`try_wait_for_completion()`|非阻塞尝试（检查是否已完成）|&c|否|true=已完成并消耗，false=未完成|

### 常见错误与正确写法对比

```c
// 错误：复用时用了 init_completion() → 可能破坏等待队列
init_completion(&done);           // ← 第二次使用不能这样写

// 正确：复用时用 reinit_completion
reinit_completion(&done);         // 推荐

// 错误：在中断中调用 wait_for_completion() → 死锁
wait_for_completion(&done);       // 禁止在 atomic 上下文

// 正确：在中断中只用 complete()，等待者在进程上下文
complete(&done);                  // OK

// 错误：忘记 reinit_completion 就重复使用 → done 值累加，可能永不等待
complete(&done);
complete(&done);                  // done=2，下次 wait 直接通过（bug）
```

### 现代推荐使用模式总结

1. **一次性使用**（模块加载时等） → `DECLARE_COMPLETION()` 或 `init_completion()`
2. **重复使用**（如每个请求/每个中断都用同一个 completion） → `reinit_completion()` + `wait_for_completion_timeout()` / `complete()`
3. **多等待者场景** → `complete_all()` + `reinit_completion()`
4. **可打断等待**（如模块卸载时） → `wait_for_completion_interruptible()`

