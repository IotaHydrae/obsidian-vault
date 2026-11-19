# Linux 时间子系统

> 读取当前时间、安排未来事件和执行超时回调分别由不同机制承担；时钟分辨率不等于任务实际唤醒延迟。

## 关键概念

| 概念 | 作用 | 常见误解 |
| --- | --- | --- |
| clocksource | 提供可读取的递增计数，供 timekeeping 换算时间 | 不是负责触发定时中断的通用名称 |
| clockevent | 在指定时间产生事件，用于驱动内核定时 | 不等于用户 API 的 clock ID |
| `jiffies` / 普通 timer | 管理适合该精度的超时 | 单位依赖 `HZ`，回调可能晚于期限 |
| hrtimer | 支持更细粒度的到期管理 | 不是实时执行保证，仍受硬件和调度影响 |
| RTC | 维护平台的实时时钟，常用于断电保持或唤醒 | 不等于系统所有单调时钟 |

## 选择和排查

- 测量间隔通常选单调时钟；日历时间用 realtime，并考虑系统时间被调整的影响。是否计入 suspend 要按所选 clock 的定义判断。
- 在可睡眠上下文等待，用合适的 sleep / timeout API；不要为了等待硬件完成持续 busy-wait。
- 普通内核 timer 的启动、不可睡眠回调与安全停止见 [[Kernel API - timer_setup]]；延时等待见 [[Kernel API - schedule_timeout]]。
- 排查唤醒延迟时区分“期限已经到达”和“线程真正得到 CPU”；可结合 timer 与 sched tracepoint。

## 参考

- 蜗窝科技：[时间子系统架构](https://www.wowotech.net/timer_subsystem/time-subsyste-architecture.html)、[timekeeping](https://www.wowotech.net/timer_subsystem/timekeeping.html)、[Linux 时钟](https://www.wowotech.net/timer_subsystem/clock-id-in-linux.html)。
- Linux 6.18.62：[timekeeping](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/timekeeping.rst)、[高精度定时器](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/timers/hrtimers.rst)。
