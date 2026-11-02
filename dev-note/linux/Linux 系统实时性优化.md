# Linux 实时性优化与延迟测量

> 先在目标负载下测量调度延迟，再按瓶颈评估 PREEMPT_RT、实时调度、CPU/IRQ 亲和性和内存锁定；没有适用于所有机器的延迟保证。

## TL;DR

- 检查内核抢占配置，并用 `cyclictest` 在有代表性的负载下记录最大延迟。
- 按需测试 `SCHED_FIFO` / `SCHED_RR`、CPU 绑定、IRQ 亲和性和内存锁定；逐项修改、重复测量。
- 实时优先级、CPU 隔离、禁用省电或 swap 都可能影响系统可用性，必须考虑恢复方式。
- 延迟指标必须同时给出硬件、负载、测试参数和运行时长，不能脱离条件比较。

## 检查与测量

```bash
uname -a
test ! -r /sys/kernel/realtime || cat /sys/kernel/realtime
grep PREEMPT /boot/config-$(uname -r)
ps -eo pid,cls,rtprio,comm
cat /proc/cmdline
cat /proc/interrupts
```

内核配置文件和 `/sys/kernel/realtime` 是否存在取决于发行版及内核配置；RT 内核配置可检查 `CONFIG_PREEMPT_RT=y`。

在 Debian / Ubuntu 环境中，可使用以下工具测量：

```bash
sudo apt install rt-tests
sudo cyclictest -p99 -m -n -i1000
```

高优先级测试会影响其他任务；先确认选项与本机版本兼容，在可恢复的测试环境运行。记录内核、CPU、负载、运行时间、测试参数和最大延迟；不同条件下的数值不可直接当作保证。

## 可调整项

| 方向 | 例子 | 边界 |
| --- | --- | --- |
| 调度 | `chrt -f 99 ./app` | 需要权限；高优先级线程若不让出 CPU，可能饿死其他任务 |
| CPU 亲和性 | `taskset -c 1 ./app` | 绑定不等于隔离；CPU 必须存在且可用 |
| 启动参数 | `isolcpus=1 nohz_full=1 rcu_nocbs=1` | 依赖内核配置和其他任务安排，需重启验证 |
| IRQ 亲和性 | 检查 `/proc/interrupts` 及 `/proc/irq/` | IRQ 编号、CPU 拓扑和中断控制方式依设备而异 |
| CPU 频率 | `cpupower frequency-info`，按需选择 `performance` governor | 可用 governor、功耗和散热影响依平台而异 |
| 内存 | 检查 `mlockall()` 返回值；评估 swap | 受权限和资源限制约束；锁页不等于消除所有缺页或延迟 |
| 空闲状态 | 评估 CPU 空闲状态及相关启动参数 | 参数与架构相关，降低休眠深度可能增加功耗 |

例如检查和调整 governor：

```bash
cpupower frequency-info
sudo cpupower frequency-set -g performance
```

执行前确认工具、目标 CPU 和现有电源策略；执行后复测，必要时恢复原配置。

## 周期任务与驱动

- 周期任务可考虑 `CLOCK_MONOTONIC` 和 `clock_nanosleep(..., TIMER_ABSTIME, ...)`，避免相对睡眠导致周期误差逐次累积；仍需测量实际唤醒延迟。
- 在允许睡眠的上下文中，按时序要求评估 `usleep_range()` 等睡眠方式，不要机械地在原子上下文中替换 `mdelay()`。
- 缩短持锁和中断处理路径；可延后的工作按实际约束考虑 threaded IRQ 或 workqueue。
- 大量日志可能影响测量；调整日志或关闭服务前记录原设置。

## 延迟来源

可按需使用 `ftrace`、`trace-cmd`、KernelShark 分析调度和中断路径，例如：

```bash
sudo trace-cmd record -e sched
```

跟踪本身也有开销。测量前确认 tracefs/debugfs、工具版本、权限和追踪范围，测量后恢复配置。
