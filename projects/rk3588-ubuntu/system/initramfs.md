# initramfs（`initrd.img`）：里面有什么、怎么生成、谁搬到启动分区

> 数据来源：2026-09-16 在开发板 `lubancat-5-v2`（Ubuntu 24.04、内核 6.1.118）上
> `lsinitramfs /boot/firmware/initrd.img` 实测；**2026-09-26 在工程机（内核 6.1.172、
> initramfs-tools 0.142ubuntu25.8）上复查，修掉了两个固件 hook 并删掉一份冗余的 `scripts/functions`（§3.4、§4）**。
> 仓库侧现在对应 **5 个文件**（原来 9 个，删了 4 个：见 §3.4）。
> 相关：[DTB 合并与启动文件更新](dtb-update.md)（准备失败保留旧件、发布失败回滚、自定义 overlay 保留）。

---

## TL;DR

- 里面有什么：内核模块（184 个）+ 固件（10 条）+ 我们的 **4 个 hook**；生成链条与时间线见 §2/§3。
- 谁生成、谁搬：`initramfs-tools` 在**内核包安装后**生成，`/etc/kernel/postinst.d` 钩子把它搬到启动分区。
- 2026-09-26 复查（工程机）：修掉两个固件 hook、删掉一份冗余的 `scripts/functions`（§3.4/§4）。
- ⛔ 别手改 `/boot/firmware/initrd.img` —— 下次内核/initramfs 更新会覆盖。
- 重建与查看命令见 §4；复现本文所有结论的命令见 §5。

## 1. 一句话

`/boot/firmware/initrd.img` 不是手工做的：**内核包安装时 `update-initramfs` 自动生成**
（内容 = 发行版标准件 + `MODULES=most` 挑出来的 184 个模块 + 我们 hooks 塞进去的东西），
生成后由 `/etc/initramfs/post-update.d/zz-update-firmware` **暂存、校验并发布到启动分区**（当前镜像为 ext4），
U-Boot 再用 `boot.scr` 把它和 `vmlinuz` 一起加载进内存。

板上现役那份的实测画像：

```
$ ls -l /boot/firmware/initrd.img
-rwxr-xr-x 1 root root 18932239 Sep 14 18:09 /boot/firmware/initrd.img     # gzip，19 MB
$ lsinitramfs /boot/firmware/initrd.img | wc -l
778
$ lsinitramfs /boot/firmware/initrd.img | grep -c '\.ko$'
184
```

---

## 2. 生成链条（板上实测时间线）

```
Sep 14 18:06     /etc/initramfs/post-update.d/zz-update-firmware   装上（我们的钩子）
Sep 14 18:08:55  dpkg.log: install linux-image-6.1.118:arm64 6.1.118-1
Sep 14 18:09     /boot/initrd.img-6.1.118          18,932,239 B
Sep 14 18:09     /boot/firmware/initrd.img         18,932,239 B      ← 同一份，被拷过去
```

1. `dpkg -i linux-image-*.deb` → 内核包 postinst → **`/etc/kernel/postinst.d/initramfs-tools`**
   （板上确有此脚本，863 B）→ `update-initramfs -c -k <ver>`。
2. `update-initramfs` 跑 `/usr/share/initramfs-tools/hooks/*`（我们 2 个 + 发行版自带的），
   配置来自 `/etc/initramfs-tools/{conf.d/compression.conf, conf-hooks.d/plymouth, modules}`；
   `MODULES` 没设 ⇒ 用默认的 `most`。产出 `/boot/initrd.img-<ver>`（`COMPRESS=gzip`）。
3. 生成完之后 `run-parts /etc/initramfs/post-update.d/` → 我们的 **`zz-update-firmware`**：
   - 暂存新 `dtbs/`，保留内核包没有的自定义 overlay（同名用新包），按合并清单生成新 DTB；
   - 暂存 `/boot/initrd.img-<ver>`、`/boot/vmlinuz-<ver>`，校验当前 `fdtfile`/overlay 引用后发布。
     准备失败保留现役启动文件，发布失败回滚并返回非零；详见 [dtb-update.md](dtb-update.md)。
   - ⚠️ **没有 flash-kernel**：板上 `dpkg -l flash-kernel` 为空 —— "把内核/initrd 搬到启动分区"这件事
     就是我们这个钩子做的，不是发行版工具。
