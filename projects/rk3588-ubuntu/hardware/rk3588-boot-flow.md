# RK3588 完整启动流程

> 本文以 LubanCat-5（RK3588）Ubuntu 桌面镜像为例，记录从 SoC 上电到桌面的完整启动链路，
> 以及本仓库镜像的存储布局与关键产物。文中数据均为**实测**（命令见文末）。

## TL;DR

- **一句话**：`BootROM → idbloader(DDR+SPL) → u-boot.itb(ATF+U-Boot) → boot.scr → 内核 → systemd`。
- **ATF（BL31）是 SPL 与 U-Boot 之间不可省略的一环**（运行在 EL3）；FIT 里 `conf` 的
  `firmware = "atf-1"` 就是这个跳转的声明 ⇒ **`u-boot.itb` 缺 ATF 会卡在"SPL 之后、U-Boot 之前"**。
- 存储布局：镜像头部 16 MB 是 bootloader 保留区（`idbloader` @32 KB、`u-boot.itb` @8 MB），
  之后才是 GPT 分区（boot + rootfs）。
- 启动介质：eMMC / SD / NVMe 都能启动；**要从 SPI NOR 启动必须走 SFC 那组引脚**（不能用任意 SPI）。
- 校验与防篡改（"改了一块会不会被发现"）→ [boot-integrity.md](boot-integrity.md)。
- BL31 机制 / SIP・PSCI 接口面 → [bl31-mechanism.md](bl31-mechanism.md)；
  关机/重启走哪条路 → 其 §4（本文 §3.8 是同一件事的简版）。
- 验证命令与实测记录在文末（§6/§7）。

## 1. 总览

```
上电
 └─> BootROM（SoC 内部固化，不可改）
      │  读取启动介质，从固定偏移加载 idbloader
      ▼
     idbloader.img                                    @ 32KB（扇区 64）
      ├─ RKNS 头（2KB）                               ← header0_info_v2
      ├─ DDR 初始化固件                               ← rk3588_ddr_*.bin（fwver v1.18）
      └─ U-Boot SPL
           │  初始化 DRAM 后，从介质加载 u-boot.itb（FIT）
           ▼
         u-boot.itb（FIT）                            @ 8MB（扇区 16384）
          ├─ atf-1 / atf-2 / atf-3   = ATF (BL31)     ← 运行于 EL3
          ├─ uboot                   = U-Boot proper  ← 运行于 EL1/EL2
          └─ fdt                     = U-Boot 控制 DTB
           │  SPL 先跳 ATF（conf 的 firmware = "atf-1"）
           ▼
         ATF BL31（EL3）：初始化安全世界 / PSCI，再跳转到 U-Boot
           ▼
         U-Boot proper（EL1/EL2）
          │  读启动分区 boot.scr → ubuntuEnv.txt，加载内核 / initrd / DTB
          ▼
         内核 + initramfs（/boot/firmware/）
           ▼
         systemd → display-manager → 桌面
```

**一句话概括**：`BootROM → idbloader(DDR+SPL) → u-boot.itb(ATF+U-Boot) → boot.scr → 内核`。

其中 **ATF 是 SPL 与 U-Boot proper 之间不可省略的一环**：SPL 把控制权交给运行在 EL3 的 ATF，
ATF 完成安全初始化后再把控制权交给 U-Boot。FIT 里 `conf` 的 `firmware = "atf-1"` 正是这一跳转的声明。
这也是为什么 **u-boot.itb 里缺 ATF 会卡在「SPL 之后、U-Boot 之前」**。

## 2. 存储布局

### 2.1 bootloader 保留区（镜像头部 16MB）

RK3588 的 bootloader 区固定占用磁盘开头 **16MB**，各段偏移如下（源自
`packages/u-boot-lubancat-rk3588/debian/rules` 里 `rkspi_loader.img` 的分区定义）：

| 区域 | 起始扇区 | 起始偏移 | 大小 | 说明 |
| --- | --- | --- | --- | --- |
| idbloader | 64 | 32 KiB | ~3.5 MiB | DDR 固件 + U-Boot SPL |
| vnvm | 7168 | 3.5 MiB | 256 KiB | |
| reserved_space | 7680 | 3.75 MiB | 192 KiB | |
| reserved1 | 8064 | 3.94 MiB | 32 KiB | |
| uboot_env | 8128 | ~4 MiB | 32 KiB | U-Boot 环境变量 |
| reserved2 | 8192 | 4 MiB | 4 MiB | |
| uboot | 16384 | 8 MiB | ~8 MiB | u-boot.itb（含 ATF） |
| — | 32768 | **16 MiB** | — | bootloader 区结束 |

