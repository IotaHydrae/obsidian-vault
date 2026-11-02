# 换成 USB 3.0 读卡器会更快吗？—— 不会（实测）

> 本文是 [sd-card-performance.md](sd-card-performance.md) 的**对照实验**：同一张卡、同一 ext4、
> 同一套 fio 参数，换到 USB 3.0 读卡器上再测一遍 —— 结论是**反而更慢**，以及为什么
> （走 BOT 不是 UAS + 内核限制）。
> 板载 SD 控制器的性能画像见主篇与它的"数据篇"。
> ⚠️ 正文**原样搬自主篇的 §9**（节号保留）。

## TL;DR

- 先确认它**确实接在 SuperSpeed 口上**（§9.1），但传输**走 BOT 不是 UAS**、还被内核加了限制（§9.2）。
- 实测对比（§9.3）与原因分析（§9.4）：**读卡器反而更慢**，所以板载 SD 槽就是本项目的最优解。


## 9. 换成 USB 3.0 读卡器会更快吗？—— 不会（实测）

同一张卡、同一个文件系统、同一套 fio 参数，只把卡从板子的 **SD 插槽**换到
**USB 3.0 读卡器**（插在板子的 USB3 口）再测一遍。

### 9.1 先确认：确实接在 USB 3.0（SuperSpeed）口上

```
$ lsusb -t
/:  Bus 002.Port 001: Dev 001, Class=root_hub, Driver=xhci-hcd/1p, 5000M
    |__ Port 001: Dev 002, If 0, Class=Mass Storage, Driver=usb-storage, 5000M
$ lsusb -d 05e3:0749
Bus 002 Device 002: ID 05e3:0749 Genesys Logic, Inc. SD Card Reader and Writer

$ cat /sys/bus/usb/devices/2-1/speed ; cat /sys/bus/usb/devices/2-1/version
5000
 3.10
$ sudo dmesg | grep -E "usb 2-1|usb-storage|sda"
[ 9777.751512] usb 2-1: new SuperSpeed USB device number 2 using xhci-hcd
[ 9777.773836] usb-storage 2-1:1.0: USB Mass Storage device detected
[ 9777.774219] usb-storage 2-1:1.0: Quirks match for vid 05e3 pid 0749: 420
[ 9779.299133] sd 0:0:0:0: [sda] 2000011264 512-byte logical blocks: (1.02 TB/954 GiB)
[ 9779.299824] sd 0:0:0:0: [sda] Mode Sense: 21 00 00 00
[ 9779.300425] sd 0:0:0:0: [sda] Write cache: disabled, read cache: enabled, doesn't support DPO or FUA
[ 9779.867716] EXT4-fs (sda1): mounting with "discard" option, but the device does not support discard
```

- **是 USB 3.0**：挂在 `xhci-hcd` 的 5000M 根集线器上（Bus 002 Port 001），
  `speed=5000`、`version=3.10`、dmesg `new SuperSpeed USB device` ✓。
- 块设备 `/dev/sda`，`sda1` 就是那张卡上的 ext4，自动挂到 `/media/board/Data1T`（路径不变）。
- 产品串是 `USB3.0 Card Reader` / 厂商 `Generic`，`bMaxPower=896mA`。

### 9.2 但它走的是 BOT（不是 UAS），而且被内核加了限制

| 项 | 值 | 说明 |
| --- | --- | --- |
| 接口 class/subclass/protocol | `08 / 06 / 50` | **设备自称支持 UAS**（protocol 0x50） |
| 实际驱动 | `usb-storage`（BOT） | 不是 `uas`（`uas` 是 builtin，但没绑上） |
| 内核 quirk | `Quirks match for vid 05e3 pid 0749: 420` | 内核 `unusual_devs.h` 里就有这个 VID/PID |
| `max_sectors_kb` / `max_hw_sectors_kb` | **64 / 64** | 单条命令最多 128 个扇区（64 KiB）；调大到 1024 报 `Invalid argument` |
| `nr_requests` | 2 | 加上 BOT 本身「一次只能一个命令在飞」⇒ 队列深度实际是 1 |
| `rotational` | 1 | usb-storage 的老默认值 |
| 写缓存 | `Write cache: disabled` | 读卡器不向上报告写缓存，也不支持 DPO/FUA |
| TRIM | **不支持** | `fstrim: the discard operation is not supported` |

