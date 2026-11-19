# Linux DMAEngine 数据传输

> DMAEngine 提供 DMA 控制器通道和异步事务抽象；DMA 映射负责设备可访问的地址与缓存所有权，两者要配合但不能互相代替。

## TL;DR

- 映射使用与通道匹配的 DMA 设备，检查结果并遵守 CPU / 设备所有权。
- 准备描述符后提交并启动待处理队列；提交成功不代表硬件已经完成。
- 超时或卸载时先终止并同步 DMA 和回调，再撤销映射、释放缓冲区。

适用范围：Linux 6.18 LTS。

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

## 通道、映射与描述符

| 步骤 | 关键约束 |
| --- | --- |
| 请求通道 | `dma_request_chan()` 按设备和通道名获取；检查 `ERR_PTR`，不再使用后调用 `dma_release_channel()` |
| 配置外设传输 | `dmaengine_slave_config()` 配置外设地址、宽度和突发等；值取自控制器及外设能力 |
| 映射 scatterlist | 取 `dmaengine_get_dma_device(chan)`，使用对应 DMA 设备和方向 `dma_map_sg()`；返回 `0` 是失败 |
| 准备事务 | `dmaengine_prep_slave_sg()` 接收映射后的有效段数；`NULL` 表示准备失败，此时已成功的映射仍要撤销 |
| 提交并启动 | 先设好 callback 与上下文，`dmaengine_submit()` 后用 `dma_submit_error()` 检查 cookie；成功才调用 `dma_async_issue_pending()` |
| 完成或退出 | 完成后按映射契约同步 / unmap；终止路径先确保 DMA 和回调不再访问内存 |

`dma_map_sg()` 可以合并相邻段：传给准备函数的是**返回的 DMA 段数**；`dma_unmap_sg()` 使用映射时的**原始输入段数**。映射方向 `DMA_TO_DEVICE` / `DMA_FROM_DEVICE` 与 DMAEngine 的 `DMA_MEM_TO_DEV` / `DMA_DEV_TO_MEM` 是不同枚举，语义必须一致。

例如外设 RX 的原始 scatterlist 有 4 段，映射返回 2，则准备描述符时传 2；撤销时用相同 DMA device、scatterlist、`DMA_FROM_DEVICE` 和原始段数 4。回调只通知完成时可唤醒等待者；若还会在通知之后访问请求对象，等待者不能仅凭这次通知释放对象。

### 外设通道配置

`dma_slave_config` 的源 / 目标地址、总线宽度和 burst 配置描述 DMA 控制器如何访问外设 FIFO；它们不是任意内存地址，也不由 `dma_map_sg()` 自动推断。设备树中的通道名与请求线需要和 DMA provider 及硬件连接相符。

对于外设到内存方向，DMAEngine 使用 `DMA_DEV_TO_MEM`，缓冲区映射方向对应 `DMA_FROM_DEVICE`；反方向分别使用 `DMA_MEM_TO_DEV` 和 `DMA_TO_DEVICE`。还需验证请求触发、长度对齐和控制器支持的传输类型。

提交后描述符属于 DMAEngine，驱动不能再按自己拥有的普通对象使用。准备与提交应紧密相邻；回调及其参数所指向的对象必须保持有效。

## 回调与终止

DMAEngine client 回调由 DMAEngine 的 tasklet 调用，不是可睡眠的普通工作线程；需要睡眠时转交普通 workqueue。slave / cyclic 传输的回调可以按 API 契约准备后续工作，但要保证退出过程不再产生新事务。

- 一次性传输：等待完成后读取结果，确认设备和回调结束，再处理缓冲区。
- cyclic 传输：周期回调不代表整条流结束；停止数据源和通道后再回收环形缓冲区。
- `dmaengine_terminate_sync()` **成功**才证明通道及相关回调已同步；应在允许睡眠的上下文调用，不能在该通道完成回调中调用。不完整事务经终止后不会补发完成回调。
- 必须从原子上下文发起停止时，可先异步终止，再在合适上下文同步；内存释放必须等同步完成。

异步终止可使用 `dmaengine_terminate_async()`，成功后再在可睡眠上下文调用 `dmaengine_synchronize()`。两者之间不能重新调用 `dma_async_issue_pending()`。若终止失败，不能直接撤销映射或释放回调上下文；先按 provider 契约恢复或取得别的停机证据。

排查超时时，先区分没有启动（通道、方向、描述符、`issue_pending`）、没有设备请求（握手、FIFO 地址、水位）、传输后没有收到完成（IRQ、provider、回调）。`dmatest` 通过只说明测试的内存操作通道正常，不能证明 slave DMA 的外设握手或 cyclic 流工作。

## 参考

- 蜗窝科技：[DMAEngine 概述](https://www.wowotech.net/linux_kenrel/dma_engine_overview.html)、[DMAEngine API](https://www.wowotech.net/linux_kenrel/dma_engine_api.html)。
- Linux 6.18.62：[DMAEngine client](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/dmaengine/client.rst)、[DMA API HOWTO](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/dma-api-howto.rst)。
- [dmatest 的覆盖范围](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/dmaengine/dmatest.rst)。
- Linux 6.18.62：[`include/linux/dmaengine.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/dmaengine.h)：submit、issue pending、terminate 和 synchronize 的接口契约。
