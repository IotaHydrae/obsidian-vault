# T527：从 5.15 BSP 向 Linux 6.18 LTS 迁移

## TL;DR

- 可行，而且正式 6.18 已有 Avaota-A1／Orange Pi 4A 的 T527 设备树，不必从空白 SoC 支持开始。
- 优先评估上游 6.18 加目标板 DT／必要补丁；5.15 BSP 保留为硬件行为与缺失驱动参考，不能整目录复制后宣称完成。
- “能进系统”和“uConsole 的 DSI、USB键盘、电源、GPU及所有厂商功能可用”是不同验收层次，内屏 DSI 是必须单独解决的缺口。

## 当前核验的来源

2026-11-01 查询 kernel.org releases.json，6.18 为 longterm，当时发布点是 6.18.60（2026-10-30）。
正式 v6.18 与 stable v6.18.60 的 Allwinner DT 目录都包括：

- `sun55i-t527-avaota-a1.dts`
- `sun55i-t527-orangepi-4a.dts`
- 公共 `sun55i-a523.dtsi`，由上述板级 DT 明确引用，不是仅凭近似芯片名推断兼容。

官方 A1 DT 的 compatible 包括 `allwinner,sun55i-t527`。当前目录未发现 WalnutPi CM2 专属 DT；
CM2 要根据公开接口、可恢复原厂系统和实际模块信息建立自己的板级 DT，不能改 model 名就替换 A1。
文件、仓库和网页快照身份见 [迁移来源清单](../../reference/avaota-a1/kernel-port-sources.json)。
这是 kernel.org／stable 的上游基线，不等于本工程固定 Raspberry Pi 分支中相同版本号的目标支持。

## 上游已提供的起点与尚缺部分

下面表示**源码已描述／启用的支持**，不是本项目已在 A1／CM2 测试通过。

| 路径 | v6.18.60 A1 DT／公共 dtsi 的事实 | 迁移时仍要验证 |
| --- | --- | --- |
| CPU／串口／基础时钟 | Cortex-A55、PSCI、UART0、CCU／pinctrl／IRQ | boot固件、时钟／供电、串口console和稳定运行 |
| SD／eMMC | A1启用mmc0与mmc2，声明供电／总线宽度 | CM2控制器／cd／VIO／容量／DDR初始化与根分区身份 |
| 以太网 | A1启用gmac0，外部PHY与reset／供电 | 目标PHY、接口模式／延时和收发；不能推导所有网口均可用 |
| USB | A1启用EHCI/OHCI及PHY，OTG口设peripheral | CM2的物理口、ID／VBUS、Host角色和uConsole Hub/HID |
| PMIC | AXP717／AXP323、regulator／ADC等描述 | CM2内部PMIC未知；主板AXP管理和关机仍需独立适配 |
| GPU | A1启用Mali Valhall-JM节点及供电／clock／power-domain | Panfrost／Mesa版本、render作业与显示输出分别验收 |
| 内置显示 | 当前A1／公共DT未提供完整DSI/TCON/DE/HDMI显示链 | 上游／候选补丁或BSP前移植、CWU50时序／命令、PHY、图和电源 |

GPU渲染设备与显示控制器不是一回事；GPU节点存在不能证明DSI屏幕可扫描输出。
DVFS／散热、音频、摄像头、NPU、视频编解码与其他高速外设也要逐项核对，不能从基础启动推广。

## 两条路线

| 路线 | 适用目标 | 主要代价和边界 |
| --- | --- | --- |
| 上游6.18＋目标板支持／必要补丁 | 长期维护、先最小系统，逐项补uConsole功能 | 已有基础设施可复用；缺失显示／高级外设需要回移补丁或重写绑定 |
| 将厂商5.15 BSP向前适配到6.18 | 必须保留某些厂商独有能力 | 需调整Kbuild、内核API、binding和子系统集成，可能与主线同硬件驱动冲突 |

Avaota把驱动、include、configs等放在独立bsp目录，文档有跨内核接入方法，
这能降低组织耦合，但Linux内核内部没有跨版本稳定驱动ABI；5.15 `.ko` 不能直接用于6.18。
Kconfig／Makefile／头文件接入只是开始，probe、IRQ、clock/reset、regulator、DMA/IOMMU、
pinctrl、PM、DRM和用户态接口需要针对真实调用点调整并验证。
不同时启用两个驱动去占同一控制器，也不把私有bsp DT属性当作主线binding。