4. U-Boot 侧 `/boot/firmware/boot.cmd`（编译成 `boot.scr`）：
   ```
   load ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r}  /vmlinuz
   load ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} /initrd.img
   booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
   ```
   启动分区卷标为 `system-boot`，这三个文件的**根目录路径**由启动脚本固定。

### 构建镜像时（`board` 阶段）有什么不同

`dpkg -i linux-image-*.deb` 同样会跑上面第 1–2 步，**但钩子在 chroot 里会自己退出**
（`zz-update-firmware` 开头：`systemd-detect-virt --quiet --chroot` → `exit 0`），
所以"搬到启动分区"由构建脚本手动做（`scripts/build-lsp-image.sh` 里把 `/boot/vmlinuz-*` 拷进 `/boot/firmware/` 那段）：

```bash
cp "${chroot_dir}"/boot/initrd.img-*  "${chroot_dir}/boot/firmware/initrd.img"
cp "${chroot_dir}"/boot/vmlinuz-*    "${chroot_dir}/boot/firmware/vmlinuz"
cp -r "${chroot_dir}"/usr/lib/linux-image-*/. "${chroot_dir}/boot/firmware/dtbs/"
cp ${OVERLAY_DIR}/boot/firmware/dtbs/rockchip/overlay/*.dtbo → 启动分区
```

⇒ 镜像里那份 initrd 与板上这份**内容同源**（同样的 hooks + 配置 + `MODULES=most`），
区别只是"谁负责搬运"：构建时是脚本，运行时（`apt` 升级内核 / 手动 `update-initramfs`）是钩子。

---

## 3. 里面有什么

### 3.1 框架（initramfs-tools 标准件）

`/init` + **busybox**（`usr/bin/busybox` 加一百多个 applet 软链：`cp`/`mount`/`fsck`/`ip`/`ifconfig`/`wget`…）
+ **udev** + **kmod** + **plymouth**（`scripts/init-premount/plymouth`、`init-bottom/plymouth`、`panic/plymouth`
+ `usr/lib/aarch64-linux-gnu/plymouth/renderers/drm.so`）+ `scripts/local-premount/resume` + `usr/bin/resume`（休眠恢复，标准件）。

### 3.2 内核模块：184 个（`usr/lib/modules/6.1.118/`）

⚠️ **2026-09-29 起本节数字已过时**：镜像改成 `MODULES=dep`，initrd 里只剩 5 个模块 ——
原委、实测数字与"为什么别改成 list"见下面的 **§3.2.1**（本节余下内容保留作 `MODULES=most` 时代的记录）。

⚠️ **启动关键路径一个都不在 initrd 里 —— 它们全是内建的**，这就是这块板子不需要 `MODULES=dep` 那种
精细配置也能起来的原因（`build/linux-rockchip/.config`）：

| 子系统 | 配置 | initrd 里的模块数 |
| --- | --- | --- |
| SD/eMMC | `CONFIG_MMC_DW_ROCKCHIP=y`、`CONFIG_MMC_BLOCK=y` | 0 |
| 根文件系统 | `CONFIG_EXT4_FS=y`、`CONFIG_VFAT_FS=y` | 0 |
| PCIe / NVMe | `CONFIG_PCIE_DW_ROCKCHIP=y`、`CONFIG_BLK_DEV_NVME=y` | 0 |
| USB 存储 | `CONFIG_USB_STORAGE=y` | 0 |
| Combo PHY | `CONFIG_PHY_ROCKCHIP_NANENG_COMBO_PHY=y` | 0 |

initrd 里剩下的 184 个 = `MODULES=most` 的通用集合 + 我们钩子手动加的，按子系统大致是：

| 类别 | 条目数（含目录） | 备注 |
| --- | --- | --- |
| `drivers/input/keyboard` | 32 | `adc-keys` / `atkbd` / `adp5588-keys` / `gpio-keys`… |
| `drivers/i2c/{busses,muxes,algos}` | 24 | |
| `drivers/usb/storage` | 14 | U 盘/读卡器在 initrd 阶段可用 |
| `drivers/net/usb` | 13 | `r8152` `ax88179_178a` `rtl8150` `smsc95xx` `smsc75xx` `pegasus` `dm9601` `catc` `kaweth` `lan78xx` `sr9700` `sr9800` `int51x1` |
| `drivers/net/phy`、`net/ethernet` | 7 + 5 | |
| `drivers/nvme` | 7 | host + target/fabrics |
| `drivers/gpu/drm` | 6 | |
| `drivers/spi` / `gpio` / `video/backlight` / `power/supply` | 若干 | |
| `fs/` | — | `squashfs` `isofs` `udf` `jfs` `reiserfs` `fuse`（ext4/vfat 是内建，不在这） |