内核源码里这条 quirk（`drivers/usb/storage/unusual_devs.h:930`，板上 `build/linux-rockchip` 实测）：

```c
UNUSUAL_DEV(  0x05e3, 0x0749, 0x0000, 0xffff,
		"Genesys Logic",
		"USB Storage",
		USB_SC_DEVICE, USB_PR_DEVICE, NULL,
		US_FL_GO_SLOW | US_FL_MAX_SECTORS_64 | US_FL_IGNORE_RESIDUE ),
```

`US_FL_GO_SLOW`（命令之间加延时）+ `US_FL_MAX_SECTORS_64`（传输上限 64 扇区）+
`US_FL_IGNORE_RESIDUE`（忽略残留字节数）—— 这三个 flag 就是 420（`0x1A4`）。
**也就是说：内核认为这颗芯片要"慢点、小块点"才稳**，USB3 的带宽优势在这里基本被抵消。

### 9.3 实测对比（同一张卡、同一 ext4、同一套 fio 参数）

| 项目 | SD 插槽（`mmcblk1p1`） | USB3 读卡器（`sda1`） | 读卡器 |
| --- | --- | --- | --- |
| 顺序读 1M QD1 | 81.1 MiB/s | 71.9 MiB/s | **慢 11%** |
| 顺序读 1M QD32 | 87.8 MiB/s | 69.7 MiB/s | **慢 21%** |
| 顺序写 1M QD1 | 74.1 MiB/s | 67.5 MiB/s | 慢 9% |
| 顺序写 1M QD32 | 76.9 MiB/s | 67.7 MiB/s | 慢 12% |
| 持续写 5 分钟 | 77.4 MiB/s（22.7 GiB，不掉速） | 66.7 MiB/s（19.5 GiB，不掉速） | 慢 14% |
| 4K 随机读 QD1 | **2 675 IOPS** / 10.5 MiB/s | 1 504 IOPS / 5.9 MiB/s | **慢 44%** |
| 4K 随机读 QD32 | **2 925 IOPS** / 11.4 MiB/s | 1 511 IOPS / 5.9 MiB/s | **慢 48%（QD 完全没用）** |
| 4K 随机写 QD1 | 1 091 IOPS / 4.37 MiB/s | 785 IOPS / 3.07 MiB/s | 慢 28% |
| 4K 随机写 QD32 | 1 201 IOPS / 4.81 MiB/s | 836 IOPS / 3.27 MiB/s | 慢 30% |
| 4K 混合 70 读/30 写 QD32 | 1 199 + 513 IOPS = 6.8 MiB/s | 907 + 388 IOPS = 5.1 MiB/s | 慢 25% |
| 128K 随机读 QD16 | 432 IOPS / 54.1 MiB/s | 376 IOPS / 47.1 MiB/s | 慢 13% |
| 4K 写 + 每次 fsync | 160 IOPS / 643 KiB/s | 180 IOPS / 721 KiB/s | **快 12%**（见下） |
| ioping 4K 单发 | 6.01 ms | 6.19 ms | ≈ |
| ioping 1M 单发 | 18.3 ms | 18.7 ms | ≈ |
| `dd` 裸读 1 GiB（direct） | 86.3 MB/s | **43.1 MB/s** | **慢 50%** |

关键读数（读卡器这一轮的原始输出）：

```
### seq-read-1M-q32
      read: IOPS=69, BW=69.7MiB/s (73.1MB/s)(2092MiB/30014msec)
        slat (usec): min=11920, max=19339, avg=14338.75
        clat (usec): min=1774, max=502351, avg=441501.19     ← 441 ms = 32 × 13.8 ms：命令被串行化了
### rand-read-4k-q32
      read: IOPS=1511, BW=6048KiB/s (6193kB/s)               ← 与 QD1 的 1504 几乎一样
### 持续写 5 分钟
      write: IOPS=66, BW=66.7MiB/s (69.9MB/s)(19.5GiB/300015msec)
### fstrim
fstrim: /media/board/Data1T: the discard operation is not supported
### dd 裸设备顺序读 1 GiB
    1073741824 bytes (1.1 GB, 1.0 GiB) copied, 24.9113 s, 43.1 MB/s
```