对应的写入命令（`scripts/build-lsp-image.sh` `image` 阶段同样使用这两个偏移）：

```bash
dd if=idbloader.img of=<disk> seek=64    conv=notrunc   # 64 × 512B  = 32 KiB
dd if=u-boot.itb   of=<disk> seek=16384  conv=notrunc   # 16384 × 512B = 8 MiB
```

> 注意 `dd` 默认块大小为 512 字节，所以 `seek` 的单位是**扇区**而非字节。
> 若使用合并镜像 `u-boot-rockchip.bin`，则一次写入即可：`dd if=u-boot-rockchip.bin of=<disk> seek=1 bs=32k conv=fsync`。

### 2.2 GPT 分区表（本仓库镜像）

| 分区 | 起止 | 类型 | 标签 | 内容 |
| --- | --- | --- | --- | --- |
| 1 | 16 MiB – 528 MiB | **ext4**（2026-09-26 起；此前是 fat32/`BC13C2FF-...`） | `system-boot` | 内核 `vmlinuz`、`initrd.img`、`dtbs/`、`boot.scr`、`ubuntuEnv.txt` |
| 2 | 528 MiB – 100% | ext4（`0FC63DAF-...`） | `writable` | 根文件系统 |

启动参数由 `ubuntuEnv.txt` 的 `bootargs=` 提供。**2026-10-03 板上实测的那一行**（原文照抄）：
```
bootargs=rootfstype=ext4 rootwait rw console=ttyS2,1500000 console=ttyFIQ0 cgroup_enable=cpuset \
cgroup_memory=1 cgroup_enable=memory swapaccount=1 systemd.unified_cgroup_hierarchy=0 quiet splash \
plymouth.ignore-serial-consoles vt.global_cursor_default=0 usbcore.autosuspend=-1
```
⚠️ 两点与旧文档不同：① **没有 `console=tty1`** —— 面板不做内核控制台（2026-09-24 起，构建期有断言拦着，
见 [../build/build-guide.md](../build/build-guide.md) 与 `scripts/build-lsp-image.sh` `image` 阶段）；
② 这一行里**没有 `root=`**（根设备哪来的没在本轮核到底：`boot.scr`/U-Boot 侧提供，未验证）。

### 2.3 单独一个启动分区是不是 Ubuntu 的规定？

**不是。** Ubuntu 没有"必须有 `/boot` 分区"这条规定，这里的布局是为 U-Boot 选的：

- **x86 桌面 Ubuntu 真正"必须有"的是 ESP**（FAT32，挂 `/boot/efi`）—— 那是 UEFI 固件的要求，
  与 Linux 无关。只有根分区在 LVM / 加密卷上时，`/boot` 才**实际必须**单独分区
  （bootloader 读不了 LVM）。普通 ext4 单分区 `/` 的 Ubuntu 桌面一切正常。
- **arm64 板子上是惯例而非规定**：U-Boot 需要一个"自己能读"的分区放 `vmlinuz` / `initrd.img` / `dtbs`，
  各家官方镜像都用一个 FAT 分区挂到 `/boot/firmware`（树莓派、Ubuntu generic arm64 均如此）。
  ⚠️ **本工程 2026-09-26 把它换成了 ext4**（与上游 `9c7f45b01` 同向）：vfat 没有 journal，异常断电
  后要靠我们自己塞进 initramfs 的 fsck 补救（"boot 分区损坏 = 起不来"），ext4 有 journal + 真 fsck。
  代价是同时改了分区类型、fstab、initramfs 那个 hook（`fsck-vfat` → `fsck-ext4`）与构建期脚本；
  U-Boot 侧不用动（`boot.cmd` 用的是通用 `load ${devtype} …`，prebuilt 里 ext4load 在）。
  详见 [../build/upstream-sync-commits.md](../build/upstream-sync-commits.md) §2.3 与
  [../system/initramfs.md](../system/initramfs.md) §3.4。
