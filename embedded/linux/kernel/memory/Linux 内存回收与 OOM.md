# Linux 内存回收与 OOM

> 内存不足要区分容量、可回收性、碎片、分配上下文和控制组限制；`MemFree` 小本身不足以证明内存泄漏。

## 主要概念

| 项目 | 作用和边界 |
| --- | --- |
| page cache / reclaim | 缓存提高 I/O 效率，内存压力下部分页面可回收；脏页、锁定页和活跃页面的处理不同 |
| swap / zram | 为匿名内存提供交换空间；zram 在内存中压缩数据，不等于凭空增加物理内存 |
| slab | 为内核对象分配缓存；不同对象的可回收性和生命周期不同 |
| 内存碎片 / 高阶分配 | 总空闲量足够也不保证有足够大的物理连续区域 |
| OOM | 在无法满足内存需求时选择恢复措施；可能受全局或 memory cgroup 限制影响 |

用户空间看到的虚拟内存大小、常驻内存和物理连续内存需求不是同一指标。驱动分配还受 GFP 标志、执行上下文和 DMA 寻址约束；分配接口见 [[Kernel API - kmalloc]]，地址类型见 [[Kernel Address - Virtual and Physical]]。

## 排查顺序

1. 保存 OOM 前后日志，区分全局 OOM、cgroup 限额和单次分配失败；确认触发任务不必然等于最终被选择的任务。
2. 结合 `/proc/meminfo`、进程 RSS、swap 和 cgroup 指标观察趋势；缓存、匿名页、共享内存与 slab 不能只看一个数字。
3. 针对高阶分配失败检查碎片与实际连续性需求；不把反复重试或调整 OOM 参数当作根因修复。
4. 怀疑泄漏时结合分配 / 释放生命周期与适当调试工具；KASAN、slab 调试等有配置、性能和适用范围限制。

## 参考

- 蜗窝科技：[页面回收](https://www.wowotech.net/memory_management/page_reclaim_basic.html)、[内存碎片](https://www.wowotech.net/memory_management/memory-fragment.html)、[zram](https://www.wowotech.net/memory_management/zram.html)、[OOM 参数](https://www.wowotech.net/memory_management/oom.html)。
- Linux 6.18.62：[页面回收](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/mm/page_reclaim.rst)、[OOM](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/mm/oom.rst)。
