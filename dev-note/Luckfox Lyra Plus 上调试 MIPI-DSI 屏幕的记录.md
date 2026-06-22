
编译更新内核

```bash
./build.sh kernel
adb push kernel-6.1/zboot.img /root
```

如果设备使用 SPI-NAND 启动，你可以通过如下方式确定内核镜像处于哪一个分区

```bash
cat /proc/mtd
```

对于 SD 卡启动的

```bash
root@luckfox:/# fdisk /dev/mmcblk0
Found valid GPT with protective MBR; using GPT


Command (m for help): p
Disk /dev/mmcblk0: 61069312 sectors, 1147M
Logical sector size: 512
Disk identifier (GUID): 23000000-0000-4c4a-8000-699000005abb
Partition table holds up to 128 entries
First usable sector is 34, last usable sector is 61069278

Number  Start (sector)    End (sector)  Size Name
     1            8192           16383 4096K uboot
     2           16384           40959 12.0M boot
     3           65536        61069247 29.0G rootfs
```

烧录时，如果从 SPI-NAND启动，使用

```
dd if=/root/zboot.img of=/dev/mtdblockx bs=1M && sync
```

从SD卡启动则使用

```
dd if=/root/zboot.img of=/dev/mmcblk0p2 bs=1M && sync
```