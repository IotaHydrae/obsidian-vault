# Linux 时间子系统

> 读取当前时间、安排未来事件和执行超时回调分别由不同机制承担；时钟分辨率不等于任务实际唤醒延迟。

## TL;DR

- clocksource 提供计数，clockevent 安排事件；二者解决不同问题。
- 测间隔优先选合适的单调时钟；是否计入 suspend 由所选 clock 决定。
- 定时到期不等于线程立刻执行；普通内核 timer 的回调不可睡眠。

适用范围：Linux 6.18 LTS。

## 关键概念

| 概念 | 作用 | 常见误解 |
| --- | --- | --- |
| clocksource | 提供可读取的递增计数，供 timekeeping 换算时间 | 不是负责触发定时中断的通用名称 |
| clockevent | 在指定时间产生事件，用于驱动内核定时 | 不等于用户 API 的 clock ID |
| `jiffies` / 普通 timer | 管理适合该精度的超时 | 单位依赖 `HZ`，回调可能晚于期限 |
| hrtimer | 支持更细粒度的到期管理 | 不是实时执行保证，仍受硬件和调度影响 |
| RTC | 维护平台的实时时钟，常用于断电保持或唤醒 | 不等于系统所有单调时钟 |

## 时钟源与事件如何配合

clocksource 的递增计数配合频率换算，供 timekeeping 维护内核时间；clockevent 根据计划期限触发事件，推动到期任务处理。一个硬件定时器可能同时提供多种能力，但软件中职责仍应区分。

普通 timer 常用 `jiffies` 表示期限，适合超时和周期维护；hrtimer 使用更细的时间表示并依赖系统配置。实际回调时间仍受中断屏蔽、CPU 负载和执行上下文影响。tickless 配置可以减少不必要的周期 tick，不意味着系统没有计时或定时中断。

## 选择时间基准

| 需求 | 用户空间 clock / 内核常用入口 | 关键边界 |
| --- | --- | --- |
| 计算运行期间的间隔 | `CLOCK_MONOTONIC` / `ktime_get()` | 不计系统 suspend 时间；可能受频率校正 |
| 计算包含 suspend 的间隔 | `CLOCK_BOOTTIME` / `ktime_get_boottime()` | 计入系统 suspend 时间 |
| 保存日历时间 | `CLOCK_REALTIME` / `ktime_get_real()` | 可因时间设置而跳变，不宜直接用于超时差值 |
| 观察未经 NTP 频率校正的时基 | `CLOCK_MONOTONIC_RAW` / `ktime_get_raw()` | 同样需要考虑具体测量场景 |

周期任务若每次在处理完成后再等待固定间隔，处理耗时会使周期漂移。需要固定周期时可使用合适时钟的绝对期限，并明确超期后跳过还是补处理；实际执行仍受调度影响。

## 测量一次超时延迟

把时间线拆成三个点：请求设定的到期期限、定时器回调或唤醒发生的时刻、等待线程真正运行的时刻。前一段受 clockevent、中断及回调处理影响，后一段还包含运行队列等待；两段相加才接近应用可观察到的延迟。

| 观测方法 | 有用信息 | 限制 |
| --- | --- | --- |
| 应用记录单调时间 | 实际端到端耗时 | 不能单独区分定时器与调度延迟 |
| timer / hrtimer tracepoint | 定时器到期和回调情况 | 是否存在以及字段取决于内核配置 |
| `sched_wakeup` / `sched_switch` | 线程从唤醒到得到 CPU 的等待 | 已处于可运行状态的线程不一定有新的唤醒事件 |

Linux 6.18 的实现可从 `kernel/time/timekeeping.c`、`kernel/time/hrtimer.c` 的 `hrtimer_interrupt()` 和 `kernel/time/timer.c` 查起；实际触发路径取决于时钟设备及配置。

## 选择和排查

- 日历时间用 realtime，并考虑系统时间被调整的影响。
- 在可睡眠上下文等待，用合适的 sleep / timeout API；不要为了等待硬件完成持续 busy-wait。
- 普通内核 timer 的启动、不可睡眠回调与安全停止见 [[Kernel API - timer_setup]]；延时等待见 [[Kernel API - schedule_timeout]]。
- 高精度定时器的期限、周期推进及退出见 [[Linux hrtimer 高精度定时器]]。
- 排查唤醒延迟时区分“期限已经到达”和“线程真正得到 CPU”；可结合 timer 与 sched tracepoint。

## 参考

- 蜗窝科技：[时间子系统架构](https://www.wowotech.net/timer_subsystem/time-subsyste-architecture.html)、[timekeeping](https://www.wowotech.net/timer_subsystem/timekeeping.html)、[Linux 时钟](https://www.wowotech.net/timer_subsystem/clock-id-in-linux.html)。
- Linux 6.18.62：[timekeeping](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/timekeeping.rst)、[高精度定时器](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/timers/hrtimers.rst)。
- Linux 6.18.62：[`timekeeping.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/time/timekeeping.c)、[`hrtimer.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/time/hrtimer.c)。
