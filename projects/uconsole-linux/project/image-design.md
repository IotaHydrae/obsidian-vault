# Image composition

镜像配置分成独立选择：**根文件系统来源、桌面环境、板型、内核来源与启动后端**。Ubuntu Base 是根文件系统来源；Ubuntu Desktop、XFCE、KDE 是桌面组件。它们不应被当作同一个枚举，也不应让所有发行版依赖 pi-gen，或让所有计算模块依赖 Raspberry Pi 的固件与内核。通用板型模型与 Rockchip 扩展边界见 [板型/平台设计](board-design.md)。

## 配置与实现边界

`image/profiles/*.toml` 是组合配置，schema_version 当前为 1：

| 字段 | 含义 |
| --- | --- |
| `name` | 配置档标识，用于默认镜像名与构建身份 |
| `rootfs_provider` | 根文件系统构建后端，如 `pi-gen`、`ubuntu-base`、`arch-base` |
| `distribution` / `release` | 发行版与版本，如 raspios/trixie、ubuntu/24.04.5、archlinuxarm/rolling |
| `architecture` | 目标架构，与所选板型声明匹配；CM4/CM5 使用 arm64 |
| `desktop` | 桌面组件标识，也可以是 `none` |
| `board` | `board/<target>/board.toml` 的板型；当前实现 uconsole-cm4、uconsole-cm5、cm5-io-board |
| `kernel` | 内核来源标识，当前 Pi 后端支持 project；其他后端可定义厂商/上游来源 |

`image/components.toml` 描述兼容性和实际实现状态。`providers.*.available` 表示后端已实现，`platforms` 声明可接入的平台，可选 `boards` 列表进一步限制板型；`desktops.*.providers` 列出已实现该桌面安装路径的后端。pi-gen 仅支持 raspberry-pi；Ubuntu Base 的组件模型预留不同平台，当前具体启动与导出实现只接入 Raspberry Pi。已登记但未实现的选项会在构建前明确报错；登记不代表已经能生成或启动该系统。

| 组件 | 当前实现 |
| --- | --- |
| pi-gen / Raspberry Pi OS Trixie | 已实现构建路径 |
| Raspberry Pi Desktop | pi-gen stages 0–4 |
| 无桌面 | pi-gen stages 0–2；Ubuntu Base 和 Arch Base 使用各自的原生根文件系统流程 |
| ubuntu-base | 已实现 24.04.5 ARM64 原生构建，配置档 `ubuntu-noble-base` 默认 CM4、无桌面 |
| arch-base | CM5 IO Board、ARM64、无桌面；独立 Arch Linux ARM rootfs、pacman 与 mkinitcpio |
| labwc + Waybar | Ubuntu 配置档 `ubuntu-noble-labwc`；CM4 已确认横屏方向、轨迹球和终端快捷键，默认缩放 150% |
| labwc + Noctalia 5 | 可选配置档 `ubuntu-noble-labwc-noctalia`；CM4 已完成功能验证，复用两个预编译包；Waybar 保持默认 |
| Ubuntu Desktop / XFCE / KDE | 已预留组件，安装策略待实现 |

选择示例：

```sh
make image-build PROFILE=raspios-trixie-desktop
make image-build PROFILE=raspios-trixie-lite
make image-prepare PROFILE=raspios-trixie-desktop DESKTOP=none
make image-build BOARD=uconsole-cm4 PROFILE=raspios-trixie-desktop
make image-build BOARD=uconsole-cm4 PROFILE=ubuntu-noble-base
make image-build BOARD=uconsole-cm4 PROFILE=ubuntu-noble-labwc-noctalia
make image-build BOARD=cm5-io-board PROFILE=ubuntu-noble-base
make image-build BOARD=cm5-io-board PROFILE=arch-base
```

