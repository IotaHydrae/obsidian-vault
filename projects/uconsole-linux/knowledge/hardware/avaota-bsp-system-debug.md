# Avaota BSP：时钟、系统与采集外设的调试判据

## TL;DR

- 官方 [Linux BSP 手册](../../reference/avaota-a1/bsp-manual-index.md)覆盖多代全志示例，先确认真实驱动／DT／依赖，再选可测的外部判据。
- MMIO读、RTC闹钟、跨核锁、SID选择、PHY寄存器等不都属于无副作用观察，默认不执行原文命令。
- CSI是摄像头输入，不能替代DSI屏幕输出；日志／节点／一次吞吐数字不等于功能验收。

## 时钟 CCU

从外设consumer追到父时钟、PLL、gate、rate、reset和regulator；在实际debugfs已挂载时，
`clk_summary`可帮助定位parent／rate／enable计数，再与对应控制器probe日志关联。
时钟被列出或计数非零，只证明框架状态，不证明引脚上的实际晶振或总线时序。
要测输出频率，选择明确可输出的测试点和独立时间基准；不要为测量随意改共享PLL／DDR时钟。
6.18下核对mainline CCU/reset binding，不能复制BSP时钟ID、物理地址或把`clk_get()`错误指针当NULL。

## 加密 CE

先通过实际算法注册／driver与配置确认硬件驱动候选，再运行公开的已知答案向量，
比较完整摘要／密文／解密结果，并记录算法、mode、key长度、buffer条件和实际driver。
OpenSSL成功或性能提升不能单独证明硬件CE参与，软件fallback可能产生同样正确结果。
文档旧`crypto_alloc_hash/crypto_hash_*`接口不作为6.18新代码模板；重新核对shash/ahash等当前API。
不将实际安全密钥或业务数据写入测试日志／知识库。

## 以太网 EMAC／GMAC

沿MAC → MDIO／PHY → 接口模式 → clock/reset/regulator → link → driver／IRQ／DMA统计排查。
先记录实际接口名、carrier、速率／双工、丢包和错误，再核对对端／网线／PHY的真实条件。
读PHY状态寄存器也要查clear-on-read等语义，不把MDIO读取当任意无损探测。

功能判据是已知对端的数据往返／文件校验和及错误计数变化；吞吐只记录该链路条件下的测量值。
`127.0.0.1`不经过实体MAC／PHY，不能用于验收网卡。
内／外loopback各覆盖有限路径；delay、MTU／jumbo、重新协商或复位均为单独变量，变更会断链。
文档的不同rxdelay步长互相矛盾，且有`iperf3 -s -u`和`&;`错误示例，不原样移入工具。

## 硬件自旋锁 HWSPINLOCK

先确认provider、lock ID、所有参与的CPU／固件owner和超时／释放协议。
能看到驱动节点不表示锁未被使用；读取`SPINLOCKN_LOCK_REG`在该硬件语义下可能立即拿锁。
因此禁止把一段任意MMIO dump当作被动检查，误持锁可能阻塞远端核或固件。
功能测试使用两端明确协议、受控竞争、超时和释放后的重获判据，不与未参与测试的固件争同一把锁。

## 红外 IRRX／IRTX

接收端先核对pinmux、时钟、接收器电源、协议、keymap和input/rc设备对应关系；
用已知遥控器的固定按键，比较码值、协议和重复帧／事件，不能只看input设备存在。
发送端核对LED／驱动电路和载波／占空比／pulse-space单位，用独立接收端或仪器确认帧。
向发送设备write成功，只证明调用接受了数据，不证明光学输出或解码正确。

IRTX页`3800`与38kHz文字冲突，某公式量纲也不一致，不能据此定义新工具默认值。
内部`evdev_*`函数不是用户态LIRC ABI；选定工具和实际驱动后按其接口和已知协议重写最小测试。

## LEDC

确定灯珠类型、数据电压、供电、颜色顺序、灯数、pinmux与LEDC时钟／DMA。
先读实际driver绑定和可用ABI，再用低亮度、少量灯的已知图案确认首／末灯、颜色和时序；
需要时观察数据波形。发送返回值或buffer长度不证明物理灯序正确。
文档不同`/sys/class/leds`与`/sys/class/led/light`路径，以及PG0／固定灯数／ns参数不是通用板型事实。
图案、亮度和负载改变供电条件，采样结果不直接推广为整串灯或其他温度的裕量。

## MIPI CSI／VIN

CSI是摄像头输入。先核对sensor电源／reset／powerdown、I²C ID、MCLK、lane数量／映射、
media graph、V4L2 subdevice、格式／分辨率／帧率和接收器配置。
节点存在、sensor probe成功和`vi`状态都不能代替本次真实流；有的状态保留上次采集信息。

用已知场景／测试图案采集完整帧，检查帧序、时间戳、像素格式、丢帧和PHY／parser错误。
受控改settle／时钟后重复比较；“试到最大值再减一”不建立跨温压时序裕量。
摄像头例程、CSI lane和sensor表不能当作CWU50 DSI／DCS屏幕初始化方法。
BSP的VIN/ISP和私有属性在6.18需要重新匹配驱动／binding，不是加一个DT节点就完成移植。

## RTC

通过`/sys/class/rtc/rtc*/name`和driver识别正确RTC，读取date/time并与独立已知时钟比较。
保留测试间隔、漂移与供电条件；系统时间正确不证明RTC备份电源保持功能。
设置RTC、修改系统时钟、写闹钟／reboot flags、suspend或关机是另外的主动试验。
闹钟先初始化完整结构并明确`enabled`、绝对／相对时间转换，再测事件计数和真实唤醒。
原文C例的未初始化`rtc_wkalrm`与日期0／00:00:10不是“当前时间后十秒”的有效默认例程。
电池类型／可充电能力与充电配置另核对，不从“RTC可读”推广为可以充电。

## SID／eFuse

从NVMEM provider、cell和公开布局定位身份／校准字段；读取只选确知非敏感的范围，
校准值与芯片规格或公开格式比较。读到非零数不是校准正确的判据。
`key_info`的echo可能只是选择读取项，不等同烧写，但仍是特定BSP写ABI，不自动批量dump。
ChipID、密钥、secure boot字段不进入公共知识篇；SID例子混用了不同SoC布局，不能照抄偏移。
烧写eFuse／保护位属于不可逆操作，不列入日常调试或本文测试步骤。

## Timer

区分Linux clocksource／clockevent与额外硬件timer，先核对当前选择、时钟频率、IRQ和DT资源。
已知间隔的定时／capture以独立时间基准比较偏差、抖动和丢失，明确测试负载与容差来源。
改clocksource、重载／中断周期或占用系统timer会影响调度与时间，不能把任意计数器写作为普通观察。
本文没有给没有测量条件的通用“时序通过”阈值。

## 跨6.18复用

上述定位链、已知激励和预期判据可沿用；`CONFIG_AW_*`、BSP sysfs/debugfs/ioctl、DT cell格式、
寄存器地址、时钟／IRQ编号和内部驱动API要以目标6.18源码与实际载板为准。
[BSP来源清单](../../reference/avaota-a1/bsp-manual-sources.json)保留18页原件身份，
[迁移评估](../kernel/t527-linux-6-18-migration.md)说明上游已有支持与缺口。
本次只读取资料，没有执行外设命令、改设备状态或给出实机通过结论。
