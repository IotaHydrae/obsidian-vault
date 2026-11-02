# 工程知识库

> 按问题检索可复用结论；项目当前运行状态见[开发状态](../project/status.md)。

## TL;DR

- 下列导航覆盖全部知识文章；CM4、CM5 和未来板型按同类问题归档。
- 先核对适用硬件、软件版本与证据层次，再使用结论；历史试验不自动代表当前状态。
- 每篇前 20 行给出明确 TL;DR，全文严格少于 300 行；先结论，再方法、原因与证据。
- 更新时原位合并，保留关键数据、证据引用与未知项；运行 `make knowledge-check` 检查结构。

## 分类导航

### 构建与验证（`build/`）

- [Arch Linux ARM Base：接入项目内核与镜像导出](build/arch-arm-base-image.md)
- [APT 缓存复用与软件源信任](build/apt-cache-and-trust.md)
- [ARM64 rootfs：原生、qemu-user 与完整虚拟机](build/arm64-execution-paths.md)
- [镜像压缩与产物发布](build/image-compression-and-publication.md)
- [构建输入身份与阶段复用](build/input-identity-and-cache.md)
- [rootfs 构建隔离与文件元数据](build/rootfs-isolation.md)
- [rootfs 阶段复用与失败恢复](build/rootfs-stages-and-failure-recovery.md)
- [构建工具入口、失败语义与知识审计](build/tool-contracts.md)
- [用户态预编译包复用与板级适配边界](build/userspace-package-reuse.md)
- [内核候选部署：保护 usrmerge 的 /lib 链接](build/usrmerge-kernel-staging.md)
- [镜像验证分层与判据](build/verification-layers.md)

### 启动与启动优化（`boot/`）

- [启动 IO 争抢的采样与归因](boot/boot-io-attribution.md)
- [CM4 启动时间：固件与桌面启动的实测优化](boot/cm4-boot-time.md)
- [CM4：缩短 labwc 启动前的等待](boot/cm4-early-desktop.md)
- [CM4 启动固件与 EEPROM 更新](boot/cm4-firmware-update.md)
- [CM4：定位早期 initcall 停顿](boot/cm4-initcall-profile.md)
- [CM4：uConsole 黑屏与无网络的排查边界](boot/cm4-uconsole-boot-debug.md)
- [启动计时：时钟域、起点与可用终点](boot/measurement-clock-domains.md)

### 显示（`display/`）

- [CM4 提前显示：在冷插拔枚举结束前预加载现有模块](display/cm4-early-display.md)
- [CM5：CWU50 模块的受控对照](display/cm5-panel-module-comparison.md)

### 电源管理（`power/`）

- [CM4 系统睡眠与电源键唤醒链](power/cm4-system-sleep.md)
- [Noctalia 桌面的低电量与熄屏策略](power/desktop-power-policy.md)

### 内核与设备树（`kernel/`）

- [T527：内核驱动源码、预编译模块与用户态库的区别](kernel/t527-driver-source-availability.md)
- [T527：从 5.15 BSP 向 Linux 6.18 LTS 迁移](kernel/t527-linux-6-18-migration.md)
- [CM4 适配 Raspberry Pi Linux 6.18](kernel/cm4-kernel-port.md)
- [CM5 适配 Raspberry Pi Linux 6.18](kernel/cm5-kernel-port.md)
- [设备树从源码到运行状态的验证链](kernel/device-tree-artifact-lifecycle.md)

### 根文件系统（`rootfs/`）

- [rootfs 复制：所有权、链接与 file capability](rootfs/file-metadata-and-capabilities.md)
- [登出后 IPC 对象消失：logind 与进程生命周期](rootfs/logind-ipc-lifetimes.md)
- [精简镜像时如何保留运行和维护能力](rootfs/minimal-image-trimming.md)
- [服务就绪、网络等待与本地启动的分工](rootfs/service-readiness-and-network.md)
- [Ubuntu Base 接入与内核兼容性](rootfs/ubuntu-base-integration.md)

### 存储与介质身份（`storage/`）

- [克隆镜像的 UUID 与混合介质启动](storage/cloned-media-identities.md)
- [CM4 底板：SD 启动验证方法](storage/cm4-carrier-sd-test.md)
- [CM5 eMMC 与 uConsole microSD 插槽](storage/cm5-storage.md)

### 硬件基线与性能（`hardware/`）

