# Linux 串口与 TTY 数据路径

> UART、USB 虚拟串口和 PTY 都可能通过 TTY 接口提供字节流，但硬件控制、设备节点和启动时机不同。

## TL;DR

- 串口程序先确认实际设备节点、终端参数和独占使用情况；TTY 默认处理可能改变字节流。
- 硬件 UART 还要检查 pinmux、收发交叉、电平、时钟和流控。
- earlycon、正常内核 console 和 getty 处于不同启动阶段，不能互相替代。

适用范围：Linux 6.18 LTS。

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

## 收发路径与终端配置

UART 驱动通常通过 `uart_driver`、`uart_port` 和硬件相关操作接入串口核心；具体设备使用中断、DMA 或其他方式处理 FIFO。收到的数据经 TTY 缓冲与 line discipline 到达应用，发送则由应用写入 TTY 后交给驱动与硬件。USB 串口和 PTY 的底层不是 UART 寄存器，但仍可能复用 TTY 用户接口。

| 配置 / 现象 | 含义 |
| --- | --- |
| canonical 模式 | 可进行行编辑并按终端规则交付数据，不适合直接假定二进制包边界 |
| echo / 换行处理 | 可修改或回显输入输出；测试协议时必须明确设置 |
| 软件流控 XON/XOFF | 某些字节可能被解释为流控字符，而不是业务载荷 |
| 硬件 RTS/CTS | 接线、极性和两端设置必须一致，否则发送可能被阻塞 |
| `VMIN` / `VTIME` | 非 canonical 读取完成条件；还受阻塞模式和实际数据影响 |

一次 `read()` 的长度取决于可用数据和终端设置，不能假定与设备发送的消息一一对应。应用协议需要自己的长度、分隔符或其他边界，并处理部分读写。

### 非 canonical 模式的读取

| `VMIN` | `VTIME` | 常见阻塞读取行为 |
| --- | --- | --- |
| 0 | 0 | 立即返回当前可用数据，也可能返回 0 |
| 0 | 大于 0 | 等待数据或超时；`VTIME` 的单位是 0.1 秒 |
| 大于 0 | 0 | 等待满足最小字节数的条件 |
| 大于 0 | 大于 0 | 首字节到达后启动字符间超时，或满足字节数后返回 |

具体 `read()` 长度也受请求长度、信号及 `O_NONBLOCK` 等影响。检查配置可执行 `stty -F "$TTY_DEV" -a`，其中 `TTY_DEV` 是实际目标节点；排查前还应确认该节点没有被 getty 或其他程序独占。

## console、earlycon 与 getty

bootloader 串口输出、Linux `earlycon`、正常 console 和用户空间 getty 是四个阶段。正常 console 使用相应驱动；earlycon 依赖早期硬件和启动参数；getty 在系统启动后提供登录。验证时记录最后可见的阶段，再检查对应串口和启动参数；USB Gadget 需要主机枚举，不能假定上电后立即可输出早期日志。

## 典型故障

| 现象 | 优先定位 |
| --- | --- |
| 全部乱码 | 双端波特率、校验位、电平标准与时钟来源 |
| 可收到但不能发送 | TX pinmux、交叉连接、流控与输出缓冲 |
| 数据夹杂回显或换行变化 | line discipline、echo、输入输出转换 |
| 应用一直等待完整包 | `VMIN` / `VTIME`、非阻塞设置及协议帧边界 |
| bootloader 有输出但内核无输出 | `earlycon` / `console` 参数、驱动配置、设备树和接管时机 |

## 调试

- 使用 `dmesg` 和设备树 alias / 驱动绑定确认节点；`ttyS*`、`ttyAMA*`、`ttyUSB*`、`ttyACM*` 和 `ttyGS*` 属于不同实现，编号依环境变化。
- 用 `stty -F /dev/实际节点 -a` 检查当前设置，并检查是否有 getty 或其他进程占用。
- 硬件 UART 同时检查引脚复用、电平、电源、时钟、TX/RX、GND 以及 RTS/CTS 的接线和配置。
- 无早期日志时区分 bootloader 输出、earlycon、正常 console 和用户空间 getty；一个阶段可用不代表其他阶段也可用。

虚拟串口驱动实践见 [[Linux Dummy UART 驱动]]；USB Gadget 串口登录见 [[g_serial - systemd getty@ttyGS0]]；引脚配置见 [[Linux pinctrl 与 GPIO]]。

## 参考

- 蜗窝科技：[TTY 软件架构](https://www.wowotech.net/tty_framework/tty_architecture.html)、[应用视角](https://www.wowotech.net/tty_framework/application_view.html)、[串口框架](https://www.wowotech.net/comm/serial_overview.html)。
- Linux 6.18.62：[TTY 驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/tty/index.rst)、[串口驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/serial/driver.rst)。
