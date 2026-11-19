# Linux 进程调度与等待

> 延迟问题先区分线程没有运行的原因：未就绪、等待资源、处于可运行队列，还是正在 CPU 上执行。

## TL;DR

- 线程可运行不等于立即获得 CPU；CPU 亲和性、竞争、优先级、限额和调度策略都可能影响等待时间。
- 普通、实时和 deadline 任务使用不同策略；实时优先级不会让阻塞的线程自动变成可运行。
- 睡眠等待条件时必须配合正确的状态同步；唤醒只提示重新检查条件，不代表设备操作一定成功。
- Linux 6.18 的公平调度路径使用 EEVDF；任务权重、虚拟运行时间与期限参与调度决策。

适用范围：Linux 6.18 LTS。

## 状态与调度路径

```text
等待条件或 I/O → 条件发生 / 完成 → 线程被唤醒并变为可运行
→ 排入目标 CPU 的运行队列 → 调度器选择 → 执行
```

这条路径中，**等待事件的时间**与**已经可运行但等待 CPU 的时间**是两段不同的延迟。CPU 空闲也不能证明特定线程能运行：它可能受 CPU affinity、cpuset、优先级或资源限制约束。

| 观察到的状态 | 需要继续判断 |
| --- | --- |
| `R` | 正在执行或可运行；需继续区分运行时间和排队时间 |
| `S` | 可中断睡眠；寻找等待条件、锁、I/O 或定时器 |
| `D` | 不可中断睡眠；结合内核栈及等待对象定位，不应只凭状态认定死锁 |
| `T` / `Z` | 分别与停止 / 跟踪、已退出但尚未回收相关 |

状态是采样值；查看线程而不仅是进程汇总。`wchan`、内核栈和 tracepoint 的可见性依权限和配置而定。

## 公平、实时与期限

公平调度考虑任务权重与已经获得的服务。Linux 6.18 在公平调度类中使用 EEVDF：从符合条件的实体中选择虚拟期限合适的实体；nice 值、组调度和 CPU 竞争仍影响实际份额。不要把调度器的内部虚拟时间当成墙钟时间或单次执行时长。

`SCHED_FIFO` / `SCHED_RR` 面向实时任务，`SCHED_DEADLINE` 用运行时间、期限和周期表达要求；两者都需要考虑系统配置和可运行条件。提高优先级不能消除 I/O、锁、缺页、IRQ 处理或硬件延迟，长时间占用 CPU 的实时任务还可能影响系统其他工作。

负载均衡决定任务何时迁移到其他 CPU；PELT 等统计用于描述随时间变化的负载。负载平均值、CPU 利用率和单个任务延迟不是同一指标。频率策略可能参考利用率，但温度、功耗和频率限制也会改变实际执行时间。

### PELT 与频率选择

PELT 为任务、调度组和运行队列维护随时间衰减的负载 / 利用率统计。Linux 6.18 的 `schedutil` 文档描述其指数加权统计周期约为 1024 μs，最近约 32 个周期的权重合计占一半；因此短时间突发不会立即等同于长期满载，负载消失后的统计也不会瞬间归零。

`schedutil` 可以使用调度器利用率信息参与频率决策；实际频率还受 policy、硬件、共享策略域和 thermal 等约束。排查“任务运行慢”时，应同时观察任务是否排队、统计随时间的变化和实际频率，而不是只调整 governor。

## 调度观察

| 现象 | 优先区分 |
| --- | --- |
| 线程长期没有运行 | 是否在等待锁、I/O、条件或定时器；还是已唤醒但排队 |
| CPU 高但工作不推进 | 忙等、无效重试、锁竞争、热点函数或中断负载 |
| 唤醒后延迟大 | 任务状态、亲和性、CPU 竞争、RT 任务和 cgroup 配额 |
| 高优先级线程仍卡住 | 它依赖的锁、工作线程、设备或总线是否能继续执行 |

在适用平台上先用 `ps -eLo pid,tid,cls,rtprio,psr,stat,comm` 查看策略与状态，再根据问题选择 `top -H`、`pidstat -t` 或 `perf sched`。状态是瞬时观察，采样和工具权限会影响结论。

等待一次事件可用 completion；等待持续变化的条件可用 wait queue。生产者应先安全发布条件再唤醒，消费者在同步保护下反复检查；超时不等于取消硬件或后台工作。详见 [[Kernel API - init_completion]]、[[Kernel API - wait_queue]]。

## 验证延迟来自哪里

1. 用 `ps -eLo pid,tid,cls,rtprio,psr,stat,comm` 查看线程、策略、CPU 和状态；再用 `top -H` / `pidstat -t` 观察持续变化。
2. 记录 `sched:sched_wakeup` 和 `sched:sched_switch`：从一次有效唤醒到被调度执行的间隔可用于分析运行队列等待，但须处理重复唤醒与线程迁移。
3. 若线程仍在等待，追踪对应锁、I/O、IRQ 或工作线程；若已运行，再用 `perf` 等寻找热点。
4. 核对 cgroup CPU 配额、cpuset 和 affinity；容器内看到的 CPU 数量不代表一定拥有相同计算份额。

跟踪需要内核配置和权限，也会改变系统负载。对实时问题同时记录硬件触发、IRQ、唤醒与用户线程运行时间，避免把单个 tracepoint 当作端到端延迟。

## Linux 6.18 源码入口

| 位置 | 关键入口 | 作用 |
| --- | --- | --- |
| `kernel/sched/core.c` | `try_to_wake_up()`、`ttwu_queue()` | 唤醒状态检查与可运行任务入队协调 |
| `kernel/sched/core.c` | `__schedule()`、`pick_next_task()` | 调度时选择下一个可运行任务 |
| `kernel/sched/fair.c` | `__pick_next_task_fair()`、`pick_eevdf()` | 公平调度类的实体选择 |
| `kernel/sched/core.c` | `context_switch()` | 切换任务执行上下文 |

调用关系受调度类、CPU 迁移和内核配置影响；这些符号是追踪入口，不能把它们当作每次唤醒都同步经过的唯一调用链。

## 参考

- 蜗窝科技：[CFS 基本原理](https://www.wowotech.net/process_management/447.html)、[Deadline 调度器](https://www.wowotech.net/process_management/deadline-scheduler-1.html)、[进程管理栏目](https://www.wowotech.net/sort/process_management/)。
- Linux 6.18.62：[EEVDF](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/scheduler/sched-eevdf.rst)、[调度策略概览](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/scheduler/sched-design-CFS.rst)。
- Linux 6.18.62：[PELT 与 schedutil](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/scheduler/schedutil.rst)。
- Linux 6.18.62：[`kernel/sched/core.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/sched/core.c)、[`kernel/sched/fair.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/sched/fair.c)。
