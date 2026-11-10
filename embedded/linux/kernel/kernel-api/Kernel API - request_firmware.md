# Linux Firmware：加载固件与数据生命周期

> `request_firmware()` 成功只表示固件数据可用；驱动仍须校验格式、完成设备传输，并在最后一个数据使用者退出后释放。

## TL;DR

- 可睡眠路径调用 `request_firmware(&fw, name, dev)`，成功为 0，数据在 `fw->data` / `fw->size`。
- 固件名由 firmware loader 的搜索规则解释，不是当前工作目录中的普通路径。
- 校验最小长度、格式、版本及区间边界，再解析或上传；使用完配对 `release_firmware()`。
- 异步传输不能先释放数据；固件内存也不能直接假定满足 `dma_map_single()` 的要求。

源码基准：Linux 6.18 LTS / 6.18.62。

## 接口选择

| 接口 | 场景与约束 |
| --- | --- |
| `request_firmware()` | 同步查找/加载，可睡眠，失败返回负错误 |
| `firmware_request_nowarn()` | 同步请求，减少可选固件缺失时的默认告警 |
| `request_firmware_nowait()` | 异步回调接口，但发起调用本身也不能来自原子上下文 |
| `release_firmware()` | 归还固件数据及关联资源 |

同步请求内部可能执行文件加载或等待加载结果；异步接口改变调用者等待方式，不等于调用和回调可以忽略设备、模块及上下文生命周期。

## 示例：校验并复制到自有缓冲区

```c
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/string.h>

static int firmware_image_load(struct device *dev, const char *name,
                               u8 **data, size_t *size)
{
    const struct firmware *fw;
    u8 *copy;
    int ret;

    ret = request_firmware(&fw, name, dev);
    if (ret)
        return ret;

    if (fw->size < 8 || memcmp(fw->data, "DEMO", 4)) {
        ret = -EINVAL;
        goto out_release;
    }

    copy = kmemdup(fw->data, fw->size, GFP_KERNEL);
    if (!copy) {
        ret = -ENOMEM;
        goto out_release;
    }

    *data = copy;
    *size = fw->size;
    ret = 0;

out_release:
    release_firmware(fw);
    return ret;
}
```

`DEMO` 和 8 字节头仅用于演示格式检查，不代表实际固件协议。输出参数仅在成功时修改；调用者持有副本，并在所有传输结束后 `kfree(*data)`。复制不是必需步骤：如果上传接口同步消费完数据，也可在上传完成后直接释放 `fw`，避免额外副本。

## 解析、传输和退出边界

- 校验偏移/长度时，先确认 `offset <= fw->size`，再判断 `length <= fw->size - offset`，避免 `offset + length` 溢出。
- 文件存在不等于设备支持该版本；上传成功也不等于设备已经启动，还需检查设备状态或完成事件。
- 不要用 `filp_open()` 代替固件加载机制，也不要把固定用户 home 或工程路径写进驱动。
- DMA 或异步总线传输必须持有有效缓冲区到完成；需要 DMA 映射时使用符合 [DMA API](Kernel%20API%20-%20dma_map_single.md) 要求的缓冲区，不能直接假定 `fw->data` 的分配来源。
- nowait 的回调可能晚于 remove 的开始。驱动须协调设备引用、离线状态和取消/等待协议，不能仅以发起函数已返回判断任务结束。
- 固件名可用 `MODULE_FIRMWARE()` 声明依赖，但声明本身不会完成加载或上传。

## 源码与参考

- [Firmware 请求文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/firmware/request_firmware.rst)：同步、异步和释放约定。
- [firmware.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/firmware.h)、[firmware_loader/main.c](https://github.com/gregkh/linux/blob/v6.18.62/drivers/base/firmware_loader/main.c)：数据结构和加载路径。
