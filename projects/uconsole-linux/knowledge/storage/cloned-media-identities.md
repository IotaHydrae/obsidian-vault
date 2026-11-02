# 克隆镜像的 UUID 与混合介质启动

> 同一镜像写到两块介质会复制标识；使用 UUID 或 PARTUUID 本身不能消除克隆冲突。

## TL;DR

- 检查 boot、root、fstab、扩容和更新目标是否属于同一物理介质。
- 卷标、文件系统 UUID 与分区 PARTUUID 都可能随整盘克隆重复。
- 修改标识时同步所有引用并读回核对；不能只改标签或假定 mmc 编号固定。
- CM5 eMMC 型号的 SD 插槽限制与此问题不同，见 [CM5 存储](cm5-storage.md)。

## 混合启动的症状

固件可能从介质 A 读取内核，而 root= 标识解析到介质 B。
在 A 上改文件却看不到、更新写到另一个 boot 分区、扩容作用在错误介质，都可能由此产生。
能启动不代表介质选择满足要求，不能用 rootfs 的主机名判断启动来源。

## 只读核对

```sh
findmnt -no SOURCE,UUID,PARTUUID /
findmnt -no SOURCE,UUID,PARTUUID /boot/firmware
lsblk -o NAME,TYPE,SIZE,FSTYPE,LABEL,UUID,PARTUUID,MOUNTPOINTS
cat /proc/cmdline
cat /etc/fstab
```

在已授权读取的系统执行。这些命令检查当前挂载和标识，不证明固件从何处取了最初启动文件；
后者结合固件日志、启动配置和产物哈希确认。
相同 UUID/PARTUUID 的设备须结合 sysfs 父设备、容量与连接位置区分。

## 消除歧义

可为测试镜像分配独立 disk ID、文件系统 UUID 与卷标，再同步 root=、fstab 和扩容逻辑。
另一种方式是可靠地从当前启动设备推导根分区，但必须有平台和启动链依据，不能硬编码 mmcblk 编号。
仅修改 LABEL 而 root= 用 PARTUUID 时不会解决根引用；仅改 root= 也不能保证 boot 更新目标正确。
文件系统标签变更须在卸载状态下进行，核对文件系统长度约束并读回确认。

## 本仓库状态

Ubuntu prepare（工程源码：`scripts/image_backends/ubuntu_base.py`）为新构建生成 disk ID，
导出器（工程源码：`scripts/image_backends/rpi_image.py`）为 ext4 生成 UUID，并把 PARTUUID 写进 cmdline/fstab。
不同构建通常有独立身份，但同一 `.img` 被写到多块介质后仍会重复。
现有 grow-root 检查第二分区/父盘的一致性，不能替代整个启动链的介质身份验收。
本文是机制与排查方法，没有改动任何镜像或物理存储。
