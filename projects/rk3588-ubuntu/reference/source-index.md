# notes — 笔记与排查记录

本目录存放**原理笔记、问题排查记录、功能使用指南、硬件清单与验证结论**等文档，
与源码/脚本分离。`README.md`（仓库根那个）只讲"如何编译、如何烧录"，其余说明性内容都在这里。

> 给 AI 编码代理的项目约定与已核实事实单独放在仓库根的 `AGENTS.md`（镜像源码：`AGENTS.md`），
> 动手改代码前先读它。**新增文档时按下面的分类放进对应目录，并更新本索引。**

> ⛔ **写作规范**：新增或改写本目录的文档前，先读
> `skills/developer-knowledge/SKILL.md`（镜像源码：`skills/developer-knowledge/SKILL.md`） ——
> 首屏必须给出"一句话结论 + TL;DR"，**一篇只回答一个问题**，超过 300 行要拆，
> 不写时间线/调试流水账，敏感信息要脱敏。本目录在 2026-10-02 按它重做过一轮。

## 先看哪篇（按目的找）

| 我想… | 看这篇 |
| --- | --- |
| **接手这个项目 / 换 agent / 想知道"现在到哪了、还有什么没做"** | [handoff.md](../handoff.md)（相关提交、注明日期的开发板记录、STMMAC 板测与未完成事项、专题及证据入口；区分已有板测和未验证项） |
| 往镜像里加一个新命令 / 服务 / udev 规则 / 配置 | [system/system-integration.md](../system/system-integration.md)（第 0 节的四处同步清单） |
| 加一个**需要编译**的应用（C / Rust / CMake） | [build/apps.md](../build/apps.md)（每个 app 一个目录与 Makefile，Rust 发布 deb / C 构建产物分开维护）+ [build/apps-examples.md](../build/apps-examples.md)（实例细节）+ [build/apps-toolchain-baseline.md](../build/apps-toolchain-baseline.md)（构建位置与工具链基线） |
| **改了 `overlay/` 里的脚本或配置，想在跑着的板子上立刻试**（不想重编、重刷镜像） | [build/overlay-apply.md](../build/overlay-apply.md)（`tools/apply-overlay-to-board.sh`：和 `sync-to-board.sh` 的分工、默认不碰哪些东西、权限为什么"跟目标走"） |
| 改构建流程、搞清阶段之间怎么复用 | [build/build-internals.md](../build/build-internals.md) |
| 在 Linux 下替代 Windows RKDevTool，通过 USB / MaskROM 烧录 eMMC | [build/rkdeveloptool.md](../build/rkdeveloptool.md)（工具区别、Loader、完整裸镜像写入与读回校验） |
| 想**升级内核版本** / 评估换 6.6·6.12 BSP 还是 mainline | [build/kernel-upgrade-feasibility.md](../build/kernel-upgrade-feasibility.md)（实测厂商 delta 888 个上游文件 / 185,035 行（**6.1.118 口径、未重测**）、为什么"自己打 patch"不划算、**当前版本地图**：我们 6.1.172（2026-09-30 从 6.1.118 升上来；上游 LTS 已 **6.1.188**）、Rockchip `develop-6.1`=6.1.172 / `develop-6.6`=6.6.89 / `develop-6.12`=6.12.69（2026-09-16 读分支 HEAD、**未复测**）、上游 stable **7.2.8** / mainline **7.3-rc5**（2026-10-03 实读 kernel.org）、**§5 大版本升级的性能影响面概览**、回归清单） |
| 接入自研底板 / 使用 `rk3588-lsp-evb` | [build/derived-board-bringup.md](../build/derived-board-bringup.md)（预留构建入口、5io 引导包复用与待核对硬件边界）+ [build/derived-board-usb-hub.md](../build/derived-board-usb-hub.md)（USB2 hub 选型） |
| 知道镜像里装了哪些包、为什么装 | [build/deb-packages.md](../build/deb-packages.md) |
| 镜像包集与体积裁剪 | [build/image-size-trimming.md](../build/image-size-trimming.md)（GNOME 桌面、独立开发包与厂商 SDK 裁剪；验证与空间测量口径） |
| 构建挂了（源、依赖、打包） | [build/apt-sources-deb822.md](../build/apt-sources-deb822.md)、[hardware/atf-missing-troubleshooting.md](../hardware/atf-missing-troubleshooting.md) |
| 板子起不来 / 搞清启动链路 | [hardware/rk3588-boot-flow.md](../hardware/rk3588-boot-flow.md) |
| 想搞清 **BL31 到底在干什么 / PSCI・SIP 提供哪些接口 / reboot・关机是谁做的** | [hardware/bl31-mechanism.md](../hardware/bl31-mechanism.md)（EL3 常驻固件：ELF 三段与 FIT 的对应、SIP `0x82000001~0x10` 全表与谁在用、深睡/PMU-SRAM、复位与关机的反汇编、低 2 MiB 内存地图、串口 banner 逐行、验证/升级/10 条坑） |
| 看一次**正常启动**的完整串口日志（对照自己的板子哪里不一样 / 哪些"报错"其实正常） | [hardware/boot-serial-log.md](../hardware/boot-serial-log.md)（上电 → `Starting kernel ...` 原文存档 + 逐段注解 + 可核对的 sha256） |
| 读芯片 ID / 搞清 Secure Boot 的 OTP 字段 / OTP 到底怎么读（**只读，别写**） | [hardware/otp-secure-boot.md](../hardware/otp-secure-boot.md)（NS/S 两个控制器、字段地图、出厂烧 vs 后烧、SPL/BL31 与 Linux 的读取路径、为什么写不可逆、"BootROM 能不能反汇编"、**附录含关键代码**） |
| 想知道 `initrd.img` 里有什么、谁生成、谁搬到启动分区 | [system/initramfs.md](../system/initramfs.md)（生成链条、184 个模块与 10 条固件的来源、我们的 4 个 hook、重建与排障） |
| 想知道板子 CPU 到底多快 / 为什么服务器 qemu 模拟慢 | [hardware/cpu-coremark.md](../hardware/cpu-coremark.md)、[build/qemu-user-vs-system.md](../build/qemu-user-vs-system.md) |
| 搞清 **GPU（Mali-G610）** 用的是哪个驱动 / OpenGL・Vulkan 支持到哪 / 怎么看 GPU 占用 | [hardware/gpu.md](../hardware/gpu.md)（panthor 是内建驱动且是 mainline backport、DT 两个 GPU 节点、Vulkan 走 PanVK、Mesa 来自官方 noble-updates） |
| 某个 **PCIe / SATA / Mini-PCIe / M.2 槽位能不能用**、想切复用、或插卡不识别 | [hardware/combphy-mux.md](../hardware/combphy-mux.md)（三路 combo PHY 各归谁、v2 与 5io 不同）+ [combphy-mux-switching.md](../hardware/combphy-mux-switching.md)（怎么切：overlay 写法、5io→v2 完整实例、选脚、热插拔、⛔ v2 切 sata0 会掉 WiFi） |
| 搞清电源键（PWR/PWRON）按下后谁会收到什么 / 想改长按关机行为 | [hardware/rk806-pwron.md](../hardware/rk806-pwron.md) |
| 接电量计 / 读电池电量（SOC、电压、电流、健康度） | [hardware/battery-gauge-bq40z50.md](../hardware/battery-gauge-bq40z50.md)（i2c7@0x0b + 主线 `sbs-battery`、**没有 DT 节点时怎么用 `new_device` 临时绑定**、原始寄存器逐项对照、**要不要用 TI 参考驱动**） |
| 想知道三个 RGA 核有什么区别、谁快 / 用 RGA 做 2D 加速 | [hardware/rga.md](../hardware/rga.md)（能力对照 / RGA2 的 <4G 约束 / im2d 用法 / 基准 / 选核机制）+ [hardware/rga-dma32-buffer-demo.md](../hardware/rga-dma32-buffer-demo.md)（给 RGA2 分配 <4G 缓冲的完整可编译示例）+ [hardware/rga-gstreamer-camera-pipeline.md](../hardware/rga-gstreamer-camera-pipeline.md)（相机→MP4 链路里谁在用 RGA、隐患在哪） |
| 确认某个 USB 口接的是哪个设备 | [usb/device-mapping-method.md](../usb/device-mapping-method.md)（方法）+ [usb/topology-lubancat-5-v2.md](../usb/topology-lubancat-5-v2.md) / [usb/topology-lubancat-5io.md](../usb/topology-lubancat-5io.md)（两块板的对照表） |
| 找**厂商原件**（原理图 / Datasheet / TRM 的 PDF） | ../hardware-docs/README.md（镜像源码：`hardware-docs/README.md`）（第三方资料原件，索引与命名约定在那儿；结论仍在 `notes/`） |
| USB 主机 / Quest 连接与供电 | [usb/usb-gadget.md](../usb/usb-gadget.md)、[usb/usb-gadget-power-and-adb.md](../usb/usb-gadget-power-and-adb.md)、[usb/quest3-usb-power.md](../usb/quest3-usb-power.md) |
| **Quest 到底在不在充电**（`AC powered: true` 会骗人 / 电量卡住不动） | [usb/quest3-usb-power.md](../usb/quest3-usb-power.md) 的「电池电流的观测方法」（真实瞬时电流看 **healthd 的 logcat `c=`**，不用 root；"醒着净 0、睡下 1.94 A"的实测） |
| **Quest 每秒级断连 / LCD 报"左右手柄低电、头显未连接"** | [usb/typec-detach-debounce.md](../usb/typec-detach-debounce.md)（**现象/根因**）+ [usb/typec-fault-inject-and-verify.md](../usb/typec-fault-inject-and-verify.md)（**复现/验证**：故障注入、判决实验、用户态 mask）+ [usb/typec-rollout-and-decision.md](../usb/typec-rollout-and-decision.md)（**落地/决策**：59 换内核两条路线、"放弃 device 能力"的决定与 20 轮验证） |
| 配数据 SD 卡 / 看卡的性能 | [storage/init-sdcard.md](../storage/init-sdcard.md)、[storage/sd-card-performance.md](../storage/sd-card-performance.md) |
| **量 NVMe 硬盘的读写 / 看 M.2 槽给了几条 lane** | [storage/nvme-performance.md](../storage/nvme-performance.md)（5io=2 lane / v2=4 lane 对带宽的影响、LnkCap 与 LnkSta 怎么读、**为什么不能测裸设备**） |
| 做一张「带镜像和工具」的启动/烧录 SD 卡 | [storage/boot-flash-card.md](../storage/boot-flash-card.md) |
| 改桌面 / 系统策略（熄屏、自动挂载、永不休眠…） | system/tuning/（镜像源码：`notes/system/tuning`）、[system/removable-media-policy.md](../system/removable-media-policy.md) |
| **桌面会话起不来 / 没声音 / 开机自启项与后台常驻服务审计** | [system/desktop-session.md](../system/desktop-session.md)（会话总线根因 + GDM 自动登录）、[system/audio-pipewire.md](../system/audio-pipewire.md)（判"没声音"三步、音量 >100% 的真相）、[system/desktop-autostart-audit.md](../system/desktop-autostart-audit.md)（自启项与常驻服务审计） |
| **GNOME 新 4K 屏幕首次登录默认 200% 缩放** | [system/gnome-display-defaults.md](../system/gnome-display-defaults.md)（单屏登录默认、保留用户配置、标准 monitors.xml 保存与边界） |
| **开机慢在哪 / 日志时间戳不对（时钟从哪来）** | [system/tuning/boot-network-gating.md](../system/tuning/boot-network-gating.md)（别被"等网络"门控）、[system/tuning/time-sync.md](../system/tuning/time-sync.md)（无 RTC 电池时的 2021 → 上次关机 → 网络同步三段） |
| **做 ATM 式 kiosk 会话**（全屏网页、不能 Alt+F4、不能切出去）/ 想锁死桌面逃逸路径 | [system/kiosk/session-design.md](../system/kiosk/session-design.md)（怎么搭）+ [system/kiosk/escape-hardening.md](../system/kiosk/escape-hardening.md)（逃逸面实测与加固）+ [system/kiosk/keyfilter.md](../system/kiosk/keyfilter.md)（禁 Ctrl+W） |
| **想让 Chromium 以外的软件也硬解**（播放器 / 浏览器 / GStreamer 管线） | [system/vaapi-rockchip.md](../system/vaapi-rockchip.md)（`rockchip-vaapi` 补的是**通用 VA-API 通路**；Chromium 与 ffmpeg 各自有自己的路，用不到它；含 `vainfo` / mpv / Firefox 的判据与两个静默回退软解的坑） |
| 跑镜像体检、查某个检查项的含义 | tools/verify-image.sh（镜像源码：`tools/verify-image.sh`）、[system/app-and-device-config.md](../system/app-and-device-config.md) |
| **连板子慢 / 想给 ssh 提速**（单条 0.5 s 花在哪、连接复用怎么开） | [system/ssh-latency.md](../system/ssh-latency.md)（逐段实测：TCP 13–20 ms、KEX+认证 ~0.35 s、PAM 1.4 ms；`ControlMaster` 后 44 ms） |
| **ssh 挂死 / 传大文件奇慢**（WiFi 省电、同 AP 站间转发、板子 STA 该用哪个接口） | [system/wifi-link-performance.md](../system/wifi-link-performance.md) |
| 笔记本网线直连板子、固定 IP 访问 / 向板子共享上网 | [system/ethernet-direct.md](../system/ethernet-direct.md)（产品默认 RK3588 提供 DHCP，仅本地访问）、[system/laptop-ethernet-sharing.md](../system/laptop-ethernet-sharing.md)（开发板显式切客户端，由笔记本共享上网） |
| **开机要多久 / 想砍启动时间** | [system/boot-time.md](../system/boot-time.md)（当前三次热重启的启动事件、时钟口径与限制；不代表冷上电或 HDMI 物理首帧）+ [system/boot-time-optimizations.md](../system/boot-time-optimizations.md)（已评估改动及其测量边界）+ [system/boot-io-profiling.md](../system/boot-io-profiling.md)（I/O 观测与归因限制）+ [system/chromium-cold-start.md](../system/chromium-cold-start.md)（极简启动引导页、直连恢复对照与预热评估） |
| **上位机（HMI / oem-host）怎么进镜像** | [system/oem-host-integration.md](../system/oem-host-integration.md)（当前运行契约：kiosk 账户、socket、系统 Python、共享状态与权限）+ [system/oem-host-upstream-sync.md](../system/oem-host-upstream-sync.md)（GitLab 最新源码获取、独立安装与预编译阶段） |
| **把现场脚本发给内网其他机器（一条 `curl` 就能跑）/ 自己搭一台分发机** | [system/intranet-script-http.md](../system/intranet-script-http.md)（**标准做法 = 发行版 nginx 的 docroot**，不写自定义服务：装 `nginx-light` → `tools/lsp-fix-http.sh publish` → `http://<分发机IP>/<脚本名>`；含现场完整命令、更新/核对/回退流程、已配置实例与踩过的坑） |
| **镜像里 `/`、`/etc` 属主怎么变成 board 的 / tmpfiles 报 rc=73** | 构建脚本（镜像源码：`scripts/build-lsp-image.sh`） 的厂商包权限归一与存量修复工具（镜像源码：`tools/fix-vendor-ownership.sh`）（根因是厂商 deb 携带非 root 属主和可写目录属性；工具提供 `--dry-run` 预览） |
| 用 VSCode / clangd 索引内核或 U-Boot 源码（跳转、补全、诊断） | [build/clangd-indexing.md](../build/clangd-indexing.md)（`compile_commands.json` 怎么来、`.clangd` 为什么要去掉三个参数、什么时候重新生成、cscope/gtags/bear 的取舍） |

