# Linux Dummy UART 驱动

> 基于 `serial_core` 注册一个虚拟 UART 端口，将发送缓冲中的字节送回 TTY 接收路径，用于理解驱动回调和端口生命周期。

## TL;DR

- `uart_register_driver()` 注册串口驱动，`uart_add_one_port()` 添加端口；本例得到 `ttyDUMMY0`。
- `start_tx()` 在端口锁持有期间取出 TX 字节，写入 TTY flip buffer 并推送，形成软件回环。
- 原始字节测试先把 TTY 设置为 raw、关闭回显，并处理读取超时；输入输出不保证按一次 `read()` 划分。回环状态下启用 TTY 回显可能引发持续的自回显。
- 完整源码：[dummy_uart.c](examples/uart/dummy_uart.c)；构建文件为 `examples/uart/Makefile`。

范围：Linux 6.18 LTS、单端口短数据软件回环；不模拟真实 UART 寄存器、波特率、电平、FIFO、中断或 DMA。入口见 [[embedded/linux/drivers/dummy/README|虚拟驱动学习目录]]。

## 分层和数据路径

```text
应用 write(/dev/ttyDUMMY0)
  → TTY / line discipline → serial_core TX 软件缓冲
  → uart_ops.start_tx() → uart_port_tx() 取字节
  → tty_insert_flip_char() → tty_flip_buffer_push()
  → TTY / line discipline → 应用 read(/dev/ttyDUMMY0)
```

| 对象或接口 | 本例作用 | 真实硬件中对应的职责 |
| --- | --- | --- |
| `uart_driver` | 定义驱动名、设备名和端口数 | 注册一组同类 UART 端口 |
| `uart_port` | 描述唯一虚拟端口并连接 `uart_ops` | 描述一个控制器端口、时钟、IRQ 和 MMIO |
| `uart_ops` | 提供启动、停止、收发、参数等回调 | 控制 UART 寄存器、FIFO、中断和电源 |
| TTY flip buffer | 保存待送往 line discipline 的接收字节 | 接收中断或 DMA 收到的数据进入此处 |

本例是 `serial_core` 驱动，不是 PTY、USB 串口或仅注册 `tty_driver` 的示例。`DUMMY_UART_TYPE` 是为实验设置的本地非零标记，仅用于让 serial core 将端口视为已配置；它不是 Linux 分配的 UART UAPI 类型，不应作为正式设备 ABI。

## 端口生命周期

```text
模块加载：root_device_register → uart_register_driver → uart_add_one_port
首次打开：serial_core 分配 TX 缓冲 → startup → set_termios
读写期间：start_tx → 软件回环 → TTY 接收
最后关闭：shutdown；模块卸载：uart_remove_one_port → uart_unregister_driver
```

`root_device_register()` 只为设备模型提供虚拟父设备，不代表物理 platform device。本例没有 MMIO 地址，也不会映射或申请硬件资源；真实设备通常在 platform 驱动的 `probe()` 获取资源后注册端口。

端口注册失败时反向释放已注册的 driver 和父设备。卸载先移除端口，serial core 会解除该端口与 TTY 的关联并处理挂断，再注销 driver；使用异步中断、DMA、work 或 timer 的真实驱动，还必须先阻止新工作并同步所有在途回调。

## 发送和接收实现

```c
static void dummy_start_tx(struct uart_port *port)
{
    u8 ch;

    uart_port_tx(port, ch, true, dummy_receive_char(port, ch));
    tty_flip_buffer_push(&port->state->port);
}
```

`uart_port_tx()` 在串口发送缓冲不为空且允许发送时逐字节调用 `dummy_receive_char()`，并处理发送统计和写唤醒；接收函数用 `tty_insert_flip_char(..., TTY_NORMAL)` 送入 flip buffer。插入失败时本例丢弃该字节并增加 `buf_overrun`，不会自动重试，因此只适合短数据与路径学习。

