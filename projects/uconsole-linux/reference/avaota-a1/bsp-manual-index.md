# Avaota Linux BSP 驱动手册索引

## TL;DR

- 固定 2026-11-01 的官方 Linux BSP 分类：18 页，含介绍与 17 个子系统，不与 RTOS／HAL 教程混合。
- 可复用的是配置、依赖、观测、受控激励与判据；示例总线号、GPIO、寄存器、DT 属性和 CONFIG_AW_* 不是跨内核通用接口。
- 原始 HTML 及摘要见 [手册来源清单](bsp-manual-sources.json)，方法已提炼到两篇知识，不执行原文测试命令。

## 官方页面与阅读用途

分类入口：[BSP 驱动手册](https://docs.avaota.fun/category/bsp-%E9%A9%B1%E5%8A%A8%E6%89%8B%E5%86%8C/)。
以下均为该 Linux BSP 分类实际链接；此目录本次没有独立 CAN、ADC、热管理或音频页。
显示 DSI／GPU、USB 和启动存储需结合已有 A1 源码／板图继续查，不用名称相似的 RTOS 示例填充缺项。

| 子系统 | 官方页 | 重点提取的定位／验证方法 |
| --- | --- | --- |
| BSP 介绍 | [bsp-intro](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/bsp-intro/) | bsp 的 Kconfig／Makefile／include／configs 接入，以及跨内核条件编译 |
| 时钟 CCU | [ccu](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/ccu/) | consumer、父时钟／频率／门控和 clk_summary；改频前确认依赖 |
| 加密引擎 CE | [ce](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/ce/) | 算法注册／驱动身份和已知向量；区分硬件驱动与软件 fallback |
| 以太网 EMAC | [gmac](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/gmac/) | MAC／PHY／MDIO／接口模式、链路、丢包和对端数据验证 |
| GPIO／Pinctrl | [gpio](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/gpio/) | 真实 pin ownership／mux／bias／drive 和输入输出／IRQ 外部观测 |
| 硬件自旋锁 | [hwspinlock](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/hwspinlock/) | provider／lock ID／跨核 owner，受控争用与超时 |
| 红外接收 | [ir-rx](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/ir-rx/) | receiver、协议、keymap 和已知遥控器的事件对应 |
| 红外发送 | [ir-tx](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/ir-tx/) | 发射载波／脉宽与独立接收端的已知帧验证 |
| LED 控制 | [ledc](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/ledc/) | 灯珠协议／颜色顺序、时钟和已知测试图案 |
| MIPI CSI | [mipi-csi](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/mipi-csi/) | sensor供电／I²C／MCLK、media graph、格式和真实采集帧 |
| PCIe | [pcie](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/pcie/) | PHY／refclk／PERST／资源、链路与endpoint枚举；存储写入另行处理 |
| PWM | [pwm](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/pwm/) | period／duty／polarity／mux及示波器；capture是主动测量 |
| RTC | [rtc](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/rtc/) | RTC身份、时间读回／保持／闹钟；唤醒必须实际观察 |
| SID／eFuse | [sid](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/sid/) | NVMEM布局、偏移、身份／校准；烧写流程不作为默认诊断 |
| SPI | [spi](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/spi/) | controller/client、CS／mode／频率、loopback或已知器件数据 |
| Timer | [timer](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/timer/) | clocksource／clockevent与硬件timer中断，独立时间基准测量 |
| TWI／I²C | [twi](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/twi/) | adapter/client地址、pinmux、上拉／VIO、ACK／超时和合法事务 |
| UART | [uart](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/uart/) | DT aliases／driver／console、波特率和有长度／校验的数据往返 |

## 已整理的工程方法

- [总线与引脚调试](../../knowledge/hardware/avaota-bsp-bus-debug.md)：GPIO/Pinctrl、I²C、SPI、UART、PWM、PCIe。
- [系统与采集外设调试](../../knowledge/hardware/avaota-bsp-system-debug.md)：CCU、CE、EMAC、跨核、红外、LEDC、CSI、RTC、SID、timer。
- [T527 向 6.18 LTS 迁移评估](../../knowledge/kernel/t527-linux-6-18-migration.md)：上游已有支持、缺口和两种迁移路线。

## 保存和纠错原则

每项永久方法记录前提、纯观察与主动事务的区别，以及能判定功能的预期数据／外部测量。
成功 probe、命令返回 0、一次温度或吞吐数字都不是普遍验收阈值。
原始网页还包含板级地址、格式错误／跨 SoC 示例和失败日志；这些保留在原件，知识篇明确不照抄。
不要把 I²C 探测／寄存器读取、UART接收、PWM捕获、PCI rescan、RTC闹钟、SID写入归为同一种“只读”。
本文是手册导航和独立提炼，不是厂商原文镜像，也不声称所有章节均对应当前 A1／CM2 的实际选中驱动。
