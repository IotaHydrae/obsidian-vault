# Board and platform design

工程支持不同厂商的计算模块和 SBC。**板型、CPU 架构、SoC 平台、内核来源、启动后端、根文件系统和桌面是不同的选择。** CM4/CM5 是已实现路径的具体板型；其他 CM 接口模块通过自己的板型与后端接入。接口外形相同不能替代具体板型的引脚、供电、显示与存储描述。

## 通用板型声明

每个实际目标放在 `board/<name>/board.toml`，schema_version 为 1：

| 字段 | 含义 |
| --- | --- |
| `name` | 与目录一致的板型标识 |
| `architecture` | 镜像 CPU 架构，与所选根文件系统匹配 |
| `platform` | 平台标识，例如 raspberry-pi、rockchip；没有封闭的厂商枚举 |
| `kernel_backend` | 内核构建与打包后端标识 |
| `boot_backend` | 固件/启动镜像安装方式，例如 rpi-firmware、u-boot |
| `payload` | `image/board/` 中的板级文件集合；允许同平台板型共享 |
| `available` | 是否实现了该目标的构建路径；不代表实机验证通过 |
| `image_profiles` | 可选的非空、唯一配置档标识列表；省略时不额外限制配置档 |
| `image_desktops` | 可选的非空、唯一桌面标识列表；省略时沿用组件兼容性 |
| `[kernel]` | 由内核后端解释的配置命名空间，不要求其他平台使用树莓派字段 |

`tools/boards.py` 只解释通用身份，不规定 GPIO 控制器、设备树名称、分区布局或引导程序。镜像配置档通过 `board` 引用它；`BOARD=...` / `--board` 可覆盖。根文件系统后端声明支持的 `platforms`，因此 pi-gen 不能被意外用于 Rockchip。

## 当前 Raspberry Pi 实现

`scripts/kernel_build.py` 负责板型加载与后端分派。`scripts/kernel_backends/raspberry_pi.py` 负责固定 Raspberry Pi 源码、交叉编译、私有固件目录与 Debian 内核包。`tools/kernel_targets.py` 解释该后端的 defconfig、Image 名称、DTB、overlay 和补丁子集。

| 目标 | 内核基线配置 | 固件 Image | GPIO/显示路径 |
| --- | --- | --- | --- |
| uconsole-cm4 | bcm2711_defconfig | kernel8.img | BCM2711 GPIO / DSI1 |
| uconsole-cm5 | bcm2712_defconfig | kernel_2712.img | RP1 GPIO / DSI1 |
| cm5-io-board | bcm2712_defconfig | kernel_2712.img | 官方 CM5IO DTB / 标准 HDMI KMS / GPIO UART |

`kernel/patches/series` 是全部补丁的应用顺序；每个板型选择保持此顺序的子集，允许空子集。两个 uConsole 板型共用面板与背光驱动，使用各自的设备树补丁和配置增量。`cm5-io-board` 使用空补丁集及单个官方 `bcm2712-rpi-cm5-cm5io.dtb`；仅允许 `ubuntu-noble-base` 和 `none`，菜单与命令行校验共享这一限制。构建对象、产物和当前报告按板型区分，镜像只接受相同板型的内核。uConsole CM5 保留已发布的 `kernel/build/current.json` 路径；其他目标为 `current-<board>.json`。

音频脚本使用 Raspberry Pi `pinctrl`，放在 `image/board/uconsole-rpi/`，由两个 uConsole 板型共享。`cm5-io-board` 使用自己的载荷和账户设置，不安装此脚本；保留 `pinctrl` 作为 IO 调试工具。它不是跨 SoC 的通用 GPIO 实现。`rpi-firmware` 的启动配置和 FAT/ext4 导出由 pi-gen 或 Ubuntu 适配器接入；当前镜像检查器明确只检查这个布局。共享包名 `uconsole-kernel` 和固件目录 `uconsole/` 是工程打包约定，不表示启用了 uConsole 硬件。

## 增加 Rockchip、Allwinner 等目标

只有确定具体模块/载板型号后才创建实际板型配置。通用模型可以表达 `platform="rockchip"`、`kernel_backend="rockchip"`、`boot_backend="u-boot"`，同时选择 `rootfs_provider="ubuntu-base"` 和独立桌面。工程测试使用临时 Rockchip 板型验证这一点，不把虚构目标放入可构建目录。

新增平台需要实现：

1. 内核后端，明确上游/厂商内核来源及其固定版本、补丁、配置、DTB 和产物校验；不能默认复用 Raspberry Pi Linux。
2. 启动后端，负责模块实际需要的 BootROM、DDR 初始化、SPL、ATF/OP-TEE、U-Boot、extlinux/FIT 等输入与布局。其分区表、存储偏移、启动文件和完整性检查由该平台定义，不能默认套用 Pi 的 config.txt 和 FAT/ext4 双分区。
3. 根文件系统后端的安装连接点，接入该平台内核、固件、启动后端与板级服务；通用系统配置与桌面选择继续共用配置模型。
4. 对应的镜像检查。先实现并验证构建，再将 `available` 标为 true；实机验证单独记录。

具体规划见 [Roadmap](roadmap.md)：CM3 复用 Raspberry Pi 平台但需独立 BCM283x 板型；
核桃派 CM2／T527 属于 Allwinner 平台，其 BootROM／DDR 初始化／引导布局和内核来源须单独确认。
CM4 接口形式不赋予 Raspberry Pi 固件、GPIO 控制器或设备树兼容性。
这些后端目前尚未实现。所有平台的镜像生成与部署都是独立操作；通用构建接口不写物理设备。
