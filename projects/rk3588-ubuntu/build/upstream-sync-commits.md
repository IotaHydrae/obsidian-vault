# 上游提交逐条台账（野火 `ubuntu-ebf-rk3588`）

> 已评估到 **`88226c2ef`**（2026-10-23）。本轮 17 个提交中，仅 RTL8822CE TXBD 两补丁
> 已纳入源码并完成编译，尚未安装到开发板运行；其他候选未实施，见 §4。
> 比对方法、总表与回归清单见 [主篇](upstream-sync.md)；上游独立 clone 为
> `~/example-project/ubuntu-ebf-rk3588`，分支 `24.04-panfrost`。以下旧轮次保留其当时的取舍依据。

## 1. 2026-09-18 那轮：`0c8d06b49` → `046145c54`（7 个提交）

| 上游提交 | 内容 | 要不要合 | 要测什么 |
| --- | --- | --- | --- |
| `046145c54` | RK3588 **BL31 v1.48 → v1.53** | ⭐ **要**（已合，见主篇 §2） | 两板启动全回归（主篇 §6） |
| `163457e95` | BL31 v1.47 → v1.48 + 新增 **BL32 v1.20（OP-TEE）** + `rules` 加 `TEE=` + `targets.mk` 加 `_bl32` | ⭐ 要，但**必须补 3 个 FIT 脚本改动**（主篇 §2） | 同上 |
| `4d661993b` | 删 rkbin 里没用到的 rk3576 二进制 | 顺手（仅省体积） | 不需要 |
| `3648476ff` / `12c0d39e6` | 新增 LubanCat-3 / 3io（**RK3576**）板卡 | ❌ 不需要（不支持的板）⚠️ 但夹带了 u-boot 大树更新（主篇 §4） | 若合 u-boot：§6 |
| `aca5c386c` | "update uenv" | ❌ 纯注释（去掉 overlay 列表的 `.dtbo` 后缀） | 无 |
| `f160bed60` | `scripts/build-rootfs.sh` 用 binfmt 做 ARM64 debootstrap + README | ❌ 上游遗留脚本（`AGENTS.md` §2.11；该脚本 2026-09-24 已从我们仓库删除） | 无 |

> **实测结论**：我们的 u-boot 树 == 上游 `0c8d06b49` 那份（只差**我们自己打的 2 个补丁**，见主篇 §4），
> 上游此后换了 **1099 个文件**；`kernel-6.1` 只差 94 个文件、**全是 rk3576**。

## 2. 2026-09-26 那轮：`046145c54` → `060a206a9`（19 个提交）

评估方法：逐提交 `--numstat` 归类 → 与**我们仓库的实际文件**对比 → 在开发板上核对**我们实际用什么**。

### 2.1 已采纳（本轮做完）

| 上游提交 | 内容 | 我们原来的状态 | 结果 |
| --- | --- | --- | --- |
| `86da08a2b` fix incomplete serial console logs | 新增 `journald.conf.d/console.conf`（`ForwardToConsole=yes` + `TTYPath=/dev/ttyFIQ0`）**并**去掉 bootargs 的 `quiet` | 我们没有 journald→console 转发（串口上只有内核日志） | ✅ 只取 journald 那半：`overlay/etc/systemd/journald.conf.d/10-lsp-console.conf`（`MaxLevelConsole=info`；上游的 `debug` 会把串口刷满）。**不跟**"去掉 `quiet`"——面板内核控制台是 2026-09-24 特意关的。开发板上用受控 pty 端到端验过 |
| `1c73a4a23` panel-simple: fix endless probe defer without nvmem | `panel_simple_dsi_of_get_desc_data()` 只在 DT 真有 `nvmem` 时才走 nvmem 分支 | 我们是旧写法（无条件走）⇒ 无 `nvmem` 的 DSI 面板**永远 probe 不上**（我们内核 `drivers/nvmem/core.c:955` 返回 `-EPROBE_DEFER`，而面板节点都没有 `nvmem`） | ✅ 已改（与该文件上游版**逐字节一致**）；服务器编内核 → 装板 → 模块哈希与 deb 内一致 → 重启后新内核在跑、`deferred` 列表与改前一致、无失败单元 |
| `9c7f45b01` boot 分区 vfat → ext4 | 见 §2.3 | 我们是 vfat + initramfs 里 `fsck-vfat` | ✅ **2026-09-26 已实施**（方向被采纳，理由见 §2.3） |

### 2.2 逐条结论（真机取证）