### 9.4 为什么读卡器反而更慢

1. **卡本身是 UHS-I（插槽上限 ~104 MB/s），USB3 的 5 Gbps 用不上。**
   插槽侧实测 87.8 MiB/s = 已到卡的接口上限；换条更宽的路对卡没用。
2. **BOT + `US_FL_GO_SLOW` + 64 KiB 上限**：每个 1 MiB 的 fio I/O 被拆成 16 条 64 KiB 命令，
   命令之间还有延时，`slat`（提交阶段）就占 12~14 ms。
3. **队列深度只能是 1**：QD32 的 `clat` 均值 441 ms ≈ 32 × 13.8 ms，
   也就是说 32 个未完成请求还是排队串行执行 —— 4K 随机 QD1→QD32 从 1504 涨到 1511 IOPS（**零收益**），
   而插槽路径同一对数字是 2675 → 2925。
4. **QD32 的 4K 延迟反而更糟**：读卡器 QD32 的 `clat` 均值 20.5 ms（插槽 ~11 ms 反推）、
   写 37 ms/p99.9 169 ms，因为请求全挤在一条命令队列后面。
5. `dd iflag=direct` 只有 43.1 MB/s（fio QD1 却有 71.9）：`dd` 是同步单发，享受不到
   块层的请求流水，对"每命令 12 ms 提交开销"的路径惩罚最大。

**fsync 那一项（读卡器快 12%）不要当成"更安全"**：`[sda] Write cache: disabled` 只说明
读卡器不向上报告写缓存，microSD 内部的页缓存照样存在；本条没做掉电实验，
不能推断掉电语义。它的真实含义是"读卡器的 `SYNCHRONIZE CACHE` 更便宜"。

### 9.5 结论

- **在这块板子上，用这个 USB3 读卡器读这张卡，比插 SD 插槽慢**（顺序 -11~21%、
  4K 随机读 -44%、混合 -25%、裸 dd -50%），只有 fsync 一项略好。
  刷机/拷贝大文件**继续用 SD 插槽**。
- 读卡器的价值在于**热插拔**（不用开机箱/不用板子断电）和**给 PC 用**，不在速度。
- 想真的更快只有换介质：**USB3 SSD/NVMe 硬盘盒**（那才是 400+ MB/s 的东西），
  或者把数据放 eMMC/NVMe 上（eMMC 顺序 231 MB/s、4K 随机 0.335 ms）。
- ⚠️ **换到读卡器后就不支持 TRIM 了**（`fstrim` 报 `discard operation is not supported`，
  ext4 挂载时还会打印 `mounting with "discard" option, but the device does not support discard`）。
  要 trim 这张卡，必须插回 SD 插槽（或换支持 UNMAP 的读卡器）。
- 待验证（**没做，有风险**）：设备自称支持 UAS（protocol `0x50`）、`uas` 也是 builtin，
  理论上可以 `unbind usb-storage` + `bind uas` 试出队列深度 >1；
  但内核既然把它列进 `unusual_devs.h` 并加 `GO_SLOW`/`MAX_SECTORS_64`，
  说明这颗芯片用 BOT 是被验证过的稳妥选择。要试就先 umount，再只做只读验证。

## 10. 相关文档

- [multi-media-label-conflict.md](multi-media-label-conflict.md) —— SD/eMMC 同镜像卷标冲突与 `-sd` 变体
- [swap.md](../system/tuning/swap.md) —— swap 与 `vm.swappiness`
- [build-speed-comparison.md](../build/build-speed-comparison.md) —— 板子原生 vs 编译服务器逐阶段耗时
- [lubancat-5-v2-hardware.md](../hardware/lubancat-5-v2-hardware.md) —— 存储接口（SD/eMMC/NVMe）清单
