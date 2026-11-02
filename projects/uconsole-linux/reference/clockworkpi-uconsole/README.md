# ClockworkPi uConsole 官方硬件资料

## TL;DR

- 主板 **J1 是 200 针 DDR2 SO-DIMM 形式核心板槽**；**J4 是 52 针 MINI_PCIE 形式扩展槽**，两者用途／针号不同。
- 官方声明兼容 CM3 系列，社区已有 CM3+ 直插实机；J1 的 NC 扩展本身不否定这种兼容。J4 不是标准 mini PCIe。
- 自制核心板以 Clockwork 接口为目标；CM4／CM5 的 BTB 只能经完整转接／电源设计接入。

## 从哪里开始

| 文件 | 用途 |
| --- | --- |
| [sources.json](sources.json) | 固定提交、官方 URL、SHA-256、长度和本机原件缓存路径 |
| [core-edge-pinout.csv](core-edge-pinout.csv) | J1 全部 200 针，并列官方 CM3+ 参考位置 |
| [expansion-j4-pinout.csv](expansion-j4-pinout.csv) | V3.14-V5 主板 J4 全部 52 针 |
| [最小核心板接口](../../knowledge/hardware/uconsole-custom-core-interface.md) | 内屏／键盘／电源／存储的必须与可选项 |
| [CM4／CM5 BTB 与尺寸资料](../raspberry-pi-cm/README.md) | 两代模块本身的接口，与 J1 金手指分开 |

原件按 2026-11-01 的官方仓库 `53a05efca806ca024aab4fbfd28a680688c2541c` 固定。
J1 表中 `source_document`／`pdf_page` 为分号分隔的并行来源：Clockwork adapter PDF 第1页、
CM3+ Figure 3 PDF 第8页。J4 表只针对 V3.14-V5 PDF 第1页；不外推未检查的 PCB 修订。
表格验证的是图纸上的 pin→net，尚无实际导通／波形或自制板验收结果。

## 两个模块槽及桥接关系

```text
CM3／CM3+（200-pin SO-DIMM）
  → uConsole 主板 J1（不经过 CM4 转接卡）

CM4 或 CM5 模块（两只 100-pin BTB）
  → CM4 形式转接卡（其 J2/J3 是 BTB，J1 是金手指）
  → uConsole 主板 J1（200-pin 核心板插槽）
  → 主板 DSI 屏／USB Hub／SD／电源等

主板 J4（52-pin 扩展插槽）
  → 4G 或其他符合 Clockwork 定义的扩展模块
```

上图 CM5 经 CM4 形式转接卡是当前项目使用方式；官方资料树只有 CM4 adapter 原理图，
没有找到官方 CM5 专用 uConsole adapter 设计。CM4／CM5 BTB 的每针兼容性另查各自数据手册。
主板 J1 和转接卡 J1 是接口两侧；其针号不等于转接卡 J2/J3 的 CM4 全局 1–200 号。

## CM3 能否直接插入

**能省掉 CM4 转接卡。** ClockworkPi 官方产品页列出主板兼容 CM3／CM4 系列，
社区原始移植作者又实际安装 CM3+ 并验证显示、USB、Wi-Fi／蓝牙、PMU 等。
不能仅因 Clockwork 使用 CM3+ 的 NC 位置扩展功能，就推断标准 CM3 不能插；
标准模块端没有连接这些触点，与额外器件主动驱动同一信号是不同情况。

“直插”不等于所有硬件批次／系统都可即插即用：

- 必须使用适配 uConsole 的 CM3 内核、DT 和面板／PMIC 驱动；本仓库当前没有 CM3 构建目标。
- Lite 型号与 eMMC 型号的启动存储路径不同，使用主板 SD 槽需匹配 Lite 配置。
- 2023-12-03，原移植作者在 CM3L+ 上报告关机后 SYS_3V3 残留约 0.9–1.6 V、
  内部 SYS_5V 未关断，并推测与回灌有关；这不是说外部 USB-A 必然仍有 5 V。
- 2023-12-11，作者报告特定电阻修改改善其板，并明确称略超器件规格。
  这里只保留历史证据，不把该改动推广为所有主板的硬改方案；本项目没有 CM3 实物验证。

