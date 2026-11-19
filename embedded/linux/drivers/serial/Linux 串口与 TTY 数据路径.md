# Linux 串口与 TTY 数据路径

> UART、USB 虚拟串口和 PTY 都可能通过 TTY 接口提供字节流，但硬件控制、设备节点和启动时机不同。

## 数据路径

```text
用户程序 read / write
↔ TTY 核心与 line discipline ↔ TTY 驱动
↔ UART 控制器 / USB 串口 / PTY 等具体实现
```

| 项目 | 职责 |
| --- | --- |
| TTY 核心 | 提供终端接口、缓冲与通用管理 |
| line discipline | 处理行编辑、回显、控制字符等；行为由当前配置决定 |
| UART 驱动 | 管理寄存器、FIFO、时钟、收发中断或 DMA |
| console / earlycon | 内核日志输出通道；早期可用性依赖平台及启动配置 |
| getty | 在已有终端上提供登录提示，不负责把设备变成 early console |

发送二进制协议时应明确配置 raw 模式、波特率、数据位、校验、停止位和硬件 / 软件流控；不能假设默认的回显或换行处理适合协议。USB CDC ACM 的名义波特率与实际 USB 线速不等同。

## 调试

- 使用 `dmesg` 和设备树 alias / 驱动绑定确认节点；`ttyS*`、`ttyAMA*`、`ttyUSB*`、`ttyACM*` 和 `ttyGS*` 属于不同实现，编号依环境变化。
- 用 `stty -F /dev/实际节点 -a` 检查当前设置，并检查是否有 getty 或其他进程占用。
- 硬件 UART 同时检查引脚复用、电平、电源、时钟、TX/RX、GND 以及 RTS/CTS 的接线和配置。
- 无早期日志时区分 bootloader 输出、earlycon、正常 console 和用户空间 getty；一个阶段可用不代表其他阶段也可用。

USB Gadget 串口登录见 [[g_serial - systemd getty@ttyGS0]]；引脚配置见 [[Linux pinctrl 与 GPIO]]。

## 参考

- 蜗窝科技：[TTY 软件架构](https://www.wowotech.net/tty_framework/tty_architecture.html)、[应用视角](https://www.wowotech.net/tty_framework/application_view.html)、[串口框架](https://www.wowotech.net/comm/serial_overview.html)。
- Linux 6.18.62：[TTY 驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/tty/index.rst)、[串口驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/serial/driver.rst)。
