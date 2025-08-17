bootargs 也就是 u-boot 传递给内核的 cmdline

```
console=tty0 console=ttyS0,115200 panic=5 rootwait mtdparts=spi0.0:512K@0x20000(u-boot),128K@0x100000(dtb),5M@0x120000(kernel),120M@0x620000(rootfs),-(reserved) rw ubi.mtd=3 rootfstype=ubifs root=ubi0:rootfs init=/linuxrc
```

-  **console=tty0 console=ttyS0,115200** 

    内核消息会输出到本地终端（tty0）。
    同时，内核消息也会通过串口 ttyS0 以 115200 的波特率输出。

-  **panic=5** 

    当内核发生 panic 时，系统会等待 5 秒，然后自动重启。

-  **rootwait** 

    这个参数告诉内核在挂载根文件系统之前，等待根设备（如磁盘或网络存储）准备好。
    如果根设备需要较长时间初始化（例如，USB 磁盘或网络文件系统），rootwait 可以防止内核在设备未准备好时尝试挂载，从而避免启动失败。
    
-  **mtdparts=spi0.0:512K@0x20000(u-boot),128K@0x100000(dtb),5M@0x120000(kernel),120M@0x620000(rootfs),-(reserved)** 

    闪存设备 spi0.0 被划分为 5 个分区：
    u-boot：512 KB，起始地址 0x20000。
    dtb：128 KB，起始地址 0x100000。
    kernel：5 MB，起始地址 0x120000。
    rootfs：120 MB，起始地址 0x620000。
    reserved：剩余空间

-  **rw** 

    这个参数指定根文件系统应以读写模式挂载。
    默认情况下，内核可能会以只读模式挂载根文件系统，以进行文件系统检查（fsck）。rw 参数确保根文件系统直接以读写模式挂载，跳过只读阶段。

-  **ubi.mtd=3** 

    ubi：表示使用 UBI 子系统。
    mtd=3：表示使用 MTD 分区编号为 3 的设备作为 UBI 设备的来源。
    这个参数告诉内核将 MTD 分区 3 初始化为 UBI 设备。

-  **rootfstype=ubifs** 

    根文件系统的类型是 UBIFS。

-  **root=ubi0:rootfs** 

    root=：指定根文件系统的位置。
    ubi0：表示 UBI 设备的编号（第一个 UBI 设备）。
    rootfs：表示 UBI 设备上的卷名称，根文件系统存储在该卷中。

    此参数表示根文件系统位于 UBI 设备 ubi0 的 rootfs 卷中。

-  **init=/linuxrc** 

    表示内核在启动后会执行根文件系统中的 `/linuxrc` 程序