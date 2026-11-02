# uconsole-linux

为 ClockworkPi uConsole 的计算模块和 SBC 构建 Linux 镜像。工程分别选择板型、根文件系统、桌面、内核后端和启动后端，不限定模块厂商。CM4、CM5 是首批具体目标，使用固定版本的 Raspberry Pi Linux 6.18 与官方 ARM64 pi-gen；Rockchip 等平台通过独立后端扩展。

## 当前状态

CWU50 显示屏、OCP8178 背光及 CM4/CM5 设备树保存在独立补丁中。工程提供内核交叉编译、Debian 内核包、Trixie ARM64 桌面镜像和 Ubuntu Base 24.04.5 ARM64 镜像构建流程；Ubuntu 可选择无桌面或 labwc 桌面；另提供 CM5 IO Board 的 Arch Linux ARM Base 无桌面镜像。实际构建结果、实机观察和未验证事项见 [开发状态](status.md)。生成镜像与部署到设备是两个独立操作。

新增 cm5-io-board（工程源码：`board/cm5-io-board/README.md`） 开发基线，面向官方 CM5 IO Board 的 rootfs 和 IO 测试，为后续终端适配积累可验证配置。该板型支持 Ubuntu Base 24.04.5 和 Arch Linux ARM Base，均为 ARM64、无桌面，不应用 uConsole 内核补丁、屏幕旋转或 GPIO 音频服务；默认账户为 `pi`、密码 `1`，启用 SSH 和 GPIO UART 登录控制台。2026-10-28 已完成 Ubuntu Base 的真实内核/镜像构建、只读检查及 CM5 eMMC 刷写与完整镜像范围读回校验；用户随后拔掉 nRPIBOOT 跳线并重新上电，首次启动、密码 SSH、以太网 DHCP/DNS 和根分区扩展均已验证，UART 软件状态正常。串口实际收发、HDMI 和其他 IO 仍待实测。 另提供[可选 USB 调试组合](../knowledge/usb/cm5-io-board-usb-debug.md)：ACM 串口、固定地址 NCM 网络和独立开关的 ADB，已完成项目载荷的板上并发验证；镜像默认关闭。

Arch Base 已完成镜像构建、CM5 eMMC 刷写与完整镜像范围读回校验，并验证了原生启动、根分区扩展和 USB 串口/网络/SSH。另已在原生 Arch 上跑通 [Omarchy 核心桌面手动会话](../knowledge/desktop/cm5-omarchy-native.md)及 V3D 渲染；软件重启后的 USB 重新枚举仍有待解决。
构建入口和配置见 Arch Base 说明（工程源码：`image/arch-base/README.md`）。
Omarchy 的离线核心包构建（工程源码：`image/packages/omarchy/README.md`）与
手动会话载荷（工程源码：`image/desktops/omarchy/README.md`）单独维护，尚未接入镜像桌面选择。

硬件与平台资料见 [参考资料索引](../reference/README.md)，包含 Raspberry Pi CM、uConsole、
核桃派 CM2 和 Avaota-A1；接口取舍见 [自制核心板最小接口](../knowledge/hardware/uconsole-custom-core-interface.md)。

后续板型计划见 [Roadmap](roadmap.md)：CM3 低配验证与核桃派 CM2／T527 平台适配分别推进。

## 构建

推荐使用交互入口，按提示选择板型、根文件系统来源、系统配置和桌面：

主机需要 Python 3.11+ 和 GNU make。pi-gen 使用 Podman；Ubuntu Base 和 Arch Base 在本机用 sudo、chroot 和原生镜像工具构建。

首次克隆前，通过发行版安装 `git-lfs`，然后执行：

```sh
git lfs install
git clone --recurse-submodules git@github.com:IotaHydrae/uconsole-linux.git
cd uconsole-linux
```

正常克隆会下载当前提交的 LFS 文件；已有副本或跳过自动下载的副本执行
`git lfs pull`。安装、缓存选择和包更新步骤见预编译包说明（工程源码：`prebuilt/README.md`）。

