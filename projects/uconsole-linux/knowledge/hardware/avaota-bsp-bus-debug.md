# Avaota BSP：总线与引脚的受控调试

## TL;DR

- 沿“板图／live DT → driver与consumer → pinctrl／clock／reset／regulator → 已知激励 → 预期结果”排查。
- 列设备和读配置是第一层观察；I²C读、UART收包、PWM捕获和PCI重扫都会与目标交互，需单独建立条件。
- 来源为 [Avaota Linux BSP 手册](../../reference/avaota-a1/bsp-manual-index.md)，适用于方法复用；不照搬示例编号、地址或5.15私有ABI。

## 先确定实际控制器

保存板型／revision、内核release／配置／源码pin、live DT 与驱动绑定，再按原理图从物理触点查到 SoC pad。
Clockwork J1、模块 BTB、SoC bank/pin、Linux GPIO chip/offset不是同一编号域。
GPIO/IRQ specifier、`CONFIG_AW_*`、`twi_drv_used`、DMA请求、debugfs节点按实际驱动binding核对。
示例来自不同 SoC，不能把文档中的地址或总线号直接写到目标板。

下列为第一层枚举示意，不包含扫描、解绑或寄存器写。实际节点按存在的文件选择：

```sh
uname -r
cat /proc/interrupts
ls /sys/bus/platform/devices
ls /sys/bus/i2c/devices
ls /sys/bus/spi/devices
ls /sys/class/tty
ls /sys/class/pwm
```

通过设备的 `driver`、`of_node`、`name` 和日志关联目标；debugfs未挂载／节点缺失时，
先核对配置和挂载状态，缺文件不自动判定硬件损坏。
新增测试要写明外部接线、合法电压、唯一owner、已知发送数据及失败／恢复条件。

## GPIO／Pinctrl

**观察：** 读取 pinctrl-devices／handles／maps 以及目标控制器的 pinmux／pinconf，
核对引脚消费者、active/sleep状态、复用、上下拉和drive；结合对应IRQ计数与probe错误。
GPIO字符设备和pinctrl debugfs是不同接口，不能把pinctrl当作通用sysfs/ioctl GPIO控制器。

**功能试验：** 选择原理图上确认可用的闲置pad。外部给定低／高或受控边沿，比较输入与IRQ计数；
输出则用独立仪表确认实际电平／时序。成功申请GPIO只证明软件资源分配，不证明管脚导通。
复位、PMIC、DDR、启动存储等已被使用的引脚不作通用loopback；变化前保留恢复状态。
6.18新驱动优先核对当前descriptor/gpiod API，不移植旧全局整数GPIO作为新合同。

## TWI／I²C

**观察：** 先查看adapter名称、live DT、已绑定client和频率／pinmux／供电依赖；
区分no-ACK、START/STOP失败和timeout，不能只根据地址列表认定控制器正常。
需要用户态访问时确认i2c-dev，但总线号由实际枚举决定。

**事务与判据：** 对已知地址执行器件手册允许的ID／状态读取，核对宽度、字节序和合法值；
读取可能清状态、弹FIFO或改变内部地址指针，属于主动事务。
全地址探测、dump和FORCE不能作为默认无损检查。EEPROM写回只选明确可清空区域，
使用独立预期字节并保留原内容；PMIC／启动EEPROM不作通用写测试。
VIO和上拉按目标电源域，不将手册某例的3.3V推广到全部T527接口。

## SPI

**观察：** 查controller与client绑定、pinctrl、CS、mode、频率、位宽和DMA，先区分主机／从机模式。
**功能试验：** 合法loopback接线或已知从机，发送长度明确、变化充分的模式，逐字节比较接收，
同时检查SCK／CS极性与边沿；mode不匹配和CS时序错误不因设备节点存在而排除。
loopback只证明该条件下的数据通路，不验证真实从机协议或最高频率。
对SPI NOR/NAND发送数据可能改写存储，不能将flash作为随意spidev测试对象。

## UART

**观察：** 从aliases／of_node／driver识别端口，核对波特率、数据位、校验、停止位和流控；
检查是否被console、getty、蓝牙或其他程序占用。A1启动参数使用ttyAS0，手册出现的ttySx不是固定设备名。
读取UART会消费输入，不是被动读取配置，可能与现有console／蓝牙抢数据。

**功能试验：** TTL电平和地参考正确后，使用独立对端或已确认loopback，
发送带序号、长度和校验的数据，比较完整接收及错误计数；`echo`能写端口不代表TX实际输出或RX正常。
不要连接RS-232电压到SoC TTL pad。修改console端口前必须保留另一条管理路径。

## PWM

**观察：** 由设备／driver确认实际pwmchip、npwm和consumer，核对DT通道、复用、clock和极性。
**功能试验：** 闲置输出上设定合法period／duty，先配置后enable，留出测量窗口；
外部示波器确认频率／占空比／极性，再disable并释放。写回sysfs值不能替代波形验证。
capture可能启动硬件并等待边沿，是主动测量；结果与已知信号源及预先定义容差比较。
文档中的pwmchip号、固定源时钟假设或特定capture输出不移为其他内核的普遍合同。

## PCIe

**观察：** 先查lane共享模式、PHY、供电、refclk、PERST和RC／endpoint角色；
用实际PCI枚举、driver绑定、BAR／中断／链路速率／宽度与错误日志判断在哪层失败。
训练失败也可能是模式／clock／reset，不只归因PCB或电压。

**功能试验：** 目标endpoint已枚举后按设备用途验证；存储先确认介质身份并从可读区域读取，
写入／格式化与链路调试分开。手册的`mkfs.ext4 /dev/nvme0n1`会清空介质，不列入默认检查。
rescan、remove、reset和全配置空间dump会访问设备或改变状态，不假定全部安全无副作用。

## 文档纠错和跨版本边界

原文GPIO示例有无效`linux,input-type`文本；pinctrl的active/sleep例不能解释成“pinctrl-0永远default”。
TWI的部分DTS把`<`变成中文，小例程中`TWI_SLAVE_FORCE`也不能代替标准`I2C_SLAVE_FORCE`。
SPI示例存在对宏取地址／工具参数混用，且BIT日志本身出现TX/RX不同和`data error`，不能当作成功样例。
UART挂载debugfs与随后dynamic_debug路径不一致；PWM示例有devm资源释放／重复分配问题。
这些错误不进入新驱动或测试；使用当前固定源码／头文件和工具`--help`复核后重写最小例程。

6.18移植可保留物理接线和已知数据判据，但应重建binding、工具、驱动和观测路径，见
[T527迁移评估](../kernel/t527-linux-6-18-migration.md)。本次只读文档，无设备命令执行或功能通过结论。
