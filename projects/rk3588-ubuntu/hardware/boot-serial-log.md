# LubanCat-5 v2 上电 → `Starting kernel ...` 完整串口日志（实测存档）

本文只做一件事：**把一次正常启动的完整串口输出原样存下来**，并逐段注明"这行是谁打的、正常不正常"。
启动链路的原理见 [`rk3588-boot-flow.md`](rk3588-boot-flow.md)；`/boot` 里各文件的来源见 [rk3588-boot-flow-files.md](rk3588-boot-flow-files.md) §3.6。

- 采集对象：**开发板**（`board@<HOST_07>`，Ubuntu 24.04，LubanCat-5 v2，8 GiB）——⚠️ **采集时板子上是 `6.1.118` 那版镜像**（现在是 `6.1.172`，同一 BSP 小步）
- 介质：**eMMC**（`/dev/mmcblk0`）；**SD 槽空**（有卡时会先尝试 SD）、**无 NVMe**
- 时间点：由 `reading /initrd.img 18932239 bytes` 判定，这是 **2026-09-16 15:11 那次
  `update-initramfs -u` 之前**的启动（重建后的 initrd 是 18,933,145 B）
- 串口：`ttyS2, 1500000`（`PreSerial: 2`）

## TL;DR

- 本文是**一次正常启动的完整串口输出存档 + 逐段注解**，用来对照自己的板子"哪里不一样、哪些报错其实正常"。
- 关键判据：`Verified-boot: 0` + 5 行 `Checking atf-*/uboot/fdt … + OK` ⇒ U-Boot 加载的 payload
  与 FIT 内声明的 sha256 一致（原理见 [boot-integrity.md](boot-integrity.md)）。
- 逐段注解在 §2：含"BL31-only 时 `No OPTEE provided by BL2` 那两行是**预期**"这类正常现象。
- 想进 U-Boot 控制台 / 抓日志的参数与操作见文末（含 `Ctrl+C` 的注意事项）。

## 1. 原始日志 → 见 [boot-serial-log-raw.md](boot-serial-log-raw.md)

正常启动的完整串口日志（约 240 行，含 BootROM/SPL/U-Boot/内核各段原文）**已单独存成一篇**，
本文只留**逐段注解与可核对的数字** → **[boot-serial-log-raw.md](boot-serial-log-raw.md)**。
带 BL32（OP-TEE）那份日志也在那一篇里。


## 2. 逐段注解