```sh
./build.sh lunch
./build.sh
```

选择保存在忽略的 `.build-config.json`，只影响 `build.sh`。无参数时依次初始化固定版本上游、准备所选后端、构建内核和镜像；`./build.sh kernel` 或 `./build.sh image` 可单独构建，镜像构建需要已有的匹配内核。`./build.sh show` 查看当前选择，`./build.sh check` 运行工程检查。`JOBS=12 ./build.sh kernel` 可调整内核编译并行数。构建命令不刷写设备；pi-gen 镜像容器需要主机已有 `sudo -n podman` 权限；Ubuntu Base 和 Arch Base 使用 `sudo -n unshare` 的私有挂载命名空间。

这些命令调用根 Makefile。也可直接执行：

CM5 IO Board 的最小 Arch 镜像使用 `PROFILE=arch-base`，见
Arch Base 构建说明（工程源码：`image/arch-base/README.md`）。

```sh
make upstreams
make test
make kernel-builder
make kernel-container-build JOBS=8
make image-builder
make image-build
```

Raspberry Pi Desktop 镜像还需要缓存修复后的 ARM64 电池插件包；在 ARM64 上运行
`make image-battery-package` 原生构建，再将包及回执放到构建主机的标准缓存目录。
固定输入、依赖和步骤见 电池插件说明（工程源码：`image/packages/wfplug-batt/README.md`）。
Lite 镜像无需该桌面包。

CM4 另有可选的精简 Mesa 合成器包，可在 ARM64 上用现有原生工具和离线输入执行
`make image-compositor-package MESA_INPUTS=/path/to/mesa-inputs` 构建。
它提供独立桌面会话，应用程序继续使用发行版 Mesa；普通镜像构建不依赖此包。
固定输入、使用范围和验证要求见 可选 Mesa 包（工程源码：`image/packages/mesa-compositor/README.md`）。

Ubuntu Noble 的独立构建入口为 `make image-mesa-noble-package`，使用相同发行版
源码生成供显式选用的桌面进程使用的 V3D/VC4 库，不替换系统 Mesa。
它与上述 Trixie 包分别维护；输入及使用边界见
Noble Mesa 包（工程源码：`image/packages/mesa-noble/README.md`）。

Ubuntu 的 labwc + Waybar 试验配置为 `ubuntu-noble-labwc`。它复用 LFS 中的
固定 Wayland 栈（工程源码：`image/packages/wayland-stack/README.md`）；也可在独立 Ubuntu
24.04 ARM64 主机原生构建。CM4 已运行普通用户会话，实机确认横屏方向、轨迹球和终端快捷键；
默认缩放为 150%。
Noctalia 5 原生包（工程源码：`image/packages/noctalia/README.md`） 已完成 CM4 功能验证，
矩形渲染优化后用户确认菜单明显流畅。可在 `./build.sh lunch` 中选择，或使用配置档
`ubuntu-noble-labwc-noctalia`；镜像构建复用通过回执校验的两个预编译包，不重新编译桌面。
首次使用会将预编译包及回执放入忽略的原生缓存；已有非空缓存优先，并保留严格校验，
缺失、过期或损坏时直接报错。构建不会自动联网下载或编译桌面包，详见
[包复用说明](../knowledge/build/userspace-package-reuse.md)。
Waybar 保持默认；功能、内存对比及冷启动、CM5 等未验证事项见
[桌面对比记录](../knowledge/desktop/labwc-shell-comparison.md)。

默认 Raspberry Pi OS 配置档的目标为 CM5；CM4 使用 `make kernel-container-build BOARD=uconsole-cm4`、`make image-build BOARD=uconsole-cm4`。`BOARD`、`PROFILE`、`DESKTOP` 分别选择板型、系统配置档和桌面。

