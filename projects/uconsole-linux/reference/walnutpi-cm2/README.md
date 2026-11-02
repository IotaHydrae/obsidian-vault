# 核桃派 CM2／T527 官方资料与接入入口

## TL;DR

- CM2 是基于 Allwinner T527 的计算模块；`walnutpi-2` 同时覆盖 CM2 和 2B，不能把 2B 的整板外设数量当 CM2 的 BTB 定义。
- 已公开 CM2 IO 底板原理图、模块引脚图／尺寸图／3D 和底板 PCB 项目入口，未发现核心模块的完整 SoC／DDR／PMIC 电路。
- `walnutpi-build` 是镜像构建入口；当前固定版本以 `walnutpi-2b` 配置组织 T527 路径，CM2 专属与 uConsole 兼容性仍须分别验证。

## 官方入口和固定版本

| 资料 | 固定调查版本／用途 |
| --- | --- |
| [walnutpi-2](https://github.com/walnutpi/walnutpi-2/tree/b96231a65670989928dd3b720a06bd488ccc1319) | `b96231a65670989928dd3b720a06bd488ccc1319`，产品、硬件、例程 |
| [walnutpi-build](https://github.com/walnutpi/walnutpi-build/tree/d6ac41ea66d9adf97adb503087a7485f82f30b28) | `d6ac41ea66d9adf97adb503087a7485f82f30b28`，组件／镜像构建 |
| [官方 Wiki 参数页](https://wiki.walnutpi.com/docs/walnutpi_2/intro/hw-parameter/) | 分开列出 2B、CM2、CM2 IO 底板 |
| [来源清单](sources.json) | 下载的文件、URL、版本、SHA-256、长度和本机缓存位置 |
| [接入调查](../../knowledge/hardware/walnutpi-cm2-platform.md) | 软件构建链、接口审查与当前未知项 |
| [Roadmap](../../project/roadmap.md) | CM3 最小验证和 CM2 新平台适配的分阶段判据 |

调查日期为 2026-11-01。上游分支仍可变化，这些 pin 是调查快照，不是本仓库已启用的构建依赖。
没有修改现有 `manifest/upstreams.env` 或子模块，也没有创建可选 CM2 板型。

## 硬件资料范围

| 官方路径 | 公开内容／限制 |
| --- | --- |
| `hardware/CM2/cm2-io-board-schematic.pdf` | **IO 底板**电路，不能冒充 CM2 核心模块原理图 |
| `hardware/CM2/pinout_en.png` | 核心模块引脚图，设计时逐针核对电平、方向、复用和电源状态 |
| `hardware/CM2/cm2-size.png` | 机械尺寸图，配合原始模型和连接器数据检查 |
| `hardware/CM2/cm2-3d.zip` | CM2 模块机械模型，不包含 SoC／DDR 电路或内部布线定义 |
| `hardware/CM2/cm2-overview.png` | 模块功能标识 |
| `hardware/CM2/cm2-io-board-overview.png` | 底板接口位置 |
| `hardware/CM2/pcb/EasyEDA/Project Link.txt` | 指向官方 CM2 IO 底板的嘉立创开源项目；外链另有版本／导出边界 |
| `examples/` | Python 外设、视觉、Qt 和 YOLO 示例；不是内核或启动后端 |

用户提供的照片可见 T527 和 Walnut Pi CM2 标识；照片不能确认已购模块的实际内存／eMMC 配置。
未找到模块完整原理图是本次公开资料范围，不据此断言厂商从未向其他渠道提供过任何文档。

## CM2 与 2B 分开记录

依据官方 CM2 参数段，CM2 标称 55 × 40 × 4 mm，提供 1／2／4 GB LPDDR4 选项、
可选 16 GB eMMC、8 MB SPI Flash 和经 BTB 引出的 microSD 接口。
官方列出一路四通道 DSI（兼容两通道）、一路四通道 CSI、USB2／USB3 与 PCIe 等接口；
PCIe 2.1 和 USB3 为二选一，不能按两个同时可用的独立通道做设计。
官方标称 GPU 为 Mali-G57 MC1、NPU 为 2 TOPS；这些是产品能力说明，不是本工程图形／推理验收。
内存容量、闪存实际装配、运行频率和裸板修订在收到实物后读回记录。

官方 CM2 与树莓派 CM4 的外形相近不代表 BCM GPIO、DSI Host、boot 或完整引脚等价。
接入 uConsole 前按[核心板最小接口](../../knowledge/hardware/uconsole-custom-core-interface.md)核查电源、
PMIC I²C／IRQ、四通道 DSI、复位／背光、USB Host 和存储；HDMI 保持可选。

## 构建与复用边界

构建仓库把 boot、kernel、rootfs、image 分开，适合参考项目的组件边界。
当前 T527 配置选 SyterKit 与厂商 Linux 5.15.147；其 `config.txt` 是核桃派自己的启动配置，
名字相同不代表 Raspberry Pi 固件能理解。不能复制 Pi 的 DTB、RP1 GPIO 或 GPU 用户态到 T527。
仓库没有名为 `walnutpi-cm2` 的 board 目录，不能仅从 README／树目录推定现成 CM2 全部适配。

通用 Ubuntu Base 用户空间、账户与服务策略可评估复用；boot 偏移、DDR 初始化、内核模块、
DT、显示、GPU、无线与电源必须绑定核桃派的源码和固件输入。完整输入来源和许可逐项检查，
不隐式执行上游 sudo 构建脚本。这个资料目录不会下载或构建系统，也不连接设备。

## 同 SoC 的设计参考

[Avaota-A1](../avaota-a1/README.md)公开了 T527 整板的原理图、PCB 和 DRAM 模板等，
可补充 CM2 未公开模块内部电路时的设计参考。它是另一块 SBC，电源、内存拓扑、
引导、GPIO 和接口电路仍由各自板型决定；A1 文件不代表 CM2 的实际内部连接。

## 驱动源码与二进制交付核查

[T527 专项审计](../t527-driver-audit/README.md)已沿NPU／VIN ISP的构建对象核对源码，
并静态检查实际ISP／NPU用户包。内核源码可见不等于用户态算法全部可重建；
旧VFE／HDMI等预编译对象也不能混同当前T527所选路径。

## 原件、校验和许可

原始文件位于忽略的 `image/work/walnutpi-cm2-research-20261101/`，逐个位置见 `sources.json`。
Git 保存独立说明和来源身份，原件原样保留；不把用户剪贴板图片或现场凭据加入公共资料。
`walnutpi-2` 根 LICENSE 为 MIT；BSP、boot、固件和第三方模型分别检查自己的条款，
不能由一个仓库的 LICENSE 推导出所有外链源码、二进制和厂商手册的统一许可。
