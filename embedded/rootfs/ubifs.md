本文所使用的开发板为 Industio 的 Purple Pi R1 （64 + 128M） 

| 部件  | 规格                            |
| --- | ----------------------------- |
| 开发板 | Purple Pi R1 （IDO-SBC2D06）    |
| CPU | SSD201 2 x Cortex-A7 @ 1.2GHz |
 ## /proc/cmdline

这是一个启动参数的示例，设备从 SPI NAND 启动

```
ubi.mtd=UBI,2048 root=ubi:rootfs rw rootfstype=ubifs init=/linuxrc rootwait=1 mtdparts=nand0:384k@1280k(IPL0),384k(IPL1),384k(IPL_CUST0),384k(IPL_CUST1),768k(UBOOT0),768k(UBOOT1),256k(ENV),256k(ENV1),0x20000(KEY_CUST),0x60000(LOGO),0x500000(KERNEL),0x500000(RECOVERY),-(UBI)
```

`ubi.mtd=UBI,2048`
	绑定名为 `UBI` 的 MTD 分区为 UBI 设备，2048 为 UBI 块相关参数（PEB 最小单元配置）

`root=ubi:rootfs`
	根文件系统位于 UBI 卷 `rootfs`

`rootfstype=ubifs`
    根文件系统格式为 UBIFS（NAND Flash 专用日志文件系统）

`rw`
    根分区以**读写模式**挂载，非只读

`rootwait=1`
    内核挂载 root 前阻塞等待 UBI 设备初始化完成，避免启动报 `can't mount root`

`mtdparts=nand0:分区定义@偏移(分区名)`

设备：`nand0` 片选 0 NAND 闪存

表格

|分区名|大小|偏移|作用|
|---|---|---|---|
|IPL0|384k|1280k|一级启动固件 A 备份|
|IPL1|384k|接续|一级启动固件 B 备份|
|IPL_CUST0|384k|接续|客户定制一级固件 A|
|IPL_CUST1|384k|接续|客户定制一级固件 B|
|UBOOT0|768k|接续|U-Boot 引导程序 A|
|UBOOT1|768k|接续|U-Boot 引导程序 B（双备份防砖）|
|ENV|256k|接续|U-Boot 环境变量分区 A|
|ENV1|256k|接续|U-Boot 环境变量分区 B|
|KEY_CUST|0x20000(128k)|接续|客户密钥、加密证书存储|
|LOGO|0x60000(384k)|接续|开机 logo 图片资源|
|KERNEL|0x500000(5MB)|接续|主系统内核镜像|
|RECOVERY|0x500000(5MB)|接续|恢复模式内核（OTA / 救砖）|
|UBI|剩余全部空间|接续|UBI 总分区，存放 UBIFS 根文件系统 rootfs|

`-(UBI)` 语法含义：NAND 所有剩余未分配空间全部划给 UBI 分区。

## /proc/mtd

```bash
# cat /proc/mtd 
dev:    size   erasesize  name
mtd0: 00060000 00020000 "IPL0"
mtd1: 00060000 00020000 "IPL1"
mtd2: 00060000 00020000 "IPL_CUST0"
mtd3: 00060000 00020000 "IPL_CUST1"
mtd4: 000c0000 00020000 "UBOOT0"
mtd5: 000c0000 00020000 "UBOOT1"
mtd6: 00040000 00020000 "ENV"
mtd7: 00040000 00020000 "ENV1"
mtd8: 00020000 00020000 "KEY_CUST"
mtd9: 00060000 00020000 "LOGO"
mtd10: 00500000 00020000 "KERNEL"
mtd11: 00500000 00020000 "RECOVERY"
mtd12: 070c0000 00020000 "UBI"
```

### 字段说明

- `dev`：mtd 设备号 mtd0~mtd12
- `size`：分区总大小（十六进制字节）
- `erasesize`：NAND 擦除块大小 `0x20000 = 128KB`
- `name`：分区名，和内核 cmdline 一一对应

### 逐分区换算与用途

#### 换算速记（1KB=0x400 字节）

- `0x20000` = 128KB（擦除块 erasesize）
- `0x60000` = 384KB
- `0xC0000` = 768KB
- `0x40000` = 256KB
- `0x500000` = 5MB
- `0x70C0000` = 112.75MB

- **擦除块统一 128KB**
    
    NAND 闪存最小擦除单元 128KB，所有分区对齐擦除块，是嵌入式 NAND 标准布局。
    
- **全链路双备份设计**
    
    IPL、UBOOT、ENV 全部做 A/B 双分区：
    
    升级写入备用分区，校验通过后切换；升级断电 / 坏块不会变砖，安防 IPC 标准高可靠方案。
    