内核构建使用 rootless Podman；pi-gen 镜像构建需要 `sudo -n podman` 和挂载/loop 权限。Ubuntu Base 根文件系统与镜像导出使用本机工具，不需要镜像构建容器；x86 主机需带 F 标志的 ARM64 binfmt/QEMU。具体依赖见 镜像说明（工程源码：`image/README.md`）。

内核产物位于 `kernel/build/artifacts/`，镜像导出到 `image/deploy/`。两种后端都预制 `pi` 账户并启用 SSH，首次启动无需屏幕创建账户。uConsole 的 pi-gen 配置在忽略的 `image/config.local` 中读取 `FIRST_USER_PASS` 和可选的 `FIRST_USER_NAME`；其 Ubuntu Base 配置在独立的 `image/ubuntu-base/config.local.toml` 中读取必需的 `password` 和可选的 `username`。这些目标未提供密码会在镜像准备阶段报错。

`cm5-io-board` 单独读取 `image/board/cm5-io-board/config.toml`，本地覆盖写入同目录下被忽略的 `config.local.toml`，不会继承 uConsole 的本地账户、网络或历史内核设置。用 `./build.sh lunch` 选择该板型后执行 `./build.sh`，或在准备好上游与内核构建环境后执行：

```sh
make kernel-container-build BOARD=cm5-io-board
make image-build BOARD=cm5-io-board PROFILE=ubuntu-noble-base
```

Ubuntu Base 配置档默认选择 CM4、无桌面。准备好匹配的内核产物并配置本地密码后运行：

```sh
make image-build PROFILE=ubuntu-noble-base BOARD=uconsole-cm4
```

固定输入、可选 Wi-Fi 配置和镜像检查见 Ubuntu Base 说明（工程源码：`image/ubuntu-base/README.md`）。CM5 可选择相同后端，构建与硬件验证状态见开发状态。Ubuntu Desktop、XFCE、KDE 仍待实现；它们与根文件系统来源独立选择，见 [镜像组合设计](image-design.md)。

## 工程入口

- 人类入口：`./build.sh lunch` 交互配置，`make help` 查看构建、检查和测试命令。
- Agent 接手：`make -f Makefile.agent handoff`；当前内核产物：`make -f Makefile.agent kernel-artifact`。
- 接口契约：[docs/agent.md](agent.md)。
- 架构与开发约定：[docs/architecture.md](architecture.md)、[多平台板型设计](board-design.md)、[docs/development.md](development.md)。
- 知识库：[按内容分类的中文索引](../knowledge/README.md)。
- 仓库技能：skills/README.md（工程源码：`skills/README.md`），含已脱敏的接手、构建验证、知识维护和驱动开发流程。
- 构建脚本：入口、后端与目录说明（工程源码：`scripts/README.md`）。
- 通用工具：工具命令与退出码（工程源码：`tools/README.md`）；跨工程的实际复用与脱敏范围见[适配清单](reference-adaptation.md)。
- 适配依据：[CM4 内核移植](../knowledge/kernel/cm4-kernel-port.md)、[CM5 内核移植](../knowledge/kernel/cm5-kernel-port.md)、[CM5 存储与 SD 插槽](../knowledge/storage/cm5-storage.md)。

## 目录

```text
board/<target>/      板型声明、平台后端选择和启动配置
docs/knowledge/       按主题分类的中文工程知识库
image/                发行版配置、板级文件与镜像定制
kernel/config/        各板型的内核配置增量
kernel/patches/       按 series 顺序应用的内核补丁
manifest/             官方上游 URL、分支和固定提交
prebuilt/             按用户态与架构分类的 LFS 包及普通 Git 校验回执
skills/               项目内共享的开发与知识维护技能
scripts/              内核与镜像构建入口、后端及文件导出脚本
tools/                项目检查、诊断与通用工具
tools/agent/          Agent 接手接口
tests/                不依赖实机的自动化测试
upstream/linux/       固定版本的 Raspberry Pi Linux 子模块
upstream/pi-gen/      固定版本的官方 arm64 pi-gen 子模块
```
