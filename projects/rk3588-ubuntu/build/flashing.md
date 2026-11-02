# 烧录与维护：SD / eMMC / NVMe / 增量写入 / 改卡

> 怎么把 `images/*.img` 写到 SD 卡、eMMC、NVMe，以及**已经烧录过的设备**怎么做增量更新、
> 怎么挂载 + chroot 改卡、怎么生成"SD 专用"镜像避免和 eMMC 撞卷标。

## TL;DR

- **SD 卡**：直接写裸 `.img`（rufus 或 `dd`）。
- **eMMC**：Windows 用 RKDevTool；Linux 用 [rkdeveloptool](rkdeveloptool.md)，经 MaskROM 加载兼容 USB MiniLoader，再写完整裸镜像。
- **NVMe**：可写入镜像，但当前 U-Boot 默认不扫描 NVMe，不能仅靠写入 `rkspi_loader.img` 获得 NVMe 启动。
- **只改一点点**时不用整盘 `dd`：见"增量写入"那节。
- ⛔ 同一份镜像同时烧到 SD 与 eMMC 会**卷标/UUID/PARTUUID 全同** ⇒ 用"SD 专用变体"。

## 安装（写入介质）


### 获取与校验
1. 从提供的下载源获取镜像（本工程只出桌面版）。

### SD 卡
1. 推荐使用 rufus（免安装、小体积）。若需其他版本可从 https://rufus.ie/zh/ 获取。
2. 打开 rufus，选择 SD 卡与镜像（我们默认出裸 `.img`，直接选它），开始写入，等待完成。

### eMMC 启动

Windows 主机使用 RKDevTool：

1. 将板子置于 MaskROM 模式，用 RKDevTool 确认设备。
2. 选择 “下载镜像”：`boot` 选 `rk3588_MiniLoaderAll.bin`（Rockchip / 板卡厂商提供，不在本仓库）；
   `system` 选解压后的 `.img`。MaskROM 模式下先加载匹配板卡 / DDR 的 USB MiniLoader，再访问 eMMC。
3. 勾选 “强制按地址写”，点击执行，等待完成。

旧野火 Ubuntu 镜像与当前镜像都使用 p1 `16MiB–528MiB`、p2 从 `528MiB` 开始的 GPT 布局；
因此已有旧固件时也不需要先擦除 eMMC。完整 `.img` 从 `0x0` 写入会覆盖 GPT、裸启动固件、boot 和 rootfs。
`boot` 中的 MiniLoader 只下载到 RAM；只有需要数据销毁、清理镜像范围外残留或处理厂商明确要求的特殊故障时才额外擦除。

Linux 主机见 [rkdeveloptool 使用说明](rkdeveloptool.md)：版本区别、下载口、Loader、
LBA0 写入及完整读回比较。完整裸 `.img` 与 Rockchip `update.img` 容器不能混用。

### NVMe 写入与启动限制

从 SD / eMMC 启动到系统后，可以把镜像写入已确认且未挂载的目标 NVMe；这会覆盖原数据：

```bash
sudo dd if=images/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img of=/dev/nvme0n1 bs=4M conv=fsync status=progress
sync
```

当前 U-Boot 的默认启动目标不含 NVMe；**写入镜像不等于能自动启动**。
`rkspi_loader.img` 使用同一 U-Boot，也不会自动恢复该启动目标。
启用 NVMe 启动需要另行核对并修改引导策略，见 [U-Boot 变更](uboot-changelog.md)
和 [NVMe 探测边界](../storage/nvme-performance.md)。


## 烧录与维护


### 写入 SD 卡
`tools/write-sd.sh` 把构建好的镜像写入 SD 卡 / U 盘：

```bash
sudo ./tools/write-sd.sh                 # 交互选择设备，自动取 images/ 下最新镜像
sudo ./tools/write-sd.sh --list          # 只列出候选设备（无需 root）
sudo ./tools/write-sd.sh -d /dev/sdb     # 指定设备
sudo ./tools/write-sd.sh -i images/xxx.img.xz -d /dev/sdb -y   # 脚本化
sudo ./tools/write-sd.sh --verify        # 写入后读回校验 sha256（耗时约翻倍）
```

安全机制：
- 自动识别并**拒绝写入系统盘**（根分区 / `/boot` / `/home` / swap 所在磁盘）
- 必须是整盘设备（拒绝 `/dev/sdb1` 这类分区）
- 设备容量小于镜像时直接拒绝
- 写入前展示设备型号/容量/是否可移动，并**要求手动输入设备名确认**（`-y` 可跳过）
- 目标设备已挂载的分区会自动卸载

