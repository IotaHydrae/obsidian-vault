# Linux IIO 工具

> 内核源码的 `tools/iio` 提供查看设备、事件和缓冲数据的示例工具。

## 工具

- `lsiio.c`：列举 IIO 触发器、设备和通道。
- `iio_event_monitor.c`：通过 IIO 事件接口监视事件。
- `generic_buffer.c`：读取、处理并打印 IIO 缓冲数据。
- `libiio`：IIO 设备访问库，与内核源码中的示例工具分别提供。

工具的源码位置、编译方法和接口应以目标内核及库版本为准。
