# Linux Dummy UART 驱动

> 基于 `serial_core` 注册一个虚拟 UART 端口，将发送缓冲中的字节送回 TTY 接收路径，用于理解驱动回调和端口生命周期。

## TL;DR

- `uart_register_driver()` 注册串口驱动，`uart_add_one_port()` 添加端口；本例得到 `ttyDUMMY0`。
- `start_tx()` 在端口锁持有期间取出 TX 字节，写入 TTY flip buffer 并推送，形成软件回环。
- 设备树版本通过 platform 匹配后注册 UART port；自注册版本不读取设备树，两者是独立的实验入口。
- 未向 TTY 传递 UART 模块 `owner` 的内核可运行回环，但本例会禁用普通卸载。要练习卸载和重载，请使用已修复 `serial_core` 的测试内核。
- 原始字节测试先把 TTY 设置为 raw、关闭回显，并处理读取超时；输入输出不保证按一次 `read()` 划分。回环状态下启用 TTY 回显可能引发持续的自回显。
- 源码：[自注册版本](examples/uart/dummy_uart.c)、[设备树版本](examples/uart/dummy_uart_of.c)、[串口回调与回环](examples/uart/dummy_uart_ops.h)。

范围：Linux 6.18 LTS、单端口短数据软件回环；不模拟真实 UART 寄存器、波特率、电平、FIFO、中断或 DMA。入口见 [[embedded/linux/drivers/dummy/README|虚拟驱动学习目录]]。

## 分层和数据路径

```text
应用 write(/dev/ttyDUMMY0)
  → TTY / line discipline → serial_core uart_write() → TX 软件 FIFO
  → uart_ops.start_tx() → uart_port_tx() 逐字节取出
  → dummy_receive_char() → tty_insert_flip_char() → TTY flip buffer
  → tty_flip_buffer_push() → line discipline → 应用 read(/dev/ttyDUMMY0)
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
最后关闭：shutdown → TTY 完成对象释放
可卸载时：uart_remove_one_port → uart_unregister_driver
```

`root_device_register()` 只为设备模型提供虚拟父设备，不代表物理 platform device。本例没有 MMIO 地址，也不会映射或申请硬件资源；真实设备通常在 platform 驱动的 `probe()` 获取资源后注册端口。

端口注册失败时反向释放已注册的 driver 和父设备。`uart_remove_one_port()` 摘除端口并挂断 TTY，但**挂断不是关闭所有 FD**；旧 FD 的释放路径仍可能访问 `uart_state` 内嵌的 `tty_port`。因此不能在 TTY 对象仍存活时调用会释放状态表的 `uart_unregister_driver()`。真实驱动还需同步 IRQ、DMA、work 或 timer 的异步访问。

### 模块引用与卸载

下表针对启用了 `CONFIG_MODULE_UNLOAD` 的测试内核；未启用该选项时，内核本身不支持卸载模块。

| 内核行为 | 本例的卸载约束 |
| --- | --- |
| 未修复的 Linux 6.18.62：`uart_register_driver()` 没有把 `uart_driver.owner` 传给 `tty_driver.owner` | 两个示例在初始化成功后持有一个长期模块引用。即使关闭了所有 FD，普通 `rmmod` 也会被拒绝；用测试虚拟机重启清理，禁止强制卸载 |
| core 在 `tty_register_driver()` 前正确设置了 TTY owner | TTY 打开时取得模块引用，最终释放 TTY 对象时归还；打开或已挂断但尚未关闭的 FD 会阻止普通卸载 |

`dummy_uart_guard_module_unload()` 只在初始化时读取 owner 来选择保护方式，不改写已注册的 TTY driver；这是针对 6.18 示例的兼容检查，而不是通用 serial core API。不能仅在 `startup()` / `shutdown()` 之间持有引用：挂断可能先调用 `shutdown()`，而 FD 仍未关闭。设备树版本的 platform 解绑只摘除端口，不注销整个 `uart_driver`。

要在 Linux 6.18.62 上验证可卸载路径，在**测试内核源码副本**的根目录中使用 `examples/uart/serial-core-tty-owner.patch`（把下列路径换成补丁所在的实际绝对路径）：

```sh
patch_file=/path/to/serial-core-tty-owner.patch
git apply --check "$patch_file" && git apply "$patch_file"
```

补丁在 TTY driver 发布前设置 owner。重新构建并在虚拟机启动该内核，再用同一构建树编译两个示例；不需要修改宿主内核。

## 最小接口及作用

下表的“必需”针对**本例能被打开、发送、关闭并支持常见 TTY 操作**的路径；真实硬件的必需接口还取决于中断、流控、console 和电源管理等功能，不能把空实现直接用于产品。

