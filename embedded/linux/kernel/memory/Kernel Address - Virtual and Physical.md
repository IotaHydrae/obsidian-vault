# Linux 内核虚拟地址、物理地址与设备寄存器

> 在启用 MMU 的常见 Linux 系统中，内核指针是虚拟地址；设备寄存器应先映射，再通过 I/O 访问器读写。

直接将物理地址强转为指针并解引用，会被 CPU 当作虚拟地址访问，可能触发异常，也可能访问到错误映射并破坏数据。无 MMU 系统和早期启动阶段另有架构相关规则。

```c
void __iomem *regs = ioremap(phys_addr, size);
if (!regs)
    return -ENOMEM;

u32 value = ioread32(regs);
iowrite32(value, regs);
iounmap(regs);
```

示例仅展示映射生命周期，不代表任意寄存器都可以回写；实际驱动优先从设备资源获取范围，并按设备和架构选择 `devm_ioremap_resource()`、`readl()` / `writel()` 等接口。

普通 RAM、`vmalloc` 地址、MMIO 和 DMA 缓冲区有不同转换和生命周期约束。设备可访问的 DMA 地址可能经过 IOMMU 或总线转换，不能把 `virt_to_phys()` 的结果直接当作 DMA 地址；应使用 DMA 映射 API。页表概念见 [[virt to phy]]。

参考：[Linux DMA API HOWTO](https://docs.kernel.org/core-api/dma-api-howto.html)。
