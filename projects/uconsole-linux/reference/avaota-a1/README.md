# Avaota-A1／T527 开源硬件与 AvaotaOS 资料

## TL;DR

- Avaota-A1 是 Allwinner T527 整板，适合参考 SoC／LPDDR4／PMIC 设计；它不是 CM4 形式核心模块，也不是 CM2 的内部电路图。
- 官方公开了分版本原理图、可编辑 PCB、制造／BOM／贴片资料、3D、DRAM 模板与 IBIS；优先固定 v1.7，保留 v1.6 修订说明。
- AvaotaOS 是独立镜像构建入口；实际 A1 配置使用厂商 5.15 BSP 与 SyterKit，不能把文档中提及的主线支持当作默认实现。

## 官方入口与版本身份

| 来源 | 本次固定／用途 |
| --- | --- |
| [用户指定的硬件参数页](https://docs.avaota.fun/avaota-sbc/avaota-a1/hardware/hardware_info/) | A1 板型与接口概览 |
| [官方开源硬件页](https://docs.avaota.fun/avaota-sbc/avaota-a1/hardware/oshw/) | 各版修订、PCB、制造数据、LPDDR4 模板、IBIS |
| [交互式 BOM](https://docs.avaota.fun/online_bom/avaota-a1.html) | 元件位置／网表查找参考，网页快照单独保存 |
| [Avaota-A1 硬件仓库](https://github.com/AvaotaSBC/Avaota-A1/tree/bbd81c54c9a971e8fd9397fec958a9d9afebdffe) | master，`bbd81c54c9a971e8fd9397fec958a9d9afebdffe` |
| [AvaotaOS 构建仓库](https://github.com/AvaotaSBC/AvaotaOS/tree/bd95d7528c232cee59e4047994be520cb7551888) | main，`bd95d7528c232cee59e4047994be520cb7551888` |
| [来源清单](sources.json) | URL、文件长度／SHA-256、版本和本机原件缓存路径 |
| [T527 参考方法](../../knowledge/hardware/avaota-a1-t527-reference.md) | 如何与 CM2 交叉使用、保留板级差异与未知项 |

调查日为 2026-11-01；仓库提交、硬件丝印版次、资料文件日期和运行镜像版本分别记录。
同一 v1.7 文件夹中有沿用 2024-08-17 的 BOM／贴片／3D，不能擅自改文件日期或声称全套重新生成。

## 已收集的硬件内容

| 官方路径 | 内容和范围 |
| --- | --- |
| `hardware/v1.7/01_SCH/` | 13 页整板 PDF 原理图，可查 T527、电源、DRAM 和各外设 |
| `hardware/v1.7/02_PCB/` | 可编辑 `.epro` PCB 数据 |
| `hardware/v1.7/03_GERBER/` | Gerber 制造数据 |
| `hardware/v1.7/04_BOM/` | XLSX BOM |
| `hardware/v1.7/05_PICKANDPLACE/` | TOP／BOTTOM 贴片图与 XLSX 点位表 |
| `hardware/v1.7/06_3D/` | 整板 3D 模型包 |
| `hardware/v1.7/07_ADPCB/` | 转换版 Altium ZIP，内有 13 张 SCHDOC 和 PCB1_3.PcbDoc |
| `hardware/v1.7/08_JLCEDA/` | 嘉立创 EDA 工程及官方在线工程入口 |
| `hardware/v1.6/01_SCH/`、`Changelog.txt` | 对照供电域与 VBUS 修复，避免只抄旧版 |
| `resource/dram_8l/` | LPDDR4 八层原理图／PCB／EDA／Altium 模板，官方注明用于 A1 |
| `resource/dram_4l/` | 四层模板的原理图参考；官方明确未在 A1 中使用 |
| `resource/ibis/IBIS.zip` | IBIS 信号完整性模型 |

电源设计可查 AXP717B／AXP323 与各 SoC bank／DRAM 电源连接；这是 A1 的公开实现，
不能视为 CM2 内部 PMIC 型号、DRAM 颗粒或训练参数的证明。

官方硬件页和源树日志列出的 v1.6 修订包括纠正 SoC 3V3 digital power domain、
增加 USB VBUS 二极管、修改 HDMI／RF reference layer；v1.7 将 DP 改到顶层并优化阻抗。
修订日志是设计取舍的出处，
不是对任何新核心板的自动改板指令。旧版入口保留在源树 inventory，不将历史图当最新生产基线。

`.epro`、Gerber、PcbDoc 和 STEP 的作用不同：PCB／模型文件不替代原理图电气检查，
Altium 转换结果也需要工具兼容性／封装／DRC 复核。本次只做原件读取与格式检查，没有投板验收。
原理图可优先读 PDF p3 电源、p5 DRAM、p6 GPIO／DSI、p8 USB／VBUS、p9 显示 FPC。
原图 title block 仍标 REV1.0，v1.7 来自目录／固定文件身份，不能将两种修订字段擅自统一。
Gerber 的八铜层与 PCB 层叠已核对；原件包含板外形、贴片图与 STEP，但未找到独立的尺寸／公差 PDF。
不把从 Gerber 主矩形估出的尺寸当作完整机械包络或 CM 型模块标准。
官方仓库没有单独提供完整 T527 芯片 TRM／datasheet；A1 原理图、BOM 和 IBIS 不能取代芯片电气极限。

## 软件资料的实际选项

固定版本 `boards/avaota-a1.conf` 声明 ARM64、`allwinner/sun55i-t527-avaota-a1`，
`bsp` 路径选择 `AvaotaSBC/linux` 的 `linux-5.15` 与 `sun55i_t527_bsp_defconfig`；
当前固定内核 Makefile 标识 **5.15.154**，不是核桃派的 5.15.147 分支。
默认 boot 后端是 `sunxi-syterkit`，固定 SyterKit
`0998aeb48289058d26b3c24febb3405e29b66898`，配置注释说明其包含 A1 4GB 启动修复；
目录里存在其他 boot 后端不表示 A1 默认使用 U-Boot。
官方[获取内核](https://docs.avaota.fun/linux-manual/sdk-develop/kernel-dev/get-kernel/)与
[BSP 介绍](https://docs.avaota.fun/linux-manual/sdk-develop/bsp-driver/bsp-intro/)区分 LTS 基线、bsp 目录及补丁；
使用 LTS 源码不等同于只用 vanilla 主线就能驱动全部 T527 外设。

AvaotaOS 把 rootfs、内核、bootloader 和打包分开，支持板级配置与 hook，适合比较后端划分。
其配置仍包含 A1 专属的网络、显示、无线和启动参数；不能将整个镜像、DTB 或内存初始化文件刷到 CM2。
所选 defconfig 启用 `AW_DRM`／`AW_DRM_PANFROST`，关闭 `AW_DISP2`。
DSI host 源码使用标准 `mipi_dsi_host_ops` 和 DRM panel 生命周期，可作为 CWU50 移植候选路径；
当前并未编译或验证该候选，Noble GNOME 的板级 hook 仍禁用 Wayland。

打包脚本把 FAT 从 16 MiB、ext4 从 256 MiB 开始，SyterKit 在镜像偏移 8192 bytes；
这不同于核桃派当前 FAT `4M` 起点。正常 pack 的目标是生成的镜像文件；底层 helper 接受目标参数，
不能把它未经改造当作本工程已有的只写文件／禁止写设备合同。

构建脚本可以参考，执行前需要建立本工程的固定输入、隔离目录、产物／错误语义和部署边界。
本次没有运行上游构建／刷写脚本，也没有把 A1 加为可构建板型。

## 外设调试手册与6.18迁移

[BSP 驱动手册索引](bsp-manual-index.md)收集18页Linux BSP官方资料，分别提炼总线／引脚、
时钟／系统／采集外设的定位步骤、主动测试条件和验收判据，保留版本限制和原文问题。
它们不自动执行厂商示例，也不把RTOS HAL接口混入Linux ABI。

[T527 6.18 LTS迁移评估](../../knowledge/kernel/t527-linux-6-18-migration.md)核对正式上游已有的A1支持、
DSI显示缺口与厂商BSP向前适配边界；来源见 [kernel-port-sources.json](kernel-port-sources.json)。
现阶段只调查可行性，未修改现有内核基线或运行移植构建。

## 与核桃派 CM2 的关系

[CM2 资料](../walnutpi-cm2/README.md)公开了模块的接口／机械和 IO 底板，缺少模块内部完整电路；
A1 的 T527 整板图与 LPDDR4 布局可以提供同 SoC 的设计对照。
能共用的是经过核对的 SoC 引脚功能、电源／复位／DRAM 设计方法和部分软件机制，
不是未经验证的电源接法、DDR 拓扑、PMIC 配置、GPIO 编号、DTB、boot 二进制或整机兼容结论。
未来自制 uConsole 核心卡还要满足 [Clockwork 最小接口](../../knowledge/hardware/uconsole-custom-core-interface.md)。

## 驱动源码与二进制交付核查

[T527 专项审计](../t527-driver-audit/README.md)已沿NPU／VIN ISP的构建对象核对源码，
并静态检查实际ISP／NPU用户包。内核源码可见不等于用户态算法全部可重建；
旧VFE／HDMI等预编译对象也不能混同当前T527所选路径。

## 原件与许可

原件位于忽略的 `image/work/avaota-a1-research-20261101/`，每份位置与摘要见 `sources.json`。
Git 保存来源目录和独立说明，原始 PDF／CAD／模型保持原样；不复制 API 作者身份、现场网络或凭据。
硬件仓库根 LICENSE 为 **CERN-OHL-S-2.0**（Strongly Reciprocal）；AvaotaOS 根 LICENSE 为 GPL-3.0。
文档、内核、boot、Allwinner IBIS 和第三方模型仍按原始条款分别核对，不能给整批归档统一再许可。
这些文件是资料库，不是自动下载器、镜像输入配方或硬件验证结果。
