# Linux IIO Trigger 与触发缓冲区

> IIO trigger 表示采样触发源；triggered buffer 把触发事件、采样读取和缓冲区输出连接起来，适合有数据就绪中断或周期触发的设备。

## TL;DR

- trigger 可以由传感器自身、定时器或其他设备提供；它的触发时刻与用户态读到数据的时刻不同。
- poll function 的 top half 通常只记录时间并唤醒线程；I²C/SPI 等可睡眠读取放在线程处理函数。
- 每次消费触发后，即使读取失败，也要按框架要求调用 `iio_trigger_notify_done()`。
- 通道顺序、对齐、scan mask、字节序与时间戳布局由 IIO scan 元数据定义，不能把 buffer 当任意 C 结构直接解析。

范围：Linux 6.18 LTS，普通 IIO triggered buffer；传感器转换和时间戳精度取决于硬件。

## 从事件到样本

```text
传感器数据就绪 IRQ / 其他 trigger
→ IIO trigger 分派 poll function
→ top half 保存时间戳并唤醒线程
→ threaded handler 读取启用通道
→ 按 scan 布局写入 IIO buffer → 用户程序读取
→ iio_trigger_notify_done()
```

一个 trigger 可以关联一个或多个适合的消费者；是否允许特定设备使用由 trigger 和设备的校验逻辑决定。中断中的时间戳表示驱动记录时刻，不自动等于传感器内部 ADC 转换时刻。

## 关键接口

| 接口 | 用途与边界 |
| --- | --- |
| `devm_iio_trigger_alloc()` / `devm_iio_trigger_register()` | 创建并注册受设备管理的 trigger；检查错误指针和返回值 |
| `iio_triggered_buffer_setup()` / `devm_iio_triggered_buffer_setup()` | 为 IIO 设备设置 poll function 和 buffer 生命周期 |
| `iio_pollfunc_store_time()` | 常用 top half：记录时间并请求线程处理 |
| `iio_trigger_poll()` | 从硬中断上下文分派 trigger |
| `iio_trigger_poll_nested()` | 在适用的内核线程上下文分派嵌套 trigger |
| `iio_trigger_notify_done()` | 告知框架本次消费者处理结束；错误路径同样需要处理 |

如果用户代码读取失败，不应把未初始化的数据推入 buffer；但仍需完成 trigger 的通知与资源清理。多个中断可能在处理期间合并或受硬件 FIFO 深度限制，回调次数不能无条件等同于物理样本数。

## 用户空间验证

`/sys/bus/iio/devices/triggerY/name` 表示 trigger 名称；支持 triggered buffer 的设备可在 `iio:deviceX/trigger/current_trigger` 选择兼容 trigger。配置 scan elements、采样频率和 buffer 后再读取 `/dev/iio:deviceX`；编号是动态分配的，应按 `name` 和拓扑识别。

先关闭 buffer 再按设备接口修改 scan 配置；读出的字节按 `scan_elements/*_type`、`*_index` 和使能状态解析。可用工具入口见 [[iio tools]]。

| 现象 | 优先检查 |
| --- | --- |
| 没有 trigger | provider 是否注册、IRQ / 定时器是否启用、sysfs 名称 |
| 首次采样后停止 | handler 错误路径是否通知 done、IRQ 是否清除、FIFO / 触发源状态 |
| 数据错位 | scan mask、索引、存储宽度、对齐和字节序 |
| 时间戳与硬件不同步 | 记录位置、设备 FIFO、实际转换时刻和系统时钟 |

## 参考

- Linux 6.18.62：[IIO triggers](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/iio/triggers.rst)、[triggered buffers](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/iio/triggered-buffers.rst)、[buffers](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/iio/buffers.rst)。
- [`industrialio-trigger.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/iio/industrialio-trigger.c)、[`triggered_buffer.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/iio/triggered_buffer.h)：分派上下文与接口。
