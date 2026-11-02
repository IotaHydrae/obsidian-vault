# SD 卡性能实测（SanDisk 1 TB microSD，LubanCat-5 V2）

> 测试日期：2026-09-12；平台：开发板 `lubancat-5-v2`（内核 6.1.118，rootfs 在 eMMC）。
> 目的：这张 1 TB 卡以后当**数据盘**用，先把它的真实能力摸清楚（也顺便确定它能不能当
> "SD 卡启动 → 刷工程机 eMMC"的介质，见 [multi-media-label-conflict.md](multi-media-label-conflict.md)）。

## TL;DR

- 这张 SanDisk 1 TB microSD 的**性能天花板由协商到的总线模式决定**（见 §2，先看这一节再读数字）。
- 实测结果在 §4；与板载 eMMC 的对照在 §5。
- ⛔ **别把它当系统盘/swap**：系统 swap 走 eMMC（见 [swap.md](../system/tuning/swap.md)）。
- 测法与可复现命令见 §3；TRIM / fstrim 见 §6。

## 0. 一句话结论

| 用法 | 合不合格 | 依据 |
| --- | --- | --- |
| 大文件顺序读写（视频、日志归档、镜像、整盘拷贝） | ✅ 合格 | 顺序读 **87.8 MiB/s**、顺序写 **76.9 MiB/s**，5 分钟持续写不掉速 |
| 大量小文件 / 随机 I/O（数据库、VM 镜像、git 仓库、`rsync` 增量、编译） | ❌ 不合格 | 4K 随机读单队列 **6.01 ms → 166 IOPS**（eMMC 是 0.34 ms），4K 混合随机只有 **6.8 MiB/s** |
| 当系统盘 / 放 swapfile | ❌ 不要 | 随机延迟是 eMMC 的 ~18 倍，swap 换页会把整机拖住 |

**顺序性能已经顶到板子 SD 控制器的天花板**（见 §2）：换更快的卡（UHS-II、A2）在**这块板子**上不会更快。

## 1. 被测对象与环境

### 1.1 卡片身份（`/sys/class/mmc_host/mmc1/mmc1:d555/`）

| 字段 | 值 |
| --- | --- |
| `name` | `SM01T` |
| `manfid` / `oemid` | `0x000003`（SanDisk）/ `0x5344`（`SD`） |
| `serial` | `0x95f6dab2` |
| `date` | `11/2025`（出厂日期，卡很新） |
| `fwrev` / `hwrev` | `0x7` / `0x8` |
| `type` | `SD` |
| 容量 | 953.68 GiB（`/dev/mmcblk1`） |

### 1.2 分区与文件系统

```
$ findmnt -no SOURCE,TARGET,FSTYPE,OPTIONS /media/board/Data1T
/dev/mmcblk1p1 /media/board/Data1T ext4   rw,nosuid,nodev,relatime,errors=remount-ro

$ sudo fdisk -l /dev/mmcblk1
Disklabel type: dos
Device         Boot  Start        End    Sectors   Size Id Type
/dev/mmcblk1p1      131072 2000011263 1999880192 953.6G  7 HPFS/NTFS/exFAT
```

- 出厂是**单分区、MBR、类型 `0x07`（HPFS/NTFS/exFAT）**，但里面格式化的是 **ext4**
  （`Block size: 4096`，含 `has_journal`/`64bit`/`metadata_csum`）。类型码与内容不一致只是出厂遗留，
  功能无影响；要给别人用（Windows）才需要重新分区。
- 挂载项里**没有 `discard`**：所以 TRIM 是手动的（`fstrim` / `fstrim.timer`，见 §5.4）。
- 测试前用量：953 GiB 里只用了 46 MB（几乎是新卡），测完 `df` 显示 28K 已用、890G 可用。

### 1.3 工具与测试条件

| 项 | 值 |
| --- | --- |
| fio | `fio-3.36` |
| ioping | `ioping 1.3` |
| 测试文件 | `/media/board/Data1T/.lsp-fio.dat`（隐藏文件，测完 `rm` + `sync` 删除，用户数据未动） |
| 全部用 `--direct=1` | 绕过页缓存，测的是卡本身而不是内存 |
| 并发 | 测试时板上 `pgrep -cf 'build-lsp-image\|apt-get\|dpkg'` = 0，没有构建/apt 在跑 |