- **本镜像里"把内核搬到启动分区"不是 Ubuntu 机制干的**：镜像**没装 `flash-kernel`**
  （板上 `dpkg -l | grep -c flash-kernel` = 0；Ubuntu arm64 官方镜像靠它），
  而是 `overlay/etc/initramfs/post-update.d/zz-update-firmware` 在 `update-initramfs` 之后
  复制 `vmlinuz` / `initrd.img` / `dtbs/` 过去的（详见 `notes/system/initramfs.md`）。
- 判据（**2026-10-03 板上实测**）：`findmnt /boot/firmware` → `/dev/mmcblk0p1 ext4 rw,relatime`，
  `lsblk -o NAME,FSTYPE,LABEL /dev/mmcblk0` → `p1 ext4 system-boot` / `p2 ext4 writable`；
  **`/boot` 本身只是根分区（p2，ext4）里的普通目录，不是挂载点**。

## 3. 各级启动阶段详解

### 3.1 BootROM（不可修改）

- 上电后由 SoC 内部固化代码执行，通过 strap/启动顺序选择介质（SPI NOR / eMMC / SD）
- 从介质**固定偏移 32 KiB（扇区 64）**读取 bootloader 头部
- 只做最小初始化，把 idbloader 载入片内 SRAM 后跳转
- 头部 CRC/合法性校验失败 → 直接无输出或停在 ROM 阶段

#### 3.1.1 SPI NOR 必须走 SFC 那组引脚，不能用任意 SPI（2026-09-30）

⛔ **"从 SPI flash 启动"不等于"板上有 SPI flash 就能启动"** —— 只能是 SoC 的 **SFC**
（Serial Flash Controller，`spi@fe2b0000`，`compatible = "rockchip,sfc"`）那组固定的引脚，
**不能**接到普通 SPI 控制器（spi0/spi1/spi2/…，`rockchip,rk3066-spi`）的 SCLK/MOSI/MISO/CS 上。原因：

- BootROM 阶段**没有**时钟框架/DMA/驱动这套东西，它只认自己内置的那一条 SPI flash 通路
  （SFC 控制器 + 它自己配的 IOMUX）；普通 SPI 控制器在 ROM 阶段根本不被初始化，接在上面 BootROM 读不到任何东西。
- 软件侧同理，两者是不同驱动：SFC 是 `drivers/spi/spi-rockchip-sfc.c`，普通 SPI 是 `spi-rockchip.c`。
  内核 DTS 里的 sfc 节点**连 `pinctrl-0` 都没有**（`rk3588s.dtsi:6889` 的 `sfc: spi@fe2b0000`），
  全树真用它的板子也只写 `status = "okay"`（例：`rk3588-nvr-demo-v10-spi-nand.dts:21`）
  ⇒ **引脚是控制器固有的那组，不是可以随便映射的**。
- SoC 引脚复用表里 FSPI 有 M0/M1/M2 三套（v2 原理图 p13：M0 = `FSPI_D0…D3/CS0N/CS1N/CLK_M0`
  对应 `GPIO2_D0…D7/A0`，M1 = `GPIO2_A6/A7/B0/B1/B3/B4/B5`，M2 = `GPIO3_*` 等）。
  厂商把 SPI flash 焊盘接在 **M0** 那组（见下），BootROM 默认走的也是它；
  M1/M2 能不能启动没验证过，别拿它赌。

**本工程两块板的情况**：

- **lubancat-5-v2：根本没有 SPI flash 焊盘**，`FSPI_*` 在原理图里只出现在 SoC 引脚复用表，
  对应网名是 `EMMC_*`（那组引脚被板载 eMMC 占着）⇒ BootROM 介质**只有 eMMC 和 SD**，
  "不依赖 eMMC/SD 启动"这条路在 v2 上不存在。