## 分类

| 目录 | 放什么 | 篇数 |
| --- | --- | --- |
| `build/`（镜像源码：`notes/build`） | **构建工程**：镜像怎么编出来、装了哪些包、apt 源怎么配、构建快慢、**体积与裁剪**、**`apps/` 怎么进镜像**、**overlay 怎么装到跑着的板子**、**内核/U-Boot 的 clangd 索引**、**内核升级可行性评估**、**上游同步评估**、**自研底板派生清单** | 28 |
| `system/`（镜像源码：`notes/system`） | **系统定制与工具**：我们往镜像里加了什么、怎么配、面向运维怎么用（`kiosk/` 与 `tuning/` 两个子目录按主题细分） | 63 |
| `hardware/`（镜像源码：`notes/hardware`） | **硬件与启动链路**：板级资源清单、**Combo PHY 复用（PCIe/SATA/USB3）**、CPU 算力实测、**GPU（Mali-G610/panthor）**、RGA 三核实测、电源键/PMIC 上电与关机、BootROM→U-Boot→内核、**BL32/OP-TEE 怎么开**、**BL31（EL3）机制与 SIP 接口全表**、**正常启动串口日志存档**、**OTP/eFuse 与 Secure Boot 字段**、**电量计/battery（bq40z50 SBS）**、启动故障排查 | 50 |
| `usb/`（镜像源码：`notes/usb`） | **USB**：Type-C 固定主机、USB 拓扑与设备定位、Quest ADB 授权与外设供电、**Type-C 掉连（瞬态被当成拔线）** | 16 |
| `storage/`（镜像源码：`notes/storage`） | **存储**：SD/eMMC/**NVMe** 盘的性能、初始化、启动/烧录卡、卷标/UUID 冲突、**采集数据掉电落盘** | 9 |

### build/ —— 构建工程

| 文档 | 内容 |
| --- | --- |
| [build-internals.md](../build/build-internals.md) | 构建内部机制：阶段复用、apt 缓存、中间 tar 与标记、镜像压缩、唯一构建入口、板卡发现、heredoc 自检与 chroot 会话边界 |
| [build-guide.md](../build/build-guide.md) | **构建详解**（§1~§9）：从哪拿源码、`sync-to-board.sh` 的默认同步行为与"为什么不能一条 rsync 推全仓库"、`build.sh`/主脚本全部开关与分阶段续跑、失败重跑与 apt 缓存复用、镜像压缩（**速度表是 4.88 GiB 那版的口径**）、预置 WiFi、桌面自动登录、ATF 覆盖（`image` 阶段）、品牌化素材、体检 |
| [build-lunch.md](../build/build-lunch.md) | `./build.sh lunch` 交互构建：板卡、桌面、任务、eMMC / SD 选择，配置预览与桌面切换时的缓存处理 |
| [overlay-apply.md](../build/overlay-apply.md) | **把 `overlay/` 装到跑着的板子上**（`tools/apply-overlay-to-board.sh`）：为什么 `sync-to-board.sh` 之后 overlay 还是旧的（它只同步工作树，overlay 只有编镜像才落到 `/`）、默认不碰哪些路径及原因、**权限为什么"跟目标走"**（镜像里 `lsp-set-rule` 是 0755、仓库里是 0664，按仓库推就错）、`--apply` 的密码怎么给、**这个工具不做的事**（daemon-reload / update-initramfs / dtbo 合并）、本机回归 44 项与板子端到端实测、4 个坑 |
| [flashing.md](../build/flashing.md) | **烧录与维护**：SD / eMMC / NVMe 写入与启动限制、增量写入已烧录设备（`update-emmc.sh`；**p1/p2 现在都是 ext4，同一套 `rsync -aHAX` 语义，旧 vfat 目标会被拒绝**）、挂载 + chroot 改卡、SD 专用镜像（避免与 eMMC 撞卷标；标签按 `blkid` 自动选 `e2label`/`fatlabel`） |
| [rkdeveloptool.md](../build/rkdeveloptool.md) | Linux 下完成 Windows RKDevTool 常用 USB 烧录任务：与 `upgrade_tool` 的区别、MaskROM / MiniLoader、两种 CLI 的长度单位、完整 `.img` 写 LBA0 与读回验证；注明尚未板测。 |
| [icecc-distributed-build.md](../build/icecc-distributed-build.md) | 内核 / U-Boot 的 icecc 接入：auto 核数阈值、工具链 / masquerade 校验、有界槽位探测与本机回退；检查配置与实际派发的区别，2026-09-30 的派发实验及增量 A/B 测量范围。rootfs 的 apt / dpkg 不走 icecc。 |
| [apt-cache.md](../build/apt-cache.md) | **apt 下载缓存的导出/导入/备份**：`build/apt-cache` 的结构与读写时机、`tools/apt-cache.sh` 全部子命令与选项、**备份到编译服务器的完整流程**（板子 → 本机 → 服务器 → 新机器 `import`）、实测数字（**2026-10-03 复查：1703 个 deb / 807.6 MB**；09-15 首测 1610 / 750.7 MB ↔ tar.zst ≈原大小、导出 2~10 s、导入后逐字节一致）、10 条坑（zstd 压不动 deb、合并语义、要 sudo、`--keep-foreign`…）、回归测试 `tests/apt-cache-tool.sh`（21 项） |
| [build-speed-comparison.md](../build/build-speed-comparison.md) | **构建历史测量**：2026-09-12 的原生 / QEMU `rootfs` 阶段端到端记录约 7.4–7.7×；区分应用编译、deb 安装、缓存、网络 / I/O 与重新测量方法，不作当前镜像固定倍率承诺 |
| [qemu-user-vs-system.md](../build/qemu-user-vs-system.md) | 同机 ARM64 qemu-user / system TCG 对照：单核与八核预算、99 个离线 deb（94 新装、5 升级）；八核安装 system 慢 41.2%，两者均支持多核。输入、验收与适用范围见正文。 |
| [deb-packages.md](../build/deb-packages.md) | 构建会装哪些 deb、各自干什么：**`packages/` 的预置包**（`arm64/` 逐包表含 `libv4l-rkmpp`/`rockchip-vaapi`、**`Chromium-rkmpp/` = 我们预置的 Chromium 151**、`cage-lsp/`、Rust `apps/`、`u-boot-*`）、内核/U-Boot deb（**实测 1995 个 `.ko`、7 基础 dtb + 264 overlay dtbo**）、apt 源安装的基础/ROS2/桌面包；snap/LibreOffice/浏览器用 apt pin 排除、`--no-install-recommends` 体积控制；**§1.1 那个厂商 `ffmpeg` 的上游是哪个 fork**（`nyanmisaka/ffmpeg-rockchip` @ `76cd3b5103ca`，FFmpeg 8.x；野火是**静态**打包所以别的程序拿不到 `rkmpp`；与 `jjm2473/ffmpeg-rk` 的对比与"为什么不能换"） |
| [image-size-trimming.md](../build/image-size-trimming.md) | GNOME 桌面、独立开发包及 FFmpeg / RKAIQ SDK 裁剪；本轮 rootfs `/usr` 实占减少约 281.77 MiB，最终镜像逻辑长度减少 281 MiB。已完成离线体检、SD 冷启动与 MPP 硬件编解码验证；启动服务异常与验收边界见正文。RGA / MPP 开发包及内核 headers 保留。 |
| [apt-sources-deb822.md](../build/apt-sources-deb822.md) | APT 源改用 deb822 格式：24.04 底包只带 `ubuntu.sources`、旧格式会导致重复源、以及「底包无 TLS 证书所以只能用 http」的取舍；ROS 2 源（**当前默认华为云**，回退清华→上游）与内置签名密钥（实测有效期到 2030-06-01）、**镜像站"同步中"的换源兜底**、**镜像源测速工具 `tools/bench-mirrors.sh`（apt/ROS2/pip，http+https、小+大文件）与已测结果** |
| [kernel-module-trimming.md](../build/kernel-module-trimming.md) | **内核模块裁剪（已立单项，未动手）**：起因＝『内核把很多用不到的驱动编成模块、本机编半小时』。**先修掉的是更要紧的那条** —— `build-kernel.sh` 每次 `rm -rf` + `cp -r` 导致改一行也全量重编，改成 `rsync -a` 保 mtime 后服务器实测：无改动重跑 `-k` 41 s、改一个 `.c` 32 s、单模块 0.95 s（改前 3.7 分钟全量）。裁剪现状量化：`.config` 里 `=m` 2002 / `=y` 2360，产物 1995 个 `.ko`、未压缩 108.6 MB；按目录 top 是 netfilter(163)/hwmon(145)/net-wireless(111)/media-usb(92)/media-dvb-frontends(88)/input-touchscreen(84)… 文档里给了**候选类别表**（含必须保留的：UVC 那套、usbhid、remotectl、我们实际用的那颗 WiFi）、**待确认项**（两块板实际用哪些驱动、逐类耗时归因）、**验收判据**（两块板各跑 verify-image + 现场功能回归）与**做法**（写成独立 `config/kernel-trim.config` 片段、在 defconfig 之后合并，不直接改 BSP defconfig；先只裁 `hwmon` 一类跑通机制）。 |
| [distributed-compilation.md](../build/distributed-compilation.md) | **编译分发（局域网多机编译）—— 已立项、暂不做**：列了 distcc / **icecream（中央 scheduler + worker，适合「一台强机 + 若干普通机」）**/ ccache / sccache-dist / mold·lld 的取舍；并给出**实测数字**说明现在不值得做：增量 `-k` 32 秒里真正的编译不到 1 秒（大头是 vmlinux 重链 + `bindeb-pkg` 打包），全量 3.7 分钟；而真正慢的 **`rootfs` 阶段 是 apt/dpkg + qemu 转译、根本不是编译** ⇒ 那条路的正解是「rootfs 在 RK3588 原生编、内核/u-boot 在 x86_64 服务器交叉编」。触发条件＝反复做全量构建 / 大型 C++ 工程；顺序＝先用 ccache → 再量占比 → 最后 icecream。 |
| [apps.md](../build/apps.md) | 应用准备与安装契约：Rust 固定 deb / C 编译的混合阶段，v2 状态校验、apt 与 install-only 安装、跨机器同步、显式源码构建工具链和性能边界。 |
| [rust-app-debs.md](../build/rust-app-debs.md) | Rust app 的 arm64 deb 发布：固定 manifest、源码与包身份校验、noble 板上显式重打包、LFS 与版本追溯；MCAP 默认文件关联及失败边界。 |
| [apps-examples.md](../build/apps-examples.md) | **三个 app 的实例细节**：`lsp-mcap-view`（终端 MCAP 预览器：Schema 全文件合并、短类型名解析、`.mcap` 默认打开程序）、`lsp-rk-monitor`（与 bash 版同参数/同 JSON 键名、百分比量程固定 0~100%、网格 rect 不裁会 panic、`sudo -S` 起 TUI 按键进不去）、`lsp-usb-map`（通用性、16 ms、回归 PC 5/0 与工程机 6/0） |
| [apps-toolchain-baseline.md](../build/apps-toolchain-baseline.md) | app 构建工具链基线与位置选择；optbench 的 x86 交叉 / ARM64 原生 A/B 数据及适用范围，动态库、ISA 与目标机运行验证边界。 |
| [kernel-upgrade-feasibility.md](../build/kernel-upgrade-feasibility.md) | **内核升级可行性**：现状（**6.1.172** = 上游 v6.1.172 + 厂商 delta，2026-09-30 从 6.1.118 升上来、提交 `e4e9c64cb`；树里没有上游 git 历史所以不能 merge）、厂商改动实测（**6.1.118 口径、未重测**：888 个上游文件被改 / +148,841−36,194 行 / 新增 12,398 文件，其中 154 个核心目录文件；含复现脚本与"别用 diff -rq 直接统计"的坑）、三条路线（跟 6.1.y stable / Rockchip `develop-6.6`·`develop-6.12` / mainline 能力盘点：NPU 6.18 起、RK3588 硬解仅 master、无编码/RGA3/ISP/CIF/HDMI-IN）、为什么自己 rebase 不划算（含 `init/main.c` 那个 `#ifdef CONFIG_ARCH_ROCKCHIP` 案例与主线 `print_kernel_cmdline()` 的正规实现）、替换内核的动作与回归清单、**§5 大版本升级的性能影响面**（调度 EEVDF/sched_ext/PREEMPT_RT、内存 folio/mTHP 及其与 RGA2 "<4G" 的交互、块层/FS 的 iomap 化、网络/图形/电源/加固开销，以及"用现成尺子量"的清单） |
| [clangd-indexing.md](../build/clangd-indexing.md) | **用 clangd 索引内核 / U-Boot**：`tools/gen-compile-commands.sh kernel\|u-boot\|all` 的用法与实测（内核 10758 条：8726 改指 `kernel-6.1/`、1997 个 `.mod.c` 留 build 树；U-Boot 636 条走树外构建目录）、**`kernel-6.1/.clangd` 为什么必须去掉 `-mabi=*` 等三项**（否则 preamble 建不出来）、DB 重建时机（**DB 不进 git，新 clone 里没有，要自己生成**）、VSCode 配置与排障对照表、**替代方案对比（cscope / gtags / bear / ccls / cpptools；⚠️ "可用性"那一列是 2026-09-16 某容器里的实测，换机器要重查）** |
| [upstream-sync.md](../build/upstream-sync.md) | 野火上游评估与采纳边界：已评估到 `88226c2ef`（2026-10-23），本轮新增 17 个提交；RTL8822CE 两补丁已纳入源码并编译，未部署。含比对方法、BL31/BL32、U-Boot 本地补丁及两板回归清单。 |
| [upstream-sync-commits.md](../build/upstream-sync-commits.md) | 上游逐提交取舍：本轮 17 项与前序记录；区分已纳入源码、已有实现覆盖和未实施候选，保留 Kconfig 丢失符号及 ext4 启动分区的判断依据。 |
| [kernel-changelog.md](../build/kernel-changelog.md) | 内核改动账：BSP 升级、panel/remotectl、CAN、FUSB302、以太网模块与 BTRFS 裁剪、VOP2 gamma、STMMAC 与 RTL8822CE TXBD 调试修复；实现、依据、板测与验证边界 |
| [armbian-kernel-delta.md](../build/armbian-kernel-delta.md) | 野火 6.1.172 相对精确 Armbian 基线的完整 Git 清单比对；CMA 边界、RS485 中断忙等、MDIO 私有写入、缺失的恢复修复与 MIPI 条件问题，区分本工程补丁和未验证项。 |
| [uboot-changelog.md](../build/uboot-changelog.md) | **U-Boot（`u-boot/` + 自建 deb + `prebuilt/`）：我们自己的改动账**：4 个提交 —— 自建 deb 打包与 ATF BL31 v1.53/BL32 开关、去掉 NVMe 启动目标（−816 ms）、删 Android 回退链、重建 prebuilt 与修 dpkg ≥ 1.23；含 prebuilt 哈希现状与"成对更新"纪律 |
| [derived-board-bringup.md](../build/derived-board-bringup.md) | `rk3588-lsp-evb` 预留入口：暂无实物，独立 DTS、配置与 overlay 清单复用 5io 核心板启动包；说明后续底板核对边界 |
| [derived-board-usb-hub.md](../build/derived-board-usb-hub.md) | ⏳ **自研底板 `USB20_HOST1` 那路 USB2 hub 的选型评估**：⛔ 这一路放不了 USB3 hub（没有 SS 通路）；**首选 CH334F**（MTT、与 FE1.1S 引脚兼容、带 `OVCUR#`/`PWREN#`/`RESET#`）vs **FE1.1S**（STT + 0~70 ℃ 商业级，降为缺货二供）；候选对比表、布线/配置 8 条要点（`RESET#/CDP` 别常态拉高、`OVCUR#` 不用要上拉、别串信号电阻）、**MTT vs STT 的实测掉速**、级联第二颗 CH334F 的方案 |

