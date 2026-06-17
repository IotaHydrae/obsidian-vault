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

