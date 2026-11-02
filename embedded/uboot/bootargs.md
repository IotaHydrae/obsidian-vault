# U-Boot bootargs：SPI-NAND 与 UBIFS 根文件系统

> `bootargs` 传给内核作为命令行；MTD 分区、UBI 设备和卷名必须与实际镜像一致。

```text
console=tty0 console=ttyS0,115200 panic=5 rootwait mtdparts=spi0.0:512K@0x20000(u-boot),128K@0x100000(dtb),5M@0x120000(kernel),120M@0x620000(rootfs),-(reserved) rw ubi.mtd=3 rootfstype=ubifs root=ubi0:rootfs init=/linuxrc
```

该示例定义以下分区，未分配的间隙不会自动属于前一分区：

| 分区 | 起始偏移 | 大小 |
| --- | --- | --- |
| u-boot | `0x20000` | 512 KiB |
| dtb | `0x100000` | 128 KiB |
| kernel | `0x120000` | 5 MiB |
| rootfs | `0x620000` | 120 MiB |
| reserved | 紧接 rootfs | 剩余空间 |

- `console=tty0 console=ttyS0,115200`：配置控制台；可用性及多个 console 的行为以目标内核为准。
- `panic=5`：发生 panic 后等待 5 秒重启。
- `rootwait`：等待根设备出现，不是所有网络或 UBI 初始化问题的通用修复。
- `ubi.mtd=3`：将 MTD 编号 3 附加为 UBI；编号可能变化，先核对 `/proc/mtd`。
- `root=ubi0:rootfs rootfstype=ubifs`：挂载 ubi0 中的 rootfs 卷。
- `rw`：请求以读写方式挂载根文件系统，不等同于“跳过文件系统检查”。
- `init=/linuxrc`：指定初始用户空间程序，需存在且可执行。

`spi0.0` 必须匹配内核实际识别的 MTD 设备名。启动后检查 `/proc/cmdline`、`/proc/mtd`、`ubinfo -a` 和挂载结果；UBI 参数细节见 [[ubifs]]。