- mtd12 = UBI 总容器
    
    内核启动参数 `ubi.mtd=UBI` 就是绑定 `mtd12`，在这个 112.75MB 大分区内创建 UBI 卷：
    
    - `rootfs`：UBIFS 根文件系统，系统应用、配置、数据都存在这里。

## uninfo -a

```bash
# ubinfo -a
UBI version:                    1
Count of UBI devices:           1    # 只有一个UBI设备 ubi0，对应 /dev/mtd12(UBI)
UBI control device major/minor: 10:59
Present UBI devices:            ubi0

ubi0
Volumes count:                           4
Logical eraseblock size:                 126976 bytes, 124.0 KiB
Total amount of logical eraseblocks:     902 (114532352 bytes, 109.2 MiB)
Amount of available logical eraseblocks: 41 (5206016 bytes, 4.9 MiB)
Maximum count of volumes                 128
Count of bad physical eraseblocks:       0
Count of reserved physical eraseblocks:  20
Current maximum erase counter value:     467
Minimum input/output unit size:          2048 bytes
Character device major/minor:            246:0
Present volumes:                         0, 1, 2, 3

Volume ID:   0 (on ubi0)
Type:        dynamic
Alignment:   1
Size:        678 LEBs (86089728 bytes, 82.1 MiB)
State:       OK
Name:        rootfs
Character device major/minor: 246:1
-----------------------------------
Volume ID:   1 (on ubi0)
Type:        dynamic
Alignment:   1
Size:        83 LEBs (10539008 bytes, 10.0 MiB)
State:       OK
Name:        miservice
Character device major/minor: 246:2
-----------------------------------
Volume ID:   2 (on ubi0)
Type:        dynamic
Alignment:   1
Size:        42 LEBs (5332992 bytes, 5.0 MiB)
State:       OK
Name:        customer
Character device major/minor: 246:3
-----------------------------------
Volume ID:   3 (on ubi0)
Type:        dynamic
Alignment:   1
Size:        34 LEBs (4317184 bytes, 4.1 MiB)
State:       OK
Name:        appconfigs
Character device major/minor: 246:4
```

**Logical eraseblock size（LEB 逻辑擦除块）**

`126976 bytes / 124KiB`

物理擦除块 PEB=128KiB (0x20000)，UBI 预留 4KiB 存放卷头 / 擦除计数元数据，可用空间 124KiB。

cmdline `ubi.mtd=UBI,2048` 里的 `2048` = NAND page size 页大小。

**总容量与空闲**

- 总 LEB：902 块 ≈ 109.2MiB（对应 mtd12 总 112.75MB，差值是 UBI 预留块 + 坏块保留区）
- 空闲 LEB：41 块 ≈ 4.9MiB（剩余可分配给卷的空间）
- 预留 PEB：20 块（UBI 预留，用于替换未来出现的坏块，NAND 容错）
- 坏块数：0，当前闪存无物理坏块
- 最大擦除计数：467，这块 UBI 分区整体擦写次数很低，闪存寿命充足

### Volume 0：rootfs（根文件系统，系统主分区）

- 678 LEB ≈ 82.1MiB
- 内核启动参数 `root=ubi:rootfs` 挂载此卷为 `/`，存放系统内核、基础工具、库、系统服务
- 设备节点：`/dev/ubi0_0`

### Volume 1：miservice（媒体业务分区）

- 83 LEB ≈ 10.0MiB
- IPC 摄像头专用：录像缓存、媒体进程持久数据、码流缓存、算法模型临时文件

### Volume 2：customer（客户业务数据）

- 42 LEB ≈ 5.0MiB
- 厂商定制业务、设备绑定信息、用户自定义业务数据、第三方插件数据

### Volume 3：appconfigs（应用配置分区）

- 34 LEB ≈ 4.1MiB
- 所有应用配置文件：网络参数、摄像头参数、账号密码、报警规则、开机配置
    
    优势：和 rootfs 分离，升级系统 rootfs 时不会覆盖用户配置

```
NAND Flash硬件
└── MTD分区 mtd0~mtd11 (IPL/UBOOT/KERNEL/RECOVERY等裸分区)
└── mtd12 "UBI" 整块容器分区
    └── ubi0 UBI设备
        ├─ ubi0_0 rootfs  → 挂载 /  (UBIFS根文件系统)
        ├─ ubi0_1 miservice → 媒体业务挂载目录
        ├─ ubi0_2 customer  → 客户数据挂载目录
        └─ ubi0_3 appconfigs → 配置文件挂载目录
```

