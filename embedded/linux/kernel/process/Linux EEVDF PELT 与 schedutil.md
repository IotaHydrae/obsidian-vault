# Linux EEVDF、PELT 与 schedutil

> EEVDF 决定公平调度类中哪个合适的实体先运行；PELT 估计随时间变化的负载；schedutil 可使用利用率估计参与频率选择。

## TL;DR

- EEVDF 的虚拟期限是公平调度内部量，不是用户请求的真实时间 deadline 或实时保证。
- PELT 用衰减统计描述 running / runnable，周期负载、迁移和 CPU 能力都会影响观察结果。
- schedutil 根据调度器利用率等信号请求频率；policy、OPP、热约束和硬件仍决定最终结果。
- 线程延迟应拆成等待条件、可运行后排队、实际执行三段；调 governor 不能消除锁和 I/O 等待。

范围：Linux 6.18.62 LTS 的公平调度类、PELT 与 schedutil；实时和 deadline 调度类有自己的语义。

## EEVDF：公平与响应

公平调度追踪调度实体获得的服务。实体可能是单个任务，也可能代表组调度中的一个实体；任务权重和运行队列层级都会影响最终 CPU 份额。

EEVDF 使用虚拟运行时间和 lag 判断实体是否有资格执行，并在合适的实体中选择较早的虚拟期限。Linux 文档将正 lag 描述为仍欠该实体服务，负 lag 描述为已经获得较多服务。虚拟时间、虚拟期限不是墙钟时间。

`nice` 值影响公平调度权重，不直接等于固定毫秒时间片；任务被选中仍要受到亲和性、CPU 竞争、cgroup、抢占和更高调度类的影响。`SCHED_FIFO` / `SCHED_RR`、`SCHED_DEADLINE` 不应套用公平类内部选择规则。

Linux 6.18.62 可从 `kernel/sched/fair.c` 的 `pick_eevdf()` 和 `__pick_next_task_fair()` 查起，完整路径还包括调度实体层级和选择条件；任务状态及 tracepoint 使用见 [[Linux 进程调度与等待]]。

## PELT：running 与 runnable

PELT 使用指数衰减统计调度实体、组和 CPU 运行队列的负载。6.18 文档中的周期约为 1024 μs，经过 32 个周期权重衰减至一半；因此周期性任务睡眠后估计会下降，重新运行时也不会用单次采样代表长期负载。

| 指标 | 描述 | 对排查的启发 |
| --- | --- | --- |
| running | 实际在 CPU 上运行的时间相关统计 | 低值可能是任务睡眠、阻塞或竞争 |
| runnable | 已可运行并处于队列中的时间相关统计 | runnable 高而 running 低时应检查 CPU 竞争 |
| util_est | 对周期性任务的利用率估计 | 用于减轻频率爬升滞后，不是硬件利用率的直接测量 |
| uclamp | 对适用任务/运行队列利用率信号的约束 | 不等同于调度策略优先级，也不消除 I/O 等待 |

频率和 CPU 容量不变性会影响统计归一化；不同架构能否提供所需信息取决于实现。不要把 PELT 数值简单等同于一次 `top` CPU 百分比。

## schedutil 与 CPUFreq

调度器在唤醒、迁移和时间推进等路径更新利用率，schedutil 可根据运行队列利用率与相关约束估计频率，再通过 CPUFreq 驱动请求目标。实际结果还受共享 policy、平台 OPP、硬件自主控制、rate limit 和 thermal 限制。

```text
任务可运行 / 运行 → PELT / util_est / 约束 → schedutil
→ CPUFreq policy / driver → 硬件实际频率
```

这是概念数据流，不表示每次事件都同步发生一次硬件频率切换。CPUFreq、OPP、thermal 和 idle 的完整边界见 [[Linux CPUFreq OPP Thermal 与 CPUIdle]]。

## 调试路径

1. 先记录目标线程 TID、调度策略、CPU affinity 和 cgroup 限制；确认线程在等待还是已 runnable。
2. 使用可用的 `sched_wakeup`、`sched_switch` 及 `power:cpu_frequency` 事件，关联一次负载的唤醒、运行与频率变化。
3. 若 runnable 但长时间未运行，检查同 CPU 竞争、高优先级工作和配额；若线程在等待，继续追锁、I/O 和事件源。
4. 将频率和温度限制与 CPU 时间线对齐；采样频率、trace clock 和丢事件会影响结论。

## 参考

- Linux 6.18.62：[EEVDF](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/scheduler/sched-eevdf.rst)、[schedutil / PELT](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/scheduler/schedutil.rst)、[`fair.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/sched/fair.c)、[`cpufreq_schedutil.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/sched/cpufreq_schedutil.c)。
- 蜗窝科技：[PELT](https://www.wowotech.net/process_management/pelt.html)、[schedutil](https://www.wowotech.net/process_management/schedutil_governor.html)。