依赖：`dd`、`lsblk`、`findmnt`、`blockdev`（util-linux）系统自带；**压缩镜像还要对应的解压工具**——
`.zst` 要 `zstd`（默认产物）、`.zip` 要 `unzip`、`.xz` / `.gz` / `.lz4` 分别要 `xz` / `gzip` / `lz4`（仅历史产物）。
可选安装 `pv` 以获得进度条。

### 增量写入已烧录的设备（改一点点就不必整盘 dd）
`tools/update-emmc.sh` 以构建好的镜像为准，用 rsync 只把**差异**同步到已烧录的 eMMC/SD：

```bash
sudo ./tools/update-emmc.sh -d /dev/mmcblk0            # 同步 rootfs(p2) + boot(p1)
sudo ./tools/update-emmc.sh -d /dev/mmcblk0 -n         # 先 dry-run 看计划
sudo ./tools/update-emmc.sh -d /dev/mmcblk0 --bootloader   # 改过 U-Boot/ATF 时
```

与 `write-sd.sh` 的分工：

| 改了什么 | 用什么 |
| --- | --- |
| 只改 rootfs 内容（包 / 配置 / 服务 / overlay） | **update-emmc.sh**（秒级~分钟级） |
| 改了内核 / dtb / boot.scr / ubuntuEnv.txt | **update-emmc.sh**（会同步 p1 boot 分区） |
| 改了 U-Boot / ATF（32KB 起的裸偏移） | **update-emmc.sh --bootloader** |
| 改了分区表本身 / 目标没烧过同一布局的镜像 | **write-sd.sh**（整盘写入） |

原理与安全机制：
- **按设备定位而不是按 LABEL**——SD 与 eMMC 的标签会重名（都是 `writable`/`system-boot`），
  所以 `sd-chroot.sh` 那套 LABEL 查找在这里不安全，必须 `-d /dev/mmcblkX`。
- 只读挂载镜像的 p1/p2，rsync 到目标；**不动分区表、不 mkfs**，因此
  LABEL/UUID 和首次启动 `x-systemd.growfs` 的扩容结果都不会被破坏。
- rootfs 用 `rsync -aHAX --numeric-ids --delete`（保留属主/权限/xattr/设备节点）；
  **boot 分区 2026-09-26 起也是 ext4，用同一套语义**（vfat 时代那套 `rsync -rlt --modify-window=1`
  已随分区格式一起删掉）—— 脚本会先按 `blkid` 校验目标 p1 是 ext4，遇到**旧 vfat 目标会直接拒绝**
  （分区起点能对上、但 rsync 语义不适用）。
- 同步前校验**目标 p1/p2 起始扇区与镜像一致**，不一致直接拒绝并提示用 `write-sd.sh`。
- 同样**拒绝写入系统盘**、自动卸载目标已挂载分区、支持 `-n` 预览与手动输入设备名确认。
- 注意：与新镜像不一致的文件会被删除（rsync `--delete` 镜像语义），这是"目标 = 新镜像"的预期行为。
- 改了 U-Boot / ATF 却只同步分区，会出现"rootfs 是新的、引导却还是旧的"；`update-emmc.sh`
  现在会在同步前**对比镜像与设备这段裸区域的哈希**，不一致就明确提示加 `--bootloader`。
- `--bootloader` 的读取与写入都从 **32 KiB** 开始（`dd skip=64 seek=64 bs=512`），
  到 p1 起点前结束；不能只写 `skip`，否则会从目标第 0 字节写起、覆盖分区表。
- 清理必须先卸载全部临时挂载，再释放源镜像 loop、删除临时目录。
  **任一卸载/loop 释放失败都报错并保留临时目录**，不能用 `rm -rf` 穿过未卸载的目录删除盘上文件。
- 回归：`bash tests/update-emmc-tool.sh`（26 项，临时文件 + 卸载失败桩）；
  开发板另用临时 GPT loop 盘跑完整流程，主/备分区表、两分区 UUID、源镜像字节保持，boot/rootfs 更新正确。

### 修改已烧录的 SD 卡（挂载 + chroot）
烧录后若发现需要调整文件系统或 boot 配置，可用 `tools/sd-chroot.sh` 把卡插回电脑挂载并 chroot：