| 注册接口 | 作用 |
| --- | --- |
| `uart_register_driver()` | 向 serial core 注册一组端口的驱动，准备对应的 TTY driver 和状态表；必须先于添加端口 |
| `uart_add_one_port()` | 把一个已初始化的 `uart_port` 及其 `uart_ops` 接入 serial core；本例 `line = 0` |
| `uart_remove_one_port()` | 先摘除端口、解除 TTY 关联并处理在途引用；设备树版本在 platform `remove()` 中调用 |
| `uart_unregister_driver()` | 最后注销 UART driver；调用前所有端口都必须已移除 |
| `root_device_register()` / `root_device_unregister()` | 自注册版本创建 / 释放虚拟父设备；设备树版本使用 platform 设备作为父设备 |
| `platform_driver.probe` / `remove` | 设备树版本匹配后添加端口，解绑时先移除端口；不是 `uart_ops` 的收发回调 |
| `of_match_table` | platform 驱动用它把节点的 `compatible` 与驱动匹配起来 |
| `MODULE_DEVICE_TABLE(of, ...)` | 导出模块自动加载所需的 DT 别名；手动加载时设备仍需通过匹配表绑定 |
| `platform_set_drvdata()` / `platform_get_drvdata()` | 绑定设备和端口指针，使 `remove()` 能找到 `probe()` 注册的端口 |
| `platform_driver_register()` / `platform_driver_unregister()` | 注册 / 注销 platform 驱动；本例需要控制其与 `uart_driver` 的注册顺序，因此显式调用 |

### `uart_ops` 回调

| 回调 | 必需性 | 作用与本例行为 |
| --- | --- | --- |
| `startup()` | 本例必需 | 首次打开端口时初始化硬件、申请 IRQ / 启用接收；本例无硬件，返回 `0` |
| `shutdown()` | 本例必需 | 最后关闭或移除时关闭接收、停止 IRQ / DMA 并回收资源；本例为空 |
| `start_tx()` | 本例必需 | 启动发送；本例从 serial core TX FIFO 取字节并送入自身接收路径 |
| `stop_tx()` | 本例必需 | 停止发送，例如停止 TX 中断；本例同步发送，没有硬件动作 |
| `stop_rx()` | 本例必需 | 关闭路径中停止接收；本例无 RX 中断，因此为空 |
| `tx_empty()` | 本例必需 | 报告发送 FIFO 和移位寄存器是否已空；本例同步完成，返回 `TIOCSER_TEMT` |
| `set_mctrl()` | 本例必需 | 设置 RTS / DTR 等 modem 输出；本例没有物理输出，故不操作 |
| `get_mctrl()` | 本例必需 | 报告 CTS / DCD / DSR 等 modem 输入；本例将这些状态固定为有效 |
| `set_termios()` | 本例必需 | 根据 `termios` 设置速率、数据位、校验、停止位和流控；本例不模拟这些硬件行为 |
| `type()` | 可选 | 返回端口类型描述字符串；本例返回 `kb-dummy-uart` |
| `request_port()` | 按资源需要 | 申请 IO / MMIO 区域；本例没有资源，返回 `0` |
| `release_port()` | 按资源需要 | 释放前述资源；本例为空 |
| `config_port()` | 配置路径需要 | 自动配置或识别端口类型时设置 `port->type`；本例使用实验专用非零类型 |

`break_ctl`、`enable_ms`、`start_rx`、`pm`、`send_xchar` 等是按功能选择的其他回调，不属于该软件回环的必需集合。每个回调的锁和睡眠约束以 `serial_core.h` 的接口注释为准。

### 数据搬运辅助接口

| 接口 | 作用 |
| --- | --- |
| `uart_port_tx()` | 在端口锁持有时从 TX 软件 FIFO 逐字节取出数据，处理发送统计和写唤醒；本例的发送目标是接收函数 |
| `tty_insert_flip_char()` | 把接收字节和状态放入 TTY flip buffer；可能因缓冲不足而失败 |
| `tty_flip_buffer_push()` | 通知 TTY 层把已写入的接收数据送往 line discipline；不是同步的用户态 `read()` |

真实 UART 通常由 RX IRQ / DMA 完成处理读取硬件 FIFO，再调用接收辅助接口；serial core 没有一个替驱动读取物理 FIFO 的通用 `receive()` 回调。

## DTS 节点与匹配

以下是 `examples/uart/dummy-uart.dtso` 的节点内容。`example,...` 仅是实验用匹配字符串，不是正式产品的硬件 binding：

```dts
serial-dummy {
    compatible = "example,kb-dummy-uart";
    status = "okay";
};
```

该虚拟端口没有实际寄存器、IRQ、时钟或引脚，因此不应编造 `reg`、`interrupts`、`clocks` 或 `pinctrl-*`。真实板通常启用 SoC DTSI 已有的 UART 节点，并按其 binding 和电路配置资源。

```text
DT 节点 → platform_device → dummy_uart_of 的 of_match_table
→ platform probe → 初始化 uart_port → uart_add_one_port → ttyDUMMY0
解绑：platform remove → uart_remove_one_port → 释放端口内存
```

