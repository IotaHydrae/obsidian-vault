# Linux 进程调度与等待

> 延迟问题先区分线程没有运行的原因：未就绪、等待资源、处于可运行队列，还是正在 CPU 上执行。

## TL;DR

- 线程可运行不等于立即获得 CPU；CPU 亲和性、竞争、优先级、限额和调度策略都可能影响等待时间。
- 普通、实时和 deadline 任务使用不同策略；实时优先级不会让阻塞的线程自动变成可运行。
- 睡眠等待条件时必须配合正确的状态同步；唤醒只提示重新检查条件，不代表设备操作一定成功。
- Linux 6.18 的公平调度路径使用 EEVDF；任务权重、虚拟运行时间与期限参与调度决策。

## 调度观察

| 现象 | 优先区分 |
| --- | --- |
| 线程长期没有运行 | 是否在等待锁、I/O、条件或定时器；还是已唤醒但排队 |
| CPU 高但工作不推进 | 忙等、无效重试、锁竞争、热点函数或中断负载 |
| 唤醒后延迟大 | 任务状态、亲和性、CPU 竞争、RT 任务和 cgroup 配额 |
| 高优先级线程仍卡住 | 它依赖的锁、工作线程、设备或总线是否能继续执行 |

在适用平台上先用 `ps -eLo pid,tid,cls,rtprio,psr,stat,comm` 查看策略与状态，再根据问题选择 `top -H`、`pidstat -t` 或 `perf sched`。状态是瞬时观察，采样和工具权限会影响结论。

等待一次事件可用 completion；等待持续变化的条件可用 wait queue。生产者应先安全发布条件再唤醒，消费者在同步保护下反复检查；超时不等于取消硬件或后台工作。详见 [[Kernel API - init_completion]]、[[Kernel API - wait_queue]]。

## 参考

- 蜗窝科技：[CFS 基本原理](https://www.wowotech.net/process_management/447.html)、[Deadline 调度器](https://www.wowotech.net/process_management/deadline-scheduler-1.html)、[进程管理栏目](https://www.wowotech.net/sort/process_management/)。
- Linux 6.18.62：[EEVDF](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/scheduler/sched-eevdf.rst)、[调度策略概览](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/scheduler/sched-design-CFS.rst)。
