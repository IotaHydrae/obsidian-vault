# 核桃派 CM2／T527：资料范围与 uConsole 接入边界

## TL;DR

- CM2 具备四通道 DSI 和 USB Host 候选链路，但不能仅凭 CM4 形式判断 uConsole 整机即插即用。
- 当前官方 T527 路径采用 SyterKit 和 Linux 5.15.147 BSP；本仓库的 Pi 固件、VC4/V3D、GPIO 和 overlay 不能直接套用。
- [官方资料目录](../../reference/walnutpi-cm2/README.md)分开保存核心模块机械／引脚资料与 **IO 底板**电路；未公开核心模块完整原理图。

## 先锁定目标和公开范围

2026-11-01，用户已购入 WalnutPi CM2；照片可见 T527／CM2 标识，内存、eMMC 与实际硬件修订仍待到货确认。
`walnutpi-2` 是产品家族仓库，同时含 2B 整板与 CM2 计算模块，不能混用板级外设数量、
存储选项或引脚定义。CM2 标称 55 × 40 × 4 mm、1／2／4 GB LPDDR4、可选 eMMC；
具体 SoC 功能和系统 API 支持必须分开记录，宣传的 GPU／NPU 指标不等于本工程已验证。

已找到 CM2 模块引脚图、尺寸图、3D，以及 CM2 IO 底板原理图／PCB 项目入口。
底板接线可核对模块接口外部用途，但不能补全 SoC／DDR／PMIC 内部电路、复位逻辑和电源完整性。
本次未下载或运行完整系统镜像，没有改变现有 CM5 或 uConsole。

## 与现有 CM4 转接卡的关键交点

依据官方 `hardware/CM2/pinout_en.png`，下面使用两只 BTB 的全局 1–200 针号，
不是 Clockwork 金手指 J1 编号；映射右侧用途再参照 [uConsole 最小接口](uconsole-custom-core-interface.md)。

| BTB 针号 | CM2 标记 | uConsole 适配意义 |
| --- | --- | --- |
| 35 / 36 | PI15/TWI2-SCK；PI16/TWI2-SDA | 经现有 adapter 对应主板 PMIC SCL／SDA，需实现正确控制器／电平与 AXP 驱动 |
| 39 | PI2/SPI1-CS0 | 经 adapter 对应 LCD_RESET，需排除 SPI 复用；不能使用 BCM GPIO8 号直接操作 |
| 40 | PI5/SPI1-MISO | 经 adapter 对应背光，需排除 SPI 复用占用并实现背光控制时序 |
| 58 | PB5/TWI1-SDA | 经 adapter 对应 PMIC IRQ，需配置为输入中断，避免 I²C 数据复用 |
| 101 / 103 / 105 | USB0-ID／USB0-DM／USB0-DP | 经 adapter 对应主板 Hub 上游；要选择 Host 角色而非仅 Device／FEL |
| 175/177、181/183、187/189、193/195、194/196 | DSI0 D0、D1、CLK、D2、D3 N/P | 物理差分位置对应 CM4 DSI1；控制器名不同，CWU50 初始化仍需移植 |
| 77/79/81/83/85/87 | 5 V | 与 adapter 主电源输入位置相交；电流、启动瞬态和倒灌不由位置相同保证 |
| 84 / 86 | 3V3_OUT | 原图说明系统完全启动后才输出；不能作为通用启动期 SD 供电假定 |
| 92 / 93 | PWRON／BOOT | 不能当作 Pi RUN_PG／nRPIBOOT 的同义信号，需独立恢复／关机设计 |

图中还标注 1V8_OUT 与部分不同的控制／保留触点。上述不是全 200 针兼容认证，
未列出的针、所有地回流、电平参考、启动状态与关机状态仍须核对。
CM4 adapter 图把模块自身 3.3／1.8 V 输出画为未连接，而主板 SDX_VDD 由自己的电源网提供；
应分析实际板卡完整电源树，不把名称相似的输出与输入硬并。

官方参数表列 CM2 USB2×1；引脚图／底板电路另外分出 USB0 OTG、USB1 和 USB2。
这些是不同计数口径，保留来源原文，不能把它们合并成“一个可随意切换的 USB 接口”。

CM2 的 SPI／GPIO 复用、MIPI Host 和 USB 角色需要 T527 对应 DT／驱动。
复用 CWU50 命令表是可评估的方法，但现有 DRM panel 接口能否直接挂入该厂商显示栈尚未建立。

## 官方构建链与本仓库的分工