`dummy_uart_of.c` 的模块初始化先注册 `uart_driver` 再注册 platform driver；退出时先撤销 platform driver，使各端口的 `remove()` 运行，再注销 `uart_driver`。`devm_kzalloc()` 只管理端口内存，**不会替代** `uart_remove_one_port()`。

platform 驱动匹配表与端口注册的关键关系是：

```c
static const struct of_device_id dummy_uart_of_match[] = {
    { .compatible = "example,kb-dummy-uart" },
    { }
};
/* platform_driver.driver.of_match_table = dummy_uart_of_match */

port->dev = &pdev->dev;
port->ops = &dummy_uart_ops;
port->line = 0;
ret = uart_add_one_port(&dummy_uart_of_driver, port);
```

片段省略了分配、`port->type` 等初始化和错误处理；可运行版本见 `dummy_uart_of.c`。设备树先匹配 platform 驱动，serial core 再接收已经初始化好的端口；不是 DTS 直接调用 `uart_ops`。

本例固定 `line = 0`、`dev_name = "ttyDUMMY"`、端口数为 1，故产生 `ttyDUMMY0`；DT 的 `serial` alias 不会自动修改这个编号，除非驱动显式读取并使用它。自注册和设备树版本使用相同 TTY 名称，**只能二选一加载**。

从仓库根目录生成**新的测试 DTB**，用它启动虚拟机或测试板，并确认目标内核启用 `CONFIG_OF`：

```sh
dtc -@ -I dts -O dtb -o /tmp/dummy-uart.dtbo \
  embedded/linux/drivers/dummy/examples/uart/dummy-uart.dtso
fdtoverlay -i /path/to/base.dtb -o /tmp/dummy-uart-test.dtb /tmp/dummy-uart.dtbo
```

在目标机执行，驱动目录应出现 `serial-dummy` 设备链接：

```sh
cd embedded/linux/drivers/dummy/examples/uart
make KDIR=/path/to/matching/linux-6.18-build W=1
sudo insmod ./dummy_uart_of.ko
ls -l /sys/bus/platform/drivers/kb_dummy_uart_of/
ls -l /dev/ttyDUMMY0
```

然后使用下方 Python 回环测试；不要同时加载 `dummy_uart.ko`。清理方式取决于上面的模块引用策略。

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

以下加载自注册版本；设备树版本加载 `dummy_uart_of.ko`，其后测试步骤相同。目标内核需要 `CONFIG_MODULES`、`CONFIG_TTY`、`CONFIG_SERIAL_CORE`，构建树及 `Module.symvers` 必须与运行内核匹配：

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
```

- **未修复的内核**：看到 `unload disabled until reboot` 日志时，不尝试强制卸载；重启测试虚拟机清理。
- **已修复的内核**：普通 `rmmod` 在仍有 FD 时应失败。关闭所有 FD 并等模块引用归零，再卸载 `dummy_uart` 或 `dummy_uart_of`。引用可用 `/sys/module/<模块名>/refcnt` 观察；TTY 对象异步释放时，最后一次 `close()` 返回不代表计数已归零。
- 打不开节点先检查模块日志、设备路径和权限；数据不符时检查 raw / echo 和其他进程占用。`2` 秒只是实验超时，不是 UART 的性能指标。

## 迁移到硬件

1. 将虚拟父设备换成真实设备，解析寄存器、IRQ、时钟、复位、引脚和端口编号，按失败路径逐项回收资源。
2. `startup()` 初始化控制器并申请 / 启用接收中断；`shutdown()` 禁止中断与 DMA，清除状态并同步在途处理。
3. `start_tx()` 根据 TX FIFO 空间填充数据并使能发送中断；RX 中断或 DMA 完成后读取状态和数据，处理溢出、校验和帧错误，再推送 flip buffer。
4. 在 `set_termios()` 中按时钟和硬件能力配置波特率、数据位、校验、停止位和流控；正确处理 modem 控制、挂断及电源管理。
5. 通过交叉接线、逻辑分析仪和目标协议测试实际波特率与错误恢复，软件回环不能证明任何电气或时序要求。

相关：[[Linux 串口与 TTY 数据路径]]、[[Linux 驱动异步退出与资源生命周期]]。

## 参考

- Linux 6.18.62：[串口驱动 API](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/serial/driver.rst)、[串口通用 binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/serial/serial.yaml)、[TTY buffer](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/tty/tty_buffer.rst)、[`serial_core.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/serial_core.h)、[`serial_core.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/tty/serial/serial_core.c)。
- TTY 模块引用：[`tty_io.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/tty/tty_io.c) 的 `tty_init_dev()`、`release_tty()` 与 `release_one_tty()`。
- 蜗窝科技：[串口框架概述](https://www.wowotech.net/comm/serial_overview.html)、[TTY 软件架构](https://www.wowotech.net/tty_framework/tty_architecture.html)。