#### 3.2.1 裁剪：`MODULES=dep`（2026-09-29 落地）

配置在仓库的 `overlay/etc/initramfs-tools/conf.d/10-lsp-modules.conf`（由主脚本整目录装进镜像），
开发板 `<HOST_07>`（内核 6.1.172）实测：

| 指标 | `MODULES=most`（改前） | `MODULES=dep`（改后） |
| --- | --- | --- |
| initrd 文件 | 18,729,691 B | 14,841,676 B |
| 解包后（tmpfs 占用） | 59 MB | 45 MB |
| initrd 里的 `.ko` | 183 | 5（fuse / uio / uio_pdrv_genirq / input-leds / joydev） |
| `Trying to unpack …` → `Freeing initrd memory` | 0.578 / 0.598 / 0.569 / 0.578 / 0.578 s（n=5） | 0.465 / 0.455 / 0.452 s（n=3） |
| `Freeing initrd` → `Run /init as init process` | 0.51~0.78 s（同批 0.66~0.78） | 0.74~0.80 s |
| 总启动（`systemd-analyze`） | 3.39~4.16 s kernel | 3.30~3.38 s kernel |

**结论：裁剪省的是体积与内存，时间只省 ~0.12 s。** 那 183 个模块本来就不会被加载
（initrd 里的 udev 冷插拔只按 modalias 命中实际存在的设备），所以"少装驱动"这件事不存在；
真正省下来的是 cpio 少解包 14 MB。总启动时间的差异（~0.1 s）淹没在 userspace ±0.5 s 的波动里，
**不要**拿 `systemd-analyze` 的总数当裁剪的判据，要看解包窗口那两个 printk。

**安全性验证（改前逐文件对比两份 initrd）：** 除去 `usr/lib/modules/` 下的文件，裁剪版只少了
`usr/lib/aarch64-linux-gnu/libnss_dns.so.2` 与 `libresolv.so.2`（glibc 的 NSS/名字解析，initrd
阶段不做名字解析），多了一个无关紧要的 `etc/ld.so.conf.d/fakeroot-*.conf`；
plymouth、busybox、`fsck.ext4`/`e2fsck`、`scripts/local-bottom/fsck-boot-firmware` 全在。
板上连续 3 次用裁剪版启动均正常（kiosk / oem-host / 上位机页面正常，无 plymouth 报错）。

⚠️ 前提是"挂根链路全内建"这件事继续成立：`CONFIG_MMC_DW_ROCKCHIP`/`MMC_BLOCK`/`EXT4_FS` 是 `=y`，
显示 `CONFIG_DRM_ROCKCHIP`/`ROCKCHIP_DW_HDMI` 是 `=y`（HDMI 出画面 + plymouth 都不需要模块）。
将来若改成像"USB 盘当根"这种挂根方式，要重新评估这个配置。

配套的防回归断言在 `tools/verify-image.sh` 的「initramfs 内容（固件 / fsck）」组（第 ⑥ 条）：
`10-lsp-modules.conf` 必须是 `MODULES=dep`、initrd 里的 `.ko` 必须 ≤ 30 个。

> ⚠️ **2026-10-03 板上复核（重要，别被"配置改了"骗了）**：**initrd 是按它生成那一刻的配置做的**。
> 开发板上现役那份 `initrd.img-6.1.172`（`/boot/` 与 `/boot/firmware/` 两份 md5 相同）是
> **2026-09-30 12:26** 生成的，里面还有 **183 个 `.ko`**、18,730,313 B；而
> `/etc/initramfs-tools/conf.d/10-lsp-modules.conf` 的落盘时间是 **2026-10-02 20:07**
> —— **晚于 initrd**（这份板子是"老镜像 + 后来用 `apply-overlay-to-board.sh` 补的文件"）。
> ⇒ 在已装好的系统上，光有配置不会改历史产物：必须 `sudo update-initramfs -u`（见 §4）才会变成 5 个模块。
> 判据（一条命令看两件事）：
> ```bash
> sh -c '. /etc/initramfs-tools/initramfs.conf; . /etc/initramfs-tools/conf.d/10-lsp-modules.conf; echo MODULES=$MODULES'   # → dep
> lsinitramfs /boot/initrd.img-$(uname -r) | grep -cE "\.ko$"                                                              # 重建前 183 / 重建后 5
> ```

