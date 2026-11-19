# Linux DMAEngine 数据传输

> DMAEngine 提供 DMA 控制器通道和异步事务抽象；DMA 映射负责设备可访问的地址与缓存所有权，两者要配合但不能互相代替。

## 一次从设备 DMA 的主要步骤

```text
申请并配置 DMA 通道 → 为 DMA 控制器设备正确映射缓冲区
→ 准备描述符并设置回调 → 提交 → issue pending
→ 等待完成或处理错误 → 必要时同步终止 → 撤销映射并释放资源
```

实际需要的 slave 配置、方向、scatter-gather 或 cyclic 模式由硬件与驱动决定；准备事务后检查返回值，提交 cookie 的错误按 DMAEngine 约定处理。`dmaengine_submit()` 提交事务不等于硬件已完成，通常还需 `dma_async_issue_pending()` 启动待处理队列。

| 问题 | 判断 |
| --- | --- |
| DMAEngine 通道与映射用的设备 | 映射必须使用 API 和通道 provider 对应的 DMA 设备；不能默认任意外设的 `struct device` 都能互换 |
| 内存被谁访问 | streaming mapping 期间遵守 DMA API 的 CPU / 设备所有权与同步规则 |
| 回调与缓冲区 | 回调能访问的对象必须一直有效；回调上下文和睡眠限制以 DMAEngine 契约为准 |
| 超时或驱动卸载 | 超时不等于 DMA 已停止；先按上下文终止并同步，再 unmap / free |

DMA 地址不能用 CPU 虚拟地址或物理地址替代；DMA mask、映射方向和失败检查详见 [[Kernel API - dma_map_single]]。`devm_*` 的自动清理不替代停止通道和同步回调。

## 参考

- 蜗窝科技：[DMAEngine 概述](https://www.wowotech.net/linux_kenrel/dma_engine_overview.html)、[DMAEngine API](https://www.wowotech.net/linux_kenrel/dma_engine_api.html)。
- Linux 6.18.62：[DMAEngine client](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/dmaengine/client.rst)、[DMA API HOWTO](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/dma-api-howto.rst)。
