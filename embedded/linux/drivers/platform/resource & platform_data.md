# Platform Device 资源与 platform_data

> platform 设备通过 `resource` 描述地址区间、中断等资源；`platform_data` 则是设备特定的平台数据。

```c
struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
int irq = platform_get_irq(pdev, 0);
void *pdata = dev_get_platdata(&pdev->dev);
```

- 检查 `res` 是否为 `NULL`、`irq` 是否为负错误码，并处理可能的延迟探测。
- 寄存器映射通常可使用设备管理接口，如 `devm_platform_ioremap_resource()`；具体接口以目标内核为准。
- `resource` 的 `start` / `end` 表示资源边界，`flags` 区分 `IORESOURCE_MEM`、`IORESOURCE_IRQ` 等；设备树或板级代码都可能提供资源。
- `platform_data` 可能为 `NULL`，其类型、来源和生命周期由设备及驱动约定；不能将它与设备树属性一概等同。

另见 [[platform_device]] 和 [[Kernel Address - Virtual and Physical]]。
