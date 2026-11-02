# 1 TB SD 卡的实测数据（顺序/随机/延迟/持续写）

> 本文是 [sd-card-performance.md](sd-card-performance.md) 的**数据篇**：测试方法（可复现）与全部实测结果
> ——顺序 I/O、随机 I/O、ioping 单发延迟、持续写 5 分钟（SLC 掉速）、以及一次测试期异常。
> 卡的身份、协商到的总线模式（**天花板**）、与 eMMC 的对照、TRIM/数据盘建议见主篇。
> ⚠️ 正文**原样搬自主篇的 §3~§4**（节号保留）。

## TL;DR

- **天花板由协商到的总线模式决定**（见主篇 §2），不是卡标称值。
- 顺序读写与持续写 5 分钟**不掉速**；随机 4K 是弱项（SD 卡的典型形态）；
  ioping 单发延迟最贴近"点一下等多久"。
- 复现命令见主篇 §8。


## 3. 测试方法（可复现）

脚本在板上 `/tmp/fio-suite.sh`，逐项 30 秒，参数：

```bash
F=/media/board/Data1T/.lsp-fio.dat
run(){ fio --name="$1" --filename="$F" --direct=1 --ioengine=libaio --group_reporting \
           --time_based --runtime=30 --randrepeat=1 --unlink=0 "${@:2}"; }

run seq-read-1M-q1      --rw=read      --bs=1M  --iodepth=1  --size=2G
run seq-read-1M-q32     --rw=read      --bs=1M  --iodepth=32 --size=2G
run seq-write-1M-q1     --rw=write     --bs=1M  --iodepth=1  --size=2G
run seq-write-1M-q32    --rw=write     --bs=1M  --iodepth=32 --size=2G
run rand-read-4k-q1     --rw=randread  --bs=4k  --iodepth=1  --size=512M
run rand-read-4k-q32    --rw=randread  --bs=4k  --iodepth=32 --size=512M
run rand-write-4k-q1    --rw=randwrite --bs=4k  --iodepth=1  --size=512M
run rand-write-4k-q32   --rw=randwrite --bs=4k  --iodepth=32 --size=512M
run mixed-4k-70r-32     --rw=randrw --rwmixread=70 --bs=4k --iodepth=32 --size=512M
run rand-read-128k-q16  --rw=randread  --bs=128k --iodepth=16 --size=1G
run fsync-4k-q1         --rw=write     --bs=4k  --iodepth=1  --fsync=1 --size=64M

# 持续写 5 分钟（找有没有"写满缓存后掉速"）
fio --name=sustained-write-1M --filename="$F" --direct=1 --ioengine=libaio --group_reporting \
    --rw=write --bs=1M --iodepth=32 --size=64G --runtime=300 --time_based \
    --write_bw_log=/tmp/fio-sustained
```

另有 `ioping -D -c 50 -s 4k` / `-c 20 -s 1M`（单队列、最接近"一次只能等一个 I/O"的真实感受）
与 `fstrim -v`。原始输出留在板上 `/tmp/fio-summary.txt`、`/tmp/fio-*.txt`、`/tmp/fio-sustained_bw.1.log`。

> ⚠️ 读 ioping 输出别被第一行骗了：`49 requests completed in 294.5 ms ... 166 iops` 里的
> "294.5 ms / 166 iops" 是**去掉 1 秒间隔后的忙碌时间**算出来的；真正要看的是最后一行
> `min/avg/max/mdev`。总带宽要用 `generated ... in 49.0 s` 那行（这里就是 4 KiB/s，毫无意义）。
> 另外脚本里那句"首 30s / 末 30s 平均带宽"的 awk **写错了**（读的是 `fio-sustained_bw.log`，
> 而 fio 实际写的是带任务号后缀的 `fio-sustained_bw.1.log`，阈值也写成了 30–60 s），
> 所以汇总里那行是空的 —— §4.4 的逐分钟数据是事后按 bw 日志重新统计的。

## 4. 实测结果

### 4.1 顺序 I/O（1 MiB）

| 项目 | IOPS | 带宽 | 平均 clat | p99 |
| --- | --- | --- | --- | --- |
| 顺序读 QD1 | 81 | **81.1 MiB/s**（85.1 MB/s） | 11.9 ms | p99.9 15.0 ms |
| 顺序读 QD32 | 87 | **87.8 MiB/s**（92.0 MB/s） | — | — |
| 顺序写 QD1 | 74 | **74.1 MiB/s**（77.7 MB/s） | — | — |
| 顺序写 QD32 | 76 | **76.9 MiB/s**（80.6 MB/s） | — | — |

队列深度从 1 提到 32 只涨 8%（读）/ 4%（写）⇒ 已经吃满总线，**并发再多也没用**。

### 4.2 随机 I/O

| 项目 | IOPS | 带宽 | 平均 clat | 尾部延迟 |
| --- | --- | --- | --- | --- |
| 4K 随机读 QD1 | 2 675 | 10.5 MiB/s | ~0.37 ms¹ | — |
| 4K 随机读 QD32 | 2 925 | 11.4 MiB/s | — | — |
| 4K 随机写 QD1 | 1 091 | 4.37 MiB/s | — | — |
| 4K 随机写 QD32 | 1 201 | 4.81 MiB/s | 26.6 ms | p99.9 **144 ms** |
| 4K 混合 70 读/30 写 QD32 | 读 1 199 / 写 513 | 读 4.8 + 写 2.0 = **6.8 MiB/s** | 读 16.7 / 写 23.3 ms | p99.5 32 / 36 ms |
| 128K 随机读 QD16 | 432 | 54.1 MiB/s | 36.9 ms | p99 61.6 ms |
| 4K 写 + 每次 fsync QD1 | 160 | **643 KiB/s** | fsync 平均 1.9 ms | fsync p99.9 18.5 ms |

