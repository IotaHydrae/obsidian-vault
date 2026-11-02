# uConsole：自制核心板的必选与可选接口

## TL;DR

- 以内置屏幕和原键盘可用为目标，必须保留兼容的电源／地／电平、四通道 DSI、屏复位／背光控制和 USB Host→主板 Hub 链路。
- 至少一种启动存储必须具备；主板 microSD 是条件必选，有自带 eMMC／其他可启动存储时可不实现该槽。
- HDMI 为可选；主板 Wi-Fi／蓝牙、声音、扩展槽和额外 USB Device 按整机需求取舍。
- 主板 J1 是 Clockwork 定义的 200 针核心槽，J4 是自定义 52 针扩展槽；均不能仅按连接器外形认定标准电气接口。

## 设计目标和证据范围

这是**替代核心板接入现有 uConsole 主板**的接口分析，以官方 V3.14-V5 主板原理图和 CM4 adapter 为准。
不要求新 SoC 复制树莓派的 GPIO 编号，但必须在物理触点提供相同功能、电平、方向和安全上电状态。
原理图事实、当前项目软件约定和设计推导分别标明；尚未制作新 PCB 或测量信号／电源裕量。
CM3／CM3+ 有直插主板的官方兼容声明与社区实机依据，不需要 CM4 转接卡；
保留针扩展本身不否定兼容。匹配系统和已报告的关机供电问题见[CM3 直插说明](../../reference/clockworkpi-uconsole/README.md#cm3-能否直接插入)。
[官方资料目录](../../reference/clockworkpi-uconsole/README.md)包含原件清单、完整 J1/J4 针表和制造数据。

## 最小接口矩阵

“必选”以用户要求的内屏＋键盘终端为判据；“条件必选”表示功能可以替代实现，不能直接删掉功能本身。

| 功能 | 分类 | 需要的链路／信号 | 少了会怎样 |
| --- | --- | --- | --- |
| 核心供电／地／IO 参考 | 必选 | 与主板电源树一致的输入、所有规定 GND、BANK0/BANK1 参考及安全时序 | 无法可靠启动，或发生供电冲突／回灌 |
| 内屏视频 | 必选 | DSI1 四对数据＋一对时钟，支持 CWU50 所需命令／RGB888 模式 | 只能有背光或完全无显示 |
| 内屏复位／背光 | 必选 | LCD_RESET、BL_CTRL、显示 3.3／1.8 V 和背光电源 | DSI 连线存在也不足以点屏 |
| 键盘／轨迹球 | 必选 | 一路 USB2 Host 到 GL850G 上游，Hub／下游键盘 5 V 电源 | 原有 USB HID 无法使用 |
| PMIC 管理 | 保留主板电源功能时必选 | PMU-SCK/SDA 的 I²C 控制器或可靠 GPIO I²C，AXP 电源驱动／regulator 配置 | 不能假定默认 rails、充电／电量和显示供电正常 |
| PMIC IRQ／电源键事件 | 保留完整电源交互时必选 | PMU-IRQ 输入、正确上拉／中断类型 | 缺失软件电源键／告警等事件；不等同硬件一定不能上电 |
| 启动存储 | 至少一种必选 | SoC 可启动的 eMMC、microSD 或其他存储 | 无启动系统介质 |
| 主板 microSD 槽 | 以该槽启动／扩展存储时必选 | SDX_CLK/CMD/D0–D3、供电／VREF、目标控制器与启动 ROM 支持 | 该卡槽不能用；自带 eMMC 可独立启动则可选 |
| 主板 USB-C 数据 | 可选，依恢复／OTG 需求 | 独立 USB0_DP/DM，及合适的 Device/OTG 控制与供电检测 | USB-C 供电存在不代表可传数据或刷机 |
| micro-HDMI | **可选** | HDMI 差分对、DDC、HPD、需要时 CEC／保护／电平处理 | 不能接外屏，内部 DSI 不因此必然失效 |
| Wi-Fi／蓝牙 | 可选实现路径 | 模块自带无线；或接主板无线的 SDIO／UART／使能／唤醒和电源 | 没有对应无线功能；不要求两套都实现 |
| 内置扬声器／耳机／麦克风 | 按声音需求选 | 对应 PWM／模拟音频路径、检测／功放使能；不能仅预留 I²S 就称兼容 | 原声音功能缺失 |
| J4 扩展／4G | 可选 | 主板 Hub 下游 USB、扩展电源及模块实际需要的 GPIO 等 | 原扩展模块不可用 |
| 摄像头、其他 GPIO、调试 | 可选／建议保留调试 | J4 CAM1、GPIO；SoC 专用 UART／恢复接口 | 对应扩展功能缺失；调试恢复会更难 |

这是功能覆盖判据，不是每根引脚都应接到新 SoC。电源输出、保留针、无关模拟针按各自规则处理。

## 主板 J1 与 CM4 BTB 的关键映射

下面 J1 是 **Clockwork 200 针金手指**；右列是 adapter J2/J3 所用的 **CM4 全局针号**。
主板网名 `GPIO0` 不保证等于 BCM GPIO0；先经过转接卡重映射。

| 用途 | Clockwork J1 针／网名 | 现有 CM4 BTB 针／信号 |
| --- | --- | --- |
| PMIC SCL | 5 / GPIO1 → PMU-SCK | 35 / ID_SC（GPIO1） |
| PMIC SDA | 3 / GPIO0 → PMU-SDA | 36 / ID_SD（GPIO0） |
| PMIC IRQ | 9 / GPIO2 | 58 / GPIO2 |
| 屏复位 | 27 / GPIO8 | 39 / GPIO8 |
| 背光控制 | 29 / GPIO9 | 40 / GPIO9 |
| USB Host D+ / D− | 165 / USB_DP；167 / USB_DM | 105 / USB_P；103 / USB_N |
| USB ID | 168 / USB_OTGID | 101 / USB_OTG_ID |
| SD CLK / CMD | 10 / SDX_CLK；12 / SDX_CMD | 57 / SD_CLK；62 / SD_CMD |
| SD D0 / D1 / D2 / D3 | 16 / 18 / 22 / 24 | 63 / 67 / 69 / 61 |
| DSI D0 N/P | 96 / 94 | 175 / 177 |
| DSI D1 N/P | 120 / 118 | 181 / 183 |
| DSI CLK N/P | 102 / 100 | 187 / 189 |
| DSI D2 N/P | 114 / 112 | 193 / 195 |
| DSI D3 N/P | 108 / 106 | 194 / 196 |

完整信号及地参考见 [J1 表](../../reference/clockworkpi-uconsole/core-edge-pinout.csv)和
[CM4 BTB 表](../../reference/raspberry-pi-cm/cm4-pinout.csv)。不能只按本摘要的高速针而省略地回流。
上述 SD 是 CM4 Lite 的物理卡槽路径；CM4／CM5 eMMC 型号的启动／SD 路由限制仍按模块规格。

## 内屏与键盘为何不能仅有接口名字

**图纸事实：** J302 屏接口使用 DSI1；LCD_RESET 来自主板 GPIO8、BL_CTRL 来自 GPIO9。
OCP8178 是独立背光控制链，面板另需 DISP_3V3／DISP_1V8。原理图给出逻辑连接，
不能单独证明实际 rail 电压、脉冲波形和复位时序。
**软件约定：** 当前 `panel-cwu50` 使用四通道 RGB888，并保留两套初始化表；
背光驱动实现其控制时序。新 SoC 必须具备匹配的 DSI Host／PHY 与驱动，不能仅声称支持 MIPI CSI。

**图纸事实：** J1 USB_DP/DM → U501 GL850G 上游 DP0/DM0；
Hub port1 DP1/DM1 → J502 四针键盘口，port2 → J503 USB-A，port3/4 → J4。
键盘图中 MCU 通过 USB 连接；当前设备曾枚举为复合 HID。
因此这里需要 **Host** 角色。单有 USB Device／rpiboot／ADB 不足以驱动键盘；
J501 USB-C 的 USB0_DP/DM 是另一组金手指触点 160/162，不应与 Hub 上游混接。

## 电源树和控制边界

**图纸事实：** SYS_5V 同网 CM_VBAT；SYS_3V3 同网 CM_3V3；
SYS_1V8 同网 CM_1V8／DISP_1V8。原理图裸电池正极另标 VBAT。
CM4 adapter 有 ETA1096 升压路径，把 CM_3V3 接到 CM_VBAT，再接 CM4 的 5 V 输入针。
这不证明任意实际板卡都具备指定电流，也不证明两个升压源可以随意并联。

| Clockwork J1 | 图纸网名 | 新设计处理原则 |
| --- | --- | --- |
| 183–186 | CM_1V8 | 明确是否由载板供入；防止与核心板稳压输出互顶 |
| 189–190 | CM_VDAC | 单独识别其模拟供电用途，勿与 TVDAC 信号混淆 |
| 191–194 | CM_3V3 | 核对载板源、峰值电流、启动和掉电顺序 |
| 197–200 | CM_VBAT | 在本主板图中是 SYS_5V 网，勿按名字接裸电池 |
| 39/40、41/42 | BANK0/1_VREF | 核对载板默认 IO 电平，不能让新 SoC 输出不兼容电平 |
| 179 | VDD_CORE | CM3+ 官方定义 DO NOT CONNECT；不能作为通用电源使用 |

当前项目通过 I²C GPIO0/1、IRQ GPIO2 管理 `x-powers,axp221` 兼容路径；
V5 图纸 U101 标为 AXP228，器件兼容声明不能代替对实际装配型号的确认。
电池计量／充电参数、温度、峰值供电和安全关机需按电芯、实际 PMIC 与新核心的功耗重新验证。

## 可复用资料和未知项

[主板／转接卡／A06／R01／键盘／4G 资料](../../reference/clockworkpi-uconsole/README.md)是后续设计起点。
A06/R01 有其他 SoC 接入该主板的例子，适合对照电源方向和功能映射，不保证新 SoC 可直接套用。
CM4 overlay（工程源码：`kernel/patches/0004-arm64-dts-add-uconsole-cm4-overlay.patch`）、
CM5 overlay（工程源码：`kernel/patches/0003-arm64-dts-add-uconsole-cm5-overlay.patch`）、
面板驱动（工程源码：`kernel/patches/0001-drm-panel-add-clockworkpi-cwu50.patch`）提供当前软件合同。

尚未完成：目标 SoC／封装与启动源选型、完整 power budget、PCB 层叠／阻抗、连接器额定电流、
USB 电源角色／保护、复位／关机、实物插槽导通与新板 EMC／热设计。
这些未知项不妨碍确定“内屏 DSI＋键盘 USB Host 必选、HDMI 可选”的当前功能范围。