| 上游提交 | 内容 | 真机取证 | 结论 |
| --- | --- | --- | --- |
| `f060c540d` 删冗余 BT/USB 服务 + 修 uEnv 拼写 | 删 `brcm_patchram_plus`/`rtk_hciattach`/`bt_load_rtk_firmware`/`ap6275p-bluetooth.*`/`enable-usb2.service`/`rtl8821cs-bluetooth.service`/`ubuntu-rockchip-install` | 开发板上前 6 类**根本没装进镜像**（只在我们 overlay 里躺着，是死文件）；`ubuntu-rockchip-install` 在但不是服务；`rtl8852be-reload.service` 在且 enabled | ✅ **已清理**（2026-09-26）：删 overlay 里 8 个死文件 + build 脚本装 `ubuntu-rockchip-install` 那行 + `.gitattributes` 对应 LFS 规则；`verify-image.sh`「移除项」加了 8 条**反向断言**。⛔ `rtl8852be-reload.service` 保留 |
| `5bed80a74` update Realtek RTL WiFi drivers | rtl8852be / rtl8851be / rtl8821cu 大更新（2165 文件、+1.9M 行） | **5-v2 用的是 `RTL8822CE`（`10ec:c822`）—— 这三款一个都没用上**（镜像里 `8852be.ko` 带着但 0 引用）；我们树里**没有 `rtl8851be`**（能力缺口） | 🟡 优先级低。只有现场/5io 真用 8852be/8821cu 且抱怨 WiFi 时才跟（要重编内核 + 回归）。5io 的模块型号待上电确认 |
| `93cdf2c7c` Revert "remotectl: prefer dedicated channel-3 IRQ" | capture IRQ 从 `platform_get_irq(pdev,1)` 改回 index 0 | 我们**还是被回退的那版** ⇒ 同一个 IRQ 被申请两次、flags 不一致。开发板每次开机：`genirq: Flags mismatch irq 91 …` + `cannot claim PWR_IRQ!!!` + `pwm sip wakeup config error` + `Donot support ATF Wakeup` | ✅ **已对齐并 A/B 验证**（2026-09-26）：改后（与上游回退版逐字节一致）服务器增量编 32 s → 装板 → 重启，那 4 行**全部消失**，换成 `remotectl-pwm fd8b0030.pwm: Controller support pwrkey capture`。⛔ IR 遥控硬件没法测（不用它） |
| `974cea162` add mesa 26.2.3 panfrost/panvk | 8.2 MB deb **隔离装到 `/opt/mesa-26.2.3`**，靠 `/etc/environment` + `gdm3.service.d` 切过去 | 我们本来就是 Mesa 栈（`libgl1-mesa-dri 25.2.8`，无 libmali）⇒ 大跳有收益；但那个 deb **依赖 `libllvm18`，我们镜像里没有**；上游脚本还给用户 `cat` 加 `render` 组（我们要改成 `board`+`kiosk`） | 🟡 **单独立项评估**（要装 libllvm18 + 回归 kiosk/Chromium 的 EGL/GBM/Vulkan） |
| `c3b5c25bc` update dtbo | u-boot 新增 `dtoverlay` 命令（`CONFIG_CMD_DTOVERLAY=y`），uEnv 从 `overlays=<列表>` 改成逐行 `#dtoverlay=…` | 我们仍用老 `overlays=` 机制，**实测生效**（HDMI0/DP0/HDMI1 + usbcon-host 都在） | 🟡 暂不动（收益是"逐个 overlay 报应用/失败"，代价是重编重刷 u-boot + 改写**用户自己写的** uEnv）。遇到"某 dtbo 不生效"再评估 |
| `fe999f4f3` switch apt mirror to Huawei Cloud | aliyun → huaweicloud | 我们 base **早就用 huaweicloud**（还是拿 `tools/bench-mirrors.sh` 实测选的） | ✅ 已覆盖 |
| `9c3b7fd59` add gstreamer packages | 核心包换成 **1.22.9** + 更新 `gstreamer1.0-rockchip1` | 我们是 Ubuntu **1.24.2**（+PPA 的 `+rkrga` 变体）+ `gstreamer1.0-rockchip1 1.14-4+git240423~noble1`，`gst-inspect-1.0 rockchipmpp` 正常 | ⛔ **不要照搬**（等于降级）。但动机值得学：**把 PPA 依赖变成仓库内 .deb**（我们 rootfs 仍依赖 `ppa:liujianfeng1994/rockchip-multimedia`，Launchpad 不可达时会挂） |
| `941cac16f` / `c50d14c53` rk3576 HDMI sound | 只改 `config/boards/lubancat-3*.conf` 的 `ID_PATH` | 我们两块板是 rk3588；音频卡 `rockchiphdmi0/1`、`rockchipdp0`、`rockchipes8388` 都在 | ⚪ 与我们无关 |
| `23b0be988`(4io ov8858 AF) / `926a74998`(gc08a8 MIPI) / `060a206a9`(camera engine 挪目录+rk3576 包) / `b2f051fcf`(忽略 *.deb) / `09d9107c6`·`145171d51`·`f35c19925`(上游 build.sh 交互 UX) | — | `060a206a9` 只是把 `camera_engine_rkaiq_rk3588` 从 `packages/arm64/` 挪到 `packages/camera/`（**版本没变**） | ⚪ 都不用跟（细节见 §2.3 ①/②） |