| 段 | 行 | 谁打的 / 含义 |
| --- | --- | --- |
| DDR 训练 | `DDR 9fa84341ce typ 25/02/06-17:24.09,fwver: v1.18` … `out` | **DDR 初始化固件**（`rkbin/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin`，二进制 blob，不在源码里）。四通道 × `Size=2048MB` = 8 GiB；`Manufacturer ID:0x13`（LPDDR4X）；四档频率 528/1068/1560/1848 MHz 各做一遍 `ttot` 训练 |
| SPL | `U-Boot SPL board init` … `Total: 110.996 ms` | SPL 阶段。**先试 SD（MMC2）→ 无卡 → 再试 eMMC（MMC1）**；`spl: partition error` 后回退到"裸偏移找 FIT"，从**扇区 0x4000 = 8 MiB** 读到 `u-boot.itb` 并逐节点校验 sha256（`## Verified-boot: 0` = 未做签名校验，只做哈希） |
| BL31 | `NOTICE: BL31: v2.3():v2.3-831-…fwver: v1.47` … `SPSR = 0x3c9` | ATF（**当时是** `rk3588_bl31_v1.47.elf`，编译于 2024-09-04，厂商提供的二进制）。⚠️ 2026-09-18 已把 `rkbin/` 里的 BL31 升到 **v1.53**（`v2.3-942-g98eaeb2f3`，见 [../build/upstream-sync.md](../build/upstream-sync.md)）⇒ **重新刷机后这一行会变成 `fwver: v1.53`**；本节其余日志内容是升级前的存档。`WARNING: No OPTEE …` + `ERROR: … opteed_fast` = **没有 OP-TEE 是正常的**（我们没上 BL32），不影响启动 |
| U-Boot proper | `U-Boot 2017.09 (Sep 12 2026 - 04:29:11 +0000)` … `Net: No ethernet found.` | U-Boot 本体。`Model: Embedfire LubanCat-5` 来自 **U-Boot 自己的 DTB**（去品牌化没改 u-boot 的设备树，仍是厂商 model 串）。`PartType: EFI` = GPT；`MMC0: HS200, 200Mhz`；`No misc partition` ×2 = 没有 Android 的 misc 分区 |
| U-Boot 的 Android 启动路径（**必然失败**） | `FIT: No boot partition` / `No resource partition` ×2 / `Failed to load DTB, ret=-19` / `No find valid DTB, ret=-22` / `Failed to get kernel dtb, ret=-22` | Rockchip U-Boot 先走 Android/`resource.img` 那一套：按名字找 `boot`/`recovery` 分区（`arch/arm/mach-rockchip/fit.c:155`）、找 `resource` 分区（`resource_img.c:393`）、从 resource 里取内核 DTB（`boot_rkimg.c:529`、`kernel_dtb.c:356`）。**我们的 GPT 里没有这些分区 ⇒ 全部打印失败，然后才落到 distro boot。看着吓人，属正常** |
| 时钟表 | `CLK: (sync kernel. …)` … `aclk_low_top_root 396000 KHz` | U-Boot 打印各 PLL/根时钟当前频率。`Net: No ethernet found.` = U-Boot 里没有可用网口（本板 GMAC 在 U-Boot 阶段未启用/无 PHY 驱动） |
| PCIe 探测 | `pcie@fe150000: PCIe Linking... LTSSM is 0x0` ×50 + `PCIe-0 Link Fail` | **没插 NVMe**：M.2 槽上链路训练不成功，U-Boot 每轮打一行（`drivers/pci/pcie_dw_rockchip.c:584`），最后 `Link Fail`。插上 NVMe 时这里会走到 L0 并识别出设备（此时 `Device 0` 会变成那块盘）。**这 50 行是纯噪音**，也是 `Total: 3627.810 ms` 里的大头 |
| distro boot | `Device 0: unknown device` / `MMC: no card present` … `Scanning mmc 0:1...` / `Found U-Boot script /boot.scr` | 依次试 NVMe（上一步 Link Fail → `unknown device`）、SD（无卡）、最后 eMMC；**扫描 `mmc 0:1` = GPT 第 1 个分区 = FAT `system-boot`**，找到 `boot.scr` |
| 加载内核 | `reading /ubuntuEnv.txt` … `Applying device tree overlay: …` ×3 … `reading /vmlinuz` / `reading /initrd.img` | 即 `boot.scr` 的四步（见 `rk3588-boot-flow.md` §3.4）：导入 `ubuntuEnv.txt`（`bootargs` 那行含 `usbcore.autosuspend=-1`）→ 载入并套 3 个 overlay（`overlays=` 那行恰好 3 个，板上 `ubuntuEnv.txt:113`）→ 载 `vmlinuz` → 载 `initrd.img` |
| 交接内核 | `Fdt Ramdisk skip relocation` … `Adding bank: …` ×3 / `Total: 3627.810 ms` / `Starting kernel ...` | U-Boot 的最后动作：DTB 就地放在 `0x08300000`（`fdt_addr_r`）、把 CMA（256 MiB @ `0x10000000`）与 ramoops 写进 DT 的 `reserved-memory`、把 3 段内存 bank 写进 DT，然后 `booti` 跳转 |

### 三段内存 bank 的由来（`Adding bank`）

```
0x00200000 - 0xf0000000   3838 MiB     低 4 GiB 区（去掉开头 2 MiB 保留 + 尾部 MMIO 洞）
0x100000000 - 0x200000000 4096 MiB     4–8 GiB
0x2f0000000 - 0x300000000  256 MiB     0x2f0000000（≈11.75 GiB）处
```

板上实测（内核侧），三段**完全一致**，不是 U-Boot 的垃圾项：

```bash
$ od -An -tx4 -N48 /proc/device-tree/memory/reg   # 3 段 × (base 2 cell + size 2 cell)，大端
 00000000 00002000 00000000 0000e0ef      # base 0x00000000_00200000  size 0x00000000_efe00000
 01000000 00000000 01000000 00000000      # base 0x00000001_00000000  size 0x00000001_00000000
 02000000 000000f0 00000000 00000010      # base 0x00000002_f0000000  size 0x00000000_10000000
$ grep MemTotal /proc/meminfo
MemTotal:        8121352 kB
$ dmesg | grep "Memory:"
[    4.360129] Memory: 7834960K/8386560K available (…, 262144K cma-reserved)
```

