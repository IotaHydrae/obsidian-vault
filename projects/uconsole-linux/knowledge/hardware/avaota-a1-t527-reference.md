# Avaota-A1：为 T527 核心板开发选择参考设计

## TL;DR

- A1 的 T527 整板原理图、可编辑 PCB、BOM、3D、DRAM 模板和 IBIS 可补充 CM2 未公开的模块内部设计参考。
- 八层 LPDDR4 模板用于官方 A1；四层模板明确未在 A1 使用，不能把两者标成同等实测基础。
- AvaotaOS 当前 A1 路径仍选厂商 5.15 BSP 与 SyterKit；同一 SoC 不能代替 DDR、电源、设备树和驱动配置核对。

## 先确定参考对象

[资料目录](../../reference/avaota-a1/README.md)固定了官方 v1.7 硬件、v1.6 修订记录和 AvaotaOS 源码身份。
Avaota-A1 是 SBC，不是 CM4 BTB 核心模块，也不是核桃派 CM2 的原理图。
用户这次要求收集用于未来开发参考，未要求实现 A1 板型、构建或连接实物。

针对自制 uConsole 核心卡，先确定[必须功能](uconsole-custom-core-interface.md)和目标外形，
再选择 T527 参考子电路；不把 A1 整板尺寸、接口数量、DDR 走线和 PMIC 配置照搬到 55 × 40 mm 模块。

## 最值得对照的硬件资料

| 材料 | 可以研究什么 | 仍要独立完成什么 |
| --- | --- | --- |
| v1.7 PDF 原理图 | SoC 引脚组、PMIC／电源域、DDR、存储与外设连接 | 新板电源预算、上下电、复位、IO 电平、未使用针处理 |
| PCB `.epro`／EDA／Altium | 布局分区、差分与 DDR 约束、回流、去耦摆放 | 新板封装／层叠／阻抗、长度匹配、DRC 与板厂审核 |
| Gerber／BOM／贴片资料 | 制造层、器件规格和可采购封装核对 | 原文件日期对应的 revision、元件替代和装配条件 |
| 3D 模型 | 外形、器件高度、安装与散热空间 | uConsole 外壳、转接卡、BTB、金手指和散热干涉 |
| 八层 LPDDR4 模板 | 官方已采用的拓扑／布局起点 | 目标 DRAM 颗粒、DDR 初始化、布线／电源完整性和实物验证 |
| 四层 LPDDR4 模板 | 另一种方案的研究材料 | 官方未在 A1 使用，不作为直接量产或稳定性依据 |
| IBIS 模型 | IO 缓冲器模型与信号完整性分析输入 | 模型适用电压／封装／角点、实际叠层及仿真设置 |

原理图、PCB、BOM、3D 的日期不全一致，v1.7 目录保留了一些早期文件；
据文件本身记录身份，不将文件夹名当作全部资料均重新验证的证明。

## 保留修订经验，不照抄修补动作

官方 v1.6 修订涉及 SoC 3V3 digital power domain、USB VBUS 二极管、HDMI／RF reference layer；
v1.7 记录为 DP 改到顶层／优化阻抗。
这些记录说明必须对照实际 SoC bank 电压、器件电源／信号方向以及目标封装；
不能只从网络名或看似相同的 40 针接口推断电气要求。
旧图、旧 PCB 和当前源码需要明确配对；没有在新板验证的修订不能写成新核心已修复。

A1 图纸可查 AXP717／AXP323（BOM 标 AXP717B）、24 MHz／32.768 kHz、LPDDR4／LPDDR4X 电源处理。
CM2 公开资料未足够确认这些内部细节；应写“A1 的参考实现，CM2 尚未知”，不能断言两者一定相同或不同。
A1 整板 DC 供电入口与 CM2 的 BTB 5 V 入口不同，要先拆出转换级再比较 SoC 实际电源域。
BOM 示例器件与产品页的 eMMC 容量选项也不完全一致，不把某一行 BOM 扩大为全部出货 SKU。

A1 的完整电路比 CM2 IO 底板更能展示一种 T527 系统实现，但仍不是芯片原厂完整 TRM／绝对最大额定值手册。
本次收集到的硬件树没有完整芯片 datasheet；设计签核还需目标芯片、DRAM、PMIC 和连接器的原始器件资料。

