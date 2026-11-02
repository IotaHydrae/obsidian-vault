# Linux USB Scatter-Gather 传输

> `usb_sg_init()` 初始化 bulk/interrupt 请求，`usb_sg_wait()` 提交并等待完成；缓冲区和 scatterlist 必须保持有效。

```c
struct usb_sg_request req;
int ret;

ret = usb_sg_init(&req, udev, usb_sndbulkpipe(udev, endpoint),
                  0, sgt.sgl, sgt.nents, length, GFP_KERNEL);
if (ret)
    return ret;

usb_sg_wait(&req);
ret = req.status;
/* req.bytes 是已传输字节数；之后按所有权释放 scatterlist 和缓冲区。 */
```

- `length == 0` 表示使用 scatterlist 所描述的全部字节；中断端点的 `period` 需按设备速度与接口要求设置。
- `usb_sg_wait()` 可能睡眠；需要取消时，按目标内核文档使用 `usb_sg_cancel()`，并协调并发及对象生命周期。
- 如果从 `vmalloc` 缓冲区构建 scatterlist，按 `PAGE_SIZE`、首个页内偏移和总长度计算页数，并检查 `vmalloc_to_page()` 和 `sg_alloc_table_from_pages()` 的结果。
- 映射、传输和释放遵守目标内核的 USB/DMA API，不要在传输尚未结束时释放或复用数据。具体行为以该版本 `drivers/usb/core/message.c` 为准。