3838 + 4096 + 256 = **8190 MiB** = 8 GiB − 2 MiB，与内核 `…/8386560K`（= 8190 MiB）完全吻合。
第三段在正常地址空间之外的合理解释是：8 GiB 里有 256 MiB 落在 `0xf0000000-0x100000000` 这段
**MMIO 区**（RK3588 外设就在这一带），被占掉的那 256 MiB 挪到 `0x2f0000000`。
（**这一步是推断**：真正做重映射的是 SPL/DDR 固件里的 param，没有去读它的源码。）

## 3. 可用于核对的固定数字

| 项 | 值 |
| --- | --- |
| SPL 校验的 5 个哈希 —— **本文那份日志**（2026-09-2x） | `atf-1 6a4a192c10…` / `uboot 0d7dbcfbf3…` / `fdt 31c141a5e8…` / `atf-2 70505bb764…` / `atf-3 569ee96047…` |
| 同样 5 个哈希 —— **当前 `prebuilt/u-boot.itb`**（2026-10-02 重建，2026-10-03 实测） | `atf-1 af59234162…` / `uboot 5b24f58f8e…` / `fdt 31c141a5e8…` / `atf-2 488c757001…` / `atf-3 b621eea76c…` |
| ⚠️ 这些值**每次重建 `u-boot.itb` 都会变**（`fdt` 只要控制 DTB 没改就稳定）⇒ 对不上先看 §4，别急着判定"板子被改过" | |
| SPL 阶段耗时 | 110.996 ms |
| U-Boot 阶段耗时 | 3627.810 ms（大头是 50 次 PCIe 轮询） |
| `boot.scr` / `ubuntuEnv.txt` | 1677 B / 4879 B |
| `vmlinuz` / `initrd.img`（启动分区） | 42,109,440 B / 18,932,239 B（重建前） |
| DTB | `rk3588-lubancat-5-v2.dtb`，334,672 B，加载地址 `0x08300000` |
| overlay（3 个） | `rk3588-lubancat-5-hdmi0-overlay`(1585 B) / `rk3588-lubancat-5-hdmi1-vp2-overlay`(1585 B) / `rk3588-lubancat-5-v2-usbcon-host`(234 B) |
| 内存 bank | 3 段，合计 8190 MiB |

## 4. ⚠️ 顺带发现：板上这份 U-Boot 与仓库 `prebuilt/u-boot.itb` **不是同一次构建**

| | 板上（本次日志） | 仓库 `prebuilt/u-boot.itb` |
| --- | --- | --- |
| U-Boot banner | `U-Boot 2017.09 (Sep 12 2026 - 04:29:11 +0000)` | `U-Boot 2017.09 (Jan 19 2026 - 01:00:47 +0000)` |
| FIT 里 `uboot` 节点 sha256 | `0d7dbcfbf3…` | `715d6ea2acde57724485b627ac262165438875c7d58c7c6ec42103fcb24fc04a`（`dumpimage -l prebuilt/u-boot.itb`） |
| `atf-1/2/3`、`fdt` 的 sha256 | 与仓库**逐位一致** | —— |

即：**ATF 三段和 U-Boot 的 DTB 相同（可复现），只有 U-Boot proper 不同** —— 而 U-Boot 二进制内嵌
`__DATE__/__TIME__`，天然不可复现，所以"哈希不同"至少说明**编译时间不同**；是不是源码/配置也有差异，
只看哈希无法断定。`packages/u-boot-lubancat-rk3588/debian/rules:39` 有一个 target 会显式
`SOURCE_DATE_EPOCH=$(shell date +%s)`，所以走不同构建路径时 banner 时间会不一样。

> ✅ **2026-09-16 后续查清（`tools/check-atf.sh` 增强后用 `sudo ./tools/check-atf.sh -d /dev/mmcblk0` 实测）**：
> 设备上那份 U-Boot **与手里这套产物（`build/u-boot-*.deb` → `build/rootfs` → `images/*.img` 8MB 处）
> 逐段一致**（FIT 区段 `5912ac74a479370e…`、`uboot` 节点 `0d7dbcfbf3551370…`），
> 只有 `prebuilt/u-boot.itb` 是**另一次构建**（FIT 区段 `f8eb76fb45ce52e8…`、`uboot` 节点 `715d6ea2acde5772…`）。
> 也就是说：设备不是用当前 `prebuilt/` 那份刷的，而是用它对应的那版 deb/镜像刷的；ATF-1/2/3 与 DTB 三处
> 全都一致。⇒ 工具现在会直接把这几列的 sha256 打出来对比，不需要再靠 banner 猜（见
> [atf-fit-and-image-validation.md](atf-fit-and-image-validation.md)）。