- **5io 用的核心板（LubanCat5CB / 野火鲁班猫5.BTB核心板，`EBF410152V1R0`）：有焊盘**，
  第 21 页标题就是 `SPI Flash`，位号 `U12`（位号列写作 `NC71` = 不贴）：

  ```
  FSPI_CS0N_M0  1  CS        VCC      8  VCC_1V8_S3
  FSPI_D1_M0    2  DO/IO1    HOLD/IO3 7  FSPI_D3_M0
  FSPI_D2_M0    3  WP/IO2    CLK      6  FSPI_CLK_M0
  FSPI_D0_M0    4  DI/IO0    GND      9
  标注：兼容 WSON8_8x6、uDFN8-5x6 两种封装 1.8V 的 NOR/NAND Flash
  备注：When using SPI FLASH with only 1 bit, it needs to be stuffed.（R36/R118 = NC 10K）
  历史版本 V0.2（2024-05-24）："2.添加SPI Flash"
  ```

  ⚠️ 但它是接在 **FSPI_*_M0** 上，而同板载 eMMC 是三星 `KLMBG2JETD-B041`，走的网名是 `EMMC_D0…`/`EMMC_CMD` ——
  **同一组 SoC 引脚**（`FSPI_D0_M0 / EMMC_D0 / GPIO2_D0_u`）⇒ **贴了 eMMC 就用不了 SPI flash，二者互斥**。
  这一点在内核 pinctrl 里也能对上：`emmc_bus8` 是 `<2 RK_PD0 1>`…（= GPIO2_D0 的 mux 1，
  `rk3588s-pinctrl.dtsi:334`），而 FSPI_M0 是同一批 GPIO2_D0~D7/A0 的**另一个 mux 值**
  ⇒ 同一个焊盘位置不可能同时给 eMMC 和 SPI flash 用。
  也就是说这块核心板的 SPI flash 焊盘是"替代 eMMC"的选项，不是额外扩一颗；焊盘是否真的可贴（BOM/钢网）
  建议向厂商确认（本结论来自原理图 + 引脚复用表，未实测贴片）。

### 3.2 idbloader.img（DDR 初始化 + SPL）

`idbloader.img`（**305,152 B**，@32 KB / 扇区 64）= **RKNS 头**（2 KB）+ **DDR 初始化固件**（75,320 B）
+ **U-Boot SPL**；由 `tools/mkimage -n rk3588 -T rksd -d <ddr>:<spl>` 打包，尾部零填充 ⇒ 容器大小固定。
BootROM 读它 → DDR 固件初始化/训练 DDR → SPL 再读 `u-boot.itb`。

⚠️ 那份 DDR 固件**不是 M0 代码**（是跑在 A 核 PMU SRAM `0xff001000` 的 AArch64 固件）；M0 的固件在
`u-boot.itb` 的 `atf-3` 里。**RKNS 头字段、DDR 固件的逆向结果、以及这两块产物的成对关系 / 刷写偏移 /
核对清单**全部收在 → **[bootloader-blobs.md](bootloader-blobs.md)**（§2 / §3 / §5）。

### 3.3 `u-boot.itb`（FIT：ATF + U-Boot + DTB）→ 见单独一篇

FIT 的节点构成（`atf-1`=BL31 / `atf-2` / `atf-3` / `uboot` / `fdt`）、external-data 布局、
SPL 怎么按 `data-offset` 直读并校验，全部搬到 → **[bootloader-blobs.md](bootloader-blobs.md)** §4。
校验机制本身（能证明什么、为什么不是防篡改）见 [boot-integrity.md](boot-integrity.md)。


### 3.4 U-Boot proper 到内核

U-Boot 启动脚本 `boot.scr`（由 `boot.cmd` 经 `mkimage` 编译）的主要工作：

1. 从启动分区加载 `ubuntuEnv.txt`，导入 `bootargs`、`fdtfile`、`overlays`
2. 依据 `fdtfile` 加载 `/dtbs/rockchip/<fdtfile>`，并按 `overlays` 逐个 `fdt apply` 设备树 overlay
   （若任一 overlay 失败则回退到原始 DTB 并提示）
3. 加载 `/vmlinuz` 与 `/initrd.img`
4. `booti <kernel> <initrd>:<size> <fdt>` 跳转内核

此后由 initramfs（含必要驱动与固件）挂载 `LABEL=writable` 根分区，切换到 systemd，
再由 display-manager（gdm3 / lightdm / sddm）拉起桌面。

> 📄 一次**完整正常启动**的串口原文（上电 → `Starting kernel ...`）与逐段注解见
> [`boot-serial-log.md`](boot-serial-log.md)，里面也标了哪些"报错"其实正常。

### 3.5 固件（firmware）的加载路径

