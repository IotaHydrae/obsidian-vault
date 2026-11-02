# Raspberry Pi CM 接口与机械资料

## TL;DR

- CM4／CM5 都使用两只 100 针、0.4 mm 间距的 BTB；这里保存完整引脚事实表、差异索引和官方资料身份。
- 两代外形均为 55 × 40 mm，但部分引脚功能、PCB 厚度、连接器及电源预算不同；相同外形不等于全功能互换。
- 官方公开的可编辑 PCB 设计是 **IO Board**；模块本体的 STEP／尺寸图不能代替完整 SoM 电路和布线文件。

## 表格入口与编号

| 文件 | 内容 |
| --- | --- |
| [cm4-pinout.csv](cm4-pinout.csv) | CM4 的 1–200 全部引脚，官方信号名和独立中文短提示 |
| [cm5-pinout.csv](cm5-pinout.csv) | CM5 的 1–200 全部引脚 |
| [cm4-cm5-pin-comparison.csv](cm4-cm5-pin-comparison.csv) | 按同一针号并列；`label_comparison` 仅比较名字，不判定电气兼容 |
| [sources.json](sources.json) | 11 份官方 PDF／ZIP 的 URL、长度、SHA-256、版次及本机缓存相对路径 |

引脚 CSV 中 `pdf_page` 是阅读器从 1 开始计数的页码，`printed_page` 是页脚数字。
`module_direction` 只在原表明确说明方向时填写；空白不等于 NC，也不意味着无电平／供电时序要求。
CM4 使用 §4 Table 6，PDF 17–23 页；CM5 使用 §4.2 Table 4，PDF 18–23 页。
CM4 表保留原文 `Reserved` 等命名，CM5 表保留原文大小写；未将保留针改写成可随意使用的 GPIO。

两份机械图都是 **模块顶视图**。天线缺口朝左时，上排连接器覆盖 1–100，下排覆盖 101–200；
右端是低编号，左端是高编号。不能把这一顶视图直接镜像／照抄成底板焊盘。
ClockworkPi 转接板的 J1 金手指编号另属一套接口，见 [uConsole 资料](../clockworkpi-uconsole/README.md)。

## 已核实的机械事实

| 项目 | CM4 | CM5 |
| --- | --- | --- |
| 板外形 | 55 × 40 mm | 55 × 40 mm |
| 安装孔 | 4 × M2.5，中心距相邻边 3.5 mm | 相同 |
| 孔中心距 | 48 × 33 mm，依据外形／边距推算 | 相同 |
| PCB 厚度 | 1.2 mm ±10% | 1.24 mm ±10% |
| 裸模块厚度 | 4.7 mm | 4.6 mm |
| 标准底板插座 | Hirose `DF40C-100DS-0.4V` | Amphenol `10164227-1001A1RLF` |
| 标准堆叠高度／模块下净空 | 1.5 mm／0 mm | 1.5 mm／0 mm |
| 加高插座 | `DF40HC(3.0)-100DS-0.4V` | `10164227-1004A1RLF` |
| 加高堆叠高度／模块下净空 | 3.0 mm／1.5 mm | 4.0 mm／2.5 mm |
| 标准／加高时安装后整体高度 | 5.078／6.578 mm | 4.94／7.44 mm |

依据：CM4 Release 4，2026-06-30，§3.1 Figure 4，PDF 14／印刷 13；
CM5 Release 3，2026-06-08，§4.1.1 Figure 4，PDF 17／印刷 16。
图中两条 BTB 中心线各距相邻板边 3 mm，中心线间距 34 mm 为推算值；
精确横向坐标、焊盘、阻焊和 courtyard 应从官方 PCB footprint 复核，不能从截图描封装。
连接器堆叠高度、裸模块厚度和安装后整体高度定义不同，不直接相加代替尺寸图。
CM5 官方明确更换了连接器品牌并按更高电流测试；不推定任意同形替代料拥有相同承载能力。

板载天线正下方避免金属／地平面，避让至少 6.5 × 11 mm，理想为 8 × 15 mm 以上；
附近建议保留 10 mm 空间。外接天线、金属外壳和散热器仍需整机检查和实测。

## layout 与机械模型的实际范围

| 缓存文件名 | 官方内容 |
| --- | --- |
| `cm4io-kicad.zip` | KiCad 6，CM4IOv5 PCB／原理图／封装／模型 |
| `cm5io-kicad.zip` | KiCad 8，原版 CM5IO 参考设计 |
| `cm5io-rev2-kicad.zip` | KiCad 9，官方标记 revision 2 的独立 ZIP，含 BOM |
| `cm4-step.zip` | CM4 模块机械 STEP |
| `cm5-step.zip`／`cm5lite-step.zip` | CM5／CM5 Lite 的独立机械 STEP |
| 两代 IO Board datasheet | 电路图、元件布局及 160 × 90 mm 底板机械图 |

两个 IO Board PCB 都声明四铜层；层叠／阻抗是参考文件设置，不能代替新设计的板厂层叠和阻抗计算。
CM5IO revision 2 ZIP 内部分 schematic title block 仍写 rev1，保留官方包名与内部记录，不自行改版号。
尚未找到 Raspberry Pi 公开的 CM4／CM5 模块完整 SoC／RAM／PMIC 原理图和原生 PCB 布线工程。

## 固定来源、缓存与许可

查询日：2026-11-01。CM4 当前固定资产 `RP-008168-DS-4`，CM5 为 `RP-008180-DS-7`；
资产编号中的修订号与 PDF 内部 Release 不等同。CM3+ `RP-008163-DS-2` 仅用于旧式接口对照。

- [CM4 官方 PIP 入口](https://pip.raspberrypi.com/categories/634-raspberry-pi-compute-module-4)
- [CM5 官方 PIP 入口](https://pip.raspberrypi.com/categories/944-raspberry-pi-compute-module-5)
- [CM4IO 设计文件入口](https://pip.raspberrypi.com/categories/1210-design-files)
- [CM5IO 设计文件入口](https://pip.raspberrypi.com/categories/1098-design-files)
- [CM5 机械模型入口](https://pip.raspberrypi.com/categories/1096-design-files)

原文件位于仓库忽略目录 `image/work/hardware-reference-20261101/`，每份具体位置见清单。
这些大文件不进入普通 Git；清单和独立事实表受版本控制。资料不是构建输入，不随镜像生成自动下载。
PDF 原件标注 CC BY-ND 4.0，保留原件不改动。设计 ZIP 没有发现覆盖全部内容的独立 LICENSE，
README 说明厂家 3D 模型沿用原厂家条款，部分模型只是近似；不把整包重新授权为本仓库许可证。
CSV 是事实索引，不能替代数据手册的完整电气限制、时序、布局规则和勘误。

CM5 原表有几处需保留的差异：Table 4 pin124 的信号为 `PCIe_TX_N`，描述却误称 positive；
Table 14 的 pin104／99／16 名称又与 Table 4 略异。CSV 统一依据 Table 4 的 Signal 列，未自行改名。