### system/ —— 系统定制与工具

串口与日志策略见 [journal-serial.md](../system/journal-serial.md)：完整 journal 持久保存，独立服务输出告警、串口堵塞隔离与登录时 `pam_lastlog.so` 加载错误修复。

| 文档 | 内容 |
| --- | --- |
| [system-integration.md](../system/system-integration.md) | **系统集成总账**：我们往镜像里加的命令 / 服务 / udev 规则 / 配置分别放在哪、怎么装、谁触发、谁验证；对厂商东西打的补丁；因 `--no-install-recommends` 必须显式加的包。**新增定制项前先看这篇的第 0 节** |
| [lvgl-drm-direct-boot.md](../system/lvgl-drm-direct-boot.md) | **实验：把 kiosk 链换成 LVGL 直上 DRM 能快多少**：四版对比（各自 unit 排序与单元起点）、板上原生编译步骤、串口抓包 + `first_frame` 判据、与 kiosk 链的同日基线对比，以及"LVGL 测试别开 plymouth / Chrome 链才需要"这条搭配结论与回退办法 |
| [upstream-diff-and-untouched.md](../system/upstream-diff-and-untouched.md) | **与上游的责任划分**：哪些是上游 EmbedFire 带的、我们**没动**（出问题先分清责任），以及**相对上游的完整改动清单**（可复现，别靠记忆）。系统集成总账（命令/服务/udev/配置/启动分区/厂商补丁/必须显式装的包）在 [system-integration.md](../system/system-integration.md) §0~§8 |
| system/tuning/（镜像源码：`notes/system/tuning`） | 系统级定制（**一篇一个主题**）：关闭自动更新、时间同步（chrony + fake-hwclock）、PAM 失败延迟、串口 getty、`profile.d` bash 守卫、swapfile 首启创建、USB 自动挂起、DNS（systemd-resolved）、开机不被「等网络」门控、服务要不要等时钟、永不休眠 |
| [auto-update-off.md](../system/tuning/auto-update-off.md) | **关闭自动更新检测（构建期开关）**：默认已关：`DISABLE_AUTO_UPDATE=Y` + `PURGE_SNAP=Y`；`=N` 分别恢复 Ubuntu 默认 / 保留 snapd。 |
| [console-getty.md](../system/tuning/console-getty.md) | **串口 / 控制台**：板载调试串口：UART2 = `ttyFIQ0`（fiq-debugger），**1500000**，earlycon `0xfeb50000`； |
| [dns-systemd-resolved.md](../system/tuning/dns-systemd-resolved.md) | **DNS：systemd-resolved（换网络自动跟随）**：三处：`BASE_PACKAGES` 显式加 `systemd-resolved`（它只是 `systemd` 的 **Recommends**， |
| [no-idle-suspend.md](../system/tuning/no-idle-suspend.md) | **设备策略：接电源、用电池都不自动休眠**：① GNOME `gsd-power`：`sleep-inactive-{ac,battery}-timeout=0` **且** `-type='nothing'` |
| [pam-fail-delay.md](../system/tuning/pam-fail-delay.md) | **登录失败延迟（PAM）：已去掉**：两处来源：`/etc/pam.d/login` 的 `pam_faildelay.so delay=3000000`（串口/本地控制台）与 |
| [profile-d-ros-guard.md](../system/tuning/profile-d-ros-guard.md) | **`/etc/profile.d/` 里注入 ROS 2：必须带 bash 守卫**：守卫写成 `[ -n "${BASH_VERSION:-}" ]`，并且必须放在 `if` 的**条件**里 —— 放 `then` 里时 |
| [services-wait-for-clock.md](../system/tuning/services-wait-for-clock.md) | **服务要不要"等时钟就绪"再启动**：`network.target` = 网络栈就绪（与有没有上联无关）；`network-online.target` = "能上网"； |
| [swap.md](../system/tuning/swap.md) | **swap：镜像里不预置 swapfile，改首启创建 + `vm.swappiness=10`**：镜像里不建；首启由 `lsp-swapfile.service` 跑 `fallocate` → `mkswap` → `swapon -p`，写入几乎为零。 |
| [usb-autosuspend-off.md](../system/tuning/usb-autosuspend-off.md) | **USB 自动挂起：已整机关闭**：三层缺一不可：**内核启动参数** `usbcore.autosuspend=-1`（PM core 级：负延迟让设备 `usage_count+1`， |
| [dds-shm-orphans.md](../system/dds-shm-orphans.md) | **停服务后 `/dev/shm` 留下的 Fast DDS 孤儿段**：原因：Fast DDS 的 POSIX shm 段是 `/dev/shm` 里的**文件**，正常退出会 `shm_unlink`； |
| [logind-removeipc-hotfix.md](../system/logind-removeipc-hotfix.md) | **老镜像一键补 `RemoveIPC=no`（现场运维用）**：脚本**自包含**（不依赖仓库其它文件）：scp 到目标机、`sudo bash` 跑即可。 |
| [lsp-set-rule.md](../system/lsp-set-rule.md) | **lsp-set-rule：运行期绑定现场设备（相机 / LCD / 手套）**：一个型号一行、互不干扰：不同型号 VID/PID 不同 ⇒ 规则不会同时命中同一台设备。 |
| [lsp-config.md](../system/lsp-config.md) | 板级配置工具 `lsp-config`：为什么"逻辑全在子命令、whiptail 只是外壳"、菜单结构、子命令速查、**WiFi 只连 example-project 的约定**、安全边界、实测记录、加新条目要动哪里 |
| [ethernet-direct.md](../system/ethernet-direct.md) | 产品默认双板载网口任选一口提供 `<HOST_13>` 与 DHCP；固定角色、单实例限制及恢复服务端步骤 |
| [laptop-ethernet-sharing.md](../system/laptop-ethernet-sharing.md) | 开发板显式切为 DHCP 客户端接入笔记本 shared 网络；笔记本提供 DHCP / DNS / NAT，客户端失败持续重试 |
| [removable-media-policy.md](../system/removable-media-policy.md) | **可移动介质（U 盘 / 读卡器 / SD・TF 卡）不自动挂载**（`automount=false`，dconf + gschema 两层），**手动挂载保持可用**；另记录试过并撤掉的更严格两层（udev `UDISKS_IGNORE`、polkit 拒绝）与实测证据、SD 卡不能用 `removable` 判、fstab 长期挂载写法 |
| [branding-customization.md](../system/branding-customization.md) | 品牌化定制（素材位与生效机制）：三处独立 logo 位、Plymouth 开机画面（默认主题 bgrt 但素材取自 spinner）、壁纸/锁屏、ESC 日志、**没生效时的排查顺序** |
| [plymouth-boot-splash.md](../system/plymouth-boot-splash.md) | **开机画面（Plymouth）单独一篇**：⚠️ 引擎前提（素材在≠`plymouthd` 在，有踩坑记录）、`bgrt` 主题名但资源来自 `spinner` 目录、各文件作用与规格、**多分辨率按「最小屏」定尺寸**、drop-in 替换方式、校验命令与位置微调 |
| [branding-assets-tool.md](../system/branding-assets-tool.md) | `tools/make-assets.sh`：把 PNG/JPEG/PDF/SVG 按各部位规格转换并放进 overlay（含"SVG 渐变会变黑"的坑） |
| [hostname-and-motd.md](../system/hostname-and-motd.md) | 主机名（`LSP_HOSTNAME`）与登录 motd / 横幅：速查表、`/opt/banner.txt` 的生成方式、`overlay/opt/logo.txt` 仍保留但不再显示 |
| [app-and-device-config.md](../system/app-and-device-config.md) | 设备配置 `device.yaml`、上位机依赖 `APP_PACKAGES` / `APP_PIP_PACKAGES`、Quest USB 访问规则、`pure-python-adb` 模块名坑，以及 `tools/verify-image.sh` 检查项明细 |
| [intranet-script-http.md](../system/intranet-script-http.md) | **内网脚本分发（可照抄到别的机器）**：把现场脚本（`fix-logind-no-removeipc.sh` / `verify.sh` / `repro.sh`）用**发行版 nginx 的 docroot（`/var/www/html`，端口 80）**发出去，现场一条 `curl -o /tmp/x && sudo bash /tmp/x`；含一次性配置、`publish` 幂等更新、7 条现场完整命令、`dpkg -V`/`nginx -T`/`ufw` 核对清单、回退，以及"脚本必须 bash"、"⛔ 别 `curl … | sudo bash`（sudo 会把脚本当密码吃掉）"这些坑；已配置实例：服务器 `<HOST_05>` |
| [chromium-rkmpp.md](../system/chromium-rkmpp.md) | **Chromium RKMPP 硬件加速浏览器**（上游 `e80577f5b`）：三个部件（chromium 151 / `libv4l-rkmpp` 插件 / 内核 MPP）、**硬解开关在 `/etc/chromium.d/default-flags`，必须走 `/usr/bin/chromium` 包装脚本**、工程机实测（H.264 与 VP9 1080p30 都 `mpp initializing` + 30fps，渲染进程 4.2% vs 软解 47.7%）、`chromium-rkmpp-nodes.service` 会截断 `/dev/video-dec0` 的描述、上游 `libv4l-0t64 1.32.0` 会顶掉 `v4l-utils`（**不采纳**），以及集成待办 |
| [chromium-first-run-defaults.md](../system/chromium-first-run-defaults.md) | **Chromium 首启默认项**：kiosk 第一次启动会弹的两个窗口、书签栏三个默认收藏，分别怎么在镜像里关掉/去掉的（kiosk 场景下弹窗会盖住现场页面） |
| [browser-choice-hardware-decode.md](../system/browser-choice-hardware-decode.md) | **换个浏览器还能硬解吗**：Firefox 156 **实测不能硬解**、启动还比 Chromium 慢约 1.4 s；"换个更轻的 Chromium 但保留硬解"**可行但要自己编**（现成轻量版都不带 H.264），收益太低暂不做 |
| [vaapi-rockchip.md](../system/vaapi-rockchip.md) | **RK3588 的 VA-API 硬解驱动（`rockchip-vaapi`）**：补的是**通用 VA-API 通路**——Chromium/ffmpeg 各有自己的路用不到它，但装它之前板上**一个 `*_drv_video.so` 都没有**，走 VA-API 的软件全在软解（mpv 连别的后路都没有）。含 `vainfo` / Firefox A/B（rkvdec 0%→10%、CPU 140%→64%）/ mpv 实测、**`--vo=null` 必须配 `--hwdec=vaapi-copy`** 这个坑、镜像集成口径（**只装 `.so`，不设环境变量、不关 RDD 沙箱**）、只补解码 / 无 AV1 的边界；**§3.4 修掉一个既有 bug**（`/etc/mpv/mpv.conf` 写的是厂商版 mpv 才认的 `hwdec=rkmpp`，Ubuntu 的 mpv 直接静默软解 ⇒ 改成 `auto-safe`；并实测**换 VLC 会更差**——它的 `--avcodec-hw` 里连 vaapi 都没有）；**§3.5 4K60 掉帧的根因**（不是解码、不是合成器、不是 VA-API —— 用**合成信号不解码也掉 36%** 定死，是 mpv 默认的 `vo=gpu` 渲染路径 ⇒ 改 **`vo=gpu-next`** 后 42% 丢帧降到 1~5%） |
| [rock-5b-ysp-crossref.md](../system/rock-5b-ysp-crossref.md) | **外部参考：`yisding/rock-5b-ysp`**（ROCK 5B 在 Armbian 上的媒体/内核集成记录，161 条 findings）里**对我们有用**的部分 + 我们逐条实测核对的结果：`earlyoom`（板子卡死是 **thrash livelock** 不是 OOM，我们 2026-09-30 刚撞过一次 ⇒ 开发板已装并按他们的实测建议配好、做过安全的触发验证；`systemd-oomd` 我们没装所以不中那条误杀坑）、他们维护的 `rockchip-vaapi` fork（比我们集成的上游多**编码**与**解码前导出稳定化**那个"整屏绿"修复）、**FFmpeg 8.1.2 + Rockchip 已有现成 port**（`ffmpeg-81` 分支 + 28 patch）、**Firefox 沙箱的正规做法**（不是关沙箱，而是 RDD broker + 窄 ioctl 白名单）、**Chromium 也能吃 VA-API**、**Mali-G610 BLIT 精度坑**、**§4.1 通读 gotchas 挑出来的**（那条到处都见的 `mpp_platform: client N driver is not ready!` 是**良性噪声**、`scale_rkrga` 默认保宽高比、librga 的开源来源、DT 别名/configfs overlay 的坑）、**§7 复现他们的 issue #4**（同血缘 BSP-6.1/noble 上：非对齐宽度 VP9 **不复现**（差在导入侧 Mesa 版本），但 **HEVC Main10 我们硬报错** —— 驱动 advertise 了却解不了，三路对照已隔离；另记"别乱升 mpp/librga"）；并记下我们**已经满足**的两条（`CONFIG_DMABUF_DEBUG` 没开、dma_heap 权限已对）|
| [rock-5b-issue4-repro.md](../system/rock-5b-issue4-repro.md) | **复现上游 issue #4**：非对齐宽度 VP9 绿屏**我们不中** ✅、HEVC Main10 **我们中且更硬** ❌、上游把问题绑在 6.18 内核（我们用 6.1 BSP）⇒ 结论不能照搬；`cma=512M` 试算（能用但无可观测收益） |
| [wpe-webkit.md](../system/wpe-webkit.md) | **WPE WebKit（替代 kiosk 浏览器栈的候选）**：⛔ **纠正**——`Cog` / `libwpe` / `WPEBackend-fdo` 都是 **legacy（pre-2.54）**，新方案是 WebKit **内置的 `WPEPlatform`**（`WPEDisplayDRM` **直接出屏、无合成器**，`WPE_PLATFORM` 选后端；launcher 只有 9 行）；**不用升 2.54**——我们停着的 2.52.6 源码里就已经有它（附 `ENABLE_WPE_PLATFORM` / `..._DRM` / 与 legacy API **互斥** 的确切开关）；含验收要测什么、板上停着的源码状态、以及它跟 Firefox / 轻量 Chromium / QtWebEngine 那几个候选的关系 |
| [initramfs.md](../system/initramfs.md) | **initramfs（`initrd.img`）**：生成链条（内核包 postinst → `update-initramfs` → 我们**6 个**文件 → `post-update.d/zz-update-firmware` 拷到启动分区 → U-Boot `boot.scr`）、里面有什么（框架/模块/固件/我们加的/没有的）、**`MODULES=dep` 裁剪的前后对照**、⚠️ **"配置改了不等于产物改了"**（板子上现役 initrd 仍是 `most` 时代那 183 个模块，因为 conf 落盘晚于 initrd ⇒ 必须重跑 `update-initramfs`）、**启动关键路径全是内建所以进不进模块都行**、重建/查看命令、`rtl_bt` 空转与 `COMPRESS=gzip` 两条观察 |
| [initramfs-rebuild-and-inspect.md](../system/initramfs-rebuild-and-inspect.md) | **initramfs 重建与查看**：改了 `conf.d/*`（尤其 `MODULES=`）后 `sudo update-initramfs -u` / `-k all`；`lsinitramfs` 列内容、`unmkinitramfs` 解开；两条观察（`rtl_bt` 空转、`COMPRESS=gzip`）与 2026-09-26 板级实测记录 |
| [dtb-update.md](../system/dtb-update.md) | **DTB 合并与内核升级失败保护**：先暂存校验再发布，写入失败保留旧 DTB，升级失败回滚启动文件，自定义 overlay 保留与回归判据 |
| [system/desktop-session.md](../system/desktop-session.md) | 维护桌面：D-Bus / RTKit 依赖、GDM Wayland 默认与实际会话检查、labwc 可行性和 HDMI 互锁边界 |
| [audio-pipewire.md](../system/audio-pipewire.md) | 音频栈：判"没声音"的三步走、`pulseaudio` 为什么 inactive、**音量 >100% 是数字增益且会削顶**（100% 已是硬件电气上限） |
| [audio-volume-ucm.md](../system/audio-volume-ucm.md) | **音量问题**：拉满还是小声的根因（**UCM 每次使能都把模拟音量写回 27**、没把音量元素交给 ACP）与我们的 UCM 修法；⛔ "aplay 没声音"的真凶是**默认输出跑到 HDMI**；⛔ 音量 **>100% 是数字增益且会削顶** |
| [desktop-autostart-audit.md](../system/desktop-autostart-audit.md) | 桌面自启条目（`Hidden=true` 屏蔽法与注意事项）与常驻服务审计清单 |
| system/kiosk/（镜像源码：`notes/system/kiosk`） | **kiosk 会话（ATM 式全屏网页）**（已落地，一篇一个问题）：会话设计与落地、逃逸面实测与加固、按键过滤（evdev 层 + 合成器层）、维护模式、sudo 白名单、启动时间线、合成器选型与取定制、开机 SIGHUP、板级集成踩坑 |
| [cage-sighup.md](../system/kiosk/cage-sighup.md) | **开机时 cage 会被 SIGHUP 拉掉几次**：判据：`journalctl -b -t lsp-kiosk-exit` 里出现 `result=success code=killed status=HUP` |
| [compositor-candidates.md](../system/kiosk/compositor-candidates.md) | **合成器选型：cage / weston / sway 实测与其他候选**：三家实测"restart → 出画面"（kmsgrab 数帧，30 fps）：**weston 0.67 s < cage 0.94 s < sway 1.27 s**。 |
| [compositor-customization.md](../system/kiosk/compositor-customization.md) | **cage fork：把按键过滤 / 开机 NumLock / 合成器底色做进合成器**：为什么便宜：cage 0.1.5 源码 3048 行、Build-Depends 只有 wlroots 等五个包，板上原生 |
| [integration-pitfalls.md](../system/kiosk/integration-pitfalls.md) | **板级集成踩坑：NumLock 补按的位置、没重编的二进制、`/dev/shm` 不可写**：⛔ **别把 `lsp-keyfilter --tap-numlock` 放回 `ExecStartPost`/`ExecStartPre`**：开机早期会被 SIGHUP |
| [maintenance-mode.md](../system/kiosk/maintenance-mode.md) | **维护模式：kiosk ↔ board 桌面（`lsp-maintenance`）**：脚本 `overlay/usr/local/sbin/lsp-maintenance`，子命令 `on|off|status|passwd`。 |
| [session-deployment.md](../system/kiosk/session-deployment.md) | **kiosk 落地：坑清单与步骤**：⛔ **`--kiosk` 不是安全边界**：能锁住的是"会话外观"，不是"用户权限"。真正的边界是**谁物理接触这台机器**。 |
| [startup-timeline.md](../system/kiosk/startup-timeline.md) | **kiosk 启动事件与显示测量口径**：当前并行启动顺序，以及 HTTP 200、FCP 与 HDMI 物理首帧的区别 |
| [sudo-policy.md](../system/kiosk/sudo-policy.md) | **kiosk 权限：不进 sudo 组，只给白名单**：构建脚本 `useradd -m -s /bin/bash kiosk`（**不带 `-G sudo`**）+ 幂等 `gpasswd -d kiosk sudo`。 |
| [collection-dds-shm-fault.md](../system/collection-dds-shm-fault.md) | **采集栈"六路全断 + 语音报错 + 不自愈"的头号原因：logind `RemoveIPC` 删掉了 Fast DDS 的共享内存对象**（2026-09-26 现场定位 + 板上 A/B 定稿；早先"Fast DDS 段被 unlink"的推断已撤回）。现象：ssh 进去看日志、退出来，或 `systemctl stop user-1000.slice`，几秒~十几秒后 LCD 报三相机+双手套+Quest 全断 + `阶段=故障运行/ERROR_RUN(6)` + 语音"出现错误"，而设备端没坏（相机照样打 fps=30）。根因：logind 默认 `RemoveIPC=yes` 在用户被"释放"时删掉该 UID 名下**所有 POSIX shm/sem + SysV IPC**⇒ `/dev/shm/fastrtps_*` 消失、发布端照发而读端全聋（原镜像 `linger=no` ⇒ **每次 ssh 登出都触发**）。修法：镜像 `overlay/etc/systemd/logind.conf.d/50-lsp-no-remove-ipc.conf`（`RemoveIPC=no`）+ `verify-image.sh` 断言（A/B：拆 slice 前 55 个 board 对象、`sysv_seg=1` → 1 s 后只剩 2 个 root 文件、`sysv_seg=0`，~13 s 后 12 路话题归零；改成 no → 55→55、两轮触发 LCD 错误计数 0）。辅助保留 `/var/lib/systemd/linger/board`（用户决定常驻）。复现与监控：`tests/dds-shm-fault-repro.sh` + `tests/dds-link-monitor.py`。另记两条实测边界：**logind 对 SysV 段有 `nattch>0` 保护、对 POSIX shm 没有**（所以 ROS 进程活着也照样被删）；**进程 fd 里 `(deleted)` 的 shm 段是 liblttng-ust 的正常现象、不能当故障判据** |
| [collection-peripheral-error-escalation.md](../system/collection-peripheral-error-escalation.md) | **另一类"六路全断"：单路外设偶发断连被程序放大**（2026-09-26 静态代码排查，只读、未改厂商代码）。叠加点：①下发给面板的 0x80 错误快照是 **OR 锁存**、只有"显式关机（进入 OVER）"才清零（`lcd_voice_bridge_node.py:488-506` / `:300`）⇒ 任何一路偶发一次 >3 s 静默就**永久**挂在屏幕上；②错误位只能靠"新鲜健康消息"清除（`error_monitor.apply()`）；③触发节点**任一路超时 3 s 就整机 ERROR**（`_check_health`，无单路降级）；④`command='error'` 会让**三路相机一起停录制**（`_STOP_COMMANDS`）；⑤ERROR 态长按 = **关机** → `tmux.kill_all()` 杀光四个外设脚本 ⇒ 一路故障经"长按恢复"放大成六路全断；⑥启动校验 60 s 窗口到点会把"未就绪"的**批量置位**（`phase_fsm.check_startup`）；⑦没有节点级看门狗。硬件侧：六路外设**全是 USB**，一路线缆的电气异常可把同一 hub/控制器带下去 —— 判据是 `journalctl -k` 里的`over-current change`/`reset ... USB device`/`error -71`。含现场区分口诀与可逆验证（`SIGSTOP` 某一路发布节点 → 只报这一路、且恢复后仍挂着） |

