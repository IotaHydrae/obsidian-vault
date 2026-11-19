# Linux IIO Trigger 与触发缓冲区

> IIO trigger 表示采样触发源；triggered buffer 把触发事件、采样读取和缓冲区输出连接起来，适合有数据就绪中断或周期触发的设备。

## TL;DR

- trigger 可以由传感器自身、定时器或其他设备提供；它的触发时刻与用户态读到数据的时刻不同。
- 普通触发可用 poll function 的 top half 记录时间；`iio_trigger_poll_nested()` 直接调用消费者的线程处理函数，不会执行这个 top half。
- 每次消费触发后，即使读取失败，也要按框架要求调用 `iio_trigger_notify_done()`。
- 通道顺序、对齐、scan mask、字节序与时间戳布局由 IIO scan 元数据定义，不能把 buffer 当任意 C 结构直接解析。

范围：Linux 6.18 LTS，普通 IIO triggered buffer；传感器转换和时间戳精度取决于硬件。

## 从事件到样本

```text
普通：数据就绪 IRQ → iio_trigger_poll()
     → poll function 的 top half 记录时间 → 唤醒线程
嵌套：父 IRQ 线程记录或传递时间戳 → iio_trigger_poll_nested()
     → 通过 handle_nested_irq() 直接执行消费者的 thread_fn

线程处理：读取启用通道 → 按 scan 布局推入 IIO buffer → iio_trigger_notify_done()
用户读取：应用独立从 IIO buffer 取数据，不参与 trigger 的完成通知
```

普通路径可用 `iio_pollfunc_store_time()` 作为 top half；嵌套路径应由父 IRQ 处理路径捕获并传递时间戳，或由消费者在自己的线程回调内采集，不能读取一个从未更新的 `pollfunc->timestamp`。时间戳代表实际采集它的时点，不自动等于 ADC 转换时刻。一个 trigger 可以关联多个兼容消费者，具体由设备和 trigger 的校验逻辑决定。

## 关键接口

| 接口 | 用途与边界 |
| --- | --- |
| `devm_iio_trigger_alloc()` / `devm_iio_trigger_register()` | 创建并注册受设备管理的 trigger；检查错误指针和返回值 |
| `iio_triggered_buffer_setup()` / `devm_iio_triggered_buffer_setup()` | 为 IIO 设备设置 poll function 和 buffer 生命周期 |
| `iio_pollfunc_store_time()` | 常用 top half：记录时间并请求线程处理 |
| `iio_trigger_poll()` | 从硬中断上下文分派 trigger |
| `iio_trigger_poll_nested()` | 从可睡眠的线程上下文（如父 IRQ 线程）同步调用消费者线程处理函数，不执行其 top half |
| `iio_trigger_notify_done()` | 告知框架本次消费者处理结束；错误路径同样需要处理 |

如果驱动读取采样失败，不应把未初始化的数据推入 buffer，但仍要在本次线程处理结束前调用 `iio_trigger_notify_done()`。多个中断可能在处理期间合并或受硬件 FIFO 深度限制，回调次数不能无条件等同于物理样本数。

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
- 嵌套采样例子：[`inv_mpu_trigger.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/iio/imu/inv_mpu6050/inv_mpu_trigger.c) 在调用 `iio_trigger_poll_nested()` 前设置 `pollfunc->timestamp`。
