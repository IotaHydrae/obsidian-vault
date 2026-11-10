# Linux MMIO：资源映射、寄存器访问与有界轮询

> MMIO 地址不是普通内存指针；先取得并映射设备资源，再按硬件规定的宽度、字节序和副作用使用访问器。

## TL;DR

- Platform 驱动优先 `devm_platform_ioremap_resource()`，同时完成资源申请和映射，失败返回错误指针。
- 用 `void __iomem *` 保存映射地址，用 `readl()` / `writel()` 等访问，不能直接解引用。
- 等寄存器条件用有界 `readl_poll_timeout()`；超时返回 `-ETIMEDOUT`，最后读值仍保存在变量中。
- 映射成功不等于设备已上电；访问还依赖时钟、复位和 runtime PM 状态。

源码基准：Linux 6.18 LTS / 6.18.62。

## 映射和访问选择

| 接口 | 边界 |
| --- | --- |
| `devm_platform_ioremap_resource(pdev, index)` | 按 MMIO 资源编号映射，解绑自动回收；用 `IS_ERR()` / `PTR_ERR()` |
| `ioremap()` / `iounmap()` | 低层映射/解除，驱动还需自行处理资源申请和释放 |
| `readb/readw/readl()` | 8/16/32 位访问，按接口语义处理字节序和顺序 |
| `ioread32be()` 等 | 按设备寄存器字节序选择，不能一概使用 readl |
| `memcpy_toio/fromio()` | 对允许连续访问的 I/O 区域复制；不能用普通 memcpy 替代 |

`__iomem` 是供 sparse 等工具检查的地址空间标记，不会自动验证地址、访问宽度或硬件寄存器语义。不要用 `virt_to_phys()` 转换映射指针来“找回总线地址”。

## 示例：映射后等待 READY

```c
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>

#define BLOCK_STATUS       0x04
#define BLOCK_STATUS_READY BIT(0)

struct mmio_block {
    void __iomem *base;
};

static int block_map(struct platform_device *pdev, struct mmio_block *block)
{
    void __iomem *base;

    base = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(base))
        return PTR_ERR(base);

    block->base = base;
    return 0;
}

static int block_wait_ready(struct mmio_block *block)
{
    u32 status;

    return readl_poll_timeout(block->base + BLOCK_STATUS, status,
                              status & BLOCK_STATUS_READY, 10, 100000);
}
```

示例假设资源覆盖状态寄存器、设备处于可访问状态、32 位小端寄存器允许重复读取且无读清副作用。偏移、位定义、10 µs 轮询间隔和 100 ms 超时仅演示用法，须按手册替换。

## 轮询与内存顺序

- `readl_poll_timeout(addr, val, cond, sleep_us, timeout_us)` 的两个时间参数都是微秒；`timeout_us == 0` 表示不设超时，不是只读一次。
- 普通变体用于可睡眠路径；原子路径评估 `readl_poll_timeout_atomic()` 并限制忙等时长，不能持锁长时间轮询。
- 轮询在超时边界仍会执行读取并检查条件，不能假设精确读取次数；读清、FIFO 弹出等寄存器不适合直接套宏。
- `writel()` 的顺序保证不等于 posted write 已到设备。需要确认写完成时，按硬件和总线要求读取安全寄存器；普通 CPU 屏障不能替代这一步。
- `_relaxed` 变体削弱顺序保证，不能在 DMA 描述符发布等路径中未经分析直接替换。

相关：[平台资源](../../drivers/platform/resource%20%26%20platform_data.md)、[Runtime PM](Kernel%20API%20-%20pm_runtime.md)、[DMA API](Kernel%20API%20-%20dma_map_single.md)。

## 源码与参考

- [platform.c](https://github.com/gregkh/linux/blob/v6.18.62/drivers/base/platform.c)、[lib/devres.c](https://github.com/gregkh/linux/blob/v6.18.62/lib/devres.c)：资源请求与托管映射。
- [iopoll.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/iopoll.h)、[Device I/O 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/device-io.rst)：轮询返回值、访问器及顺序。