## 2. 协商到的总线模式 —— 这就是天花板

```
$ sudo cat /sys/kernel/debug/mmc1/ios
clock:		200000000 Hz
actual clock:	198000000 Hz
vdd:		21 (3.3 ~ 3.4 V)
bus mode:	2 (push-pull)
chip select:	0 (don't care)
power mode:	2 (on)
bus width:	2 (4 bits)
timing spec:	6 (sd uhs SDR104)
signal voltage:	1 (1.80 V)
driver type:	0 (driver type B)
```

- `SDR104` + 4 bit + 实际 198 MHz ⇒ 198 MT/s × 4 bit = **99 MB/s 理论上限**。
- 实测顺序读 **92.0 MB/s = 上限的 93%** ⇒ **瓶颈是板子的 SD 控制器（`fe2c0000.mmc`），不是卡。**
  换 UHS-II 卡没用（该控制器不支持），想更快只能走 USB3 / NVMe。
- 实测确认调相正常：`dwmmc_rockchip fe2c0000.mmc: Successfully tuned phase to 50/51`。

## 3. 测试方法 / ## 4. 实测结果 → 见 [sd-card-benchmarks.md](sd-card-benchmarks.md)

测试条件与全部实测数据（顺序/随机/ioping 延迟/持续写 5 分钟/一次异常）**已搬成单独一篇** →
**[sd-card-benchmarks.md](sd-card-benchmarks.md)**。


## 5. 与 eMMC 的对照（同一块板子）

| 项目 | SD 卡（mmcblk1p1，ext4） | eMMC（mmcblk0p2，rootfs ext4） | 倍率 |
| --- | --- | --- | --- |
| 4K 随机读延迟（ioping `-D -c 20 -s 4k`） | **6.01 ms** | **0.335 ms**（304–403 µs） | SD 慢 **17.9×** |
| 1 MiB 读延迟（ioping `-D -c 10 -s 1M`） | 18.3 ms（54.5 MiB/s） | **4.87 ms**（205 MiB/s） | SD 慢 3.8× |
| 顺序读（`dd iflag=direct`） | 86.3 MB/s（1 GiB） | **223 MB/s**（1 GiB）／231 MB/s（2 GiB，实测两次） | SD 慢 2.6× |
| 顺序写 | 74–78 MiB/s（fio） | 早前同法实测 163 MB/s | SD 慢 ~2.1× |
| 容量 | 953 GiB | 装在板子上的 eMMC（用量见 `df`） | — |

```
$ sudo ioping -D -c 20 -s 4k /var/tmp     # eMMC
    min/avg/max/mdev = 304.2 us / 335.5 us / 403.1 us / 28.2 us
$ sudo dd if=/dev/mmcblk0 of=/dev/null bs=1M count=2048 iflag=direct
    2147483648 bytes (2.1 GB, 2.0 GiB) copied, 9.3098 s, 231 MB/s
```

结论：**eMMC 不只是更快，是快一个数量级（随机）**。SD 卡适合当仓库，eMMC/SSD 适合跑系统。

## 6. fstrim / TRIM

```
$ sudo fstrim -v /media/board/Data1T
/media/board/Data1T: 871.5 GiB (935799222272 bytes) trimmed
$ systemctl is-enabled fstrim.timer ; systemctl is-active fstrim.timer
enabled / active
```

- **卡支持 discard/TRIM**，871.5 GiB（几乎全部空闲空间）一次 trim 很快就完成了。
- 挂载项里没有 `discard`（这是对的：`discard` 会让删除变慢且抖动大），靠 **`fstrim.timer` 每周一次**
  就够 —— 镜像里它默认 `enabled/active`，不用额外配置。
- 大量删数据（> 几十 GB）后如果马上要写，手动 `sudo fstrim -v /media/board/Data1T` 一次能让写入更稳。

