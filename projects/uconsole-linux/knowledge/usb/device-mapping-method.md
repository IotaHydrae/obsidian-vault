# USB 设备、控制器与物理接口的映射

> VID:PID 识别型号，sysfs/udev 定位连接路径；板上丝印必须由原理图或受控插拔补证。

## TL;DR

- 按 `lsusb` → 拓扑 → sysfs → udev 属性的顺序定位。
- Bus/Device 号会变化，不能当稳定设备身份；相同型号也可能有重复或缺失序列号。
- `ENV{}` 与 `ATTRS{}` 来源不同，多个父链匹配必须满足同一祖先规则。
- 本文不带任何其他工程的接口命名表，CM5 IO Board 映射仍需单独观察。

## 四步法

| 步骤 | 只读命令 | 结果 |
| --- | --- | --- |
| 认设备 | `lsusb` | VID:PID、设备描述 |
| 认拓扑 | `lsusb -t` | hub、端口链、接口驱动、协商速率 |
| 认路径 | `readlink -f /sys/bus/usb/devices/<DEVICE>` | 控制器到端口的系统路径 |
| 取规则字段 | `udevadm info -q property -p <SYSFS_PATH>` | ID_PATH 等 udev 数据库属性 |

要关联 `/dev/video*`、`ttyACM*`、input 节点时，从对应 class symlink 追溯父设备。
一个复合 USB 设备可有多个接口与节点，不能将它们误当独立实体。
xHCI 通常提供 USB2/USB3 根 hub，枚举到哪条取决于设备和链路，不能仅数 root hub 判断控制器数。

## 规则匹配

`udevadm info -q property` 提供写 `ENV{}` 使用的属性；
`udevadm info -a -n <NODE>` 展示父链属性，供 `ATTRS{}`/`KERNELS` 判断。
同一规则的多个父属性匹配必须落到同一祖先；控制器节点不具有叶设备的 VID/PID。
混用时可让控制器条件与已由 udev 生成的设备属性分别匹配，但须核对规则执行顺序。

`ID_PATH` 的生成依赖实际安装的 persistent rules。最小 rootfs 没有该字段时，
先检查规则、运行事件和数据库，不写一个永远不匹配的条件。
驱动自动实例号、Bus 号和 `/dev/videoN` 可能随内核或连接顺序改变。
端口路径也只在控制器拓扑不变的范围稳定，不是跨硬件的全球身份。

## 从逻辑路径到丝印

逐次只改变一个接口连接，记录前后拓扑与实物位置，再查对应载板原理图。
对每个口分别记录 USB2/USB3 通道、hub 路径和可用角色；供电能力与数据速率是另一层。
软件枚举不能证明插座布线、线缆质量、VBUS 电流或 Type-C PD 协商。

## 证据与工具边界

依据为 [udev(7)](https://www.freedesktop.org/software/systemd/man/latest/udev.html)
及目标系统的 sysfs/udev 输出。原始输出可能包含序列号，分享前脱敏。
本仓库没有迁入其他板卡的 USB map 程序或固定端口表，避免把它们应用到 CM5。