### 2.3 三条续评与修正

**① `926a74998` gc08a8 的 Kconfig/Makefile —— 不拿，但记住机制**

它其实是我们真踩到的坑：`gc08a8.c` 在树里、defconfig 也写了 `CONFIG_VIDEO_GC08A8=y`，
但 `drivers/media/i2c/Kconfig` 里出现 **0 次**、Makefile 也没有 `obj-$(CONFIG_VIDEO_GC08A8)`
⇒ `build/linux-rockchip/.config` 里该符号**计数 0**（被 kbuild **静默丢弃**）。
我们的摄像头全是 USB UVC（不接 MIPI-CSI）⇒ **不注册**（不拿反而省内核体积）。

⚠️ 值得记住的是机制：**defconfig 里 Kconfig 不存在的符号会被静默丢掉**。顺手扫过，我们 defconfig 里
共 **13 个** symbol 没进最终 `.config`：真缺口只有 `CONFIG_VIDEO_GC08A8`（不用 MIPI，故不修）；
`CONFIG_PHY_ROCKCHIP_SAMSUNG_HDPTX_HDMI` / `CONFIG_EBF_DHT11` / `CONFIG_EBF_MODULE` 是**假报警**
（真名分别是 `…_HDPTX` / `CONFIG_DHT11`，都在 `.config` 里）；其余
（`CANFD_ROCKCHIP`、`RK_WIFI_DEVICE_UWE5621`、`SPARD_WLAN_SUPPORT`、`UNISOC_WIFI_PS`、
`WCN_BSP_DRIVER_BUILDIN`、`SC23XX`、`SERIAL_KGDB_NMI`、`KDB_KEYBOARD`、`GDB_SCRIPTS`）是历史残留。

**排查口诀**（怀疑"某驱动写在 defconfig 里却没编"）：

```bash
comm -23 <(awk -F= '/^CONFIG_[A-Z0-9_]+=y$/{print $1}' kernel-6.1/arch/arm64/configs/rockchip_linux_defconfig | sort -u) \
         <(awk -F= '/^CONFIG_[A-Z0-9_]+=/{print $1}' build/linux-rockchip/.config | sort -u)
```

**② `5bed80a74` Realtek WiFi —— 不跟**：上游新增 `rtl8851be` 驱动树（1078 文件）+ 更新
`rtl8852be`(730)/`rtl8811cu`(347)，而我们**实际在用的 `8822ce` 这次一个字没动** ⇒ 对现有硬件零收益。
（同上表：5-v2 实测是 8822CE。）

**③ `9c7f45b01` boot 分区 vfat → ext4 —— ✅ 已实施（2026-09-26）**

上游改法（commit message **没写理由**）：分区类型 GUID `BC13C2FF…`(EFI/FAT) → `0FC63DAF…`(Linux)、
`mkpart fat32` → `ext4`、`mkfs.vfat -F32 -i <8字符>` → `mkfs.ext4 -U <完整UUID>`、
`/etc/fstab` 的 `vfat`→`ext4`、initramfs 的 `fsck-vfat` hook → `fsck-ext4`、
`local-bottom/fsck-boot-firmware` 里 `vfat`→`ext4`。

**为什么切**（推翻了我最初的"暂不改"）：

- ⛔ "FAT 能在 PC 上直接改 `ubuntuEnv.txt`"这个反对理由**基本不成立**（用户指正）：
  **eMMC 的机器现场本来就没法在 PC 上改**，只有 TF 卡那种工作流才成立 ⇒ 不能当反对理由。