`start_tx()` 由 serial core 在端口锁持有、不可睡眠的上下文中调用；上述 helper 的回调也不能睡眠。`tty_flip_buffer_push()` 将数据提交给 TTY 层，应用何时收到还受 line discipline 和调度影响。本例的 `stop_tx`、`stop_rx`、`set_termios` 等没有真实硬件操作；设置波特率、奇偶校验或流控不会产生对应的硬件行为。

回环会把输出重新变成输入；若 TTY 的 `ECHO` / `ECHONL` 将输入再次写回，可能形成持续的自回显。**发送任何测试数据前先关闭回显**，并避免 getty 或其他程序同时使用该节点。下面的 Python 示例在写入前设置 raw 模式。

## 构建和验证

目标内核需要 `CONFIG_MODULES`、`CONFIG_TTY`、`CONFIG_SERIAL_CORE`，构建树及 `Module.symvers` 必须与运行内核匹配：

```sh
cd embedded/linux/drivers/dummy/examples/uart
make KDIR=/path/to/matching/linux-6.18-build W=1
sudo insmod ./dummy_uart.ko
ls -l /dev/ttyDUMMY0
```

设备节点还依赖 devtmpfs / 用户空间设备管理，当前用户需有读写该节点的权限。使用 Python 3 完成短二进制回环，期望输出 `PASS: 00017e80ff`；使用完毕会恢复 TTY 参数：

```sh
python3 - /dev/ttyDUMMY0 <<'PY'
import os
import select
import sys
import termios
import time
import tty

fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
old = termios.tcgetattr(fd)
try:
    tty.setraw(fd)
    payload = bytes.fromhex("00017e80ff")
    if os.write(fd, payload) != len(payload):
        raise RuntimeError("short write")
    received = bytearray()
    deadline = time.monotonic() + 2
    while len(received) < len(payload):
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([fd], [], [], remaining)[0]:
            raise TimeoutError(f"received {received.hex()}")
        received.extend(os.read(fd, len(payload) - len(received)))
    if received != payload:
        raise RuntimeError(f"unexpected data: {received.hex()}")
    print("PASS:", received.hex())
finally:
    termios.tcsetattr(fd, termios.TCSANOW, old)
    os.close(fd)
PY
sudo rmmod dummy_uart
```

打不开节点时先检查模块日志、节点权限和是否存在 `ttyDUMMY0`；收到多余字节时检查 raw / echo 和是否有其他进程占用。卸载前关闭所有使用该设备的程序，否则 `rmmod` 可能因模块仍被使用而失败。超时或内容不一致均不是通过；`2` 秒只是实验超时，不是 UART 的性能指标。

## 迁移到硬件

1. 将虚拟父设备换成真实设备，解析寄存器、IRQ、时钟、复位、引脚和端口编号，按失败路径逐项回收资源。
2. `startup()` 初始化控制器并申请 / 启用接收中断；`shutdown()` 禁止中断与 DMA，清除状态并同步在途处理。
3. `start_tx()` 根据 TX FIFO 空间填充数据并使能发送中断；RX 中断或 DMA 完成后读取状态和数据，处理溢出、校验和帧错误，再推送 flip buffer。
4. 在 `set_termios()` 中按时钟和硬件能力配置波特率、数据位、校验、停止位和流控；正确处理 modem 控制、挂断及电源管理。
5. 通过交叉接线、逻辑分析仪和目标协议测试实际波特率与错误恢复，软件回环不能证明任何电气或时序要求。

相关：[[Linux 串口与 TTY 数据路径]]、[[Linux 驱动异步退出与资源生命周期]]。

## 参考

- Linux 6.18.62：[串口驱动 API](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/serial/driver.rst)、[TTY buffer](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/tty/tty_buffer.rst)、[`serial_core.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/serial_core.h)、[`serial_core.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/tty/serial/serial_core.c)。
- 蜗窝科技：[串口框架概述](https://www.wowotech.net/comm/serial_overview.html)、[TTY 软件架构](https://www.wowotech.net/tty_framework/tty_architecture.html)。