### hardware/ —— 硬件与启动链路

串口命令与后台记录见 [serial-exec.md](../hardware/serial-exec.md)：独占连接、命令退出码、主机时间戳和超时恢复。

| 文档 | 内容 |
| --- | --- |
| [lubancat-5-v2-hardware.md](../hardware/lubancat-5-v2-hardware.md) | LubanCat-5 V2 硬件资源清单（基于内核 DTS）：SoC、电源、存储、网络、USB、显示、音频、摄像头、GPIO、调试串口等，及 V1/V2 差异 |
| [u-boot-boot-scr-uenv.md](../hardware/u-boot-boot-scr-uenv.md) | **U-Boot 启动机制**：`boot.scr`（编译自 `boot.cmd`）→ 读 `ubuntuEnv.txt` 里的 `bootargs`/`overlays=`/`fdtfile` → 载 `vmlinuz`/`initrd.img` 并**在 U-Boot 里合并 dtb overlay**；⛔ U-Boot 环境**不持久化**（`CONFIG_ENV_IS_NOWHERE=y`），`fw_setenv` 改不动它 |
| [lubancat-5-v2-vs-5io-dts.md](../hardware/lubancat-5-v2-vs-5io-dts.md) | **v2 与 5io 的 DTS 差异速查**（逐节点 diff）：只有一边覆盖的节点（功能差异）、同名节点但配置不同（板级差异）、派生自研底板时要记住什么；⚠️ 读 DTS 一律用仓库 `kernel-6.1/`（别读 `build/linux-rockchip/` 的旧副本） |
| [combphy-mux.md](../hardware/combphy-mux.md) | **RK3588 三路 Combo PHY 复用（PCIe / SATA / USB3）**：`combphy0_ps`/`combphy1_ps`/`combphy2_psu` 各能接哪些控制器、**v2 与 5io 的接线不一样**（同一"Mini-PCIe"槽在两块板上挂不同 PHY）、专用 `pcie30phy` 的 5 种 `phymode`；复用机制（模式由消费者 `phys = <&combphyX PHY_TYPE_*>` 决定、冲突时**后者静默覆盖**、`validate` 才报错）；**开发板实测**（Mini-PCIe=fe190000 链路 up 且 WiFi 卡就在上面、E-Key/M-Key 空槽 Link Fail）、两条易误判日志与一条旧记录更正；排查命令清单；对自研底板的资源账。"怎么切"已拆到 [combphy-mux-switching.md](../hardware/combphy-mux-switching.md)，`usbhost3_0` 挂 hub 那条已拆到 [combphy-mux-usbhost3-hub.md](../hardware/combphy-mux-usbhost3-hub.md) |
| [combphy-mux-switching.md](../hardware/combphy-mux-switching.md) | **Combo PHY 复用怎么切**（怎么动手那一半）：现成的两个"PCIe 槽切 SATA"overlay 范例与三条注意（重启才生效、必须只留一个消费者、⛔ **v2 上切 sata0 会掉 WiFi**）、**以 5io 为基线改回 v2 归属的完整实例**、`reset-gpios`/`disable-gpios` 怎么选（不能随便挑空闲 GPIO）、PCIe 热插拔为什么不做、⛔ 别拿"另一块板的 DTB"模拟自研底板 |
| [combphy-mux-usbhost3-hub.md](../hardware/combphy-mux-usbhost3-hub.md) | ⛔ **为什么不能在 `usbhost3_0`（`combphy2_psu`）上挂 4 口 USB hub**：那条只有 SS 通路、没有 USB2 半，而现场外设几乎全是 USB2 ⇒ 4 个口跑不起来；最终采用的"分家"方案见 [usb/carrier-board-vl817.md](../usb/carrier-board-vl817.md) |
| [rk3588-boot-flow.md](../hardware/rk3588-boot-flow.md) | **RK3588 完整启动流程**：BootROM → idbloader（DDR 初始化 + SPL）→ ATF → U-Boot → 内核。含 **存储布局**（镜像头部 16 MB bootloader 保留区、GPT 分区表、**§2.3 单独一个启动分区不是 Ubuntu 的规定**）、各级启动阶段详解（BootROM 能/不能做什么、U-Boot proper 到内核、固件加载路径）、**§3.7 全链路总表（谁跑在哪个 EL、是否常驻、各干什么）**。已拆出三篇配套：`u-boot.itb` 内部结构 → [bootloader-blobs.md](../hardware/bootloader-blobs.md)、文件映射/镜像组装/验证速查 → [rk3588-boot-flow-files.md](../hardware/rk3588-boot-flow-files.md)、关机与重启路径 → [shutdown-reboot-path.md](../hardware/shutdown-reboot-path.md)、校验与防篡改 → [boot-integrity.md](../hardware/boot-integrity.md) |
| [rk3588-boot-flow-files.md](../hardware/rk3588-boot-flow-files.md) | 启动链路的**文件映射与组装速查**（`rk3588-boot-flow.md` 的配套篇）：`/boot/firmware` 里每个文件**谁生成的**（内核 deb 的 postinst、`boot.scr`/`boot.cmd`、`ubuntuEnv.txt`、dtbs/dtbo，板上实测）、关键文件对照表、本仓库镜像的 **六个具名阶段组装**、验证命令速查、历史实测记录 |
| [ddr-firmware-reverse-engineering.md](../hardware/ddr-firmware-reverse-engineering.md) | **A 核那份 DDR 初始化固件逆向**（rkbin，AArch64）：自报串 / 布局 / 入口（PMU SRAM `0xff001000` + 芯片 ID 校验）/ 寄存器页直方图 / 尾部符号参数目录（`*_fsp_g` 等）/ 能力字符串 |
| [mcu-firmware-reverse-engineering.md](../hardware/mcu-firmware-reverse-engineering.md) | **`atf-3`（M0 固件）逆向**：向量表与镜像结构、命令分发器（`0xfd58a21c` 高 8 位 = `0xF1`/`0xF2`）、IRQ2 信箱与状态码 `0xf30000xx`、寄存器清单（DDR 之外还动 CRU/电源域/VOP QoS）、函数图（56 入口）与伪 C |
| [bootloader-blobs.md](../hardware/bootloader-blobs.md) | **两个裸启动产物（一对）是干什么用的**：`idbloader.img`（RKNS 头 + DDR 初始化固件 + SPL）与 `u-boot.itb`（external-data FIT：`atf-1/2/3`+`uboot`+`fdt`）各自装什么、谁读它、落在哪个偏移、怎么成对更新/核对/刷写（逆向细节见 `ddr-firmware-reverse-engineering.md`） |
| [shutdown-reboot-path.md](../hardware/shutdown-reboot-path.md) | **关机 / 重启走哪条路**：`reboot: Power down` → PSCI `SYSTEM_OFF` → **BL31 写 IOC/GPIO0**，重启走 `SYSTEM_RESET`；这条路上内核侧**没有 PMIC 代码参与**（与电源键的硬长按断电是两套机制，见 [rk806-pwron.md](../hardware/rk806-pwron.md)）|
| [boot-integrity.md](../hardware/boot-integrity.md) | 启动校验与防篡改：FIT 里的 sha256 只能发现"被改"（完整性），**不是防篡改**（没有签名）；要真防篡改得一整条链（BootROM→SPL→U-Boot→rootfs，**只补中间一层没用**）；`Verified-boot: 0` 是从 OTP 读出来的、idbloader 那层 BootROM 校验的实测（哈希可重算）；**自己核对的三步**（启动日志↔`dumpimage -l`、**从 FIT 头自推导偏移重算 payload 哈希**、从板上读回逐字节比对）与"别用 `dumpimage -p` 提取"的坑 |
| [bl32-optee.md](../hardware/bl32-optee.md) | **BL32（OP-TEE）怎么开、怎么验、什么时候才值得开**：默认关闭的开关（`UBOOT_WITH_BL32=1`）与**一条命令**的用法、开关在 4 个文件里是怎么实现的、**产物验证判据**（FIT 多出 `(optee)` 节点、Load `0x08400000`、`atf-` 仍是 3 个）、**不碰 eMMC 的 SD 卡上板验证法**（含守卫/回读校验/`boot.scr` 处理/回滚）、**实测到的能力矩阵**（SPL 校验 optee / BL31 初始化 BL32 / 内核 `revision 3.13` / `/dev/tee0` / 用户态 ioctl 探针 / CPU 热插拔共存 / `MemTotal` −16 MB）、⛔ **没有 `tee-supplicant` 时 `OPEN_SESSION` 会不可中断阻塞（D 状态）**、"真要**用** TEE 还差 TA+签名密钥"、以及 8 条坑（上游 `gen_bl32_node` 在 arm64 上的 `ARCH` 分支 bug、`_bl32` 缺失静默不生效…） |
| [bl32-optee-enable-and-verify.md](../hardware/bl32-optee-enable-and-verify.md) | **怎么开 BL32（OP-TEE）并上板验证**：一条命令开启 + **验证产物真的开了**（FIT 里出现 `optee` 节点）、开关在 4 处代码里怎么实现、备选做法与厂商老路径、**不碰 eMMC 的 SD 卡上板验证法**与回滚 |
| [bl31-mechanism.md](../hardware/bl31-mechanism.md) | **BL31（TF-A / EL3 常驻固件）机制详解**：出货这颗的版本/哈希/构建时间；**ELF 三个 `PT_LOAD` ↔ FIT `atf-1/2/3`**（主镜像 / PMU-SRAM 代码 / DDR M0）与 `decode_bl31.py` 的拆分、按大小降序编号、逐节尺寸（`stacks` 8×2 KB、`xlat_table` 72 KB）；**低 2 MiB 内存地图**（BL31 `0x40000`、DDR M0 `0xf0000`、ramoops `0x110000`、BL33 `0x200000`）；**EL3 运行时服务**：PSCI v1.1（含 `SYSTEM_RESET2`、`MIGRATE_INFO_TYPE not supported` 的含义）、**Rockchip SIP `0x82000001~0x10` 全表 + `SIP_DRAM_FREQ` 子功能 + 每个号谁在用**、opteed 与 secure 侧时钟/电源域、深睡/PMU-SRAM/唤醒源、HDCP 与 OTP 只读、EHF 与 last log；**`reboot`/`poweroff` 的反汇编实证**（写 CRU `GLB_SRST_FST ← 0xfdb9` vs 写 IOC/GPIO0；BL31 里 `wdt_config` 是死代码）；**串口 banner 逐行读法**；构建侧/运行侧怎么验（含「BL31 版本主要看串口」这条的修正）；**按版本号的 changelog（v1.47→v1.56：发布日期/TF-A 版本/构建时间/段表/sha256/逐版能看见的变化）**；怎么换/升级与版本配套要求；10 条坑 |
| [kernel-initcall-profiling.md](../hardware/kernel-initcall-profiling.md) | **内核启动耗时分析（`initcall_debug` + 内核自带 `bootgraph.pl`）**：怎么开（必须同时加 `ignore_loglevel`，否则被 `quiet` 吞）、串口抓包怎么转成 dmesg 喂工具、实测 Top 榜（GMAC 618 ms / **RAID6 基准 510 ms** / deferred probe 309 ms）、可动杠杆与代价，以及"打印本身让这次启动慢 0.80 s"这条口径坑 |
| [bl31-runtime-and-upgrade.md](../hardware/bl31-runtime-and-upgrade.md) | **BL31 的运行行为、验证与升级**：复位/关机/重启到底是谁做的（反汇编 + 真机实证）、怎么验证产物里嵌的是哪颗 BL31（构建侧 + 运行侧）、记录过的真实 banner、**怎么换/升级**（含 eMMC 升级实录）、与其它组件的关系 |
| [bl31-version-diff.md](../hardware/bl31-version-diff.md) | BL31 版本差异：v1.47→v1.56 的 changelog（口径警告：厂商 commit 不带变更说明）+ v1.47↔v1.53 二进制对比；v1.54 起主体段从 0x40000 挪到 0x60000 |
| [suspend-and-deep-sleep.md](../hardware/suspend-and-deep-sleep.md) | RK3588 deep 的入口、配置和验证判据；V2 `vdd_log_s0` 休眠供电条件 |
| [rk3588-cpu0-immediate-wake.md](../hardware/rk3588-cpu0-immediate-wake.md) | UART2 / CPU0 早醒的隔离方法、关键证据与 GPIO-only 规避限制 |
| [rtl8822ce-wakeup.md](../hardware/rtl8822ce-wakeup.md) | Mini-PCIe WAKE# → GPIO3_D0 接线图、RTL8822CE 深睡唤醒的软件条件与验证缺口 |
| [rtl8822ce-txbd-cost.md](../hardware/rtl8822ce-txbd-cost.md) | RTL8822CE TXBD 调试备份的编译期成本、关闭宏与调用保护；旧模块运行态 A/B 的观测与限制，补丁模块尚未部署。 |
| [boot-serial-log.md](../hardware/boot-serial-log.md) | **一次正常启动的完整串口日志存档**（上电 → `Starting kernel ...`，238 行原文）：逐段注解 DDR 训练 / SPL / BL31 / U-Boot / PCIe / `boot.scr` / 交接内核；**哪些"报错"其实正常**（`FIT: No boot partition`、`No resource partition`、`No OPTEE`、`PCIe Link Fail`…）；三段内存 bank 的由来与内核侧核对；可用于核对的 sha256 与尺寸；⚠️ **板上这份 U-Boot 与仓库 `prebuilt/u-boot.itb` 不是同一次构建**（banner/`uboot` 节点哈希不同，ATF 与 DTB 相同；**2026-10-03 复核：`atf-1/2/3` 与 `fdt` 与 BL32 那份日志一致，只有 `uboot` 变**）。⚠️ 日志全文已单独存成 [boot-serial-log-raw.md](../hardware/boot-serial-log-raw.md) |
| [boot-serial-log-raw.md](../hardware/boot-serial-log-raw.md) | **串口启动日志的原始文本存档**（知识库不收日志全文，所以单独一篇）：正常启动那份 + 带 BL32（OP-TEE）那份，一字未改；`## ` 开头的行是日志内容本身不是标题。分析、逐段注解、可核对的固定数字、怎么自己抓 → [boot-serial-log.md](../hardware/boot-serial-log.md) |
| [otp-secure-boot.md](../hardware/otp-secure-boot.md) | **OTP（eFuse）与 Secure Boot 字段**：NS `otp@fecc0000` / S `secure-otp@fe3a0000` 两个控制器谁都能访问；**字段地图**（NS 的 `cpu-code`/`id`/漏电与 OPP cells，S 的 `0x20` SB 标志、`0x9c0` RSA 公钥哈希、`0x150` 防回滚）；板上只读实测（`/sys/bus/nvmem/devices/rockchip-otp0/nvmem` 全局可读 + 前 32 字节逐字节解释）；**程序里怎么读**（U-Boot `misc_otp_get_device(OTP_S)` → `rk3588-secure-otp.S`；Linux 只能读 NS，secure OTP 无 DT 节点且需 OP-TEE）；⛔ 写不可逆的证据与"不要执行"；**BootROM 为什么不能反汇编**（掩膜 ROM，手里没有二进制）；**§7 附录把引用到的代码原样贴了**（U-Boot 字段宏/读取入口/`Verified-boot` 来源/防回滚/烧写例程/签入的生成汇编、内核寄存器级读法与 `cpu-code`/`id` 解释、DT 节点）|
| [otp-secure-boot-code.md](../hardware/otp-secure-boot-code.md) | **OTP / Secure Boot 的代码附录**（原样摘录，代码存档篇）：U-Boot 的 OTP 字段宏与读取入口、`## Verified-boot: N` 的来源、防回滚计数、⛔ **烧 Secure Boot 字段的例程（只看不执行）**、安全 OTP 驱动为什么是"签入的生成汇编"、内核读非安全 OTP 的寄存器级代码、`cpu-code`/`id` 的解释、DT 写法 |
| [atf-missing-troubleshooting.md](../hardware/atf-missing-troubleshooting.md) | 排查记录：「自建 U-Boot 缺 ATF 导致卡在 SPL→U-Boot」的完整过程、根因、修复与验证方法；`image` 阶段检测/覆盖逻辑；`build/` 源码副本与 dpkg ≥1.23 打包坑；`tools/check-atf.sh` 断点表 |
| [atf-fit-and-image-validation.md](../hardware/atf-fit-and-image-validation.md) | **ATF 相关工具与原理**（那次修复后沉淀下来的）：FIT 结构解析脚本、**`image` 阶段的 ATF 检测与覆盖完整逻辑**、`build/` 源码副本与打包规则相关的坑、`tools/check-atf.sh` 的用法与判据，外加**方法论沉淀**（本次排查的通用套路） |
| [rk3588-hw-monitor.md](../hardware/rk3588-hw-monitor.md) | **各硬件模块占用率怎么看**（工具 **`lsp-rk-monitor`（app，`apps/lsp-rk-monitor/`，镜像里自带）** + 兜底脚本 `tools/lsp-rk-monitor.sh`，两者同参数、同 JSON 键名）：CPU 每核 / GPU(Mali) / NPU / RGA / VPU(MPP) / DDR / 温度的**接口地图**（哪个 sysfs、单位、要不要 root）、TUI 选外设（默认 CPU 每核一张小图）、`load_interval` 与 `lsp-mpp-load.service`、**NPU 的 devfreq load 是假的（恒 100%）**、VPU 的 load vs utilization、写脚本时踩的 bash 坑（set -u 空数组、关联数组下标、TUI 闪烁）与写 app 时踩的坑（**sudo `use_pty` 让 TUI 收不到键盘**、X 轴自动缩放、ASCII 模式为什么关网格） |
| [cpu-coremark.md](../hardware/cpu-coremark.md) | **CPU 与进程启动实测**：大小核拓扑、CoreMark 单核 / 多核及原生 / QEMU 分数口径；动态程序启动为 QEMU 9.7 ms、板子 1.2 ms，作为构建瓶颈调查线索，不推定端到端耗时分布 |
| [rk806-pwron.md](../hardware/rk806-pwron.md) | **RK806 的 PWRON（电源键）全链路**：PWRON 与 PWRCTRL1 是两个别混的引脚（前者纯 PMIC、SoC 看不见）；上电判定 500 ms（驱动改的）、运行中用户态只收到 `KEY_POWER` 的 down/up（**没有自动重复，时长得自己算**）、硬件长按约 8 s（可设 6/8/10/12 s）直接断电且 **Linux 无任何通知**；`eventN` 编号漂移与 `root:input` 权限坑、软关机 vs 硬断电的日志判据、两块板的差异与自查命令。寄存器级手段（`0x76`/`DEV_OFF`/实测快照）→ [rk806-pmic-registers.md](../hardware/rk806-pmic-registers.md)；落地成 app → [lsp-powerkey-implementation.md](../hardware/lsp-powerkey-implementation.md) |
| [rk806-pmic-registers.md](../hardware/rk806-pmic-registers.md) | **RK806 寄存器级操作**：硬件长按断电时长**只**由 `PWRON_LP_OFF_TIME`（`0x76` bit5:4，4 档）决定、`0x76` 写入**跨软关机保留**、⛔ **bit7 绝对不能动**（会毁掉按键开机）、`DEV_OFF`（`0x72` bit0）= 用户态"立即断电"入口、`/sys/rk806single/debug` 的 volatile 读取、缓存与总线错误边界、板上寄存器快照与"能动的地方"清单 |
| [lsp-powerkey-implementation.md](../hardware/lsp-powerkey-implementation.md) | **`lsp-powerkey` 的落地实现**：按住 2 秒正常关机 / 10 秒强制断电的状态机（**零内核改动**，用户态 C 程序）、为什么必须 `-static -s`、板上实测三级行为、⚠️ 2026-10-02 修掉的"force 触发后忙等 100% CPU、连松手都读不到"；回归 `tests/powerkey-hold-timer.sh` |
| [battery-gauge-bq40z50.md](../hardware/battery-gauge-bq40z50.md) | **电量计 bq40z50（SBS）接入与读数验证**：挂 5io 的 **i2c7@0x0b**（与音频 codec `es8388@0x11` 共总线、不冲突）、主线 `sbs-battery` 是 builtin 只缺 DT 节点、**驱动值 vs 原始 SBS 寄存器逐项对照（稳定量完全相等，通过 23/失败 0）**、⛔ **`CAPACITY_MODE`(0x03 bit15) 决定 `0x0F/0x10/0x18` 是 mAh 还是 cWh**（所以 `energy_now` 与 `charge_now` 数值不同但等价，换算电压 10.58 V ≠ 端电压）、⛔ **状态位口径 FC=b5 / FD=b4 / DSG=b6 / INIT=b7**（内核宏与 TI 驱动同口径，别按 SBS 表格那套写成 b3/b4）、`voltage_max_design==min_design` 是驱动口径问题、**rk3x 无 SMBus 块读 ⇒ 块数据无 PEC**；以及 **TI 参考驱动为什么不用**（缺主线属性编不过、只多 RaTable/Qmax 与中断） |
| [battery-gauge-semantics.md](../hardware/battery-gauge-semantics.md) | **bq40z50 的 SBS 语义要点**（容易踩的那些）：那些"名字像但意思不一样"的字段/单位/符号 —— `RemainingCapacity` vs `FullChargeCapacity`、`AverageCurrent` 的符号、`DSG` 位含义（放电**或静置**）、真值在 `CellVoltage1..4`、以及 TRM 里"预计充满耗时"的口径 |
| [battery-gauge-live-monitoring.md](../hardware/battery-gauge-live-monitoring.md) | **bq40z50：TI 参考驱动的取舍 / 怎么复跑 / 实时看数据**：为什么**不用** TI 那份参考驱动（内核自带 SBS 驱动够用）、复跑一次测量的步骤、实时看数据的两条路、5io 上的临时落地（**board-local，不进仓库/镜像**） |
| [rtc-hym8563-battery.md](../hardware/rtc-hym8563-battery.md) | **RTC 电池（hym8563 / CR1220）：受控断电验证与寿命**：BAT54C 供电图、真实断电保持与 35～40 mAh 的条件估算；当前 CLKOUT 开启，未测整板备电电流 |
| [rtc-hym8563-clkout.md](../hardware/rtc-hym8563-clkout.md) | **RTC CLKOUT 常开与解绑边界**：critical → prepare → I²C 写 FE 的流程图、内建 / 模块构建、资源清理；关闭及生命周期修复均未实施 |
| [gpu.md](../hardware/gpu.md) | **GPU（Mali-G610 / panthor）**：现状速查（`CONFIG_DRM_PANTHOR=y` 内建、绑在 `fb000000.gpu-panthor`、`card1`/`renderD129`、驱动 1.5.0 + CSF 固件 v1.5.0、空闲 200 MHz / 37 °C）；DT 里 **两个互斥 GPU 节点**（`&gpu`=kbase / `&gpu_panthor`=panthor）与我们两块板各自使能哪个；**树里三个 Mali 驱动共存**（panthor 内建 / panfrost 模块但无匹配 DT / ARM kbase 也编进内核但未绑定）；panthor 是 **mainline backport，全目录只有一条 `rockchip,rk3588-mali` compatible**、uapi 停在 1.5（上游 2025-10 才 bump 1.6）；固件 `mali_csffw.bin`（arch10.8→arch10.10，zst）；用户态 Mesa 25.2.8 **来自 Ubuntu 官方 `noble-updates` 而非 PPA**（overlay 里那条 panfork pin 不生效）、GL 3.1 core、**Vulkan 可用（PanVK：`driverName=panvk`/`apiVersion=1.4.318`）**；怎么量占用（devfreq load 语义 + lsp-rk-monitor）；已知告警与待核项（`shader_present=0x50005` 与 MP4 的关系） |
| [vop2-inactive-gamma-panic.md](../hardware/vop2-inactive-gamma-panic.md) | HDMI 无信号与整机启动崩溃：Plymouth 在停用 VOP 上设置 gamma 的 SError、缓存与并发保护、真实 DRM ioctl 回归及验证边界 |
| [gpu-compositing.md](../hardware/gpu-compositing.md) | **合成器在不在用 GPU（wlroots 渲染器 A/B）**：`WLR_RENDERER=gles2|pixman` 的切法（瞬态单元 + `PAMName=login` 才有真 DRM master）、硬判别式（maps 里的 `libEGL_mesa`/`libgallium`、`/dev/dri/renderD*`）、实测（GPU 合成 **7% CPU / GPU 99% / 1098 FPS** vs CPU 合成 **90% CPU / GPU 0% / 4 FPS**）与四个坑；配套 `tests/wlroots-renderer-ab.sh` |
| [gpu-vkmark-benchmark.md](../hardware/gpu-vkmark-benchmark.md) | **为什么"别人的 vkmark 6666、我们的 1123"**：三层拆解（评分口径两版一样、**呈现模式与分辨率是数量级变量** 320x240→3908 / 800x600→1315 / 1080p→346 / `-p fifo`→59、**同参数下 GL 6120 vs Vulkan 1334 差 4.6×** 且 Vulkan 下 GPU 99% 满载 ⇒ 差距在 PanVK 不在硬件）；以及"要变成可比"的三步与**记录纪律**（版本+winsys+size+present-mode+Mesa+频率） |
| [rga.md](../hardware/rga.md) | **RK3588 的 RGA（2D 加速器）**（**已按"一篇一个问题"拆成三篇**）：debugfs 证据说明是「两个同一型号的 rga3 + 一个上一代 rga2」；三核能力对照表（输出上限 8128² vs **4096²**、缩放范围、stride 对齐、CSC）；⛔ **RGA2 的硬约束是"物理地址必须 < 4 GB"（不是"必须连续"，2026-09-15 已更正）**，用户态要喂它就开 `/dev/dma_heap/system-dma32`；⛔ **相机（UVC）的 buffer 给不了 RGA2**（`uvcvideo` 用 vmalloc 页，工程机三路实测全 FAIL）；im2d 用法与 `DMA_BUF_IOCTL_SYNC` 常量坑；三核 copy/resize/cvt 实测表（**两 rga3 同级；copy/cvt 上 rga3 快 ~2.1×，resize 反而 rga2 快 ~13%**）；**选核机制与那条"legacy API 查不到 mm_flag"的驱动漏洞**。基准脚本 `tests/rga-bench.sh` |
| [rga-dma32-buffer-demo.md](../hardware/rga-dma32-buffer-demo.md) | **给 RGA2 分配 <4G 缓冲的完整可编译示例**（从 `system-dma32` 取缓冲 → CPU 写 + `DMA_BUF_IOCTL_SYNC` → RGA2 转换/缩放 → **失败自动回退 RGA3** → 读回校验），附 V4L2 `VIDIOC_EXPBUF` 变体 |
| [wlroots-rga-renderer.md](../hardware/wlroots-rga-renderer.md) | wlroots 0.17.1 实验 RGA / Pixman renderer：独立 DMA-BUF 对照、同步提交时间与进程 CPU、正确性及 RGA2 DMA32 限制 |
| [wlroots-rga-replay.md](../hardware/wlroots-rga-replay.md) | 逐帧 cached SHM → staging → DRM 三缓冲重放：计入上传与导入；全帧更慢，uncached 4K 局部约 1.07×，未测 Chromium 呈现 |
| [lvgl-rga-gpu.md](../hardware/lvgl-rga-gpu.md) | LVGL 9.6.0 NEON / RGA 混合 / NanoVG 真 GPU 对照：完整帧耗时、像素差异、DMA32 缓存同步与 GPU 频率控制 |
| [rga-gstreamer-camera-pipeline.md](../hardware/rga-gstreamer-camera-pipeline.md) | **相机 → NV12 → MP4 这条链路用不用 RGA**：MJPEG→NV12 是 MPP 硬解（不经 RGA，镜像里也没有 `rgaconvert`）；真正碰 RGA 的是 **wrists 两路 `mpph264enc` 内部随机做的 `NV16→aligned NV12`**（`core_mask=0x0` = auto）⇒ 那是驱动漏洞唯一的真实触发点；chest 是纯 CPU 浪费（0.35 核/路）；链路不消耗 CMA；码率/容量与"异常终止丢 moov" |