定制镜像**不安装** `linux-firmware` 包，改为构建时解压 `prebuilt/firmware.tgz` 到 `/lib/firmware`，
只保留板卡实际用得到的固件（`rtw88/` WiFi、`rtl_bt/` 蓝牙、`rockchip/dptx.bin`、`arm/mali/` GPU）。
**进 initrd 的固件不用任何自定义 hook**（2026-09-26 更正）：`mkinitramfs` 的
`add_builtin_firmware` 会读 `modules.builtin.modinfo`，把**内建（=y）驱动**声明的固件自动收进去
（panthor → `arm/mali/arch10.8/mali_csffw.bin`，cdn-dp → `rockchip/dptx.bin`，cfg80211 →
`regulatory.db`）；可加载模块的固件由 `dracut-install --firmwaredirs` 带。蓝牙驱动 `btrtl` 是模块
且不在 initrd 的模块集里 ⇒ BT 固件只在切到 rootfs 之后加载。原先的 `hooks/mali-firmware` 与
`hooks/rtl-bt` 用错了 `copy_exec` 的目标语义（`rtl_bt` 落成了一个 18344 B 的**文件**），已删；
详见 [`../system/initramfs.md`](../system/initramfs.md) §3.3/§4。

> Ubuntu 24.04 为 usrmerge，`/lib/firmware` 即 `/usr/lib/firmware`，两者等价。

### 3.6 根文件系统 `/boot` 里的文件是谁生成的

`/boot/firmware` 里各文件的来源（内核 deb 的 postinst 钩子、`boot.scr`/`boot.cmd`、`ubuntuEnv.txt`、
dtbs/dtbo）**已搬到配套速查篇** → **[rk3588-boot-flow-files.md](rk3588-boot-flow-files.md)** §3.6。


### 3.7 全链路总表：谁跑在哪个 EL、是否常驻、各干什么（2026-09-18 补）

```text
BootROM(EL3, 退出即走) → ② DDR 初始化固件(SRAM) → ③ U-Boot SPL(=BL2) → ④ BL31(EL3, 常驻)
                                                                       ├─(可选) ⑤ BL32/OP-TEE(S-EL1, 常驻)
                                                                       └→ ⑥ U-Boot proper(=BL33, EL2) → ⑦ 内核(EL1)
运行时：U-Boot/内核 --smc--> ④(PSCI/SIP)；内核 optee 驱动 --smc--> ⑤(若有)
```

| # | 实体 | 地址 / 运行在 | 常驻？ | 干什么 | 我们的证据 |
| --- | --- | --- | --- | --- | --- |
| ① | **BootROM**（SoC 掩膜 ROM） | EL3 | ❌ 跳走即退场 | 从启动介质读 `idbloader` 到 SRAM、按其头部字段做校验、跳进去 | 不可见（没有 ROM 二进制）；头部 `RKNS` 魔数 + `boot_flag=1`(sha256) 是我们直接读 `idbloader.img` 核对的 |
| ② | **DDR 初始化固件**（`rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin`） | 先 SRAM | ❌ | 初始化/训练 DRAM | 日志开头 `DDR 9fa84341ce typ … fwver: v1.18` … `out`（4 通道 × 2048MB、1848 MHz） |
| ③ | **U-Boot SPL**（TF-A 语境里的 **BL2**） | 先 SRAM，后 DRAM | ❌ | 解析 FIT、**逐节点现场重算 sha256**、把 BL31/U-Boot/DTB 装载到位、跳 BL31 | `U-Boot SPL board init`、`Trying fit image at 0x4000 sector`、`## Checking atf-1 … + OK`、`Jumping to U-Boot(0x00200000) via ARM Trusted Firmware(0x00040000)` |
| ④ | **BL31**（ARM TF-A） | `0x40000`（另两段 `0xf0000`/`0xff100000`）· **EL3** | ✅ **直到断电** | 安全监控态运行时：EL3 异常向量、**GICv3**、EHF、runtime services；**PSCI**（CPU 上电/热插拔、cpuidle/suspend、`reboot`/`poweroff`）；**厂商 SIP**（DDR 变频、VPU reset…）；初始化 ⑤（若在）后把控制权交给 normal world | `NOTICE: BL31: … fwver: v1.53`、`GICv3 … initialized in EL3`、`dfs DDR fsp_params[0..3]`、`Preparing for EL3 exit to normal world`、`Entry point address = 0x200000`、`SPSR = 0x3c9` |
| ⑤ | **BL32**（OP-TEE OS） | 默认 `TEE_LOAD_ADDR = 0x8400000` · **S-EL1** | ✅（存在时） | **安全世界的操作系统/TEE**：安全存储（RPMB）、密钥与加解密、DRM（Widevine…）、secure video path；由内核 `optee` 驱动经 SMC 调用 | **我们没有**：`WARNING: No OPTEE provided by BL2 boot loader …` + 内核 `optee: api uid mismatch` → `probe of firmware:optee failed with error -22` |
| ⑥ | **U-Boot proper**（**BL33**） | `0x200000` · **EL2** | ❌ | distro boot：读 `boot.scr` + `ubuntuEnv.txt`、装载内核/initrd/DTB、`booti` | `Entry point address = 0x200000`；`SPSR = 0x3c9` 的低 5 位 = `0b01001`（AArch64 + **EL2h**） |
| ⑦ | **内核** | `0x400000` 起 · EL1（开 KVM 才在 EL2） | — | 正常世界 | `Adding bank: 0x00200000 - 0xf0000000`；内核 `psci: PSCIv1.1 detected in firmware` = ④ 在提供 PSCI |

