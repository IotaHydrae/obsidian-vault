# Luckfox Lyra Plus：更新内核镜像

> 先确认启动介质和 `boot` 分区，再将编译出的 `zboot.img` 写入对应分区；分区号不可跨设备或镜像版本照搬。

## 编译与传输

在 SDK 根目录执行：

```bash
./build.sh kernel
adb push kernel-6.1/zboot.img /root/
```

## 确认分区

```bash
cat /proc/mtd
fdisk -l /dev/mmcblk0
```

此示例的 SPI-NAND 布局将 `mtd1` 标为 `boot`，SD 卡布局将第二分区标为 `boot`；实际目标以当前分区表和镜像说明为准。

## 写入示例

**写错设备会破坏数据或导致设备无法启动。** 先备份原镜像、确认启动介质与目标分区，并准备可用的恢复方式。

SD 卡写入示例：

```bash
dd if=/root/zboot.img of=/dev/mmcblk0p2 bs=1M && sync
```

SPI-NAND 示例使用对应的 `/dev/mtdblockX`；`X` 必须按当前设备的 `boot` 分区确定，不能直接照搬上面的 SD 卡设备名。写入成功后重启，并结合启动日志和 `uname -a` 检查实际运行的内核。
