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