> 补充：日志里 `## Checking … sha256(…) + OK` 打印的**是 SPL 现场算出来的哈希**（不是 FIT 里记的
> 期望值，见 `boot-integrity.md`）⇒ 这 5 个哈希直接反映**板上这份 payload 的实际内容**，
> 所以"uboot 节点不同"是板上固件本身不同，不是比对方法的问题。

> 🔁 **2026-10-03 复核（对着现在的 `prebuilt/u-boot.itb`）**：它 2026-10-02 重建，1,379,328 B，
> 五个节点是 `atf-1 af59234162…` / `uboot 5b24f58f8e…` / `fdt 31c141a5e8…` /
> `atf-2 488c757001…` / `atf-3 b621eea76c…`（用本文 §3 的自推导方法复算，5/5 与 `dumpimage -l` 一致）。
> ⚠️ 有意思的是：**`atf-1/2/3` 与 `fdt` 跟本文 §6 那份 BL32 日志（2026-09-18）完全相同**，
> 变的只有 `uboot`（U-Boot proper 重建）⇒ 说明 BL31 v1.53 与那几个 ATF 段一直没动。
> 板上 **2026-10-02 已重刷镜像**，所以再抓一份串口日志就能与这份 prebuilt 直接对 ——
> ⚠️ 本机没有 USB 串口（`/dev/ttyUSB*` 不存在），**这次没重抓**，以上是"仓库侧"的实测。

**对以后开发的意义**：怀疑"板子上的 U-Boot 行为和仓库不一致"时，先核这条 banner 与这 5 个 sha256；
要完全对齐板上 bootloader 与仓库，只能重刷 bootloader 区（**本次未做任何重刷**）。
判据命令：

```bash
# 仓库这份的 banner 与各节点哈希
strings prebuilt/u-boot.itb | grep "U-Boot 2017.09"
dumpimage -l prebuilt/u-boot.itb | grep -A2 "^ Image"

# 板上跑的那份（banner 在启动日志里；也可从 eMMC 读出来比对）
#   u-boot.itb 写在头部 8 MiB = 扇区 16384，长度别写死（每次重建都变）：
#   2026-10-02 那版 = 1379328 B = 2694 扇区
SIZE=$(stat -c%s prebuilt/u-boot.itb)
sudo dd if=/dev/mmcblk0 of=/tmp/uboot-onboard.itb bs=512 skip=16384 count=$((SIZE/512))
cmp -l /tmp/uboot-onboard.itb prebuilt/u-boot.itb | head
```

## 5. 怎么自己抓这份日志

- 串口参数：**1500000 8N1**，接板子的调试串口（`PreSerial: 2`，即 `ttyS2`）。

### 5.1 从开发机连接和抓取

开发机 `/dev/ttyUSB0`，1500000 8N1；账号在 `dialout` 组时不用 sudo。
交互连接用 `tio -b 1500000 /dev/ttyUSB0`（Ctrl-T q 退出）。
命令自动化与后台抓取使用 [serial-exec.md](serial-exec.md)，两者不要同时占用串口。

```bash
tools/serial-exec run -c 'systemd-analyze time' --json
tools/serial-exec capture start --output /tmp/boot-record --duration 90 --json
tools/serial-exec capture status --json
tools/serial-exec capture stop --json
```

`tio -r` 的一次性响应模式在本机实测会把命令回显当成响应；不能用它判断命令完成。

### 5.2 历史热重启实测：复位 → Starting kernel 4.385 s

做法：`tio -t --log --log-file …`（带主机时间戳）先连着串口，再 `systemctl reboot`，抓"复位 →
`Starting kernel`"那一段。原始日志留档在 `.scratch-boot/serial-20260929-1041-poweron.log`（170 KB，
gitignore）。主机时间戳量得 **4.385 s**；U-Boot 的 `Total: 3979.580 ms` 少了约 **0.405 s**，
不能代替完整复位前缀。`Total` 的起点是 `gd->sys_start_tick`，见
`u-boot/arch/arm/lib/bootm.c` 的 `boot_jump_linux()`。当前实测见 [boot-time.md](../system/boot-time.md)。