## 7. 用它当数据盘的实践建议

1. **只放"大块顺序"数据**：视频/图片/日志归档/整盘备份/镜像文件。
   避免：数据库、VM/容器根目录、git 工作区、几十万个小文件的目录树。
   `rsync -a` 这种元数据密集的同步在这张卡上会很慢（每个小文件都要 ~6 ms 起步）。
2. **别把 `swapfile` 放在这张卡上**（系统 swap 走 eMMC/SSD；见 [swap.md](../system/tuning/swap.md)）。
3. **别拿它当系统盘**：开机 + apt + 桌面会产生大量 4K 随机 I/O，6 ms 的单发随机延迟会让整机明显发卡。
4. **拷贝大文件用大块顺序读**（`dd bs=4M`、`cp` 默认 128K～1M 都行），
   别用大量小 I/O；`direct=1` 与缓冲读的差别主要是页缓存，不影响卡的上限。
5. `read_ahead_kb` 当前是 128（`/sys/block/mmcblk1/queue/read_ahead_kb`）。
   它只影响**缓冲读**，对 `direct=1`、`dd iflag=direct` 无效；要提速先确认应用用的是大块顺序读。
   顺手可试 512～1024（`echo 1024 > .../read_ahead_kb` 即时生效、重启丢失），**本 note 未测其收益**。
6. **写延迟在压力下是 0.1~1 秒级**（QD32 平均 411 ms、p99.5 785 ms）。
   写入密集的应用要留缓冲/异步，不要指望它响应及时。
7. **当"刷机介质"完全够用**：整镜像写（5.2 GB 级）顺序写 76 MiB/s ⇒ ~1 分多钟；
   反而 `rsync` 增量同步（`tools/update-emmc.sh`）会踩 §4.2 的小块弱点，
   刷整套系统**优先用整镜像**（`tools/write-sd.sh` / `tools/make-sd-image.sh`）。
8. **别为了"更快"把卡插到 USB3 读卡器上** —— 实测比 SD 插槽更慢（§9），
   而且换过去就没有 TRIM 了。

## 8. 复现命令清单

```bash
# 板上（root）
sudo apt-get install -y fio ioping
bash /tmp/fio-suite.sh                      # 完整套件，约 13 分钟，结果在 /tmp/fio-summary.txt
sudo ioping -D -c 50 -s 4k /media/board/Data1T
sudo fstrim -v /media/board/Data1T

# 对照 eMMC
sudo ioping -D -c 20 -s 4k /var/tmp
sudo dd if=/dev/mmcblk0 of=/dev/null bs=1M count=1024 iflag=direct

# 总线模式 / 调相
sudo cat /sys/kernel/debug/mmc1/ios
sudo dmesg | grep -E "mmc1|tuned phase"
cat /sys/block/mmcblk1/queue/{scheduler,nr_requests,rotational,read_ahead_kb,max_sectors_kb}
sudo tune2fs -l /dev/mmcblk1p1 | grep -E "Block size|Filesystem features|Lifetime writes"

# 换成 USB3 读卡器再测一遍（§9）
lsusb -t | grep -A2 5000M                 # 确认挂在 SuperSpeed 根集线器上
cat /sys/bus/usb/devices/2-1/{speed,version}
sudo dmesg | grep -E "usb 2-1|usb-storage|\[sda\]"
cat /sys/block/sda/queue/{max_sectors_kb,max_hw_sectors_kb,nr_requests,rotational}
lsblk -o NAME,SIZE,TRAN,MOUNTPOINT
bash /tmp/fio-usb.sh                       # 与插槽同一套参数，结果在 /tmp/fio-summary-usb.txt
```

## 9. 换成 USB 3.0 读卡器会更快吗 → 见 [sd-card-usb-reader-comparison.md](sd-card-usb-reader-comparison.md)

同卡同参数换 USB3 读卡器的对照实验（结论：**更慢**，因为 BOT 而非 UAS + 内核限制）**已搬成单独一篇** →
**[sd-card-usb-reader-comparison.md](sd-card-usb-reader-comparison.md)**。
