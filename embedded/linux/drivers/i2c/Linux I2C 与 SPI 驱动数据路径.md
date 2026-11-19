# Linux I²C 与 SPI 驱动数据路径

> I²C 和 SPI 都由总线控制器与外设驱动协作，但寻址、事务边界和返回值不同；驱动先核对器件协议和 adapter/controller 能力。

## TL;DR

- I²C 的 adapter 表示总线，client 表示地址上的设备；`i2c_transfer()` 返回完成的 message 数量。
- SPI 的 controller 驱动总线，`spi_device` 表示从设备；一个 `spi_message` 可包含多个 `spi_transfer`。
- 普通同步传输可能睡眠；不能在硬中断或持有不可睡眠锁时直接发起。
- DT 通过 `compatible`、`reg` 和资源建立设备，但 pinmux、电压、时序、片选/地址和设备协议仍须验证。

范围：Linux 6.18.62 LTS，常见 I²C 控制器和 SPI 外设。

## 对象与传输

| 层次 | I²C | SPI |
| --- | --- | --- |
| 控制器 | `i2c_adapter` + `i2c_algorithm` | `spi_controller` |
| 外设 | `i2c_client`，包含总线地址 | `spi_device`，包含片选、mode 等 |
| 操作 | 一个或多个 `i2c_msg` | 一个 `spi_message` 包含一个或多个 `spi_transfer` |
| 同步入口 | `i2c_transfer()`、`i2c_smbus_*()` | `spi_sync()`、`spi_write_then_read()` |
| 成功返回 | `i2c_transfer()` 返回完成的消息数 | `spi_sync()` 等通常返回 0 |

I²C 控制器提供 `I2C_FUNC_*` 能力；client 的地址通常由设备树或其他枚举方式提供。SPI 的 mode、bits per word、最大频率和片选必须与外设及 controller 相符。实例化方式见 [[如何实例化I2C设备]]，虚拟 adapter 见 [[通过 dummy I2C adapter 驱动了解 i2c adapter 驱动框架]]。

## I²C 组合读取示例

假设器件使用 7 位地址，要求先写入一个 8 位寄存器地址，再通过 repeated START 读取一个字节；调用者已确认 `I2C_FUNC_I2C`、`client` 和 `value` 有效，当前上下文允许睡眠：

```c
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/types.h>

static int read_byte_reg(struct i2c_client *client, u8 reg, u8 *value)
{
    struct i2c_msg msgs[2] = {
        { .addr = client->addr, .len = 1, .buf = &reg },
        { .addr = client->addr, .flags = I2C_M_RD, .len = 1, .buf = value },
    };
    int ret;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret == 2)
        return 0;
    return ret < 0 ? ret : -EIO;
}
```

示例只适用于上述协议；目标设备可能要求其他地址宽度、字节序或 STOP 条件。`i2c_transfer()` 的 1 表示只完成一条消息，不能当作两条都成功；`i2c_master_send()` 等接口的成功结果按字节数解释，不能混用返回约定。

该示例没有将缓冲区声明为 `I2C_M_DMA_SAFE`；是否需要 DMA 安全缓冲、bounce buffer 或其他限制应遵守 adapter 和 I²C core 接口契约，不能给栈地址随意加该标志。

## SPI 消息边界

外设驱动可初始化 `spi_message`，加入描述发送、接收、长度、速度等的 `spi_transfer`，再调用 `spi_sync()`；短命令也可评估 `spi_write_then_read()`。多个 transfer 是否需要保持片选、delay 或全双工行为，必须按 SPI API、controller 支持和器件协议配置。

`spi_sync()` 等待消息完成；其 buffer 的 DMA / 生命周期要求与同步或异步方式有关。`spi_async()` 的回调、message 和 buffer 必须保持有效到完成，拆除设备前需确保没有后续回调访问已释放对象。

## 调试

1. 先区分总线设备未创建、驱动未匹配、probe 失败和实际传输失败。
2. I²C 检查地址、ACK、上拉、电压、时钟、功能集与返回消息数；SPI 检查片选、CPOL/CPHA、位宽、时钟、数据方向和片选间隔。
3. 使用可用的内核日志、tracepoint 和总线分析仪；读取一次寄存器成功不证明异常、重试和并发路径正确。
4. 设备已由内核驱动管理时，避免随意用用户态工具操作同一设备；扫描也可能产生设备不兼容的事务。

## 参考

- Linux 6.18.62：[I²C client 驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/writing-clients.rst)、[I²C DMA 注意事项](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/dma-considerations.rst)、[SPI 概览](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/spi/spi-summary.rst)。
- [`i2c.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/i2c.h)、[`spi.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/spi/spi.h)。
- 蜗窝科技：[I²C 架构](https://www.wowotech.net/comm/i2c_overview.html)、[I²C consumer](https://www.wowotech.net/comm/i2c_consumer.html)。
