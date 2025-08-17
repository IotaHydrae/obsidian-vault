在 U-Boot（Das U-Boot）中，bootcmd 是一个环境变量，定义了系统启动时自动执行的命令。
它通常用于引导操作系统，比如从 NAND、eMMC、SD 卡、网络（TFTP）、USB 设备等加载内核和根文件系统。

```bash
mtd read spi-nand0 0x80C00000 0x100000 0x4000
```

mtd read：从 MTD 设备（SPI NAND）读取数据。
spi-nand0：目标 MTD 设备，表示 SPI NAND flash 的第一个设备。
0x80C00000：目标 RAM 地址，存放 devicetree（设备树）。
0x100000：SPI NAND Flash 的起始偏移地址（0x100000）。
0x4000：读取的大小（0x4000 = 16 KB）。

作用：从 SPI NAND 的 0x100000 位置读取 16 KB 数据，并存入 RAM 0x80C00000 处（通常是设备树 dtb）。

```bash
mtd read spi-nand0 0x80008000 0x120000 0x500000
```

mtd read：从 MTD 设备读取数据。
spi-nand0：仍然是 SPI NAND flash。
0x80008000：目标 RAM 地址，存放内核 zImage。
0x120000：SPI NAND Flash 的起始偏移地址（0x120000）。
0x500000：读取的大小（0x500000 = 5 MB）。
作用：从 SPI NAND 的 0x120000 位置读取 5 MB 数据，并存入 RAM 0x80008000 处（通常是 zImage）。

```bash
bootz 0x80008000 - 0x80C00000
```

bootz：引导 Linux zImage 格式的内核。
0x80008000：内核（zImage）在 RAM 中的地址。
-：省略第二个参数（根文件系统的 RAM 地址，通常用于 initrd）。
0x80C00000：设备树（dtb）的地址。
作用：启动 Linux 内核，并加载设备树（dtb）。

可以使用如下方式一次性执行多个命令：
```bash
mtd read spi-nand0 0x80C00000 0x100000 0x4000;mtd read spi-nand0 0x80008000 0x120000 0x500000;bootz 0x80008000 - 0x80C00000
```

## 将 bootcmd 保存到 u-boot 配置中

打开menuconfig，搜索 `BOOTCOMMAND` ，定位后，将其中内容替换为上述命令

## 执行结果