**initramfs 阶段到底加载了哪几个模块（2026-09-29 探针实测）**：在 `scripts/local-bottom/` 里加一个
一次性脚本，切根前把 `/proc/modules` 写到 `${rootmnt}/var/tmp/`，重启后对比 `lsmod`：

```
切根前（< 3.89 s，Run /init as init process 之前）：input_leds  joydev  uio  uio_pdrv_genirq   ← 4 个
切根后（19 个）：8822ce 8852be adc_keys binfmt_misc bnep btbcm btintel btmtk btrtl btusb
                dm_mod fuse ip_tables ipv6 nfnetlink pwm_fan rfcomm rtk_btusb sch_fq_codel
```

⛔ **"在 initrd 里"不等于"initramfs 阶段被加载"**——最典型的反例是 `fuse`：它的 `.ko` 在 initrd 里，
但实际是切根后 **5.75 s 由 `modprobe@fuse.service`**（systemd 的 kmod 静态单元）加载的。
那 4 个是 initramfs 里 udev 冷插拔按 modalias 命中的（`/etc/initramfs-tools/modules` 与 initrd 的
`conf/modules` 都是空的，没有任何显式加载清单）。切根后的加载时刻（本次启动，monotonic）：
`dm_mod`/`fuse`/`ip_tables`/`nfnetlink`/`ipv6`/`sch_fq_codel` ≈ 5.76 s（systemd 的 modprobe 批次）、
`btusb`/`rtk_btusb`/`pwm_fan`/`adc_keys`/`8822ce`/`8852be` ≈ 6.3 s（真 rootfs 的 udev 冷插拔）、
`bnep` 6.86 s 与 `rfcomm` 8.17 s（bluetoothd 起来后按需加载）。
⚠️ 别用 `/sys/module/<x>` 的目录 mtime 当加载时刻，实测不可靠（同一批模块的 mtime 会挤在一起）。

**这些模块加载要花多少时间（2026-09-29 实测）**：

- ⛔ **我们的 `.ko` 不是 zst 压缩的**：板上 `find /lib/modules/6.1.172 -name '*.ko'` = 1995 个、
  `*.ko.zst` = 0 个；`/boot/config-6.1.172` 是 `CONFIG_MODULE_COMPRESS_NONE=y`（GZIP/XZ/ZSTD 全 not set），
  抽样 `joydev.ko` 是 `ELF 64-bit LSB relocatable, ARM aarch64`。**带 `.zst` 的是固件**
  （`/lib/firmware/rtl_bt/rtl8822cu_fw.bin.zst`、`mali_csffw.bin.zst`、`dptx.bin.zst`），
  由 `CONFIG_FW_LOADER_COMPRESS_ZSTD=y` 在加载时解压。
- 单模块冷缓存 `insmod`（含 kmod 进程本身）：26 KB 的 `bcm-phy-lib` 27 ms、56 KB 的 `isofs` 22 ms、
  28 KB 的 `joydev` 21 ms；**热缓存 3~5 ms** ⇒ 大小几乎无关，成本是"起进程 + relocate + module_init"的固定开销。
  系统自己的记账同量级：`modprobe@fuse.service` 39 ms、`modprobe@dm_mod.service` 20 ms、
  `systemd-modules-load.service` 13 ms（`systemd-analyze blame`）。
- 冷读 eMMC 只算读盘：8.0 MB 44 ms（155 MB/s）、6.3 MB 35 ms、3.7 MB 24 ms、几十 KB 的小文件 12~13 ms。
- **最大单项是固件下载，不是装模块**：BT 链 `[6.3238] usbcore: registered new interface driver btusb`
  → `[6.8215] Bluetooth: hci0: RTL: fw version 0x0cc6d2e3` = **0.50 s**。
- 整批：23 个模块 / 13 MB（其中 `8852be.ko` 8.0 MB + `8822ce.ko` 3.7 MB 占 11.7 MB）≈ **0.8~1.0 s**，
  分散在 3.8~8.2 s 之间。