- ✅ 支持切的**我们自己的历史证据**：`6734a194d 修复异常断电后的文件系统检查与启动恢复`
  —— 那次改的正是 boot 分区（vfat）：`/etc/fstab` 里 `/boot/firmware` 从 `defaults 0 2` 改成 **`0 0`**，
  并**新增** initramfs 的 `fsck-vfat` hook + `local-bottom/fsck-boot-firmware`（挂根后、切根前按 fstab
  显式 `checkfs`，`fsck … -y` 强制自动修复）。⇒ **"异常断电把 boot 分区搞坏、需要修复"在我们这儿真实发生过**；
  而 FAT **没有 journal**，再严重一点就是变砖（要重刷）。ext4 有 journal + 真 fsck，从根上消掉这类故障，
  还能去掉 `dosfstools`/`fsck.vfat`/IBM850 gconv 那套 FAT 专用补丁。
- 落地（都在镜像侧，**u-boot 不用动**：我们的 `boot.cmd` 用通用 `load ${devtype} …` 而不是 `fatload`，
  `prebuilt/u-boot.itb` 里 `ext4load`/`ext2load`/`fatload` 三个命令都在）：
  ① `scripts/build-lsp-image.sh` 分区表/`mkfs.ext4 -U`/fstab；② initramfs hook 换 `fsck-ext4`；
  ③ `tools/update-emmc.sh` 去掉 `--modify-window=1`；④ `tools/verify-image.sh` 的 FAT 断言换 ext4 断言。
  核实：`build-lsp-image.sh:3974/4015/4021`（`mkpart primary ext4`、`mkfs.ext4 -U … -L system-boot`）
  与 `:2111`（initramfs 里带的是 `fsck.ext4`）都是 ext4 了。
- ⚠️ **新旧必须配对**：老 vfat 启动卡/镜像与新 ext4 镜像不能混用 ⇒ 产线与仓库里现存的卡要一起换
  （见 `../storage/boot-flash-card.md`）。

**④ 两条澄清（用户问的）**

- **ext4 是不是也更快**：只在**元数据/目录密集**操作上明显快（htree vs FAT 线性扫描 + 链遍历）——
  我们 boot 分区里 `dtbs/rockchip/overlay/` 有 257 个 dtbo，这类查找 ext4 占优。但 u-boot 读的
  `vmlinuz`(42 MB) + `initrd.img`(18 MB) 是**大文件顺序读**，两者都受 eMMC/TF 带宽限制，文件系统开销是
  毫秒级噪声 ⇒ **"切 ext4 能让启动更快"基本不成立**；动机是**掉电可靠性**与一致性。
- **u-boot 读到脏 journal 会怎样**：我们这版 `fs/ext4/ext4_journal.c` 只处理
  `EXT3_FEATURE_INCOMPAT_RECOVER`，而 `ext4fs_mount`/`ext4fs_read_super` **不检查 RECOVER**
  ⇒ **u-boot 不会因脏 journal 拒绝挂载**，照读没在写的文件（kernel/initrd/dtbs 都没问题）；
  journal 重放交给内核挂载 `/boot/firmware` 时自动做、以及 initramfs 里的 `fsck.ext4 -y`。
  ⇒ 风险窗口窄，且已列入验证计划的"拔电 ×3 复测"；若实测有问题再考虑给 u-boot 打开
  `CONFIG_CMD_EXT4_WRITE`（含 journal replay）重编 loader。

## 3. 2026-09-29 之后的 3 个提交（`060a206a9` → `3b064db4b`）

| 上游提交 | 内容 | 结论 |
| --- | --- | --- |
| `3b064db4b` enable CAN config | `rockchip_linux_defconfig` 加一行 `CONFIG_CAN_RK3568=y` | ✅ **已采纳**（2026-09-30，用户："我们虽然暂时不用 CAN，但这是一个隐患"）—— 我们 defconfig 第 514 行已有该符号 |
| `4ab81373d` fix README: kernel version, image name and system line | 只改上游 README | ❌ 不需要（我们的 README 是自己的） |
| `6d2a83149` fix image name in README | 同上 | ❌ 不需要 |

## 4. 2026-10-23 评估：`3b064db4b` → `88226c2ef`（17 个提交）

范围为该分支新增的全部 17 个提交，逐项对照本工程当前源码与安装链。
下表“已覆盖”仅表示现有实现已达到对应目的；“候选”均未随本轮实施。

