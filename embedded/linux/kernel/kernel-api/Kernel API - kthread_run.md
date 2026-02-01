
kthread_run 是 Linux 内核中最常用的创建并立即运行内核线程的宏（helper macro），定义在`<linux/kthread.h>`中。

### 函数原型（实际是宏）

```c
struct task_struct *kthread_run(int (*threadfn)(void *data), void *data,
                                const char namefmt[], ...);
```

- 它等价于：kthread_create() → wake_up_process() 的组合
- 创建成功后**立即唤醒**线程开始执行（不像 kthread_create 需要手动 wake）

### 参数说明

|参数|含义|必填|常见用法示例|
|---|---|---|---|
|threadfn|线程执行函数，类型 `int (*)(void *)`|是|`my_thread_func`|
|data|传递给线程函数的参数（void*）|是|`&my_struct` 或 `NULL` 或 `(void*)123`|
|namefmt|线程名称格式（类似 printf）|是|`"mydev-%d"`、`"kworker/%u"`|
|...|namefmt 的可变参数|视情况|`minor`、`cpu` 等|
返回值：

- 成功：返回创建的 struct task_struct *
- 失败：返回 ERR_PTR(-ENOMEM) 或其他错误指针

### 典型正确用法示例

```c
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/delay.h>

static struct task_struct *my_kthread;
static int should_stop = 0;

static int my_thread_func(void *arg)
{
    int i = 0;
    char *str = (char *)arg;           // 示例：接收字符串参数

    printk(KERN_INFO "kthread started: %s\n", str);

    while (!kthread_should_stop()) {   // 推荐的退出判断方式
        printk(KERN_INFO "running... count=%d\n", i++);
        msleep(1000);                  // 休眠1秒（可被信号打断）

        if (should_stop)               // 或者用自定义标志
            break;
    }

    printk(KERN_INFO "kthread exiting...\n");
    return 0;
}

static int __init my_init(void)
{
    printk(KERN_INFO "module init\n");

    // 最常用写法
    my_kthread = kthread_run(my_thread_func, "Hello from module", "mymod-thread");

    // 带格式化名称的写法
    // my_kthread = kthread_run(my_thread_func, "param", "mydev-%d", 0);

    if (IS_ERR(my_kthread)) {
        printk(KERN_ERR "create kthread failed: %ld\n", PTR_ERR(my_kthread));
        return PTR_ERR(my_kthread);
    }

    return 0;
}

static void __exit my_exit(void)
{
    printk(KERN_INFO "module exit\n");

    if (my_kthread) {
        should_stop = 1;               // 可选：配合自定义标志
        // 最推荐的停止方式
        kthread_stop(my_kthread);      // 会发送信号并等待线程退出
        my_kthread = NULL;
    }
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
```

### 线程退出判断的几种常用写法（优先级从高到低）

1. **最推荐**：`while (!kthread_should_stop())`
    - 内核会通过 `kthread_stop()` 发送信号
    - 安全、规范
2. `while (!signal_pending(current))`
    - 比较原始的方式
3. 自定义标志位（如上面的 `should_stop`）
    - 配合 `smp_mb()` 或 atomic 变量使用

### 快速对照表

| 需求          | 推荐用法                                            |
| ----------- | ----------------------------------------------- |
| 创建并立即运行     | `kthread_run(...)`                              |
| 只创建、不运行     | `kthread_create(...)` + `wake_up_process()`     |
| 停止线程        | `kthread_stop(task)`                            |
| 检查是否需要退出    | `kthread_should_stop()`                         |
| 线程内休眠（可被打断） | `msleep()`、`schedule_timeout()`                 |
| 线程内不可打断休眠   | `ssleep()`、`schedule_timeout_uninterruptible()` |