¹ QD1 的 4K 读延迟按 IOPS 反推为 0.37 ms 量级（受队列内并行影响），**单发实测见 §4.3 的 6.01 ms**——
这才是"同步小读"的真实代价。

注意两个"非线性"的地方：

- 4K 随机读 QD1→QD32 只从 2 675 涨到 2 925 IOPS（**几乎不变**）⇒ 卡内部对小读没有并行度，
  32 个未完成请求还是排队。
- 4K 随机写 QD32 平均 26.6 ms、p99.9 144 ms：偶发 0.1 秒级卡顿，交互式场景能感觉出来。

### 4.3 ioping 单发延迟（最贴近"点一下等多久"）

```
$ sudo ioping -D -c 50 -s 4k /media/board/Data1T
    --- /media/board/Data1T (ext4 /dev/mmcblk1p1 937.6 GiB) ioping statistics ---
    49 requests completed in 294.5 ms, 196 KiB read, 166 iops, 665.6 KiB/s
    min/avg/max/mdev = 5.96 ms / 6.01 ms / 6.12 ms / 26.5 us      ← 抖动极小，稳定得可怕

$ sudo ioping -D -c 20 -s 1M /media/board/Data1T
    min/avg/max/mdev = 18.3 ms / 18.3 ms / 18.3 ms / 11.1 us
```

- 4K 随机读单发 **6.01 ms**（166 IOPS）；1 MiB 单发 **18.3 ms**（54.5 MiB/s）。
- 1 MiB 单发比 fio QD1 的 11.9 ms 慢，因为 ioping 每个请求之间隔 1 秒，
  卡/控制器在这段空档里降速，下一个请求要重新"起步" —— 也就是说
  **隔一会儿读一次的代价比连续读更高**。

### 4.4 持续写 5 分钟：不掉速

```
### 持续写 5 分钟（64G 区域，1M/iodepth32，看有没有 SLC 掉速）
      write: IOPS=77, BW=77.4MiB/s (81.2MB/s)(22.7GiB/300430msec); 0 zone resets
        clat (msec): min=12, max=960, avg=411.26, stdev=91.13
         | 99.00th=[  760], 99.50th=[  785], 99.90th=[  818], 99.95th=[  844]
```

按带宽日志逐分钟统计（bw 日志每个采样 = 1 个 1 MiB I/O）：

| 分钟 | 1 MiB 块数 | 等效带宽 |
| --- | --- | --- |
| 1 | 4 619 | 77.0 MiB/s |
| 2 | 4 702 | 78.4 MiB/s |
| 3 | 4 593 | 76.5 MiB/s |
| 4 | 4 628 | 77.1 MiB/s |
| 5 | 4 680 | 78.0 MiB/s |

**5 分钟写了 22.7 GiB，带宽平得像一条直线**（76.5–78.4 MiB/s），没有"前几秒很快、之后腰斩"的
SLC 缓存行为。但注意 QD32 下平均 clat 411 ms、p99.5 785 ms —— 吞吐靠排队顶出来的，
**写延迟在压力下是 0.1~1 秒级**。

### 4.5 一个测试期异常：第一次 raw 读只有 39.7 MB/s

第一轮用 `dd` 直接读裸分区时，第一遍只有 **39.7 MB/s**，之后同一命令稳定在 86.3 MB/s。
`dmesg` 里能找到原因：

```
[ 7984.937952] mmc_host mmc1: Bus speed (slot 0) = 400000Hz (slot req 400000Hz, actual 400000HZ div = 0)
[ 7985.048748] mmc_host mmc1: Bus speed (slot 0) = 198000000Hz (slot req 200000000Hz, actual 198000000HZ div = 0)
[ 7985.256159] dwmmc_rockchip fe2c0000.mmc: Successfully tuned phase to 50
[ 7988.382276] mmc_host mmc1: Timeout sending command (cmd 0x200000 arg 0x0 status 0x80200000)
[ 7988.512126] mmc1: Card stuck being busy! __mmc_poll_for_busy
[ 7989.512164] mmc_host mmc1: Timeout sending command (cmd 0x202000 arg 0x0 status 0x80200000)
[ 7989.527698] mmc_host mmc1: Bus speed (slot 0) = 400000Hz ...   ← 降速到 400 kHz
[ 7989.610483] mmc_host mmc1: Bus speed (slot 0) = 198000000Hz ...
[ 7989.816035] dwmmc_rockchip fe2c0000.mmc: Successfully tuned phase to 51
...（同一 12 秒内共 6 次"降速 + 重新调相"）
```

即：卡在 `__mmc_poll_for_busy` 上超时（`Card stuck being busy!`），控制器把总线降到 400 kHz
重新协商、重新调相（phase 50/51）。**这是控制器/卡的初始化动作，不是卡的持续性能**；
调相完成后实测就是 86.3 ~ 92.0 MB/s。所以：

- 首次访问（尤其是脚本一上来就 `dd` 裸设备）**不要采信第一个数字**，跑两遍或先预热；
- 后续所有数字（§4.1–4.4）都是在调相稳定之后测的，可复现。