### usb/ —— USB

| 文档 | 内容 |
| --- | --- |
| [usb-gadget.md](../usb/usb-gadget.md) | USB 主机工具 `lsp-usb`：固定 host/source 查询、source 只读确认、外设重新枚举与 I/O 中断边界；device 功能退役及旧配置清理 |
| [usb-device-reference.md](../usb/usb-device-reference.md) | USB device 配置经验：未来专项恢复的角色前提、ConfigFS 单 ACM 示例、功能依赖与厂商 BSP 限制；当前镜像不可直接启用 |
| [usb-gadget-power-and-adb.md](../usb/usb-gadget-power-and-adb.md) | Quest ADB 主机客户端：board/kiosk 共用固定密钥、目录与密钥权限、Quest 侧授权及更换密钥后的会话影响 |
| [camera-uvc-stall.md](../usb/camera-uvc-stall.md) | **相机"节点还在但 ROS 帧率 0、只有拔插才恢复"（UVC 流半死）**（2026-09-26 排查，只读）：坏在**主机侧**（内核 uvcvideo / USB 控制器 / hub / 供电），不是相机 —— 判据是相机节点自己就会每 3 s `_close_pipeline()` 再**重开设备**（`mjpeg_capture.run()`），重开救不回来、只有**USB 重新枚举**（拔插）能救 ⇒ 说明应用层够不着那个半死状态。工程机实测拓扑：三台同型号 `0c45:636b`（**500 mA 总线供电**）分别在 xHCI / EHCI / **与 btusb+2×CDC+HID 共用同一条 EHCI** 上；`uvcvideo` 内建且 `quirks=0xFFFFFFFF`；`lsp-usb-no-autosuspend` 在跑、全部 `control=on`（**排除自动挂起**）。三个可疑成因：①uvcvideo 放弃重投 URB（`Non-zero status (-71/-110)`+`Failed to resubmit video URB`）②EHCI 周期表/带宽被挤 ③供电跌落。含现场取证命令（`journalctl -k`/`-b -1`、`v4l2-ctl --stream-mmap`、节点日志三行分辨法）与**软件重新枚举**（`authorized 0/1`）替代拔插的验证方法 |
| usb/（镜像源码：`notes/usb`） | **USB**（一篇一个问题）：设备定位方法（四步法）、v2 / 5io 两块板的物理口映射、自研底板的 VL817 上游接法评审、板载 hub 的 STT vs MTT 实测、Type-C 固定主机与外设供电 |
| [hub-stt-mtt.md](../usb/hub-stt-mtt.md) | **板载 USB2 hub 的 STT vs MTT：5io(FE1.1s) vs v2(CH334F)**：**判据一**：TT 模式是 hub 自己上报的 —— 读 hub 的 `bDeviceProtocol`（`02` = MTT），不用查手册。 |
| [device-mapping-method.md](../usb/device-mapping-method.md) | **怎么确认某个 USB 口接的是哪个设备（方法）**：四步法 —— ①认人（`lsusb`）②认口（三种粒度：`lsusb -t` / sysfs 名字 / `readlink`）③取字段（`udevadm info -q property` 给 `ENV{}`、`-a` 给 `ATTRS{}`、`ID_PATH` 是谁给的）；起因见文内"为什么要有这套方法" |
| [device-mapping-in-practice.md](../usb/device-mapping-in-practice.md) | **把方法用起来**：现场相机规则**逐行解读**、symlink 没出现时的**排查清单**、⛔ `xhci-hcd.N.auto` 的 `N` 换内核就变**不要写进 udev 规则** |
| [topology-lubancat-5-v2.md](../usb/topology-lubancat-5-v2.md) | ⭐ **v2 的 USB 物理口 ↔ 控制器映射（板级依据）**：7 个符号链接实测、**4 个 USB-A 口 ↔ 内核控制器映射**（2026-09-17）、原理图网名交叉验证、J16A/J16B ↔ 上下层实测、旁边那句 `U: OTG / D: HOST` 怎么读；5io 的口表见 [topology-lubancat-5io.md](../usb/topology-lubancat-5io.md)。⚠️ 本篇 2026-10-03 **拆出**了"口细节"那一半 → [topology-lubancat-5-v2-ports-and-hub.md](../usb/topology-lubancat-5-v2-ports-and-hub.md) |
| [topology-lubancat-5-v2-ports-and-hub.md](../usb/topology-lubancat-5-v2-ports-and-hub.md) | **v2 的 USB 口细节**：同一个物理口上 **USB2 与 USB3 设备的 `usbN` 为什么不同**、蓝色双层 A 口的 USB2/USB3 **稳定标识**、白色双层两个口背后那颗 **CH334F hub**、工程机现场接线实测（`tools/lsp-usb-map.sh` 一条命令读出）|
| [quest3-usb-power.md](../usb/quest3-usb-power.md) | Type-C 固定 source/host 的当前策略、角色纠正服务、5V/3A 硬件上限；用 healthd 电池电流与电量趋势判断净充电，区分合约额度、电池电流和 USB 输入电流 |
| [typec-detach-debounce.md](../usb/typec-detach-debounce.md) | **"Quest 每秒级断连 / LCD 报手柄低电、头显未连接"的现象与根因**（2026-10-02）：不是电池也不是线 —— 主机侧 FUSB302 把 CC 比较器（`COMP_CHNG`）与 `VBUSOK` 的**瞬态**直接当成"对端被拔掉"，tcpm 随即拆 Type-C 口，`usbdrd3_0` 上的伴生 xHCI 被 unregister 再 probe，口上设备全部重枚举（实测一个 boot **706 次拆除 / 233 次断连**）。含：两类触发（VBUS 真掉 vs CC 阈值误判）的日志判读、`rd_mda_value[3A]=61`(2604 mV) 与 `source-pdos 5V/3A` 的余量问题、**为什么 `data-role="host"` 挡不住 dwc3 切 device**（`USB_ROLE_NONE` → 默认角色）、**`VBUS off` 是 tcpm 自己关的**、去抖修法，以及**未决项**（故障条件为何自己停了） |
| [fusb302-debounce.md](../usb/fusb302-debounce.md) | FUSB302 source CC/VBUS 去抖重写：独立截止时间、有界读取失败恢复、Ra/Ra 重新检测、确定性 C 回归与真实 CC 寄存器 A/B |
| [typec-fault-inject-and-verify.md](../usb/typec-fault-inject-and-verify.md) | 上面那篇的**复现·验证·落地**（2026-10-02）：**不用改内核的故障注入**（把 `MEASURE(0x04)` 压到 CC 电平以下 ⇒ 芯片真报 `COMP_CHNG`，自愈可重复）、现场撒网监视器（"滴灌"读 tcpm/fusb302 debugfs log）、两个 dtbo（`-dwc3-rolehost` 让控制器不被拆、`-usbcon-fixedsrc` 把口锁成老镜像的固定 source）、**判决实验实测：光保住控制器不够，设备照样掉线**、**用户态 mask 中断方案 A 实测通过**（不改内核不用重启）、59（6.1.118、不能刷镜像）的两条落地路线与风险，以及"要不要放弃 device 能力"的取舍（DRP 口在 detach 后会被 Quest 抢走角色 ⇒ 停充、被反供电） |
| [typec-rollout-and-decision.md](../usb/typec-rollout-and-decision.md) | **落地与决策**（2026-10-02）：59（内核 6.1.118、内建 fusb302、不能刷镜像）怎么改 —— 换内核（git 里有 6.1.118 全树，只换 vmlinuz）或用户态 mask；**"放弃 device 能力"的决定**与依据（DRP 下 5 轮注入 1 次被抢成 sink ⇒ Quest 反供电、头显耗光 ⇒ 设备停摆）、代价（这个口不能当 USB device 用了）、以及**新默认配置（`-usbcon-fixedsrc` + `-dwc3-rolehost`）的 20 轮验证：0 次被抢 / 0 次拆控制器 / `SNK_=0`**（⚠️ 但掉线重枚举仍在，要靠去抖/mask） |

