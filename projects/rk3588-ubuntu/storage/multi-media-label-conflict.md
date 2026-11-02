# 排查记录：SD 卡与 eMMC 卷标相同导致的混合启动

> **结论先行**：同一份镜像分别烧到 SD 卡与 eMMC 时，两者的卷标、文件系统 UUID、PARTUUID
> **全都相同**，所以"按标识符解析"这条路无解 —— 会出现「U-Boot 从 SD 启动，根文件系统却是
> eMMC 的」。解决办法是给 SD 用的镜像副本打上独立标签（`writable-sd` / `boot-sd`），
> eMMC 保持标准镜像不变。

## TL;DR

- 结论：同一份镜像分别烧到 SD 与 eMMC 时，**卷标 / 文件系统 UUID / PARTUUID 完全相同**
  ⇒ 两块卡同时插着时**可能从错的那块启动**。
- 根因见 §2；危害不止 root（§3）。
- 解决：**给 SD 用"带后缀标签"的镜像变体**（§4）；被否掉的方案与原因见 §5；使用方式见 §6。

## 1. 现象

- 板子同时插着 SD 卡和 eMMC，两块烧的是**同一份镜像**
- 期望：SD 优先启动（便于拔插测试），用 SD 上的系统
- 实际：U-Boot 从 SD 读内核/DTB 启动，但挂载的根文件系统是 **eMMC** 的
- 表现：在 SD 上做的修改"不生效"、apt 装的包"不见了"、改动跑到 eMMC 上去了

## 2. 根因

### 2.1 启动链路上的标识符全部相同

同一份 `.img` 写到两块介质 → 分区上的元数据完全一致：

| 标识 | SD 卡 | eMMC | 能否用于区分 |
| --- | --- | --- | --- |
| 卷标 LABEL | `writable` / `system-boot` | `writable` / `system-boot` | ✗ 相同 |
| 文件系统 UUID | 同一 UUID | 同一 UUID | ✗ 相同（都是 `mkfs.ext4 -U` 的值，来自镜像）|
| 分区 PARTUUID | 同一 PARTUUID | 同一 PARTUUID | ✗ 相同 |

**关键点**：UUID/PARTUUID 是镜像内容的一部分，所以"改用 UUID 就安全了"是**错的**。
只要烧的是同一份镜像，任何内容派生的标识都无法区分两块介质。

### 2.2 U-Boot 优先扫 SD

`u-boot/include/configs/rockchip-common.h`：

```c
#define BOOT_TARGET_MMC(func) \
	func(MMC, mmc, 1) \      /* mmc 1 = SD 卡，先扫 */
	func(MMC, mmc, 0)        /* mmc 0 = eMMC，后扫 */
```

所以插着 SD 时，U-Boot 必然从 SD 读 `system-boot` 与内核。

### 2.3 内核侧的解析是"设备枚举序优先"

`boot.cmd` 传的是 `root=LABEL=writable`（`scripts/build-lsp-image.sh` 内联生成）。
initramfs 解析 `LABEL=` 时会扫描所有块设备取**第一个**匹配 —— 而 eMMC 通常先枚举
（本板 `mmcblk0` = eMMC、`mmcblk1` = SD），于是挂到了 eMMC 的 rootfs。

## 3. 危害不止于 root

| 位置 | 现状 | 后果 |
| --- | --- | --- |
| boot.cmd | `root=LABEL=writable` | 根文件系统挂到另一块介质 |
| fstab | `LABEL=system-boot /boot/firmware` | 内核/initrd 更新写到另一块介质的启动分区 |
| fstab | `LABEL=writable / ... x-systemd.growfs` | **最隐蔽**：从 SD 启动却去扩容 eMMC 的分区 |

三者叠加后的状态可能"看起来正常但处处错位"，排查成本很高。

## 4. 解决方案：给 SD 用带后缀标签的镜像变体

由 `tools/make-sd-image.sh` 产出 SD 专用变体：

| | SD 测试镜像（变体）| eMMC 镜像（标准）|
| --- | --- | --- |
| rootfs 卷标 | `writable-sd` | `writable`（不动）|
| 启动分区卷标 | `boot-sd` | `system-boot`（不动）|
| fstab | 引用 `*-sd` | 不动 |
| boot.cmd 的 `root=` | `LABEL=writable-sd` | 不动 |