| 段 | 主机时间戳 | 相对复位 | 说明 |
| --- | --- | --- | --- |
| `reboot: Restarting system` | 10:41:50.911 | 0 | 内核最后一行 |
| DDR 初始化（DDR fw `v1.18`）开始打印 | 10:41:50.932 | +21 ms | BootROM 到这里没输出（串口静默） |
| `U-Boot SPL board init` | 10:41:51.168 | +257 ms | |
| SPL 自报 `Total: 110.296 ms` ⇒ 跳 ATF | 10:41:51.279 | +368 ms | SPL 自己量的 110 ms |
| `NOTICE: BL31: … fwver: v1.53` | 10:41:51.300 | +389 ms | BL31 = ATF |
| `U-Boot 2017.09 …`（BL33） | 10:41:51.374 | +463 ms | |
| `Hit key to stop autoboot('CTRL+C'):  0` | 10:41:51.514 | +603 ms | 零等待（`bootdelay=0`）✓ |
| PCIe 探测失败刷屏 → `PCIe-0 Link Fail` | 10:41:51.747 → 52.264 | +836 → +1353 ms | **白等 517 ms** |
| `Found U-Boot script /boot.scr` → 读 dtb | 10:41:52.304 → 52.326 | +1393 ms | 32 ms |
| **5 个 dtbo overlay**（hdmi0-vp0 / dp0-vp1 / hdmi1-vp2 / bq40z50 / usbcon-host） | 10:41:52.579 → 54.724 | +1668 → +3813 ms | **2.15 s**（每个 ~0.43 s，文件只有 234~1676 B！） |
| 读 kernel 42.4 MB + initrd 18.7 MB | 10:41:55.113 / 55.229 | +4202 / +4318 ms | 257 ms + 116 ms（~157 MB/s） |
| U-Boot `Total: 3979.580 ms` + `Starting kernel ...` | 10:41:55.296 | +4385 ms | |
| 内核第一行（printk 时间戳 `[4.026885]`） | 10:41:55.330 | +4419 ms | 内核入口 34 ms |

⇒ **U-Boot 这 3.98 s 里，两个大头是「5 个 dtbo overlay 2.15 s」和「PCIe 探测 0.52 s」**（合计 2.7 s）；
真正的数据搬运（42 MB 内核 + 18.7 MB initrd）只占 0.37 s。⏳ **可优化**（都还没做）：
把 5 个 overlay 在**构建期合并进一份 dtb/dtbo**（`fdtoverlay` 或构建时 `fdt apply`）⇒ 预计省 ~2 s；
U-Boot 里对没有设备的 PCIe 控制器（`pcie@fe150000`，M.2 空着）**别做 LTSSM 重试** ⇒ 省 ~0.5 s。

### 5.3 2026-09-29 下午：叠加层改「构建期合并」后的实测（省 2.5 s）

用户口径："我们目前 dtbo 只用到了 **hdmi0** 跟 **usb-con 那个 `try-power-role="source"` 修复**"
⇒ ① 基础 `overlays=` 从 3 条减到 1 条（去掉 DP0-vp1、HDMI1-vp2）；② 自定义只用 `usbcon-host`
（`bq40z50` 是 5io 专用的，不再加到 v2 上）；③ 剩下的按上面 §5.2 的思路**在构建期合进基 dtb**。

| 配置 | 复位 → `Starting kernel` | U-Boot 自报 `Total:` |
| --- | --- | --- |
| 改前：5 个 overlay 逐个 `fdt apply` | **4.385 s** | 3979.58 ms |
| 只合并、但 `overlays=` 里还留着 hdmi0（被 apply 了第二遍） | 2.284 s | 1878.64 ms |
| **最终：合并成一份 dtb + `overlays=` 清空** | **1.860 s** | **1454.85 ms** |

佐证：内核第一条打印的时间戳从 `[ 4.016733]` 变成 **`[ 1.481118]`**（那个偏移就是"上电→内核"）；
`systemd-analyze` 这次是 `3.391s (kernel) + 6.315s (userspace) = 9.706s`（改前 10.712 s）。
画面/上位机同一口径下也早了 ~2.5 s：`uvicorn` 监听 8080 在 6.52 s、`cage` 7.61 s。

