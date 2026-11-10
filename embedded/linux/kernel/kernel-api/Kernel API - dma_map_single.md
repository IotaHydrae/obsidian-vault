# Linux DMA：地址映射与 CPU/设备所有权

> CPU 虚拟地址、物理地址和设备 DMA 地址是不同概念；streaming DMA 映射还定义 CPU 与设备之间的缓冲区所有权交接。

## TL;DR

- 根据硬件能力设置 DMA mask，检查返回值；不要用 `virt_to_phys()` 代替 DMA API。
- 长期共享描述符常用 `dma_alloc_coherent()`；单次数据传输常用 `dma_map_single()`。
- map 后用 `dma_mapping_error()` 判断失败，DMA 地址为 0 不代表失败。
- 只有设备停止访问后才能 unmap/释放；CPU 和设备不能无协议地同时修改 streaming 缓冲区。

源码基准：Linux 6.18 LTS / 6.18.62。

## 两类映射

| 接口 | 地址与释放 |
| --- | --- |
| `dma_alloc_coherent(dev, size, &dma, gfp)` | 返回 CPU 指针，另输出 DMA 地址；失败 NULL，配对 `dma_free_coherent()` |
| `dma_map_single(dev, cpu, size, direction)` | 将合法连续缓冲区映射给设备；检查 mapping error，配对 `dma_unmap_single()` |
| `dma_sync_single_for_cpu/device()` | 保留 streaming 映射时，在设备/CPU 所有权之间切换 |
| `dma_map_sg()` | 返回映射后的 DMA 段数；unmap 使用原始 nents，而不是返回段数 |

probe 中按器件实际寻址能力调用 `dma_set_mask_and_coherent()`；例如真正只支持 32 位寻址的器件使用 `DMA_BIT_MASK(32)`。若 streaming 与 coherent 能力不同，应分别设置，不能无条件试 64 位成功就认为硬件也支持。

方向相对设备：`DMA_TO_DEVICE` 表示设备读取内存，`DMA_FROM_DEVICE` 表示设备写内存；双向使用 `DMA_BIDIRECTIONAL`。方向是缓存维护契约，不能随意用双向掩盖协议不清。

## 示例：准备发送映射，完成后释放

```c
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

struct tx_mapping {
    struct device *dev;
    void *buffer;
    size_t length;
    dma_addr_t dma;
};

static int tx_map(struct device *dev, struct tx_mapping *tx,
                  const void *data, size_t length)
{
    dma_addr_t dma;
    void *buffer;

    if (!length)
        return -EINVAL;

    buffer = kmemdup(data, length, GFP_KERNEL);
    if (!buffer)
        return -ENOMEM;

    dma = dma_map_single(dev, buffer, length, DMA_TO_DEVICE);
    if (dma_mapping_error(dev, dma)) {
        kfree(buffer);
        return -EIO;
    }

    tx->dev = dev;
    tx->buffer = buffer;
    tx->length = length;
    tx->dma = dma;
    return 0;
}

/* 仅在设备完成或可靠停止 DMA 后调用一次。 */
static void tx_unmap(struct tx_mapping *tx)
{
    dma_unmap_single(tx->dev, tx->dma, tx->length, DMA_TO_DEVICE);
    kfree(tx->buffer);
}
```

前提：设备的 DMA mask 已配置，`data` 指向 `length` 字节有效 CPU 数据，`tx` 没有旧的未释放映射，设备和 tx 对象的生命周期覆盖整个传输。成功后把 `tx->dma` 和长度按硬件协议提交；示例省略描述符及启动操作。提交失败且确认设备没有开始访问时也要 unmap；等待超时本身不证明 DMA 已停止。

## 所有权与顺序

```text
CPU 填充 → map / sync_for_device → 设备访问
→ 确认设备停止访问 → unmap / sync_for_cpu → CPU 访问
```

- 若映射长期保留，CPU 访问前按方向调用 `dma_sync_single_for_cpu()`，交还设备前调用 `dma_sync_single_for_device()`。即使某平台实现为空，也必须遵守所有权协议。
- 不要直接映射栈、vmalloc、模块静态存储等不符合单块 DMA 条件的内存；局部字段和共享 cache line 还可能导致非一致性平台的数据破坏。
- coherent 只解决相应的缓存一致性，不自动保证描述符字段发布顺序；按设备协议使用 `dma_wmb()` 等屏障，再交出所有权或写 doorbell。
- unmap 的设备、长度和方向必须与 map 匹配。驱动退出先停止 DMA 并同步完成路径，再撤销映射和释放资源。

## 源码与参考

- [DMA API HOWTO](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/dma-api-howto.rst)：地址、可映射内存和 ownership。
- [DMA API](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/dma-api.rst)、[dma-mapping.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/dma-mapping.h)：映射、同步与错误检查。
