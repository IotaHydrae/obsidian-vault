# USB 3.0 设备的枚举过程

> USB 3.0 SuperSpeed 沿用设备识别、地址分配、配置选择和接口驱动绑定的基本模型；链路建立、复位和 EP0 参数需要按 SuperSpeed 规则理解。

## TL;DR

- **先确认实际速度**：设备支持 USB 3.0，不代表本次连接一定运行于 SuperSpeed；以主机日志、`lsusb -t` 等信息为准。
- **SuperSpeed 使用独立的发送、接收差分信号对与链路训练**；USB 2.0 的 D+/D− 上拉和 High-Speed chirp 不能解释这条路径。
- **EP0 最大包长为 512 字节**；设备描述符中的 `bMaxPacketSize0=9` 表示 `2^9`，不是 9 字节。
- **Linux 6.18.62 对 SuperSpeed 设备不使用 USB 2.0 的 new scheme**；抓包时不应预期地址 0 上先读最多 64 字节描述符的那套顺序。

范围：USB 3.0 的 SuperSpeed 设备，以 **Linux 6.18.62 LTS 主机侧**为实现参照。标准请求、描述符树、配置与驱动绑定的共同机制见 [[USB 2.0 设备的枚举过程]]。

## 连接与枚举的边界

SuperSpeed 使用与 USB 2.0 D+/D− 分离的信号路径。主机和设备需要建立可用的 SuperSpeed 链路，再进行设备识别、寻址与配置；链路训练和复位不能直接套用 USB 2.0 的电平与 chirp 流程。

支持 USB 3.0 且兼容 USB 2.0 的设备，在电缆、端口或链路条件受限时可能通过 USB 2.0 路径连接。此时应按实际运行速度分析枚举；`bcdUSB` 是协议版本声明，主机使用 xHCI 也不能证明当前设备运行于 SuperSpeed。

## EP0 与 Linux 初始化差异

| 项目 | USB 3.0 SuperSpeed |
| --- | --- |
| EP0 最大包长 | 512 字节；`bMaxPacketSize0=9` 按指数编码 |
| 设备描述符长度 | 仍为 18 字节；最大包长不等于描述符长度或控制传输总长度 |
| Linux 初始化方案 | `use_new_scheme()` 对 SuperSpeed 返回 false，不执行 USB 2.0 new scheme 的预读流程 |
| 地址分配 | `hub_set_address()` 可经 HCD 的 `address_device` 回调完成；xHCI 使用 Address Device 命令 |
| 后续步骤 | 读取设备能力及配置等描述符，注册设备，选择配置并匹配接口驱动 |

Linux 的方案名称是兼容策略标签，不意味着 SuperSpeed 使用 USB 2.0 的物理复位或包级协议。xHCI 控制器自主完成的地址操作也不必表现为 usbcore 提交的普通 `SET_ADDRESS` URB。

BOS 用于描述设备能力，**并非 USB 3.0 独有**：Linux 在 `bcdUSB >= 0x0201` 时尝试读取，USB 2.x 扩展设备也可能使用。配置成功与接口驱动可用仍是不同检查点。

## 抓包与排障

先用 `lsusb -t` 确认设备实际速度，再结合日志判断故障发生于连接/链路、EP0、配置还是接口驱动阶段。USB 2.0 路径可用而 SuperSpeed 路径失败时，应检查线缆的 SuperSpeed 信号路径、连接器、PHY 和链路状态。

usbmon 能观察软件向 HCD 提交的请求及其完成状态，不能完整反映物理链路训练、复位和控制器自主事务。这些阶段需要结合控制器跟踪、PHY 状态或硬件协议分析仪判断；不能仅凭缺少某条 URB 推断线上没有对应操作。

## 源码与参考资料

- Linux 6.18.62 [`hub.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/usb/core/hub.c)：`use_new_scheme()`、`hub_port_init()`、`hub_set_address()` 与 BOS 读取条件。
- [`xhci.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/usb/host/xhci.c)：`xhci_address_device()` 与控制器地址命令路径。
- [`ch9.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/uapi/linux/usb/ch9.h)：标准请求、描述符与端点包长定义。
- [usbmon](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/usb/usbmon.rst)：软件抓包的观察范围。
