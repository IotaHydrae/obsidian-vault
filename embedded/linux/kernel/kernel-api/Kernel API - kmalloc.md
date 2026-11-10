# Linux 内存分配：kzalloc、kcalloc 与 kvzalloc

> 按执行上下文选择 GFP 标志，按尺寸和连续性选择分配器；数组分配要防乘法溢出，释放前必须结束所有异步访问。

## TL;DR

- 小型驱动对象通常用 `kzalloc(..., GFP_KERNEL)`；数组用 `kcalloc()` 或 `kmalloc_array()`。
- `GFP_KERNEL` 可能睡眠；`GFP_NOWAIT`、`GFP_ATOMIC` 都可能失败，后者还会尝试动用紧急保留资源。
- 大块、只要求虚拟连续的内存可用 `kvzalloc()`，配对 `kvfree()`。
- 普通分配失败返回 `NULL`；不要用 `IS_ERR()` 判断，也不要把任意分配结果直接作为 DMA 缓冲区。

源码基准：Linux 6.18 LTS / 6.18.62。

## 接口选择

| 接口 | 适用场景 | 释放 |
| --- | --- | --- |
| `kmalloc(size, gfp)` | 物理连续的小块内存，内容未初始化 | `kfree()` |
| `kzalloc(size, gfp)` | 同上，但将分配区域清零 | `kfree()` |
| `kmalloc_array(n, size, gfp)` | 检查 `n * size` 溢出的数组分配 | `kfree()` |
| `kcalloc(n, size, gfp)` | 数组分配并清零 | `kfree()` |
| `kvzalloc(size, gfp)` | 先尝试 kmalloc，必要时退到 vmalloc，清零 | `kvfree()` |
| `struct_size(obj, member, n)` | 计算含柔性数组的对象尺寸，并处理溢出 | 仅计算尺寸 |

`vmalloc` 类内存保证虚拟地址连续，不能据此认为物理地址连续。`kvmalloc` 类接口应遵守其 GFP 限制，通常用于可睡眠上下文；不能靠传 `GFP_ATOMIC` 把它变成通用原子分配器。

零尺寸分配可能返回特殊的非 NULL 指针，不能解引用。业务不允许空数组时应先拒绝 `n == 0`。

## 示例：分配采样结果数组

```c
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/types.h>

struct sample {
    u32 channel;
    u32 value;
};

static int sample_table_alloc(struct sample **out, size_t count)
{
    struct sample *samples;

    if (!count)
        return -EINVAL;

    samples = kcalloc(count, sizeof(*samples), GFP_KERNEL);
    if (!samples)
        return -ENOMEM;

    *out = samples;
    return 0;
}
```

成功后调用者持有数组，使用结束且所有 IRQ、work、DMA 等访问者均已退出后执行 `kfree(samples)`。失败时 `out` 不变。不要改成 `kmalloc(count * sizeof(*samples), ...)`，否则可能在分配前就发生尺寸溢出。

## 上下文和常见误用

- `GFP_KERNEL` 的直接回收可能调度，不能在硬中断、关中断或持普通自旋锁时使用。
- 不能睡眠时应先考虑预分配；`GFP_NOWAIT` 不直接回收，确有保留池需求才考虑 `GFP_ATOMIC`。PREEMPT_RT 还需遵守分配器和锁的上下文限制。
- 清零不等于完成初始化：mutex、list、completion 等仍须调用各自初始化接口。
- `kfree(NULL)` 合法，但释放后清空一个变量不能防止其他引用发生 use-after-free。
- 若资源寿命与设备绑定一致，可考虑 [devres](Kernel%20API%20-%20devres.md)；DMA 的地址和一致性另见 [DMA API](Kernel%20API%20-%20dma_map_single.md)。

## 源码与参考

- [内存分配指南](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/memory-allocation.rst)：GFP、数组尺寸与分配器选择。
- [slab.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/slab.h)、[mm/slub.c](https://github.com/gregkh/linux/blob/v6.18.62/mm/slub.c)：数组溢出检查、kvmalloc 与 kvfree。