- ⚠️ **但它们在 cage 的关键路径之外**：同一条单调时钟上
  `systemd-udev-trigger.service` 3.7585→4.0213 s（263 ms，冷插拔 + 那批模块）、`modprobe@fuse` 3.7498→3.7887 s、
  `lsp-kiosk.service`（cage）4.9073 s 起；`systemd-analyze critical-chain lsp-kiosk.service` 里
  既没有 udev-trigger 也没有任何 modprobe。所以"画面出来晚"跟模块加载无关。

### 3.3 固件：5 个文件，全是"早于 rootfs 就要用"的

- `arm/mali/arch10.10/mali_csffw.bin.zst`（GPU 实体）+ `arch10.8/mali_csffw.bin.zst`（软链到前者）
- `rockchip/dptx.bin.zst`（DP 输出）
- `regulatory.db` + `regulatory.db.p7s`（cfg80211 的无线管制库）

⛔ **这些不是任何 hook 拷进去的**：`mkinitramfs:380` 的 `add_builtin_firmware()`（跑在 hooks
（`:445`）**之前**）读 `/lib/modules/<ver>/modules.builtin.modinfo`，把**内建（=y）驱动**声明的
`firmware=` 自动收集起来：

```
$ tr '\0' '\n' < /lib/modules/$(uname -r)/modules.builtin.modinfo | grep -E '^(panthor|cfg80211)\.firmware='
panthor.firmware=arm/mali/arch10.8/mali_csffw.bin        # CONFIG_DRM_PANTHOR=y
cfg80211.firmware=regulatory.db                          # modinfo -n cfg80211 → (builtin)
cfg80211.firmware=regulatory.db.p7s
```

可加载模块的固件则由 `manual_add_modules` → `dracut-install --firmwaredirs /lib/firmware` 自动带
（`hook-functions` 原话："manual_add_modules() takes care of adding firmware for things that were
built as modules"）⇒ **不需要任何自定义 hook 来搬固件**。2026-09-26 据此删掉 `rtl-bt` 与
`mali-firmware`（原委见 §3.4 末表与 §4）。

⚠️ 固件本体**不在内核镜像里**：defconfig 没有 `CONFIG_EXTRA_FIRMWARE`，固件是 `/usr/lib/firmware/`
下的独立文件，运行时由内核固件加载器读（`.zst` 能直接用是因为 `CONFIG_FW_LOADER_COMPRESS_ZSTD=y`）。
顺带说明"驱动内置"≠"固件内置"：panthor 是 =y，但固件仍是外部文件，只是被上面那个机制自动收进 initrd。

### 3.4 我们自己加的东西（仓库里 6 个文件）

| 文件 | 作用 | 在 initrd 里的体现 |
| --- | --- | --- |
| `etc/initramfs-tools/conf.d/10-lsp-modules.conf` | `MODULES=dep`（只带挂根必需；原委与实测见 §3.2.1）。⛔ 不要改成 `list` + 手工清单 | 直接影响 `usr/lib/modules/` 里带什么 |
| `usr/share/initramfs-tools/hooks/fsck-ext4` | 带 `fsck.ext4`/`e2fsck` + `fsck` 入口；**boot 分区 2026-09-26 起是 ext4**（早先是 vfat，那时这个 hook 叫 `fsck-vfat`、还要带 glibc 的 `IBM850` gconv）。⛔ 缺 `e2fsprogs` 时只警告 + 跳过（`copy_exec` 返非 0 会被 `set -e` 打断整轮 `update-initramfs`） | `usr/sbin/fsck.ext4`、`usr/sbin/e2fsck` |
| `usr/share/initramfs-tools/scripts/local-bottom/fsck-boot-firmware` | 根挂上后按 fstab 找 `/boot/firmware` 并 `checkfs` 一遍（`checkfs`/`read_fstab_entry` 由**包自带**的 `scripts/functions` 提供，我们不再自带拷贝）。⚠️ 开头的 `PREREQ`/`prereqs()` **不是死代码**，见 §4 | `scripts/local-bottom/fsck-boot-firmware` |
| `etc/initramfs-tools/conf.d/compression.conf` | `COMPRESS=gzip`（明确不用 zstd/lz4 —— 启动早期解压快、实现简单） | 影响压缩方式 |
| `etc/initramfs-tools/conf-hooks.d/plymouth` | `FRAMEBUFFER=y` | 开机会画 plymouth |
| `etc/initramfs/post-update.d/zz-update-firmware` | 见 §2 第 3 步 | **不在 initrd 里**（宿主侧钩子）|