A1 5.15.154 BSP有基于标准MIPI-DSI／DRM panel生命周期的host，
可作为 [CWU50适配候选](../hardware/avaota-a1-t527-reference.md)研究；
其显示engine／TCON／PHY／clock依赖仍需要闭包分析，不只移植面板文件。
核桃派5.15.147路径、GPU模块和用户态包又不同，不把两套BSP统称为同一个版本。

## 已有6.18运行参考与不能推广的部分

[Orange Pi 4A 社区原作者项目](https://github.com/ut-slayer/orangepi-4a-mainline/tree/38eb27cff2181ca784bca1030b83e41d8ec789e1)
在所查提交声明基于 vanilla 6.18.55 加 143 个补丁，并报告其 T527 板上 HDMI KMS、
Panfrost／GNOME Wayland、USB HID、eMMC、网络和温控／DVFS等运行结果。
这是一手其他板的实测报告，本项目没有复现；该系列没有声明DSI通过，suspend/resume仍列为失败。
补丁涉及显示engine／TCON／HDMI PHY／IOMMU、时钟、电源与热管理，适合按需审查依赖，
不能将整套DT、GPU OPP或静态memory／simplefb workaround直接施加到CM2。

Avaota官方当前仓库配置／patch目录未找到6.18目标；手册里的6.6目录是结构示意，
不是6.18已完成支持。所查 `bsp/platform/Kconfig.common` 的版本数公式
`major*100 + minor*10 + sublevel` 对5.15.154算804、6.18.0算780，会颠倒版本关系。
实际迁移需要修复／替代这种比较并验证所有相关条件编译，不能因存在版本宏就认为跨版本可靠。

## 建议的上板顺序

1. **保存5.15可恢复基线。** 固定模块／载板、boot固件、DRAM容量、串口、存储和原厂DT／日志；
   用独立介质或候选启动项，保留能覆盖早期卡死的恢复方法。
2. **6.18最小启动。** 使用上游匹配板级DT或审查过的CM2 DT，先串口、RAM、根文件系统和SD／eMMC；
   旧BSP的ttyAS0与主线UART命名／earlycon参数需核对，不能把无网络等同未启动。
3. **键盘与电源。** 验证USB2 Host→Hub→实际HID输入，PMIC I²C／IRQ、稳压器与真实关机；
   rootfs账户／SSH可以复用，Pi GPIO脚本、rpiboot和Pi固件布局不能复用。
4. **内屏。** 补完整DRM显示链，再做CWU50 DCS、reset、backlight、电源和lane验证；
   写已知画面并确认实体可见，背光亮、connected或截图有内容都不是点屏通过。
5. **GPU和可选功能。** 独立确认GPU作业、桌面输入、冷热启动、温控／功耗，再做音频、摄像头、NPU等。

bootloader可以先保留目标板上已验证的DDR初始化和标准ARM64 Image/DTB加载路径，
Linux升级不意味着可以拿另一块板的DDR／BL31／SCP二进制替换。内核DT与用户态驱动闭包须重新匹配。
HDMI仍为可选，不让外屏先亮替代uConsole内屏验收。

## 驱动源码可得性的后续核查

[专项审计](t527-driver-source-availability.md)确认所查NPU与当前VIN／ISP内核路径有实现源码，
不是只有预编译ko。另已确认VIPLite用户运行库和ISP算法包存在二进制实现，
旧VFE／HDMI路径也有预编译archive；应按目标选择和用户ABI分别评估，不一概视为内核移植堵点。

## 预期与未知项

本次是迁移可行性调查和Roadmap补充，没有开始代码移植、编译或刷写。
当前证据支持“6.18可作为T527的新基线”，不支持“所有BSP功能已经在6.18上可用”。
实际优先路径是主线基础＋最小目标板适配，再按[手册方法](../../reference/avaota-a1/bsp-manual-index.md)验证缺口。
工期取决于目标CM2的启动信息、DSI显示链和厂商功能取舍，不在没有原型证据时承诺全功能完成时间。