### storage/ —— 存储

| 文档 | 内容 |
| --- | --- |
| [sd-card-performance.md](../storage/sd-card-performance.md) | **SD 卡性能实测（SanDisk 1 TB，fio/ioping）**：卡片身份与协商模式（SDR104/4-bit/198 MHz ⇒ 99 MB/s 上限，实测顺序读 87.8 MiB/s 已到顶）、逐项 fio 数据（顺序/随机/混合/fsync/5 分钟持续写）、**4K 随机单发 6.01 ms（eMMC 0.335 ms，慢 17.9×）**、第一次 raw 读偏慢的调相伪影、fstrim、当数据盘的实践建议；**§9 换成 USB3 读卡器反而更慢**（BOT 不是 UAS、`US_FL_GO_SLOW｜MAX_SECTORS_64` 让队列深度=1、不支持 TRIM） |
| [sd-card-benchmarks.md](../storage/sd-card-benchmarks.md) | **1 TB SD 卡的实测数据**（数据篇）：测试方法（可复现）与全部结果 —— 顺序 I/O、随机 I/O、**ioping 单发延迟**（最贴近"点一下等多久"）、持续写 5 分钟（看 SLC 掉速）、一次测试期异常（第一次 raw 读偏低） |
| [sd-card-usb-reader-comparison.md](../storage/sd-card-usb-reader-comparison.md) | **换 USB 3.0 读卡器会更快吗？—— 不会**（同卡同参数实测）：先确认接在 SuperSpeed 口、但**走 BOT 不是 UAS** 且被内核加限制 ⇒ 读卡器反而更慢，板载 SD 槽是最优解 |
| [nvme-performance.md](../storage/nvme-performance.md) | **NVMe 性能实测（Kingston SNV3S1000G 1 TB，文件级 fio）**：5io 上顺序 **1548/1572 MiB/s** 贴着 PCIe 3.0 **x2** 链路顶（理论 1.97 GB/s、效率 84%）、4K 随机读 **160.9k IOPS**/写 92.5k、QD1 读延迟 **66.6 µs**；**LnkCap（盘声明 Gen4 x4）vs LnkSta（实际 Gen3 x2）怎么分别读**、为什么 Gen3 是双方取交集、x2 是 DT/原理图的 2 lane；**⛔ 为什么不测裸设备（会毁 ext4）**、v2（4 lane）待测与判据 |
| [init-sdcard.md](../storage/init-sdcard.md) | **数据 SD 卡初始化命令 `lsp-init-sdcard`**（上位机通过 ssh 调用）：出厂 exfat → **单个吃满整卡的 ext4 分区**（起始扇区 2048 → 整盘末尾）→ 写 `/etc/fstab`（**`LABEL=Data1T`**，卡可在设备间通用）→ 挂 `/mnt/data_1T` → chown board；**原脚本 15 处问题逐条对照**、参数、退出码、板上实测（exfat→ext4 34s / 幂等 / 拒绝系统盘 / loop 假卡测重建分区 / `LABEL=` 改造）；**§5.1 开机动兜底 `lsp-init-sdcard.service`**（1T 且非 ext4 才动手，四道闸门防误伤启动卡） |
| [init-sdcard-board-tests.md](../storage/init-sdcard-board-tests.md) | **`lsp-init-sdcard` 的板上实测记录**：出厂 exfat → 单个吃满整卡的 ext4（**34 s**）、幂等（再跑不动分区表/文件系统）、安全护栏（指定系统盘**拒绝**）、loop 假卡测重建分区表、fstab 改 `LABEL=Data1T` 的验证 |
| [data-card-durability.md](../storage/data-card-durability.md) | **采集数据掉电会丢什么 + 兜底守护 `lsp-sync-data`**：`d?????????` 的含义（`lstat` 失败、不是属主丢失）；**8 轮"脏页 1.8~6MB 时 `reboot -ff`"实测 ⇒ ext4+journal 自愈、0 个 `?`、fsck 干净**（所以截图那种更像 FAT/exFAT 卡或卡自身退化/物理拔电打到卡内部缓存）；采集侧落盘现状（`metadata.json` 无 fsync，只有关机前 `os.sync()`）；inotify + `syncfs` 只在封口触发、为什么不周期性 sync、以及"采集侧该在 `_seal()` 里 fsync"这条首选修法 |
| [boot-flash-card.md](../storage/boot-flash-card.md) | **SD 启动/烧录卡**：为什么卡上系统要用 SD 专用卷标（`boot-sd`/`writable-sd`）避免与 eMMC 混合启动；卡的最终布局（`/opt/lsp-flash/images` + `tools` + README，工具不带 `-i` 自动找镜像，卡里 README 的正文就是仓库的 `tools/flash-card-README.txt`）；**从生成 `-sd` 变体到写卡、`growpart` 扩到整卡、放镜像的实测步骤**；到目标机器上先跑启动测试、再按需写 eMMC 的命令与「写完必须拔卡再上电」；顺带修掉 `make-sd-image.sh` 的 `mv ... are the same file`（导致脚本非 0 退出 + 不生成 `.sha256`）；**压缩已与做镜像解耦**（2026-09-15）：`make-sd-image.sh` 只出裸 `.img`，压 `.zst`/`.zip` 统一走 `tools/compress-image.sh` |
| [multi-media-label-conflict.md](../storage/multi-media-label-conflict.md) | 排查记录：SD 卡与 eMMC 烧同一镜像导致卷标/UUID 全同，进而"用 SD 启动却挂 eMMC 根文件系统"的根因与解决（SD 专用 `-sd` 变体镜像） |