- [Avaota BSP：总线与引脚的受控调试](hardware/avaota-bsp-bus-debug.md)
- [Avaota BSP：时钟、系统与采集外设的调试判据](hardware/avaota-bsp-system-debug.md)
- [Avaota-A1：为 T527 核心板开发选择参考设计](hardware/avaota-a1-t527-reference.md)
- [核桃派 CM2／T527：资料范围与 uConsole 接入边界](hardware/walnutpi-cm2-platform.md)
- [CM4／CM5：BTB 引脚、机械尺寸与底板布局](hardware/raspberry-pi-cm-btb-and-mechanics.md)
- [uConsole：自制核心板的必选与可选接口](hardware/uconsole-custom-core-interface.md)
- [CM4 超频的受控试验与恢复](hardware/cm4-overclock-testing.md)
- [CM5 IO Board 超频与恢复边界](hardware/cm5-io-board-overclock-testing.md)
- [CM5 IO Board 第三方风扇低速控制](hardware/cm5-io-board-fan-control.md)
- [微雪底板上的 CM4：原系统基线](hardware/cm4-waveshare-baseline.md)

### 桌面集成（`desktop/`）

- [终端产品：模拟器预装、游戏内容导入与商用分发](desktop/uconsole-emulator-distribution.md)
- [uConsole：模拟器选型与内置按键／外接手柄配置](desktop/uconsole-game-emulators.md)
- [CM5：不接屏幕验证 labwc 桌面](desktop/cm5-headless-desktop.md)
- [CM5：在独立 Arch ARM 用户空间试跑 Omarchy](desktop/cm5-omarchy-arch-trial.md)
- [CM5：在原生 Arch Base 上验证 Omarchy 核心桌面](desktop/cm5-omarchy-native.md)
- [CM4：Raspberry Pi Desktop 电量与横屏集成](desktop/cm4-postflash-desktop.md)
- [CM4 Celluloid：硬件解码与显示丢帧](desktop/cm4-video-playback.md)
- [Ubuntu 精简桌面的浏览器与影音应用](desktop/desktop-applications.md)
- [Ubuntu CM4 的 labwc 外壳对比边界](desktop/labwc-shell-comparison.md)
- [Noctalia 菜单掉帧的矩形渲染优化](desktop/noctalia-rendering.md)
- [私有 Mesa 的启动收益与进程隔离](desktop/private-mesa-startup.md)
- [精简桌面的回收站与卷挂载](desktop/removable-storage.md)
- [labwc ScreenCast：验证真实 PipeWire 画面](desktop/screencast-validation.md)
- [labwc 截图门户的运行时依赖](desktop/screenshot-portal.md)

### 网络（`network/`）

- [精简桌面的蓝牙用户态依赖](network/bluetooth-userspace.md)
- [CM4：启动时 brcmf_set_channel 返回 -52](network/cm4-brcmfmac-channel-survey.md)
- [CM4：移入 uConsole 前的 Wi-Fi 访问准备](network/cm4-uconsole-wifi.md)
- [Ubuntu：保留 Netplan 功能，缩短 NetworkManager 生成等待](network/ubuntu-netplan-generate.md)
- [Ubuntu Base：能连接 Wi-Fi，但域名无法解析](network/ubuntu-networkmanager-dns.md)

### USB 与接口定位（`usb/`）

- [CM5 IO Board：USB 串口、固定地址网络与可选 ADB](usb/cm5-io-board-usb-debug.md)
- [USB 设备、控制器与物理接口的映射](usb/device-mapping-method.md)

## 阅读与维护约定

写作方法见知识写作技能（工程源码：`skills/developer-knowledge/SKILL.md`），批量复核见
知识库复核技能（工程源码：`skills/knowledge-base-review/SKILL.md`）。跨工程取舍见
[参考工程适配清单](../project/reference-adaptation.md)。

- 一篇回答一个主要问题。章节只保留需要的内容，结论 → 方法 → 原因 → 证据不要求空套模板。
- 中文解释保留必要的命令、源码标识、版本、哈希与证据路径，确保可检索、可复核。
- 分开标注源码事实、设备观察、推断和未知项。没有重测的历史结果保留原条件与日期。
- 超过 150 行先压缩重复；达到 300 行必须按问题拆分。原始日志另存为证据，不以删证据凑篇幅。
- 同一知识保留一个主条目，关联主题链接过去；新增、改名和拆分同步全部引用与本索引。
- 外部材料去除私网、身份、凭据和无关硬件；本库已有证据路径只保留复核所需部分。
- 审计仅验证篇幅、TL;DR、标题、索引与本地链接，事实、锚点和敏感信息需要人工核对。
