# Roadmap

## TL;DR

- 近期增加 uConsole CM3 的最小系统验证；用户计划购入二手低配模块，实物型号／存储版本待确认。
- 核桃派 CM2／全志 T527 列为新的平台适配候选；用户已购入，当前先固定官方资料、接口差异和 BSP 构建链。
- 规划不代表已有可构建板型；实现、离线检查、真实启动和整机功能分别验收。

## 当前基线与约定

已实现的板型和设备证据以[开发状态](status.md)为准，通用模型见[板型与平台设计](board-design.md)。
本页维护尚待实施的目标、依赖和完成条件；不替代 `board.toml` 的 `available` 或机器交接状态。
只在实际构建／检查路径实现后开放菜单。镜像生成和设备刷写继续分开，按需构建镜像。
平台迁入时保留通用 rootfs／工具，内核、引导、GPU、GPIO、显示和电源分别使用真实板型后端。

## uConsole CM3：低配硬件验证路线

**状态：计划中，等待实物。** 用户计划购买二手最低配 CM3，用于验证工程的板型扩展；
不以现代重型桌面或 AI 应用为本阶段验收目标。到货记录 CM3／CM3+、Lite／eMMC、硬件修订、
存储容量及现有主板修订，不能从商品简称推断全部身份。

官方 uConsole 产品页列出 CM3 兼容性，社区 CM3+ 已有直接插主板的运行记录，省去 CM4 转接卡；
系统仍需面板、PMIC、无线和电源适配。参考来源与关机供电限制见
[CM3 直插说明](../reference/clockworkpi-uconsole/README.md#cm3-能否直接插入)。

| 阶段 | 待做工作 | 完成条件 |
| --- | --- | --- |
| C3-0 身份与恢复 | 确认实物与主板、选择 SD 或 eMMC 启动／写入路径、保存原系统和回退配置 | 有可识别启动介质和实际可用的串口／恢复方法，不以“插得进去”替代确认 |
| C3-1 内核与启动 | 新增独立 CM3 板型、BCM283x DT／GPIO；以现 pin 的 bcm2711_defconfig 派生配置，候选 bcm2710-rpi-cm3.dtb、kernel8.img；核对旧固件链 | 固定源码、编译、产物身份和 DT 组合检查通过；不复用 BCM2711／RP1 设备树 |
| C3-2 最小 rootfs | 优先 ARM64 无桌面，复用项目基础账户／SSH和镜像导出方法；评估 Ubuntu Base 路径 | rootfs／内核安装离线检查通过，首次启动、根分区和日志正常，串口或经验证网络可进入系统 |
| C3-3 uConsole 基本功能 | CWU50 四通道 DSI、OCP8178 背光、USB Hub／键盘／轨迹球、PMIC 与电量 | 实体屏显示已知内容、实际 HID 输入、PMIC 遥测和系统事件分别有证据 |
| C3-4 电源与复现 | 受控关机／重新上电、检查电源残留与恢复、重复冷启动 | 不仅 SSH 消失；确认关机状态，记录主板批次和物理供电观察／测量 |
| C3-5 可选功能 | Wi-Fi／蓝牙、音频、HDMI、轻量桌面 | 按实际需求单独验收；不阻塞最小基础系统 |

CM3 的固件候选为 `bootcode.bin`、`start.elf/fixup.dat` 和 `arm_64bit=1`。
当前 Ubuntu 后端仍固定检查 `start4.elf/fixup4.dat`，实现时必须按目标修改并扩充检查，
不能仅新增 board.toml；Pi 4 专用固件不能当作 CM3 需求。
固定内核存在 CM3 ARM64 DT（工程源码：`upstream/linux/arch/arm64/boot/dts/broadcom/bcm2710-rpi-cm3.dts`），
`bcm2711_defconfig` 名称并不意味着只支持 BCM2711；uConsole 的载板 DT 仍需独立。
CM3 直接使用主板无线和原生槽 GPIO 映射，与 CM4 自带无线／转接卡映射分开处理。

Lite 没有板载 eMMC，适合用可替换 SD 卡做测试；eMMC 型号也可纳入，但首次写入／恢复路径不同。
不要求高内存配置；CM3 系列本就属于受限资源目标，先完成基础系统再评估桌面。
已报告的 CM3L+ 关机后内部 5 V 母线保持问题列为 C3-4 必查项；不默认套用社区电阻修改。

## 核桃派 CM2／Allwinner T527：新平台路线

**状态：硬件已购入，资料调查中，工程尚未实现。** 本目标是计算模块 CM2，不能与核桃派 2B 整板混同。
用户提供的两个官方入口分别负责资料／例程与镜像构建：

- [walnutpi-2](https://github.com/walnutpi/walnutpi-2)：CM2／2B 产品资料、底板电路和示例。
- [walnutpi-build](https://github.com/walnutpi/walnutpi-build)：boot、kernel、rootfs 与 image 构建。

公开范围按已核实资料记录：目前提供 **CM2 IO 底板**原理图／设计入口、核心模块的尺寸／引脚图／3D，
没有核心板完整 SoC／DDR／PMIC 原理图，不能从底板图补猜核心板内部连线。
机械接近 CM4 也不自动建立与 uConsole CM4 adapter 的全部电气／软件兼容。

| 阶段 | 待做工作 | 完成条件 |
| --- | --- | --- |
| C2-0 固定资料 | 区分 CM2 与 2B，固定官方硬件、构建仓库、BSP 与启动源身份 | 有来源／版本／摘要、已公开文件范围、CM2 专属参数与未知项 |
| C2-1 接口与电源 | 对照 CM2 BTB、CM4 adapter 和主板 J1；核对 GPIO 电平、供电方向、DSI、USB Host、SD 与恢复信号 | 逐针适配清单完成；不以“CM4 兼容”宣传代替电气检查 |
| C2-2 独立底板基线 | 在已知匹配的底板／恢复条件下记录原厂启动、存储、串口、内核／DT／驱动和关机 | 原厂可恢复基线和日志身份清楚，避免将载板问题归因于新 rootfs |
| C2-3 平台后端 | 为 Allwinner/T527 接入独立内核、启动和镜像导出／检查后端，记录 BSP 依赖与许可 | 真实 CM2 构建路径与离线检查实现后才开放板型，不沿用 Pi 固件配置 |
| C2-4 最小 rootfs | 评估复用 Ubuntu Base 账户、SSH、软件包策略，先无桌面 | 独立 boot/kernel/rootfs/export 可组合，板上启动与恢复分别验证 |
| C2-5 uConsole 适配 | T527 DSI Host／CWU50、背光、USB Hub/HID、PMIC、存储和电源控制 | 内屏、键盘与基本电源功能满足[最小接口要求](../knowledge/hardware/uconsole-custom-core-interface.md) |
| C2-6 图形与扩展 | 独立核对 Mali 用户态／内核接口、Mesa 或厂商方案，以及无线、音频、HDMI、NPU | 真实 GPU 作业／应用功能验证；T527 指标和厂商 API 不是工程已通过的证据 |

内核升级候选为[正式6.18 LTS加目标板必要补丁](../knowledge/kernel/t527-linux-6-18-migration.md)：
上游已有T527/A1基础，优先据此评估headless启动／USB Host／PMIC，再补内屏DSI与图形。
5.15 BSP保留为硬件行为参考和回退，不将其全部驱动迁移视为已开始或已完成的默认工作。

[Avaota-A1 官方开源设计与 AvaotaOS](../reference/avaota-a1/README.md)作为同 T527 的参考库，
用于对照 SoC／DRAM／电源／显示和构建后端，不新增 A1 可构建板型或假定其固件可直接刷入 CM2。

HDMI 继续为可选项；内部屏幕和原键盘是接入 uConsole 的基本目标。
本阶段不运行上游 `sudo build.sh`、不生成新镜像、不刷写或更改现有设备。

## 已有路线继续保留

- Arch Base／Omarchy：独立核心包和手动会话已形成，后续按需要验证新工具部署与桌面镜像集成。
- CM5 USB 暖重启：继续隔离枚举层问题，冷启动正常不代替重启可靠性。
- 其他 SoC：延续通用板型模型，以实际模块／载板／启动后端为单位接入，不从接口外形推断驱动支持。