依据：[官方产品页](https://www.clockworkpi.com/uconsole)、
[原作者移植项目](https://github.com/PotatoMania/uconsole-cm3)、
[关机供电报告](https://forum.clockworkpi.com/t/uconsole-now-supports-rpi-cm3-y/11351/10)、
[后续修改与限制](https://forum.clockworkpi.com/t/uconsole-now-supports-rpi-cm3-y/11351/15)。
社区资料独立标注为第一手实测，不能包装成官方系统或全批次电气验收。

## J4 扩展槽接线

J4：2/4/6/8/10 为 `SYS_5V`，17/19 为名为 `3V3` 的电源网；
GND 为 5/11/12/21/27/33/39/45/51，全部针位见 CSV；
7/9 和 13/15 是 GL850G 下游 USB 的 DP3/DM3、DP4/DM4；
18–52 偶数脚提供 GPIO28–45；另有 CAM1 和四根扬声器功放输出。
没有从图中发现 PCIe TX/RX／REFCLK／PERST 链路，所以不能插普通 PCIe 卡。
`SPK_LP/LN/RP/RN` 是桥接功放输出，不按耳机地参考信号接线。

## 已归档的官方资料

| 原件／包 | 对后续设计的价值 |
| --- | --- |
| `clockwork_Mainboard_V3.14_Schematic.pdf` | 早版主板电路参考，独立保留 |
| `clockwork_Mainboard_V3.14_V5_Schematic.pdf` | 当前这次 J1/J4、电源、屏幕、USB、音频和 Wi-Fi 分析依据 |
| `clockwork_Adapter_CM4_Schematic.pdf` | 金手指与 BTB 的完整重映射、升压和恢复控制 |
| `PCB/CPI_3.14_Mainboard_V5_Gerber.7z` | 官方主板 CAM／装配／贴片制造数据；含 TOP、BOTTOM、ART03、PWR04、GND02/GND05 等层 |
| `PCB/RPI CM4 to CPI v3.14 Adapter.zip` | 转接板 Gerber／钻孔／IPC 文件，非 KiCad 原生工程 |
| `clockwork_DevTerm_A06_Core_for_Mainboard_V3.14_Schematic.pdf` | 非树莓派核心板接入该主板的官方参考 |
| `clockwork_DevTerm_R01_Core_for_Mainboard_V3.14_Schematic.pdf` | 另一种 SoC 核心板电源／接口适配参考 |
| `keyboard_220816.pdf` | 键盘／轨迹球 MCU、USB 与电源电路，两页 |
| `clockwork_UC_4G_Schematic.pdf`／`clockwork_UC_4G_drawing.pdf` | 扩展板电路与机械图 |
| 两份 `JD9365DA-H3` PDF | 官方资料树提供的屏控制器资料，面板批次仍须识别 |
| `Clockwork_uConsole_Assembly_Guidelines.pdf` | 整机装配、固定和连接参考 |

主板 Gerber 包与转接板 ZIP 已分别用 `7z t`／ZIP CRC 检查，无解包执行行为。
主板／adapter 原理图各一页，但覆盖很多功能区，必须局部放大，避免按抽取文字相邻位置判断接线。
官方树没有找到主板／转接板的可编辑原生 PCB 工程、完整尺寸机械图或 STEP。
Gerber 的轮廓、钻孔、镀金触点可用于 CAD 复核；尚未从 CAM 重建并校验装配尺寸，
不能把 CM4 的 55 × 40 mm 当作整个转接卡、主板或机壳尺寸。

## 电源与兼容性边界

主板 POWER 图把 `SYS_5V` 与 `CM_VBAT` 接在同一电源网，`SYS_3V3` 对应 `CM_3V3`，
`SYS_1V8` 对应 `CM_1V8` 和 `DISP_1V8`；`CM_VBAT` 这个名字不代表裸电池正极 `VBAT`。
转接卡另有 ETA1096 从 `CM_3V3` 升压到 `CM_VBAT`，供 CM4 BTB 5 V 输入。
这是两张图的连接事实；实际器件装配、电源启动顺序、并联路径／回灌和可用电流仍须单独核实。

J1 183–186 是 `CM_1V8`，191–194 是 `CM_3V3`，197–200 是 `CM_VBAT`。
CM4/5 BTB 上的 3.3／1.8 V 是模块输出定义，不能与这些核心槽电源按同号或同名硬并。
`VDD_CORE`（J1/179）参照 CM3+ 官方要求不连接，不能当通用外部供电输入。

与官方 CM3+ 普通型号的 NC 相比，Clockwork 至少使用了 124/126/128/130/132 的
麦克风／IGPIO，以及 154/156/158/160/162 的耳机／音频开关／额外 USB 信号。
4/6 的 SD 参考电源又与 CM3+ Lite 区别相关。这些扩展不是否定 CM3 直插的依据；
自制核心板仍须检查完整接口及电气条件，不能仅凭机械形状推广到任意模块。

## 来源和许可

- [ClockworkPi 官方仓库固定提交](https://github.com/clockworkpi/uConsole/tree/53a05efca806ca024aab4fbfd28a680688c2541c)
- [官方 README 的原理图入口](https://github.com/clockworkpi/uConsole/blob/53a05efca806ca024aab4fbfd28a680688c2541c/README.md#schematic)
- [官方 PCB 制造数据目录](https://github.com/clockworkpi/uConsole/tree/53a05efca806ca024aab4fbfd28a680688c2541c/PCB)

原文件位于忽略的 `image/work/hardware-reference-20261101/uconsole/`，按清单可核对和重新下载。
清单与事实表进入 Git，原件保持原样。所选树根未找到 LICENSE；官方产品页另明确称设计图与
相关材料遵循 GPL v3。第三方控制器文档和厂家模型仍分别核对原版权，不将全部归档统一再许可。
键盘代码 README 在官方仓库中，但其中编译说明明确注明社区贡献，来源层级单独记录。