```bash
=> mtd read spi-nand0 0x80C00000 0x100000 0x4000;
Reading 16384 byte(s) (8 page(s)) at offset 0x00100000
=> mtd read spi-nand0 0x80008000 0x120000 0x500000;
Reading 5242880 byte(s) (2560 page(s)) at offset 0x00120000
=> bootz 0x80008000 - 0x80C00000
Kernel image @ 0x80008000 [ 0x000000 - 0x468128 ]
## Flattened Device Tree blob at 80c00000
   Booting using the fdt blob at 0x80c00000
Working FDT set to 80c00000
   Loading Device Tree to 816fb000, end 816ffd82 ... OK
Working FDT set to 816fb000
sunxi_simplefb_setup
sunxi_simplefb_fdt_match, offset: 1096

Starting kernel ...

[    0.000000] Booting Linux on physical CPU 0x0
[    0.000000] Linux version 6.13.2-tasks-todo+ (developer@DESKTOP-D4KIBMK) (arm-linux-gnueabihf-gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0, GNU ld (GNU Binutils for Ubuntu) 2.38) #1 Mon Feb 17 13:35:52 +08 2025
[    0.000000] CPU: ARM926EJ-S [41069265] revision 5 (ARMv5TEJ), cr=0005317f
[    0.000000] CPU: VIVT data cache, VIVT instruction cache
[    0.000000] OF: fdt: Machine model: Recoil Tech Tasks To Do
[    0.000000] Memory policy: Data cache writeback
[    0.000000] Zone ranges:
[    0.000000]   Normal   [mem 0x0000000080000000-0x0000000083dfffff]
[    0.000000] Movable zone start for each node
[    0.000000] Early memory node ranges
[    0.000000]   node   0: [mem 0x0000000080000000-0x0000000083dfffff]
[    0.000000] Initmem setup node 0 [mem 0x0000000080000000-0x0000000083dfffff]
[    0.000000] OF: reserved mem: Reserved memory: No reserved-memory node in the DT
[    0.000000] Kernel command line: 
[    0.000000] printk: log buffer data + meta data: 131072 + 409600 = 540672 bytes
[    0.000000] Dentry cache hash table entries: 8192 (order: 3, 32768 bytes, linear)
[    0.000000] Inode-cache hash table entries: 4096 (order: 2, 16384 bytes, linear)
[    0.000000] Built 1 zonelists, mobility grouping on.  Total pages: 15872
[    0.000000] mem auto-init: stack:off, heap alloc:off, heap free:off
[    0.000000] SLUB: HWalign=32, Order=0-3, MinObjects=0, CPUs=1, Nodes=1
[    0.000000] NR_IRQS: 16, nr_irqs: 16, preallocated irqs: 16
[    0.000014] sched_clock: 32 bits at 24MHz, resolution 41ns, wraps every 89478484971ns
[    0.000155] clocksource: timer: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 79635851949 ns
[    0.001024] Console: colour dummy device 80x30
[    0.001100] printk: legacy console [tty0] enabled
[    0.002435] Calibrating delay loop... 203.16 BogoMIPS (lpj=1015808)
[    0.070420] CPU: Testing write buffer coherency: ok
[    0.070872] pid_max: default: 32768 minimum: 301
[    0.071573] Mount-cache hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.071799] Mountpoint-cache hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.100552] Setting up static identity map for 0x80100000 - 0x8010003c
[    0.103042] Memory: 50260K/63488K available (7168K kernel code, 654K rwdata, 1992K rodata, 1024K init, 254K bss, 12840K reserved, 0K cma-reserved)
[    0.104882] devtmpfs: initialized
[    0.113016] clocksource: jiffies: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 19112604462750000 ns
[    0.113294] futex hash table entries: 256 (order: -1, 3072 bytes, linear)
[    0.113776] pinctrl core: initialized pinctrl subsystem
[    0.117137] NET: Registered PF_NETLINK/PF_ROUTE protocol family
[    0.119575] DMA: preallocated 256 KiB pool for atomic coherent allocations
[    0.122747] thermal_sys: Registered thermal governor 'step_wise'
[    0.122809] thermal_sys: Registered thermal governor 'user_space'
[    0.123062] cpuidle: using governor menu
[    0.147460] SCSI subsystem initialized
[    0.148078] usbcore: registered new interface driver usbfs
[    0.148432] usbcore: registered new interface driver hub
[    0.148750] usbcore: registered new device driver usb
[    0.156683] clocksource: Switched to clocksource timer
[    0.212789] NET: Registered PF_INET protocol family
[    0.213661] IP idents hash table entries: 2048 (order: 2, 16384 bytes, linear)
[    0.215883] tcp_listen_portaddr_hash hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.216160] Table-perturb hash table entries: 65536 (order: 6, 262144 bytes, linear)
[    0.216326] TCP established hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.216500] TCP bind hash table entries: 1024 (order: 1, 8192 bytes, linear)
[    0.216793] TCP: Hash tables configured (established 1024 bind 1024)
[    0.217239] UDP hash table entries: 256 (order: 1, 8192 bytes, linear)
[    0.217464] UDP-Lite hash table entries: 256 (order: 1, 8192 bytes, linear)
[    0.218191] NET: Registered PF_UNIX/PF_LOCAL protocol family
[    0.220645] RPC: Registered named UNIX socket transport module.
[    0.220844] RPC: Registered udp transport module.
[    0.220939] RPC: Registered tcp transport module.
[    0.221019] RPC: Registered tcp-with-tls transport module.
[    0.221101] RPC: Registered tcp NFSv4.1 backchannel transport module.
[    0.232240] Initialise system trusted keyrings
[    0.233510] workingset: timestamp_bits=30 max_order=14 bucket_order=0
[    0.946003] Key type asymmetric registered
[    0.946197] Asymmetric key parser 'x509' registered
[    0.946638] Block layer SCSI generic (bsg) driver version 0.4 loaded (major 252)
[    0.946964] io scheduler mq-deadline registered
[    0.947072] io scheduler kyber registered
[    1.300206] Serial: 8250/16550 driver, 8 ports, IRQ sharing disabled
[    1.319566] [drm] Initialized simpledrm 1.0.0 for 83e00000.framebuffer on minor 0
[    1.378983] Console: switching to colour frame buffer device 60x50
[    1.428357] simple-framebuffer 83e00000.framebuffer: [drm] fb0: simpledrmdrmfb frame buffer device
[    1.434355] usbcore: registered new interface driver rtl8xxxu
[    1.435868] usbcore: registered new interface driver usb-storage
[    1.437502] i2c_dev: i2c /dev entries driver
[    1.443243] sunxi-wdt 1c20ca0.watchdog: Watchdog enabled (timeout=16 sec, nowayout=0)
[    1.448015] usbcore: registered new interface driver usbhid
[    1.448448] usbhid: USB HID core driver
[    1.449971] NET: Registered PF_INET6 protocol family
[    1.458383] Segment Routing with IPv6
[    1.459101] In-situ OAM (IOAM) with IPv6
[    1.459739] sit: IPv6, IPv4 and MPLS over IPv4 tunneling driver
[    1.462721] NET: Registered PF_PACKET protocol family
[    1.463550] Key type dns_resolver registered
[    1.512346] Loading compiled-in X.509 certificates
[    1.587448] gpio gpiochip0: Static allocation of GPIO base is deprecated, use dynamic allocation.
[    1.645931] suniv-f1c100s-pinctrl 1c20800.pinctrl: initialized sunXi PIO driver
[    1.668505] suniv-f1c100s-pinctrl 1c20800.pinctrl: supply vcc-pa not found, using dummy regulator
[    1.732706] 1c25400.serial: ttyS0 at MMIO 0x1c25400 (irq = 116, base_baud = 6250000) is a 16550A
[    1.753496] printk: legacy console [ttyS0] enabled
[    2.339308] suniv-f1c100s-pinctrl 1c20800.pinctrl: supply vcc-pc not found, using dummy regulator
[    2.370301] sun6i-spi 1c05000.spi: Failed to request TX DMA channel
[    2.397624] sun6i-spi 1c05000.spi: Failed to request RX DMA channel
[    2.447809] spi-nand spi0.0: Winbond SPI NAND was found.
[    2.464416] spi-nand spi0.0: 128 MiB, block size: 128 KiB, page size: 2048, OOB size: 64
[    2.521571] usb_phy_generic usb_phy_generic.0.auto: dummy supplies not allowed for exclusive requests (id=vbus)
[    2.578317] suniv-f1c100s-pinctrl 1c20800.pinctrl: supply vcc-pf not found, using dummy regulator
[    2.633839] cfg80211: Loading compiled-in X.509 certificates for regulatory database
[    2.694071] Loaded X.509 cert 'sforshee: 00b28ddf47aef9cea7'
[    2.742006] sunxi-mmc 1c0f000.mmc: initialized, max. request size: 16384 KB
[    2.798846] Loaded X.509 cert 'wens: 61c038651aabdcf94bd0ac7ff06c7248db18c600'
[    2.828791] clk: Disabling unused clocks
[    2.864985] platform regulatory.0: Direct firmware load for regulatory.db failed with error -2
[    2.896180] cfg80211: failed to load regulatory.db
[    2.913468] List of all partitions:
[    2.928412] 1f00          131072 mtdblock0 
[    2.928466]  (driver?)
[    2.957099] No filesystem could mount root, tried: 
[    2.957134] 
[    2.985409] Kernel panic - not syncing: VFS: Unable to mount root fs on "" or unknown-block(0,0)
[    3.015602] CPU: 0 UID: 0 PID: 1 Comm: swapper Not tainted 6.13.2-tasks-todo+ #1
[    3.044404] Hardware name: Allwinner suniv Family
[    3.060035] Call trace: 
[    3.060089]  unwind_backtrace from show_stack+0x10/0x14
[    3.089635]  show_stack from dump_stack_lvl+0x38/0x48
[    3.105764]  dump_stack_lvl from panic+0xec/0x318
[    3.121277]  panic from mount_root_generic+0x228/0x29c
[    3.137425]  mount_root_generic from prepare_namespace+0x1a0/0x230
[    3.165179]  prepare_namespace from kernel_init+0x10/0x12c
[    3.192124]  kernel_init from ret_from_fork+0x14/0x28
[    3.208100] Exception stack(0xc400dfb0 to 0xc400dff8)
[    3.224001] dfa0:                                     00000000 00000000 00000000 00000000
[    3.253424] dfc0: 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
[    3.282630] dfe0: 00000000 00000000 00000000 00000000 00000013 00000000
[    3.310358] ---[ end Kernel panic - not syncing: VFS: Unable to mount root fs on "" or unknown-block(0,0) ]---
```