```bash
sudo ./tools/sd-chroot.sh mount              # 只挂载（改 ubuntuEnv.txt / dtb / fstab 等无需 chroot）
sudo ./tools/sd-chroot.sh chroot             # 挂载 + 准备环境 + 进入交互 shell
sudo ./tools/sd-chroot.sh exec apt-get update   # 非交互执行单条命令
sudo ./tools/sd-chroot.sh status             # 查看当前挂载状态
sudo ./tools/sd-chroot.sh umount             # 卸载并还原（可独立执行）
```

要点：
- **按 LABEL 定位分区**（`writable` / `system-boot`），不依赖 `/dev/sdX` 名字变化
- **FAT 启动分区挂到 `<rootfs>/boot/firmware`**：否则 chroot 内 `update-initramfs` 会把 initrd 写进 rootfs 内部、被启动分区遮蔽，板子拿不到新 initrd
- **x86_64 主机自动拷入静态 qemu**（`qemu-aarch64-static` / 26.04 起的 `qemu-aarch64`），**卸载时自动删除**（它是主机架构二进制，留在卡上无用）
- 挂载状态记录在 `/run/sd-chroot.state`，**`umount` 可独立执行** —— 交互 shell 被 Ctrl-C/断线打断后仍能从外部清理
- 拒绝挂载**系统盘**上的分区，且校验目标确实是本镜像的 rootfs
- 主机本身就是 `aarch64` 时无需 qemu，直接原生 chroot

依赖（仅 x86_64 主机需要）：`qemu-user-static`（提供 `/usr/bin/qemu-aarch64-static`，会被拷进卡里）
+ `qemu-user-binfmt` + `binfmt-support`；Ubuntu 26.04 起 `qemu-user-static` 并入 `qemu-user`。

> 多数 boot 调整（`ubuntuEnv.txt`、设备树、`boot.scr`、`fstab`）用 `mount` 即可，不必 chroot；
> 只有运行 aarch64 程序（`apt`/`dpkg`/`update-initramfs`/`systemctl enable`）才需要 `chroot`。

### SD 卡与 eMMC 同时使用时：生成 SD 专用镜像
同一份镜像烧到 SD 卡和 eMMC 时，两者的**卷标、文件系统 UUID、PARTUUID 全都相同**，
而 U-Boot 优先从 SD 启动 → 会出现「用 SD 的 system-boot 启动了，根文件系统却是 eMMC 的」，
`fstab` 的 `LABEL=system-boot` 与 `x-systemd.growfs` 也会作用到另一块介质。改用 UUID/PARTUUID
**无法解决**（同一份镜像的 UUID 也相同）。

解决办法是给 SD 用的镜像副本打上独立标签，eMMC 保持标准镜像：

```bash
# 产出 SD 专用变体：卷标改为 writable-sd / boot-sd，并同步改写其 fstab 与 boot.cmd
sudo ./tools/make-sd-image.sh
# → images/<name>-sd.img（裸镜像；要压缩包另跑 ./tools/compress-image.sh images/<name>-sd.img --zstd）

# 烧录：SD 用变体，eMMC 用标准镜像
sudo ./tools/write-sd.sh -i images/<name>-sd.img -d /dev/sdb
```

- 启动分区标签用 `boot-sd`（而不是 `system-boot-sd`）：这是**沿用的名字**，不再有 11 字符的硬限制
  —— 2026-09-26 起启动分区是 **ext4（标签上限 16 字符）**；`make-sd-image.sh` 会按 `blkid` 自动判类型、
  ext4 用 `e2label`、vfat（老卡）用 `fatlabel`。⚠️ 脚本里 `--boot-label` 的长度检查仍是按 FAT 的
  **11 字符**卡的（改长名字会被拒）—— 需要更长标签时先改那处检查。
- 工具会在改标签后**读回校验**，并自检镜像内无旧标签残留
- 详细分析见 [notes/storage/multi-media-label-conflict.md](../storage/multi-media-label-conflict.md)


## 常见问题（烧录相关）

- NVMe 启动失败：先核对 U-Boot 是否配置 NVMe 启动目标；当前默认未启用，不能仅靠重刷 `rkspi_loader.img` 解决。
- GPG/仓库问题：如 apt 更新失败，检查网络与镜像源，必要时更换为官方/就近镜像。
- 启动卡在 SPL→U-Boot 之间：`u-boot.itb` 缺少 ATF，详见 [`notes/hardware/atf-missing-troubleshooting.md`](../hardware/atf-missing-troubleshooting.md)。

