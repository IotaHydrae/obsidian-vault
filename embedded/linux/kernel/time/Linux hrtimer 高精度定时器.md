# Linux hrtimer：高精度定时与停止

> hrtimer 使用 `ktime_t` 表达期限，并通过时钟事件机制安排回调；期限分辨率不等于回调或线程实际执行精度。

## TL;DR

- `hrtimer_setup()` 初始化并指定回调与时钟，`hrtimer_start()` 用绝对或相对期限启动。
- 回调应按不可睡眠的定时器上下文设计；需要访问可睡眠总线时交给普通 workqueue。
- 周期回调可用 `hrtimer_forward_now()` 推进期限并返回 `HRTIMER_RESTART`；超期次数不等于实际处理次数。
- 停止前必须阻止外部重新启动；`hrtimer_cancel()` 等待运行中的回调，但不永久禁止其他调用者重新 arm。

范围：Linux 6.18.62 LTS；具体分辨率和延迟取决于时钟设备、配置、IRQ 与调度。

## 接口和时间基准

| 接口 / 概念 | 用途与边界 |
| --- | --- |
| `hrtimer_setup()` | 初始化 timer、回调、时钟和模式；不自动启动 |
| `hrtimer_start()` | 指定 `ktime_t` 期限及 `HRTIMER_MODE_REL` / `ABS` 等模式 |
| `HRTIMER_NORESTART` | 回调返回后不按该返回值继续周期运行 |
| `HRTIMER_RESTART` | 按已更新的到期时间继续运行；驱动必须正确推进期限 |
| `hrtimer_forward_now()` | 从原到期点按周期推进到当前时刻之后，返回推进的次数；它本身不重新入队 |
| `hrtimer_cancel()` | 等待回调并移除 timer；不能与回调所需锁形成等待环 |

`CLOCK_MONOTONIC` 不计入系统 suspend 的时间；`CLOCK_BOOTTIME` 会计入。`CLOCK_REALTIME` 可能因时间设置而跳变，选择时钟要与业务的超时语义一致，见 [[Linux 时间子系统]]。

## 示例：周期计数

示例用于已分配的对象，在允许睡眠的初始化/退出路径调用；外部调用者必须串行化 start/stop，退出前停止所有其他重新启动者。回调只计数，不访问硬件：

```c
#include <linux/atomic.h>
#include <linux/container_of.h>
#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

struct periodic_sample {
    struct hrtimer timer;
    ktime_t period;
    atomic_t callbacks;
};

static enum hrtimer_restart periodic_fn(struct hrtimer *timer)
{
    struct periodic_sample *sample;

    sample = container_of(timer, struct periodic_sample, timer);
    atomic_inc(&sample->callbacks);
    hrtimer_forward_now(timer, sample->period);
    return HRTIMER_RESTART;
}

static int periodic_start(struct periodic_sample *sample, unsigned int period_ms)
{
    if (!period_ms)
        return -EINVAL;

    sample->period = ms_to_ktime(period_ms);
    atomic_set(&sample->callbacks, 0);
    hrtimer_setup(&sample->timer, periodic_fn, CLOCK_MONOTONIC,
                  HRTIMER_MODE_REL);
    hrtimer_start(&sample->timer, sample->period, HRTIMER_MODE_REL);
    return 0;
}

static void periodic_stop(struct periodic_sample *sample)
{
    hrtimer_cancel(&sample->timer);
}
```

`callbacks` 记录回调次数，不代表硬件事件数或按期完成的周期数；回调可能延迟并跨过多个周期。停止后才能释放 `sample`；不要对活动 timer 重复调用初始化。周期任务若要保存每个样本或处理过期周期，需要额外的队列和业务策略。

例如周期 `10 ms`、原到期点 `10 ms`，回调实际在 `37 ms` 运行：`hrtimer_forward_now()` 将到期点推进 **3 次**到 `40 ms`，但只执行了**一次**回调。若每次改成 `now + period`，下一次是 `47 ms`，相位会随执行延迟漂移。回调若依靠同一个 `work_struct` 处理数据，`queue_work()` 在 work 已排队时会返回 false；要逐个保留样本，应单独管理计数或队列。

## 排查定时延迟

1. 记录计划期限、定时器回调/唤醒和线程被调度的时间；不要把整个差值都归因于 hrtimer。
2. 结合 [[ftrace]] 的可用 timer、IRQ 和 sched 事件，检查中断屏蔽、softirq、CPU 竞争与目标线程调度。
3. 检查 clocksource / clockevent、高精度定时器配置、CPU idle 和时钟选择；系统时间更改不能解释单调时钟的所有问题。
4. 卸载时先阻止新的 start，再取消 timer、同步被回调排队的 work，最后释放数据；不能持有 work 或 timer 回调需要的锁等待。

## 参考

- Linux 6.18.62：[`hrtimer.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/hrtimer.h)、[`hrtimer.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/time/hrtimer.c)、[高精度定时器](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/timers/hrtimers.rst)、[时间读取](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/timekeeping.rst)。
- 蜗窝科技：[clockevent](https://www.wowotech.net/timer_subsystem/clock-event.html)、[timekeeping](https://www.wowotech.net/timer_subsystem/timekeeping.html)。
