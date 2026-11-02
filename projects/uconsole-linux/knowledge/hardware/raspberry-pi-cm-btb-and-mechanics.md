# CM4／CM5：BTB 引脚、机械尺寸与底板布局

## TL;DR

- CM4／CM5 都是 55 × 40 mm、两只 100 针 BTB；使用完整针表和顶视尺寸图，而非凭照片布封装。
- CM5 复用了 CM4 的部分 MIPI／控制针并提高电源需求；必须检查接口差异，不能只对外形。
- [资料目录](../../reference/raspberry-pi-cm/README.md)已有完整 200 针 CSV、官方 PDF 身份、STEP 与 IO Board KiCad 清单。

## 先确认要制作的板

自制 **CM4/5 兼容模块**需要满足模块侧接口／电源和全部机械约束；
自制 **载板**需要模块的匹配插座、供电和外设参考设计；
自制 **uConsole 金手指核心卡**则首先满足 Clockwork 的 200 针接口，见
[uConsole 最小接口](uconsole-custom-core-interface.md)。三者不能共用一套未核实的引脚编号。
官方 IO Board layout 可帮助做载板，不提供完整 SoM 的 CPU／DRAM／PMIC 克隆设计。

## 机械与方向

CM4：当前固定数据手册 Release 4，2026-06-30，§3.1 Figure 4。
CM5：当前固定手册 Release 3，2026-06-08，§4.1.1 Figure 4。
两代都有四个 M2.5 固定孔，孔中心距相邻边 3.5 mm；由此推算孔中心距 48 × 33 mm。
两图为顶视图，不能按底面照片猜连接器 1 脚或镜像 footprint。

| 机械差异 | CM4 | CM5 |
| --- | --- | --- |
| PCB 厚度 | 1.2 mm ±10% | 1.24 mm ±10% |
| 标准底板插座 | Hirose DF40C-100DS-0.4V | Amphenol 10164227-1001A1RLF |
| 标准堆叠高度 | 1.5 mm | 1.5 mm |
| 官方列出的加高选项 | 3 mm | 4 mm |

标准高度下模块正下方无元件净空；加高形式、整机厚度和天线避让按原图检查。
精确插座坐标／焊盘／courtyard 取自参考 KiCad，再核对连接器厂家图纸。
CM5 手册明确更换品牌并按更高电流测试，不能直接推断任意兼容外形插座的额定电流。

## CM5 对 CM4 的重要功能变化

依据 CM5 Appendix B，纸面 34–35／PDF 35–36 页；两份全针表可逐行查询。

| CM4 针／功能 | CM5 变化 |
| --- | --- |
| CAM0，128–142 范围内对应信号针 | USB3 port0；其中还包含该端口的独立 USB2 D+/D− |
| DSI0，157–171 范围内对应信号针 | USB3 port1；不能再接 CM4 的 DSI0 设备 |
| 保留的 CAM1／DSI1 | 两组 MIPI 可分别配置 CSI 或 DSI，仍需主机／DT 支持 |
| 16 / 19 | 风扇测速／PWM |
| 76 | RTC VBAT；不是主 5 V 输入 |
| 92 | PWR_Button，替代 RUN_PG 语义 |
| 94 / 96 | USB-C CC1 / CC2，替代模拟输入 |
| 99 / 100 | PMIC_Enable / CAM_GPIO1，查具体启动与输出时序 |
| 104 / 106 | PCIe 唤醒／电源使能 |
| 111 | VBUS_EN，替代 VDAC_COMP |

CM5 的 USB2 OTG 103/105 与两个 USB3 口各自 D+/D− 不同，不能因都叫 USB2 就接成同一路。
PCIe 时钟 AC 耦合和部分 HDMI 控制针 ESD 保护也有变化，查 Appendix B 和对应接口章节。
官方给 CM5 供电设计提出 5 V、最高 2.5 A 的预算；这不是任何 uConsole 电源树已具备该能力的证明。

## 布局取舍

使用全部规定 GND／电源触点；按 GPIO_VREF 选择允许的 1.8／3.3 V，禁止悬空或接地。
根据模块状态设计掉电／上电顺序，防止关机时外设通过 IO 回灌。
同一差分对 P/N 推荐长度误差不超过 0.15 mm；接口间的阻抗、对间 skew、AC 耦合和 ESD
分别遵守各接口章节和最终 PCB 层叠，不把某一条长度规则推广到所有总线。

板载天线下方不放金属／地平面，至少 6.5 × 11 mm 避让，理想 8 × 15 mm 以上；
附近建议 10 mm 空间，外壳／散热器环境须测。四层 IO Board 是参考设计，不是新核心板的层数要求。

## 证据和未验证项

[来源清单](../../reference/raspberry-pi-cm/sources.json)固定 URL、版次、SHA-256、长度、机械页和原件路径；
[全针对照](../../reference/raspberry-pi-cm/cm4-cm5-pin-comparison.csv)只比较信号名称，不判定电气兼容。
原始资料下载与 ZIP CRC 检查已完成，原件保持不改动，未制作或上电新 PCB。
连接器采购料号、电流额定、板厂阻抗、信号眼图、散热和 EMC 均尚未完成新设计验收。