## 同 T527，软件参考也要按板型拆开

| 维度 | Avaota-A1 当前调查 | 核桃派 CM2／2B 当前调查 | 可复用结论 |
| --- | --- | --- | --- |
| 构建入口 | AvaotaOS 的 boards 配置、bootloader／kernel／rootfs／pack | walnutpi-build 的 board、boot／kernel／rootfs／image | 可以比较阶段划分，不能混用整套板型参数 |
| 内核选择 | AvaotaSBC/linux 的 linux-5.15，所查版本 5.15.154／sun55i_t527_bsp_defconfig | walnutpi/linux 的 5.15.147，walnutpi2b_defconfig | 先比较实际补丁／配置／DT；分支名字不证明二进制或 ABI 一致 |
| 设备树目标 | allwinner/sun55i-t527-avaota-a1 | sun55i-t527-walnutpi-2b；CM2 覆盖方式仍待验证 | SoC 可共用部分 dtsi，板级 DTB 不能直接替换 |
| 启动 | A1 指定 SyterKit 提交，含 A1 4GB 启动兼容说明 | 核桃派自己的 SyterKit 分支及板配置 | DDR／训练／容量／固件依赖需逐项确认 |
| 显示／GPU | A1 的板级显示 hook 与 BSP，特定 Ubuntu GNOME 配置选择 Xorg | 核桃派 disp／GPU 模块及 Mali 用户态 | API／驱动闭包和 compositor 支持都要实测 |

A1 的固定内核配置为 `CONFIG_AW_DRM=y`、`CONFIG_AW_DRM_PANFROST=y`、
`AW_DRM_ALIAS="sun4i-drm"`，`AW_DISP2` 关闭；`bsp/drivers/drm/sunxi_drm_dsi.c`
实际注册 attach／detach／transfer 并调用 DRM panel 的 prepare／enable 等生命周期。
因此，在该路径适配现有 `panel-cwu50` 有源码依据，是值得优先调查的候选；
5.15 API、DT graph、PHY、时序、供电／复位和实体显示尚未编译／上板确认。
A1 使用的 `bsp/drivers/gpu/panfrost` 与核桃派额外构建的 `bsp/modules/gpu` 结构不同，
不能仅按 Mali 型号或分支版本合并安装步骤和用户态包。

固定 A1 配置对 Ubuntu Noble GNOME 禁用 GDM Wayland，明确选择 `sun4i-drm` 显示控制器；
这是对应组合的处理，不推广成 T527 永远不能运行 Wayland，也不能反向声称 Omarchy 已可用。
A1 无线配置显式加载 AIC SDIO 驱动，是另一项板级策略，不能把硬件“同 T527”当作无线电路一致的证明。

官方网页提到主线系统或 GPU API 能力，不能替代构建配置的实际选择和用户态验证。
AvaotaOS A1 配置只实现 `bsp)`；其他板型的 mainline 路径不证明 T527 已具备同一支持。
A1 的 pack 使用 FAT 16 MiB、根分区 256 MiB 和 raw boot 8192 bytes 的文件布局，
核桃派 FAT 起点是字面 `4M`；启动文件名相同也不能替代镜像布局与 DDR 输入校验。
这次没有编译内核／bootloader，也没有把任何 A1 DTB、boot 二进制或镜像安装到 CM2。

## 下一步如何使用

1. 从 [A1 来源清单](../../reference/avaota-a1/sources.json)选择版本匹配的电路、PCB 与 DRAM 模板。
2. 与 [CM2 接口资料](../../reference/walnutpi-cm2/README.md)和目标 uConsole 电源树逐项比对，标出可用、需重做与未知项。
3. 固定具体 SoC／DRAM／PMIC、bootloader／DDR 初始化与内核输入，再实现独立 Allwinner 后端和离线检查。
4. 先验证可恢复启动、存储、USB Host／HID 和电源，再做 DSI 实体画面、GPU 作业和长期稳定性。

这套资料扩充 [Roadmap 的 CM2 平台研究](../../project/roadmap.md)，不自动新增 A1 实机适配承诺。
原件和来源校验只能证明文件身份与格式可读，不能代替电气签核、投板或运行验收。