**2026-09-26 删掉的 4 个**（从上游/厂商带过来的，逐个查完确认是错的或死代码）：

| 删掉的文件 | 为什么 |
| --- | --- |
| `hooks/rtl-bt` | 把**目录路径**当 `copy_exec` 的第二个参数，而 `copy_file()` 只在 `${DESTDIR}${target}` **已存在**时才当目录 ⇒ 实际把 glob 里第一个固件拷成了名为 `usr/lib/firmware/rtl_bt` 的**文件**（18344 B = `rtl8192ee_fw.bin.zst`），后面 45 个全撞 `[ -e ]` 被静默跳过。而且 `btrtl` 根本不在 initrd 里（`MODULES=most` 的类别清单不含蓝牙），initrd 阶段用不到 BT 固件 |
| `hooks/mali-firmware` | 同一个用法错误。只是 `arm/mali/` 已被 `add_builtin_firmware` 建好，所以没炸，但落下去的是**扁平化的 `arm/mali/mali_csffw.bin.zst`** —— 内核请求的是 `arm/mali/<arch>/mali_csffw.bin`，这个副本永远用不上（58 KB 白塞） |
| `hooks/usb_modeswitch` | 把 `usr/sbin/usb_modeswitch`（69 KB）和**两份** `40-usb_modeswitch.rules`（43 KB×2）拷进 initrd，但 initrd 阶段没有任何设备需要"切模式"（根在 eMMC/SD/NVMe）；而且它**不是任何 Debian 包带的**（`dpkg -L usb-modeswitch` 里没有 hook，是厂商抄来的）。运行期真正起作用的是构建脚本写进 `/etc/udev/rules.d/40-usb_modeswitch.rules` 的那条 Realtek 8811CU/8821CU 规则（`0bda:1a2b` → `usb_modeswitch -K`），**删 hook 不影响它**；`usb-modeswitch` 这个包本身也保留 |
| `scripts/functions` | 与 `initramfs-tools-core 0.142ubuntu25.8` 包里那份**逐字节相同**（md5 `5351984436…`）。它原本是为回带 `_checkfs_once` 里重复 `logsave … -y` 的修复（厂商提交 6734a194d），而该修复上游早就有了 ⇒ 纯冗余拷贝。它是包的文件、不是 conffile，留着只会让人误以为"我们打过补丁"（apt 升级还会静默覆盖） |

### 3.5 没有的

`cryptsetup` / LVM / mdadm **全 0**；没有 `dhclient` / `netplan` / `curl`（busybox 自带 `ip`/`ifconfig`/`wget`，
但没有网络引导配置）。就是"本地 SD / eMMC / NVMe 启动"的最小集。

---

## 4. 重建、查看与两条结论 → 见 [initramfs-rebuild-and-inspect.md](initramfs-rebuild-and-inspect.md)

`update-initramfs` 的用法、`lsinitramfs`/`unmkinitramfs` 查看方式、两条观察与 2026-09-26 的板级实测记录
**已搬成单独一篇** → **[initramfs-rebuild-and-inspect.md](initramfs-rebuild-and-inspect.md)**。


## 5. 复现本文所有结论

```bash
# 板上
ls -l /boot/firmware/initrd.img /boot/initrd.img-"$(uname -r)"
lsinitramfs /boot/firmware/initrd.img | wc -l
lsinitramfs /boot/firmware/initrd.img | grep -c '\.ko$'
lsinitramfs /boot/firmware/initrd.img | grep '^usr/lib/firmware/'
grep -h 'install linux-image' /var/log/dpkg.log*        # 生成时间对应哪次装包
ls /etc/kernel/postinst.d/ /etc/initramfs/post-update.d/

# 仓库侧：内建还是模块
grep -E '^CONFIG_(MMC_DW_ROCKCHIP|MMC_BLOCK|EXT4_FS|VFAT_FS|PCIE_DW_ROCKCHIP|BLK_DEV_NVME|USB_STORAGE|PHY_ROCKCHIP_NANENG_COMBO_PHY)=' \
     build/linux-rockchip/.config
```