| 上游提交 | 更新 | 本工程取舍 |
| --- | --- | --- |
| `04219ab6d` | 通过 ABC ID 识别板型 | 不采纳通用板型流程；维持 `5-v2` / `5io` 各自的构建与配置契约。 |
| `152692122` | CPUID 派生固定以太网 MAC | 不直接采纳。当前 BSP 已优先使用有效地址及 vendor storage；无地址漂移实测依据时不改网络身份。 |
| `1a1598f9d` | MAC 查询表与 CPUID 记录 | CPUID 只读查询可独立借鉴，未实施；MAC 反查依赖上游派生规则，不能用于任意出厂地址。 |
| `735c278e0` | PCIe endpoint 配置 | 暂不采纳；当前 WiFi / NVMe 使用 host，endpoint 不属于现有需求。 |
| `27909eadc` | 开机自动设置 MAC、记录 CPUID | 不采纳自动改地址。服务未保证排在 NetworkManager 前，且一次性枚举可能与本工程延后加载 GMAC 冲突。 |
| `31b0fcbaf` | 持久化设置 / 重置 MAC | 不直接采纳。工具可能 down/up 网口，`reset` 恢复派生地址而非出厂地址；需单独制定运维接口。 |
| `6fc9b297b` | `rtl8811cu` 目录改名为 `rtl8821cu` | 暂不采纳；这是另一款 USB WiFi 驱动的命名整理，对现用 PCIe RTL8822CE 无直接收益。 |
| `3a28689bd` | 关闭 RTL8822CE `DBG_TXBD_DESC_DUMP` | 已纳入源码，移除调试专用状态及发送路径记账；编译验证与性能边界见下文。 |
| `7faee4cfa` | 关闭 journald 同步转发到串口 | 已覆盖。保留本工程独立的 warning 日志回放服务，见 [现行串口策略](../system/journal-serial.md)。 |
| `f277e0abf` | U-Boot 选择 DTB、去掉 `rk-kernel.dtb` | 不直接采纳。本工程已按板卡选用并预合并 DTB；上游流程会覆盖 `lsp-merged` 契约。其中 PCIe 断电后释放 PERST 的独立改动也排除。 |
| `773e102b8` | 删除未使用的通用板卡变量 | 无需跟随；本工程未采用其通用板型构建流程。 |
| `5764911fa` | 去掉 MOTD 时间标题 | 已覆盖；本工程保留时间在第二行末尾。 |
| `a029a1c36` | MOTD 板卡信息前增加空行 | 不替换用户定制布局；当前紧凑信息块已有空行。 |
| `409968622` | 为 `core_get_tx_ring_ext()` 增加调试宏保护 | 与 `3a28689bd` 成对纳入源码，避免关闭宏后仍编译仅用于调试的函数。 |
| `d74ddecdb` | root / boot 跟随启动脚本介质 | 候选：独立适配 `boot.scr` 所在介质，尚未实施。当前仍使用 [SD 专用变体](../storage/multi-media-label-conflict.md)；此机制不证明 BootROM / U-Boot 二进制来源。 |
| `d183e425b` | MOTD 增加 CPUID、介质及构建信息 | 可将 CPUID、当前根介质借鉴到只读命令，未实施；不覆盖受保护 MOTD，不以根目录时间代替镜像构建指纹。 |
| `88226c2ef` | 新增 `ebf-config` 硬件软件总览 | 可选择性扩展 `lsp-config info`，未实施。负载已由 `lsp-rk-monitor` 覆盖；库文件存在不能证明浏览器硬解启用，节点缺失也不能直接判定硬件故障。 |

### RTL8822CE 采纳范围与验证

仅修改 `kernel-6.1/drivers/net/wireless/rtl8822ce/include/autoconf.h` 的调试宏，
以及 `hal/rtl8822c/rtl8822c_ops.c` 中 `core_get_tx_ring_ext()` 的条件编译边界。
两版均使用当前 `#13` 配置编译成功，关闭调试后 `.bss` 减少 **8968 B**。
该数字表示静态未初始化数据的差异，不能换算为 CPU 占用或吞吐收益。
补丁模块仅完成源码与编译验证，未安装到开发板运行；补丁版运行与最大吞吐尚未验证。
旧模块运行态 A/B 观测与限制见专题，不替代补丁模块的运行验证。
调用路径、成本来源与测量口径见 [RTL8822CE TXBD 调试成本](../hardware/rtl8822ce-txbd-cost.md)。

本轮未修改板卡配置、uEnv、MOTD、MAC、启动介质选择或 PCIe 行为。
后续评估从 `88226c2ef` 往后比；单项部署与性能结论分别以对应专题的证据为准。