`DESKTOP` 只覆盖桌面组件，其他身份保持在配置档中。为可长期保存、同时导出的不同镜像建立独立配置档；默认文件名包含配置档名称、桌面标识和架构。`image/config` 和忽略的 `image/config.local` 仅用于 pi-gen。Ubuntu Base 使用 `image/ubuntu-base/config.toml` 和忽略的 `config.local.toml`；Arch Base 使用 `image/arch-base/config.toml` 及同目录忽略的本地文件，各后端的密码和软件源设置保持独立。发行版、架构和桌面的选择以配置档为准。

`cm5-io-board` 是独立 rootfs/IO 开发配置，板型声明允许 `ubuntu-noble-base` 或 `arch-base`，桌面限 `none`；通过命令行覆盖桌面也不能绕过限制。其 Ubuntu 设置改用 `image/board/cm5-io-board/config.toml` 和同目录的忽略本地文件，默认 `pi` / `1`、主机名 `cm5-io-board`，不读取 uConsole 的本地设置。快照不复制原始账户 TOML，工作配置仅保存密码哈希。GPIO UART 使用 GPIO14/15、`ttyAMA0`、115200 8N1，并启用串口登录服务；保留 HDMI 文本控制台。检查器要求此串口配置并拒绝 uConsole 面板/背光驱动、overlay、旋转及 GPIO 音频载荷。

## 构建层次

交互入口为 `./build.sh lunch`。菜单从板型、配置档和组件目录读取选项，使用组件可选的 `label` 显示名称；可用性与兼容性仍由共同模型校验，待实现选项不能保存为构建配置。

```text
组合配置
  ├── 根文件系统后端：获取/构建基础系统，配置软件源和账户策略
  ├── 桌面组件：安装匹配发行版的桌面包、显示管理器和会话默认值
  ├── 共享板级输入：校验过的内核包、启动配置、音频服务
  └── 镜像导出：分区布局、文件系统、boot/root 文件与产物检查
```

`scripts/image_build.py` 只处理参数、配置校验和后端分派。`tools/image_profiles.py` 定义配置模型和兼容性检查。`scripts/image_backends/pi_gen.py` 拥有 pi-gen 的源码快照、stage 选择、容器调用和导出细节。pi-gen stages 将上述层次串在同一工具中，这是该后端的实现方式。`scripts/image_backends/ubuntu_base.py` 则取得固定的 Ubuntu Base 压缩包，通过原生 ARM64 chroot 安装 Ubuntu 软件包、内核和板级文件，再调用 `rpi_image.py` 生成普通镜像文件。

板型与启动文件放在 `board/<target>/`，镜像文件集合由板型的 `payload` 选择；Pi 音频服务由 CM4/CM5 共用 `image/board/uconsole-rpi/`。内核通过 `tools/kernel_artifact.py` 按板型提供经过校验的安装包。`image/stage-uconsole/` 只负责把这些输入接入 pi-gen，不作为其他发行版或 SoC 的构建入口。

## Arch Base 实现

`arch-base` 先限定 CM5 IO Board / ARM64 / 无桌面。它用独立 provider 和
`scripts/image_backends/arch_base.py` 获取固定、签名已验证的 Generic AArch64
归档，完整更新官方 Arch ARM 软件源中的软件包，再安装项目内核和板级配置。
Debian 格式内核包只作为文件容器，暂存后复制并保护 usrmerge；mkinitcpio 与
ALPM hook 管理 initramfs。网络、用户、SSH 和首次启动密钥环采用 Arch 的原生
服务。导出复用 `rpi_image.py`，检查分派到 Arch 包数据库与服务约定。
Omarchy 尚不属于此配置，构建/检查与实机启动分开记录。
输入信任、配置和产物详见 Arch Base 说明（工程源码：`image/arch-base/README.md`）。

## Ubuntu Base 实现

