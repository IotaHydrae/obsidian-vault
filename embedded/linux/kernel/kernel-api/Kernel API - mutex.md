# Linux Mutex：可睡眠路径的互斥

> Mutex 串行化任务上下文中的共享状态；加锁可能睡眠，不能用于硬中断，也不能代替对象引用或设备退出协议。

## TL;DR

- 动态对象用 `mutex_init()`，静态锁用 `DEFINE_MUTEX()`；不要依赖内存清零初始化锁。
- `mutex_lock_interruptible()` 成功为 0，被信号中断返回负错误；失败后不能解锁。
- `mutex_trylock()` 成功为 1、未取得锁为 0，返回约定与 interruptible 不同。
- 同一把锁不能递归获取，必须由持有者释放；所有共享状态访问遵守同一锁协议。

源码基准：Linux 6.18 LTS / 6.18.62。

## 为什么能保护睡眠操作

Mutex 无竞争时通过原子操作取得所有权；竞争时可能先乐观自旋，再进入等待队列睡眠。它适合保护可睡眠的配置流程，例如 sysfs、ioctl、I²C 寄存器操作之间的互斥。

可以持 mutex 执行睡眠操作，但必须检查锁依赖：若等待的 IRQ 线程、work 或回调也要取得这把锁，持锁等待它结束会死锁。多个锁之间要固定获取顺序。

## 示例：运行期间拒绝修改配置

```c
#include <linux/errno.h>
#include <linux/mutex.h>

struct sampler_config {
    struct mutex lock;
    bool running;
    unsigned int period_ms;
};

static void sampler_config_init(struct sampler_config *config)
{
    mutex_init(&config->lock);
    config->running = false;
    config->period_ms = 100;
}

static int sampler_set_period(struct sampler_config *config,
                              unsigned int period_ms)
{
    int ret;

    if (!period_ms)
        return -EINVAL;

    ret = mutex_lock_interruptible(&config->lock);
    if (ret)
        return ret;

    if (config->running) {
        ret = -EBUSY;
        goto out_unlock;
    }

    config->period_ms = period_ms;
    ret = 0;

out_unlock:
    mutex_unlock(&config->lock);
    return ret;
}
```

示例要求启停路径和读取配置的路径也持 `config->lock`，且调用者已保证 `config` 存活。初始 100 ms 仅是示例默认值，不是硬件要求。上层若需转换信号错误，应遵守相应系统调用接口约定。

## 边界与选择

- 硬中断、softirq、关中断或持自旋锁时不能调用会睡眠的加锁路径。即使预计“没有竞争”也不能据此放宽。
- `mutex_is_locked()` 只是状态观察，不能证明当前任务持锁；helper 的持锁前提用 [lockdep_assert_held](Kernel%20API%20-%20lockdep_assert_held.md) 表达。
- 共享状态也被硬中断访问时，评估短临界区的 [spin_lock_irqsave](Kernel%20API%20-%20spin_lock_irqsave.md)，或把可睡眠处理放入 IRQ 线程。
- `mutex_destroy()` 不会等待持有者或使用者退出。释放包含锁的对象前，先关闭入口并等待全部使用者结束。

## 源码与参考

- [mutex.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/mutex.h)：初始化、锁类型与 API。
- [mutex.c](https://github.com/gregkh/linux/blob/v6.18.62/kernel/locking/mutex.c)：获取、竞争、信号退出和解锁实现。
