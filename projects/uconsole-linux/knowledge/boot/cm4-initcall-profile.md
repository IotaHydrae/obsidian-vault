# CM4：定位早期 initcall 停顿

> initcall 耗时定位等待所在函数，不能直接证明该函数消耗了全部 CPU 时间。

## TL;DR

- 用原始 dmesg 与 `initcall_debug` 对齐调用和返回，先核对日志覆盖完整性。
- 当前 CM4 的约 0.59 s 停顿位于 `init_kprobe_trace()` 内，锁竞争仍为源码假设。
- 不要仅把首个等待者加黑名单：等待可能转移到后续同步点。

## 方法：完整配对后按 initcall 自报耗时排序

正常的直接挂载根文件系统启动中，0.049259 s 的时钟源选择消息与 0.629174 s 的 quota
消息之间存在一段原因不明的间隔。一次诊断启动加入了 `initcall_debug log_buf_len=1M`，
并保留 `quiet splash`。在 quiet 日志级别下，initcall 调试消息会进入 printk 环形缓冲区，
但不会输出到控制台。配置的默认缓冲区为 128 KiB；增大缓冲区是为了保留早期输出。
此后已从磁盘上的启动命令行中移除这些参数。

**覆盖判据：** 固定构建产物的 `System.map` 在 `clocksource_done_booting` 与 `dquot_init`
之间列出了 24 个 fs 级 initcall。计入两端边界后，要求这 26 个函数各有且仅有一组顺序
正确的开始/结束记录，且全部由 PID 1 在 fs 级执行。采集日志满足此判据：26/26 组记录
完整，没有缺失或乱序。全部 26 个函数的返回值均为零。采集命令以零退出，stderr 为空，
日志也保留了从 0 s 开始的内容。

应按内核报告的 `returned ... after N usecs` 排序，相邻 printk 间隔只用于圈定调查窗口。

## 基于源码的假设

源码引用来自 `kernel/build/source-17e83bbc8a6a21cc/`，该目录由固定的 Linux 提交
`234188315037848e5b07d65b85d38edc64095caa` 准备而来。按设计，构建目录不纳入代码图谱；
这些函数通过直接阅读源码核对。

- `kernel/trace/trace_kprobe.c:2035`：`init_kprobe_trace()` 创建两个 tracefs 文件，
  然后调用 `setup_boot_kprobe_events()`。
- `trace_kprobe.c:1978`：即使没有请求启动探针，`enable_boot_kprobe_events()` 仍会获取
  `event_mutex`。本次启动没有 `kprobe_event=`。
- `kernel/trace/trace.c:10597`：`trace_eval_init()` 将异步 eval-map 工作加入队列。
  `kernel/trace/trace_events.c:3407` 在持有同一个 `event_mutex` 时更新事件映射与字段。
- `trace.c:10770`：`tracer_init_tracefs()` 也会将工作加入队列。其测得的 10 us 不包含
  异步工作；`trace_events.c:4479` 中的事件目录创建会获取 `event_mutex`。

**推导出的可能原因：** 这个耗时较长的 initcall 可能在等待异步跟踪初始化释放
`event_mutex`。配置中的 tracing 启动自测和 kprobe 生成器自测均已禁用；日志不能证明
延迟来自自测。具体的子调用、锁等待时间和工作线程 CPU 开销仍然未知。

后续 Ubuntu 试验验证了单独屏蔽 kprobe initcall 会把等待转移到另一个 tracing
初始化函数；固件早期提频更有效，见[labwc 前的等待与早期提频](cm4-early-desktop.md)。

只将 `init_kprobe_trace` 加入黑名单，可能只是转移等待位置，而非消除等待：
`trace.c:10615` 中，后续的 `trace_eval_sync()` 会销毁工作队列并等待剩余工作完成。
在本次启动中，它位于前述停顿之后，报告耗时 12 us。不能据此声称跳过该同步操作就能
优化启动或节省时间。

## 设备观察：停顿归属与耗时

| 诊断事件 | 内核单调时间 |
| --- | ---: |
| 时钟源切换消息 | 0.052874 s |
| `init_kprobe_trace` 开始 | 0.052991 s |
| `init_kprobe_trace` 返回 | 0.643867 s |
| quota 消息 | 0.644493 s |

应按内核报告的 `returned ... after N usecs` 数值排序，而不是计算相邻 printk 时间戳
之差。覆盖区间内耗时最长的几项如下：

| Initcall | 报告耗时 |
| --- | ---: |
| `init_kprobe_trace` | 590,865 us |
| `dquot_init` | 167 us |
| `secretmem_init` | 54 us |
| `init_trace_printk_function_export` | 42 us |
| `iomap_ioend_init` | 42 us |

最长的一项占两条消息间隔的 99.87%。严格位于两端边界之间的其余 23 个调用总计
334 us。quota 初始化不是所观察到的停顿来源。

## 其他观察与证据边界

`bcmgenet_driver_init` 报告耗时 193,663 us，其中 MDIO 探测为 192,743 us，嵌套的 PHY
探测为 159,708 us。`dwc2_platform_driver_init` 报告耗时 76,914 us，其中 USB 平台
探测为 76,610 us。这些嵌套耗时相互重叠，不能相加。它们定位的是探测开销，不能视为
单独测得的复位延迟。

原始日志、采集元数据和完整的配对/排序报告保存在
`~/.cache/uconsole-cm4-postflash-20261018/optimization-round3/boot-initcall-diagnostic/`，
文件分别为 `raw-dmesg.txt`、`diagnostic-capture.json` 和 `initcall-analysis.json`。
报告记录了证据哈希及预期的 26 个函数顺序。
这是加入诊断参数的测量，不是正常性能试验。日志会改变时序；整体启动速度需要通过
不带这些参数的独立启动来测量。
