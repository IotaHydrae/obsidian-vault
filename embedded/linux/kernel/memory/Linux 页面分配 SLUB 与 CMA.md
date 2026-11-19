# Linux 页面分配、SLUB 与 CMA

> 页面分配器、SLUB 和 CMA 面向不同需求；先明确对象大小、可睡眠上下文、物理连续性和设备 DMA 约束，再选择接口。

## TL;DR

- buddy 页面分配器以 `2^order` 页为粒度管理空闲块；总空闲量足够不保证高阶连续块可用。
- SLUB 管理内核小对象缓存；`kmalloc()` 适合相应范围的连续对象，分配与释放须正确配对。
- `vmalloc()` 提供虚拟连续，不保证物理连续；`kvmalloc()` 可能回退到 vmalloc。
- CMA 尝试提供连续物理页，但页面迁移、设备约束和实际占用都可能使分配失败；驱动仍使用适当的 DMA API。

范围：Linux 6.18 LTS；分配器配置、架构与设备 DMA 能力影响实际路径。

## 分层关系

```text
物理页面 / zone → 页分配器（order、迁移类型、回收）
                    ├→ SLUB 缓存 → kmalloc / kmem_cache
                    ├→ 页框映射 → vmalloc / kvmalloc
                    └→ 连续区域机制 → 适用的 CMA / DMA 分配路径
```

图表示常见依赖，不说明所有分配都会经过 CMA。`ZONE_DMA`、`ZONE_DMA32` 等含义由架构和平台决定；zone 名称也不能替代设备的 DMA mask 检查。

## buddy 与高阶分配

一个 order 为 `n` 的块包含 `2^n` 个基础页；以 4 KiB 页为例，order 3 为 8 页、32 KiB。分配器可拆分较大空闲块，释放时尝试与合适的伙伴合并。

内存碎片导致空闲页分散时，即使总 `MemFree` 较大，高阶分配仍可能失败。回收可以释放页面，compaction 尝试迁移页面以形成连续区域；两者不保证所有被固定或不可移动的页面都能被迁走。

## SLUB 与常见接口

SLUB 为对象类型或大小维护 cache，并在页面中组织多个对象。`kmalloc`、`kzalloc`、`kmalloc_array` 和 `kmem_cache_*` 的选择还要考虑对象大小、构造要求和调用上下文。

`GFP_KERNEL` 可能触发直接回收并睡眠；`GFP_ATOMIC` / `GFP_NOWAIT` 也可能失败，不能作为无限重试的保证。数组大小、零长度与释放配对见 [[Kernel API - kmalloc]]。

## CMA 与 DMA 的区别

CMA 区域用于支持需要连续物理页面的分配；未被专用分配占用时，其页面可以按内核规则用于可迁移用途。分配时需要找到合适区间并迁移可迁移页面，若区域不足或存在无法迁移的页面，就可能失败。

是否使用 CMA、使用哪个区域以及返回地址如何映射取决于 DMA API、设备和平台；**CMA 的物理连续页不等于可直接写给设备的 DMA 地址**。`dma_alloc_coherent()` 或 streaming DMA 等需要遵守对应的设备、mask、方向和释放契约，见 [[Kernel Address - Virtual and Physical]] 与 [[Kernel API - dma_map_single]]。

## 观察与排查

```sh
cat /proc/buddyinfo
grep -E 'MemAvailable|CmaTotal|CmaFree' /proc/meminfo
```

`buddyinfo` 按节点、zone 和 order 展示空闲块快照；`CmaFree` 不是某次连续分配必定成功的承诺。先记录分配大小、GFP、设备、DMA mask 和错误日志，再结合页固定、压力、区域大小与可迁移性分析。CMA debugfs / sysfs 节点取决于内核配置和权限。

OOM 与回收行为见 [[Linux 内存回收与 OOM]]；用户页面长期固定与缺页见 [[Linux 缺页与写时复制]]。

## 参考

- Linux 6.18.62：[页面分配](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/mm/page_allocation.rst)、[SLUB 管理](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/admin-guide/mm/slab.rst)、[CMA debugfs](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/admin-guide/mm/cma_debugfs.rst)。
- [`mm/cma.c`](https://github.com/gregkh/linux/blob/v6.18.62/mm/cma.c)、[`kernel/dma/contiguous.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/dma/contiguous.c)、[DMA API](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/dma-api-howto.rst)。
- 蜗窝科技：[SLUB](https://www.wowotech.net/memory_management/426.html)、[CMA](https://www.wowotech.net/memory_management/cma.html)、[内存碎片](https://www.wowotech.net/memory_management/memory-fragment.html)。