**两处常见误解（值得记住）**：

1. ⛔ **BL31 不会"交棒"给 BL32**。SPL 把控制权给了 ④ 之后，BL31 **一直留在 EL3**；如果 FIT 里有 `optee`
   节点，它只是**额外**把 ⑤ 初始化到 S-EL1，然后**自己继续**走 `Preparing for EL3 exit` 去 normal world。
   ④⑤ 是**并行的常驻服务**（内核/U-Boot 用 `smc` 按需调用），不是流水线上前后两站。
2. ⛔ **日志里的 "BL2" 指的是 ③ U-Boot SPL**，不是 TF-A 的 BL2 —— Rockchip 这套里 TF-A 只提供
   BL31（+ 可选 BL32），BL2/BL33 的角色由 **U-Boot SPL / U-Boot proper** 扮演。
   （所以 `No OPTEE provided by BL2 boot loader` = "SPL 没给 BL31 传 BL32"。）

**BL32（OP-TEE）我们为什么故意不带**（细节见 [../build/upstream-sync.md](../build/upstream-sync.md) §2）：

- 它是**安全世界（S-EL1）的独立 OS**，给 normal world 提供 TEE 服务；通道是
  内核 `optee` 驱动（DT `/firmware/optee`，`method = "smc"`）→ SMC → BL31 的 `opteed` → BL32。
- 代价：要在 DRAM 里留一段（默认 `0x8400000`）、启动链多一环、还要用户态配套
  （`optee-client` / `tee-supplicant`）。**我们的镜像和业务都不需要 TEE**，所以不带。
- 真要上：加 `_bl32`/`TEE=` **并且** backport 3 个 FIT 脚本改动（我们这版 u-boot 的
  `make_fit_atf.sh` 里 `gen_bl32_node` 是**注释掉的**）—— 见 upstream-sync.md §2。
- 现状 `optee` 驱动 probe 失败（`-22`）属**预期且无害**；BL31 的 PSCI/SIP 一切照旧。
- 📄 **要开 BL32 时怎么做**（一条命令的开关、产物验证判据、不碰 eMMC 的 SD 卡上板验证法、回滚、8 条坑）：
  见 [bl32-optee.md](bl32-optee.md)。**2026-09-18 已在 v2 板上实测通过**
  （`/dev/tee0` 出现、`optee: revision 3.13 … initialized driver`、`MemTotal` −16 MB）。

### 3.8 关机 / 重启走哪条路

`reboot: Power down` → PSCI `SYSTEM_OFF` → BL31 写 IOC/GPIO0 的完整推导（反汇编 + 真机核实）
**已搬到单独一篇** → **[shutdown-reboot-path.md](shutdown-reboot-path.md)**；
电源键那半边见 [rk806-pwron.md](rk806-pwron.md)。


## 4.~7. 文件对照表 / 镜像组装 / 验证命令 / 实测记录 → 见配套速查篇

关键文件对照表、本仓库镜像的六个具名阶段组装、验证命令速查、历史实测记录，都在
**[rk3588-boot-flow-files.md](rk3588-boot-flow-files.md)**。