固定调查的 `walnutpi-build` 以 `board/walnutpi-2b/board.conf` 描述 T527，
选择 `BOOTLOADER_NAME=syterkit`、`walnutpi-2b.cmake`、Linux `5.15.147` 分支和
`walnutpi2b_defconfig`；默认 `fdtfile=sun55i-t527-walnutpi-2b`。
未发现独立 `board/walnutpi-cm2/`，只能确认当前公开配置的组织方式，不能据此断言 CM2 绝不受支持。
官方发布镜像／CM2 DT 选择方式和实际启动配置需在实物上确认。

| 层次 | 参考官方流程 | 本工程需要明确的合同 |
| --- | --- | --- |
| boot | SyterKit，板级 DDR／固件输入，独立 boot 包 | 固定输入、介质偏移、启动／恢复方式及供电条件 |
| kernel | T527 BSP、DT、模块与固件 | 独立 Allwinner 后端；不套用 Raspberry Pi 内核或 DTS |
| rootfs | Debian／Ubuntu 包列表及服务 | 评估复用基础用户空间、账户和 SSH；按平台替换硬件依赖 |
| image | 把 boot／kernel／rootfs 组合成文件 | 由平台定义分区／raw 区域检查，保持生成与刷写分开 |
| graphics | Mali／显示 BSP 与相关用户态 | 独立核对驱动 ABI 和渲染证据，不能搬用 VC4/V3D Mesa 配置 |

官方镜像脚本使用 DOS 分区表，FAT32 从字面 `4M` 开始，后接 ext4；
`boot.bin` 写在镜像偏移 8192 bytes，另有可选的 1 MiB／2 MiB boot 文件。
更重要的是 `scripts/boot/deb/boot-postinst.sh` 会查 `/boot` 对应块设备并 raw `dd` 写 boot 区域。
因此未来复用不能把“安装该 boot 包”当作只复制文件的普通检查；
构建器、安装器与物理介质写入必须在本工程重新分离、明确目标。

内核打包还单独构建／安装 `bsp/modules/gpu`，2B Debian12 包表指定 `mesa-xorg-g57-debian12`；
厂商面板在 `bsp/drivers/video/sunxi/disp2/disp/lcd/`，含 JD9365DA 类实现。
目录／名字相似仅提供参考，不证明选择了哪份 GPU 驱动或 CWU50 已可用。
板级 boot 目录还带 BL31、SCP 和 splash 二进制，DDR 初始化也需单独固定来源与许可。

构建输入指向可变分支；调查固定的 SHA 只描述本次读到的源码，不自动使上游完整构建可复现。
本工程后续要同时固定引导／内核／固件／工具链／必要二进制及各自许可，
再建立可校验产物和镜像检查；先不直接执行上游 sudo 构建脚本。

## 先做哪一步

按 [Roadmap](../../project/roadmap.md)先确认模块身份／原厂可恢复基线，再核对 uConsole 电源和 BTB。
有条件先使用匹配底板记录启动串口、DTB、存储、网络和干净关机，再接入现有金手指转接卡。
内置屏幕、键盘 USB Host、PMIC 和至少一种启动存储构成基本验收；HDMI、NPU、摄像头和重型桌面另测。
核桃派与 CM3 两条路线分别维护，CM3 成功不建立 T527 固件／驱动兼容性。

## 同 SoC 的补充参考

[Avaota-A1 的开源设计](avaota-a1-t527-reference.md)提供整板 SoC／DDR／电源与 PCB 资料，
可用于研究 CM2 未公开的内部设计方法；其电路、固件、DRAM 拓扑和软件板型仍独立核对。
同为 T527 只是可交叉研究的依据，不是 A1 镜像可直接用于 CM2 的证明。

## 证据与未知项

[来源清单](../../reference/walnutpi-cm2/sources.json)保存官方 URL、固定仓库提交、文件摘要及本机缓存位置。
仓库和 Wiki 各自标注版本，Wiki 的网页快照不是厂商签名或硬件实测。
原件 PDF／图／ZIP 已读取，接口摘要来自引脚图与底板图的交叉核对；没有模块级电气测量。

仍未知：实际模块装配／revision、当前发布镜像如何选 CM2、BOOT／PWRON 的完整时序、
uConsole adapter 的供电裕量、CWU50 与 T527 显示栈的连接方式、USB Host 恢复和关机可靠性、
GPU 用户态／内核闭包、全 200 针兼容性及厂商未公开的内部模块电路。