工具做的事：复制/解压镜像 → loop 挂载 → 改写 fstab 与 boot.cmd → 重建 boot.scr →
卸载后改卷标（**只出裸 `.img`**，压缩用 `tools/compress-image.sh`，见下）。两块介质互不冲突，各自引用自己的标签，
`/boot/firmware` 与 `x-systemd.growfs` 也都作用在同一块介质上。

**为什么启动分区标签是 `boot-sd` 而不是 `system-boot-sd`**：**vfat 时代** FAT 卷标最长 **11 字符**，
`system-boot` 正好 11（当时已到上限），`system-boot-sd` 有 14 字符会被拒。
⚠️ **2026-09-26 起 boot 分区换成 ext4**（标签上限 16 字符）⇒ 这个 11 字符的限制**不再存在**，
但 `boot-sd` / `system-boot` 这两个名字沿用下来没改（改名要同步镜像标签、fstab、工具与老卡，得不偿失）。

## 5. 被否掉的方案与原因

| 方案 | 为什么不用 |
| --- | --- |
| 改用 UUID / PARTUUID | **无效**。同一份镜像的 UUID/PARTUUID 也相同（见 2.1）|
| U-Boot 按启动设备推导 root（`/dev/mmcblk${devnum}p2`）| 可行（本板 U-Boot 与内核 mmc 别名一致：`mmc0=sdhci`=eMMC、`mmc1=sdmmc`=SD），但需要改 `boot.cmd`/启动参数，且 `/boot/firmware` 与 growfs 还得另配一套运行时推导 |
| initramfs 里实现"SD 存在则优先用 SD" | 不改 boot.cmd，但要在 initramfs 加 hook，且 fstab 的两处引用与 growfs 都要跟着改，更绕更脆 |
| 只把 eMMC 的卷标改掉 | eMMC 自己的 `boot.cmd` 仍写死 `LABEL=writable`，启动会找不到根分区 |

**选定的思路**：不动生产（eMMC）流程的任何一个字节，只让"临时测试介质"SD 拥有独立身份。

## 6. 使用方式

```bash
# 产出 SD 专用变体（默认取 images/ 下最新镜像）
sudo ./tools/make-sd-image.sh
# → images/<name>-sd.img               ← 裸镜像（默认不算 sha256；发布加 --sha256）

# 要归档/给 Windows 再压一次（可选）
./tools/compress-image.sh images/<name>-sd.img --zstd     # → .img.zst
./tools/compress-image.sh images/<name>-sd.img --zip      # → .img.zip（RKDevTool）

# 烧录：SD 用变体，eMMC 用标准镜像（写入工具认 .img / .img.zst / .img.zip）
sudo ./tools/write-sd.sh -i images/<name>-sd.img -d /dev/sdb
```

工具带出的自检与保护：
- 依赖检查：`losetup` / `xz` / `mkimage` / `fatlabel` / `e2label` / `blkid`
- FAT 标签 >11 字符、ext4 标签 >16 字符时**提前拒绝**（而不是写进去再失败）
- 改完标签**读回校验**（FAT 卷标大小写/长度可能被底层规范化）
- 自检镜像内是否残留旧标签引用；注意这里的匹配必须**精确到标签结尾** ——
  否则 `LABEL=writable-sd` 会被 `LABEL=writable` 子串误命中（本工具已按此实现）
- 卷标改动在**卸载之后**进行（对已挂载文件系统改卷标不安全）

## 7. 方法论沉淀

1. **先分清"标识符相同"和"标识符不同"**：烧同一份镜像时，UUID/PARTUUID 也是相同的，
   不要想当然认为"用 UUID 就唯一了"。
2. **多介质共存时，凡是"按名字找东西"的地方都要检查一遍**：本次一共三处
   （bootargs 的 root、fstab 的两项），只修一处会留下更隐蔽的问题。
3. **优先消除歧义，而不是在运行时消歧**：让两块介质的标识天然不同，
   比在启动链路里加推导逻辑更简单、更不容易出错。
4. **注意工具的硬限制**：FAT 卷标 11 字符这种上限，早点校验比写坏之后再发现好得多。
5. **改完要读回确认**：文件系统工具可能对标签做规范化（大小写、截断），
   写入成功 ≠ 读回一致。
