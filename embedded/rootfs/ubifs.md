# Purple Pi R1：MTD、UBI 与 UBIFS 布局

> MTD 描述 Flash 分区，UBI 在其上管理逻辑擦除块和卷，UBIFS 挂载在 UBI 卷上；三者的容量与块大小不能混用。

## 实验环境与启动参数

Purple Pi R1 IDO-SBC2D06，SSD201，64 MiB DDR2、128 MiB SPI-NAND。启动参数示例：

```text
ubi.mtd=UBI,2048 root=ubi:rootfs rw rootfstype=ubifs init=/linuxrc rootwait=1 mtdparts=nand0:384k@1280k(IPL0),384k(IPL1),384k(IPL_CUST0),384k(IPL_CUST1),768k(UBOOT0),768k(UBOOT1),256k(ENV),256k(ENV1),0x20000(KEY_CUST),0x60000(LOGO),0x500000(KERNEL),0x500000(RECOVERY),-(UBI)
```

`ubi.mtd=UBI,2048` 中 `UBI` 为 MTD 分区名，第二个参数是 **VID header offset**，不能只凭它推断 NAND 页大小。`rootwait=1` 和 `root=ubi:rootfs` 是该系统的参数写法，需核对目标内核支持的语法，不能将其解释为所有内核通用的 UBI 等待机制。

## MTD 分区

```text
dev:    size     erasesize  name
mtd0:  00060000  00020000   "IPL0"
mtd1:  00060000  00020000   "IPL1"
mtd2:  00060000  00020000   "IPL_CUST0"
mtd3:  00060000  00020000   "IPL_CUST1"
mtd4:  000c0000  00020000   "UBOOT0"
mtd5:  000c0000  00020000   "UBOOT1"
mtd6:  00040000  00020000   "ENV"
mtd7:  00040000  00020000   "ENV1"
mtd8:  00020000  00020000   "KEY_CUST"
mtd9:  00060000  00020000   "LOGO"
mtd10: 00500000  00020000   "KERNEL"
mtd11: 00500000  00020000   "RECOVERY"
mtd12: 070c0000  00020000   "UBI"
```

`size` 和 `erasesize` 都是十六进制字节数；此设备的 PEB 为 `0x20000 = 128 KiB`，UBI 分区为 `0x070c0000 = 112.75 MiB`。存在两份引导或环境分区，只能说明布局预留了副本，不能单凭名称保证升级切换或断电恢复正确。

## UBI 测量数据

`ubinfo -a` 报告 ubi0：最小 I/O 单位 2048 字节，LEB 126976 字节，LEB 总数 902，可分配 41；坏 PEB 0，预留物理块 20，最大擦除计数 467。

| 卷 | LEB 数 | 容量（字节） | 节点 |
| --- | --- | --- | --- |
| rootfs | 678 | 86089728 | `/dev/ubi0_0` |
| miservice | 83 | 10539008 | `/dev/ubi0_1` |
| customer | 42 | 5332992 | `/dev/ubi0_2` |
| appconfigs | 34 | 4317184 | `/dev/ubi0_3` |

总 LEB 与已分配、可分配块数的差额还包括 UBI 预留和开销，不能把每项简单混加。卷内文件用途应查实际挂载和目录，不能仅凭名称推断。坏块数和最大擦除计数也不足以证明 Flash 剩余寿命。

## 制作文件系统前的参数

- `mkfs.ubifs -m` 取目标最小 I/O 单位，此例为 2048。
- `-e` 取实际 LEB 大小，此例为 126976；其计算取决于头部布局和 sub-page 等参数，不能对所有器件固定使用 `PEB - 2 × page`。
- `-c` 是 UBIFS 可使用的最大 LEB 数，应按**目标卷**容量与增长策略确定，不能直接取整个 UBI 的 902 块；例如当前 rootfs 卷只有 678 LEB。
- `mkfs.ubifs` 生成文件系统镜像，完整 UBI 镜像还需要卷布局和相应打包步骤。擦写前使用板卡支持的 NAND/UBI 更新工具核对目标。

实际板级构建命令见 [[Purple Pi R1 开发记录]]。

参考：[Linux v6.1 UBI 参数解析](https://github.com/torvalds/linux/blob/v6.1/drivers/mtd/ubi/build.c)。
