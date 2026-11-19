# Linux ftrace：事件、函数与延迟追踪

> ftrace 通过 tracefs 记录内核事件和函数执行；先用事件定位时间边界，再缩小函数范围，避免把追踪开销误当成原始延迟。

## TL;DR

- IRQ 入口/退出、线程唤醒和切换可用事件联合分析；`irq_handler_entry/exit` 主要观察 primary handler，不直接表示 IRQ 线程耗时。
- 函数调用关系使用 `function` / `function_graph`，先设置过滤，避免全系统追踪产生大量数据。
- `trace` 是非消费式读取，采集进行中可能读到不一致内容；`trace_pipe` 是消费式流。缓冲区覆盖、跨 CPU 时钟和过滤条件会影响结论。
- 使用独立 trace instance 或 `trace-cmd` 管理会话；生产系统需控制时长、权限和性能影响。

范围：Linux 6.18 LTS；可用 tracer、tracepoint 和 tracefs 路径由内核配置决定。

## 事件与函数追踪

| 需求 | 入口 | 注意 |
| --- | --- | --- |
| 查看 IRQ、调度、定时器事件 | event tracing / tracepoint | 事件覆盖的函数边界需按事件定义理解 |
| 查看函数是否执行 | `function` tracer | 编译、架构和函数是否可追踪影响覆盖 |
| 查看调用嵌套与耗时 | `function_graph` | 跟踪范围过大或递归调用会显著增加开销 |
| 从用户态记录 | `trace-cmd record` / `report` | 工具版本、权限与内核事件配置决定可用性 |

tracefs 常挂载于 `/sys/kernel/tracing`，可用 `findmnt -t tracefs` 确认。`available_events`、`available_tracers` 和各事件的 `format` 描述实际可用能力；不要假设每台设备都有同一组事件。

## 示例：分析 IRQ 后的线程调度

以下示例记录约 5 秒，不指定固定设备或线程；实际事件名先检查系统是否提供，`trace-cmd` 需要单独安装：

```sh
sudo trace-cmd record -o irq-sched.dat \
    -e irq:irq_handler_entry -e irq:irq_handler_exit \
    -e sched:sched_wakeup -e sched:sched_switch -- sleep 5
sudo trace-cmd report -i irq-sched.dat
```

分析时依次识别设备 IRQ、目标线程和 CPU：

```text
设备 IRQ handler 入口 → primary handler 返回
→ 目标线程 wakeup（若产生）→ sched_switch 切入目标线程
→ 结合函数或驱动事件确认实际处理完成
```

`sched_wakeup` 到 `sched_switch` 的差值是可观察到的一段调度等待，不等于设备从物理触发到任务完成的全部延迟。线程已处于可运行状态时，不一定每次硬件事件都产生新的 wakeup；跨 CPU 时间戳需确认选择的 trace clock 可比较。线程化中断的完整路径见 [[threaded-irq]]。

### 只匹配目标 IRQ 和线程

先在 `/proc/interrupts` 识别 Linux IRQ，再用 `ps -eLo tid,psr,comm` 找线程 TID。以下用 IRQ 42、TID 913 作过滤表达式示例，实际值从目标机取得；字段以 tracefs 中各事件的 `format` 为准：

| 事件 | 过滤字段 | 怎样配对 |
| --- | --- | --- |
| `irq_handler_entry` / `irq_handler_exit` | `irq == 42` | 同 CPU 上按相邻的匹配入口与退出计算 primary handler 时间；共享 IRQ 上可能有多个 action |
| `sched_waking` / `sched_wakeup` | `pid == 913` | 前者在唤醒者上下文，后者表示目标实际变为 runnable；不能用记录行的 `common_pid` 当目标 TID |
| `sched_switch` | `next_pid == 913` | 取对应唤醒后的首次切入；切出看 `prev_pid`，迁移时不要按 CPU 强行配对 |

例如 `sched_wakeup` 在 `20.004122` 秒、切入在 `20.004289` 秒，差值为 **167 µs**，只覆盖可运行后的等待。若没有匹配的唤醒或中间丢了事件，不应补算；跨 CPU 计算须使用目标内核提供的可比较 trace clock。

## 缩小追踪范围

- 函数追踪先检查 `available_filter_functions`，再通过 `set_ftrace_filter` 等限定函数；`set_graph_function` 可限定函数图的入口。
- 事件过滤可使用对应事件目录的 `filter`；先看 `format`，确定字段、类型和过滤表达式是否适用。
- 每 CPU ring buffer 大小有限；高事件速率可能覆盖旧数据。分析前检查丢失信息，按需要缩短采样或增大缓冲区。
- 临时实例位于 tracefs 的 `instances/`；退出前停止采集、保存结果并释放实例。它隔离部分设置与缓冲区，不能保证零性能影响。

## 常见误判

| 现象 | 应检查 |
| --- | --- |
| 看不到目标事件 | 内核配置、事件是否启用、过滤表达式与实际执行路径 |
| 记录突然中断 | 采样结束、buffer 覆盖、CPU 迁移或 trace 输出被消费 |
| 函数图耗时很大 | 追踪开销、调度切换、函数过滤以及目标工作本身 |
| IRQ 进入/退出很短但业务延迟大 | IRQ 线程、workqueue、锁和用户线程的后续等待 |

## 参考

- Linux 6.18.62：[ftrace](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/trace/ftrace.rst)、[事件追踪](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/trace/events.rst)、[IRQ tracepoint](https://github.com/gregkh/linux/blob/v6.18.62/include/trace/events/irq.h)。
- [`sched` tracepoint 定义](https://github.com/gregkh/linux/blob/v6.18.62/include/trace/events/sched.h)。
- 蜗窝科技：[ftrace 时钟精度问题](https://www.wowotech.net/linux_kenrel/ftrace-clock-issue.html)。