`ubuntu-noble-base` 固定 Ubuntu Base 24.04.5 ARM64，默认 `desktop=none`。
`ubuntu-noble-labwc` 选择 `labwc-waybar`，也可通过 `DESKTOP=labwc-waybar` 覆盖基础配置档。
桌面准备要求通过回执校验的原生 labwc/Waybar 软件包；缓存缺失时不会创建新的构建工作目录。
`ubuntu-noble-labwc-noctalia` 选择 `labwc-noctalia`，也可通过 `DESKTOP=labwc-noctalia`
覆盖 Ubuntu 基础配置档。它额外要求通过回执校验的 Noctalia 包；普通镜像构建安装缓存的
`.deb`，不重新编译桌面。Waybar 保持默认，CM4 功能验证与后续硬件边界见
[桌面对比记录](../knowledge/desktop/labwc-shell-comparison.md)。
配置档默认选择 CM4，也可选择 CM5；后端可用性不表示对应硬件已验证。
Ubuntu Desktop、XFCE、KDE 的安装与会话策略仍未实现，选择时会提前报错。

桌面源文件、软件包和回执参与 Ubuntu 工作目录身份。后端快照桌面输入，安装同一个
`image/desktops/install.py` 文件集合，记录发行版依赖版本并启用普通用户 greetd 会话。
镜像检查核对输入、文件内容与权限、账户、软件包和服务配置。检查通过不代表实体屏幕验证。

基础压缩包及 Raspberry Pi `pinctrl` 源码由 `image/ubuntu-base/recipe.json` 固定 URL、提交和 SHA-256。
基础包校验值来自官方签名清单；后续软件包由 Ubuntu Noble、updates、security 的签名仓库提供。
构建回执记录实际软件包版本和输入哈希，不承诺软件仓库随时间变化时逐字节复现镜像。

默认内核仍通过当前严格的 `load_kernel_artifact()` 校验。根文件系统试验可在独立本地配置中
显式指定历史报告与该报告的 SHA-256，再核对报告、包、配置和镜像字节。
历史输入保留自己的补丁与配置身份；它不表示符合当前源码，也不改变严格校验器。
Ubuntu 后端在保留内核包驱动字节的同时，在包配置前安装当前 initramfs 集成 hook，
并在回执中记录该 hook。镜像始终使用匹配的 initramfs，未安装 pi-gen 的 CM4 direct-root 定时策略。

镜像根文件系统在私有挂载命名空间中以 sudo/chroot 构建；x86 主机通过带 F 标志的 ARM64 binfmt/QEMU 执行目标程序。
最终导出只操作普通文件：DOS 分区表、从 1 MiB 开始的 512 MiB FAT32、从 513 MiB 开始的 ext4。
根文件系统内保留空的 `/boot/firmware` 挂载点；首次启动服务扩展第二分区并生成独立 SSH 主机密钥。
配置、命令、产物与检查方法见 Ubuntu Base 说明（工程源码：`image/ubuntu-base/README.md`）。

## 扩展其他后端与桌面

新增后端模块并登记在前端 `BACKENDS` 中，提供 `prepare(profile) -> Path` 和 `build(profile, builder) -> None`，分别返回后端配置路径、完成镜像文件导出。后端需要：

1. 从对应发行版取得可校验的 ARM64 根文件系统，记录版本和校验依据；Ubuntu Base 的解包、软件源和首次启动设置由 Ubuntu 后端负责。
2. 根据 `desktop` 选择独立安装策略。Ubuntu Desktop、XFCE、KDE 的软件包与显示管理器策略由对应发行版实现；不得将 Raspberry Pi Desktop 包直接复用于 Ubuntu。
3. 使用所选平台的产物校验器验证内核，并接入板级文件；当前 Pi 产物使用 `load_kernel_artifact()`。固件包、initramfs 更新和启动文件布局由对应平台的启动后端负责，根文件系统后端完成安装连接。
4. 按完整输入隔离工作目录、导出镜像文件、检查内核/设备树/模块/服务及启动分区引用，然后才把组件状态标为可用。

保留后端自己的配置命名空间，避免让 Ubuntu 的选项依赖 pi-gen shell 变量。所有后端都遵守相同边界：构建只产生文件，不部署到 uConsole，也不隐含授权刷写 eMMC。