实现（三处，都是 2026-09-29 加的）：

- `overlay/usr/local/sbin/lsp-merge-dtb`：按 `<启动分区>/lsp-dtb-merge.conf` 把 overlay 合进基 dtb
  （`fdtoverlay`；镜像/宿主机都要有 `device-tree-compiler`，没有就**退回**老的 `overlays=` 写法）；
- `scripts/build-lsp-image.sh`：`image` 阶段写清单 + 调上面那个脚本 + 把 `fdtfile=` 指到
  `<基名>-lsp-merged.dtb`、不写 `overlays=`；还有构建期判据（合并件里必须有 `sbs,sbs-battery` /
  `try-power-role = "source"`，缺了就报错）；
- `overlay/etc/initramfs/post-update.d/zz-update-firmware`：内核/initramfs 升级先暂存完整启动文件，
  重建合并件并校验 `fdtfile` 后发布；准备失败保留旧文件，发布失败回滚。
  失败行为与回归判据见 [DTB 更新](../system/dtb-update.md)。
- 另外：自定义 overlay 改成**按板卡清单**追加（`overlay/.../overlay/<board>.overlays`），
  不再"目录里有啥就全加到所有板"——5io 的电量计 overlay 因此不再上 v2（v2 上那条
  `sbs-battery: probe of 7-000b failed` 也没了）。

顺带记录两条这次看到的"正常现象"，别当故障：
- 暖重启时 SPL 先试 `MMC2`（SD 槽）→ `MMC: no card present`（`mmc_init: -123`），再试 `MMC1`（eMMC）
  → `spl: partition error`，最后走 **`Trying fit image at 0x4000 sector`** 成功 ✓（SPL 的分区表读不到
  就按固定偏移找 FIT，这是它的回退链）。
- BL31 那两行 `WARNING: No OPTEE provided by BL2 boot loader` / `ERROR: Error initializing runtime
  service opteed_fast` 是**预期**的（我们 BL31-only，不做 OP-TEE，见 `AGENTS.md` §6）。
- 内核起来之后也可以用 `journalctl -b -k` 看内核早期日志，但 **SPL/BL31/U-Boot 那几段只有串口才有**
  （`dmesg` 从内核开始）。
- 想连 U-Boot 命令行（**2026-09-24 起**）：`CONFIG_BOOTDELAY` 已由 2 改成 **0** ⇒ 默认**零等待**启动，
  串口那行 `Hit key to stop autoboot('CTRL+C'):  0` 只是提示。
  **按住 Ctrl+C 现在也可以了（2026-10-02 起）**：厂商默认 `bootcmd` 里的
  `boot_android/boot_fit/bootrkp` 回退链已删掉，只留 `run distro_bootcmd;`
  （改的是 `u-boot/include/configs/rockchip-common.h` 的 `RKIMG_BOOTCOMMAND`，
  见 [U-Boot 变更记录](../build/uboot-changelog.md) §1 与 `AGENTS.md` §6.3）；
  板上串口实测（用户 2026-10-02）：按住 Ctrl+C → 正常进 `=>`，不再卡死 ✓。
  ⚠️ 旧固件（2026-10-02 之前）上按住会**中止 `boot.scr`**（hush 的 `had_ctrlc()` 会打断脚本），
  `run distro_bootcmd` 失败后 `bootcmd` 继续掉进 Android 回退链，串口刷
  `Could not find misc partition` / `optee api revision is too low` / `### ERROR ### RESET the board`
  （我们 BL31-only，没有 OP-TEE）；那种机器上**轻按一下**就能稳稳停在 `=>`。
  依据与取舍（为什么不改成 -2、为什么运行期改不了）见 `AGENTS.md` §6「U-Boot 自动启动等待已去掉」。
  ⚠️ 下面这份日志是 2026-09-24 之前抓的（那时 bootdelay=2，所以还带倒计时）；新固件下 U-Boot 段会短 ~2 s。

---

## 6. 带 BL32（OP-TEE）的启动日志

那份日志（2026-09-18，同一块 v2 板，多一个 `optee` 节点）**同样存在原始日志篇里** →
[boot-serial-log-raw.md](boot-serial-log-raw.md)；BL32 怎么开、串口里哪些行是预期的见
[bl32-optee.md](bl32-optee.md)。