## 约定

- 一个主题一个文件，文件名用 kebab-case 英文，例如 `rk3588-boot-flow.md`。
- **分类规则**（拿不准就放 `system/`，那是最常查的一类）：
  - 构建流程 / 包清单 / apt 源 / 构建耗时 → `build/`
  - 镜像里新增的命令、服务、udev 规则、桌面与系统策略 → `system/`
  - 板级硬件资源、启动链路、启动故障 → `hardware/`
  - USB（主机 / 拓扑 / 外设供电） → `usb/`
  - SD/eMMC 数据盘的性能、初始化、卷标 → `storage/`
- 文档里的命令和结论尽量附**实测输出**，便于日后复核。
- 结论性的判断要写清**证据来源**（哪条命令、哪个文件、哪一行）。
- 涉及版本/环境差异的，标注当时的构建环境。
- **新增或移动文档后**：① 更新本索引；② 检查交叉引用是否还有效 ——
  用 `rg -n 'notes/' --glob '!kernel-6.1/**' --glob '!u-boot/**' --glob '!build/**' --glob '!images/**' .`
  （⛔ 不要用 `grep -rn … .`：整树遍历会走进 `kernel-6.1/`、`u-boot/`，把机器拖死 —— AGENTS.md §2 第 17 条），
  并检查每篇文档内部的相对链接是否都指向存在的文件。

## 相关代码位置

- 镜像构建脚本：`scripts/build-lsp-image.sh`（apps → base → rootfs → oem-host → board → image）
- U-Boot 打包规则：`packages/u-boot-lubancat-rk3588/debian/`
- U-Boot 源码：`u-boot/`
- 预编译件（固件 / ATF bootloader）：`prebuilt/`
- 工具脚本：`tools/`（源码同步 `sync-to-board.sh`、镜像烧录、增量写入、挂载 chroot、SD 变体镜像、镜像体检 `verify-image.sh`、ATF 全链路 `check-atf.sh`、apt 缓存分发 `apt-cache.sh`）
