# Linux 内存回收与 OOM

> 内存不足要区分容量、可回收性、碎片、分配上下文和控制组限制；`MemFree` 小本身不足以证明内存泄漏。

## TL;DR

- 结合日志区分全局 OOM、memory cgroup 限额和单次分配失败。
- 查看匿名内存、缓存、swap、slab 和变化趋势，不只看 `MemFree`。
- 高阶连续分配可能因碎片失败；调 OOM 参数不能替代定位实际需求。

适用范围：Linux 6.18 LTS。

## 主要概念

| 项目 | 作用和边界 |
| --- | --- |
| page cache / reclaim | 缓存提高 I/O 效率，内存压力下部分页面可回收；脏页、锁定页和活跃页面的处理不同 |
| swap / zram | 为匿名内存提供交换空间；zram 在内存中压缩数据，不等于凭空增加物理内存 |
| slab | 为内核对象分配缓存；不同对象的可回收性和生命周期不同 |
| 内存碎片 / 高阶分配 | 总空闲量足够也不保证有足够大的物理连续区域 |
| OOM | 在无法满足内存需求时选择恢复措施；可能受全局或 memory cgroup 限制影响 |

用户空间看到的虚拟内存大小、常驻内存和物理连续内存需求不是同一指标。驱动分配还受 GFP 标志、执行上下文和 DMA 寻址约束；分配接口见 [[Kernel API - kmalloc]]，地址类型见 [[Kernel Address - Virtual and Physical]]。

## 内存压力下发生什么

内核可以在后台回收页面，也可能让当前分配路径参与直接回收。干净文件缓存通常可丢弃后重新读取；脏页需要先完成写回，匿名内存能否换出则取决于 swap 和页面状态。锁定、正在使用或受其他约束的页面不一定能立即回收。

页面回收和内存压缩解决的是不同问题：前者尝试释放页面，后者通过迁移页面改善连续空闲区域。足够多的零散空闲页不保证高阶连续分配成功；CMA 也有其用途和约束，不应当作任意分配必然成功的保证。

`GFP_KERNEL` 等允许回收的分配可能睡眠；不可睡眠上下文须使用符合上下文的分配方式并处理失败。原子分配标志不提供无限内存，也不应通过无限重试掩盖容量设计问题。

## 看指标时避免误判

| 观察 | 应结合的信息 |
| --- | --- |
| `MemFree` 很小 | `MemAvailable` 是估计值；结合缓存、可回收性与实际压力 |
| 进程虚拟地址很大 | 结合 RSS、共享页、映射类型及 `smaps_rollup`，不要等同于物理占用 |
| swap / zram 在使用 | 判断压缩开销、换入换出及性能；不单独证明内存故障 |
| slab 持续增加 | 分别查看可回收和不可回收部分，并定位持有者 |
| 仅某容器 OOM | 检查该 memory cgroup 的 `memory.max`、`memory.current` 与 `memory.events` |

分析 OOM 日志时记录触发分配、约束范围和被选中的任务；当前执行分配的线程与最终被终止的进程不一定相同。

## 区分全局压力和 cgroup 限制

在 cgroup v2 环境中，`memory.current` 反映该 cgroup 的当前内存使用；`memory.high` 可触发节流和回收，`memory.max` 是主要的内存限制；`memory.events` 的计数用于观察相应压力与 OOM 事件。实际文件位置由进程所在 cgroup 决定，不能直接把示例路径当成所有服务的路径。

排查时先记录事件计数的变化量，再与内核日志、服务的 cgroup 路径和进程内存趋势对照。若只有某个服务触发限制，即使宿主机仍有空闲内存，该服务也可能遇到分配压力。

| 现象 | 下一步证据 |
| --- | --- |
| 持续回收、应用卡顿 | 内存压力、直接回收、swap I/O 和应用工作集 |
| 日志出现高阶分配失败 | 分配 order、GFP 上下文、碎片及是否真的需要连续内存 |
| slab 长期增长 | 可回收 / 不可回收类型、持有对象和解绑释放路径 |
| cgroup 的 OOM 事件增长 | `memory.max`、`memory.events` 和该组内进程 |

## 排查顺序

1. 保存 OOM 前后日志，区分全局 OOM、cgroup 限额和单次分配失败；确认触发任务不必然等于最终被选择的任务。
2. 结合 `/proc/meminfo`、进程 RSS、swap 和 cgroup 指标观察趋势；缓存、匿名页、共享内存与 slab 不能只看一个数字。
3. 针对高阶分配失败检查碎片与实际连续性需求；不把反复重试或调整 OOM 参数当作根因修复。
4. 怀疑泄漏时结合分配 / 释放生命周期与适当调试工具；KASAN、slab 调试等有配置、性能和适用范围限制。

## 参考

- 蜗窝科技：[页面回收](https://www.wowotech.net/memory_management/page_reclaim_basic.html)、[内存碎片](https://www.wowotech.net/memory_management/memory-fragment.html)、[zram](https://www.wowotech.net/memory_management/zram.html)、[OOM 参数](https://www.wowotech.net/memory_management/oom.html)。
- Linux 6.18.62：[页面回收](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/mm/page_reclaim.rst)、[OOM](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/mm/oom.rst)。
- Linux 6.18.62：[cgroup v2 内存控制](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/admin-guide/cgroup-v2.rst)。
