# 板载 USB2 hub 的 STT vs MTT：5io(FE1.1s) vs v2(CH334F)

> 实测结论：多台**全速**设备并发时，**MTT（CH334F）合计吞吐是 STT（FE1.1s）的 1.44×**
> （1625 vs 1125 KiB/s），而且各口互不拖累；但按现场负载（单台 ~340 KB/s）连 STT 的门槛都踩不到
> ⇒ **BOM 选 CH334F 的结论不变，只是理由要据实写**（不再依赖二手对比文章）。

## TL;DR

- **判据一**：TT 模式是 hub 自己上报的 —— 读 hub 的 `bDeviceProtocol`（`02` = MTT），不用查手册。
- **判据二**：只有**全速/低速**设备才走 TT；高速设备是直通的（所以拿高速设备测不出差别）。
- ⚠️ **负载要刷"灌满"固件**：应用层 50 Hz 上报测不出来（限速在固件自己的循环里，
  "把波特率拉高"对 CDC 设备无效 —— 波特率由主机定，限速在固件）。
- 实测量：两台 ESP32-S3 全速灌数据，MTT 合计 1625 KiB/s、STT 1125 KiB/s；
  **掉速门槛 = TT 上限 ÷ 口数**（两台时 594 KiB/s/台）⇒ 掉的是 33% 而不是 50%。
- 复现：`tests/hub-stt-mtt.sh`（自动认 hub 的 TT 模式，跑单台基线 / 两台并发 A/B）。
- 起因：自研底板要在 CH334F(MTT) 与 FE1.1S(STT) 之间选，见 [carrier-board-vl817.md](carrier-board-vl817.md)
  与 [`notes/build/derived-board-bringup.md`](../build/derived-board-bringup.md)。

> 起因：自研底板要在 **CH334F**（WCH，MTT）与 **FE1.1S**（Terminus，STT）之间选（[carrier-board-vl817.md](carrier-board-vl817.md)、
> [`notes/build/derived-board-usb-hub.md`](../build/derived-board-usb-hub.md)）。
> 此前那条依据是**二手**的（WCH 的对比文章），本文件没写过任何实测。
> 2026-09-23 两块板子都在手上，把两件事都测了：① 芯片自己上报的 TT 模式；② 多台全速设备并发时差多少。
> 复现脚本：`tests/hub-stt-mtt.sh`（镜像源码：`tests/hub-stt-mtt.sh`）。

## 判据一：TT 模式是 hub **自己上报**的，不用查手册

USB2.0 设备描述符里的 `bDeviceProtocol` 对 hub 的含义是固定的：

| 值 | 含义 |
| --- | --- |
| `00` | 全速 hub |
| `01` | 高速 hub，**Single TT**（所有下游口共用 1 个 TT） |
| `02` | 高速 hub，**Multi TT**（每个下游口各 1 个 TT） |

```console
$ cat /sys/bus/usb/devices/<hub>/bDeviceProtocol
```

实测（2026-09-23）：

| 板 | hub | `bDeviceProtocol` | 结论 |
| --- | --- | --- | --- |
| **5io**（`Embedfire LubanCat-5IO`，旧镜像 6.1.118） | `1a40:0101` Terminus | **`01`** | **Single TT** —— 传闻属实 |
| **v2**（`Embedfire LubanCat-5 V2`，6.1.172） | `1a86:8091` QinHeng = **CH334F** | **`02`** | **Multi TT** —— 传闻属实 |

root 下还能用 debugfs 复核（`D:` 行的 `Prot=`）：

```console
$ sudo grep -E '^(T|D|S):' /sys/kernel/debug/usb/devices | head -20
T:  Bus=03 Lev=00 Prnt=00 Port=00 Cnt=00 Dev#=  1 Spd=480  MxCh= 1
D:  Ver= 2.00 Cls=09(hub  ) Sub=00 Prot=00 MxPS=64 #Cfgs=  1
S:  Manufacturer=Linux 6.1.118 ehci_hcd
T:  Bus=03 Lev=01 Prnt=01 Port=00 Cnt=01 Dev#=  2 Spd=480  MxCh= 4
D:  Ver= 2.00 Cls=09(hub  ) Sub=00 Prot=01 MxPS=64 #Cfgs=  1      <- 01 = Single TT
S:  Product=USB 2.0 Hub
```

顺带记下这个 hub 的两个描述符字段（两条都说明"它不是慢在 think time"）：

```console
$ sudo lsusb -v -d 1a40:0101 | sed -n '/Hub Descriptor/,/^$/p'
  nNbrPorts             4
  wHubCharacteristic 0x0000
    Ganged power switching
    Ganged overcurrent protection
    TT think time 8 FS bits        <- 最快的档（8/16/24/32 可选），没人为加间隔
  bPwrOn2PwrGood       50 * 2 milli seconds
```

## 判据二：只有**全速/低速**设备才走 TT

TT（Transaction Translator）只做 HS 与 FS/LS 之间的转换。挂在 hub 上的设备**全是 480M** 时，
STT 和 MTT 表现完全一样 —— 这是这套测试最容易白干的地方。

```console
$ cat /sys/bus/usb/devices/<hub>.<port>/speed
12     # 全速，会走 TT
1.5    # 低速，会走 TT
480    # 高速，不走 TT
```

## 负载怎么造：要刷"灌满"固件，应用层的 50 Hz 测不出来

ESP32-S3 的原生 USB 外设是**全速**（没有高速 PHY），插上去必然走 TT，是天然的测试设备。
但**上报节奏是应用层决定的**，实测那两台（`303a:1001` = 内置 USB-Serial-JTAG，即 Arduino 的
`USB Mode = Hardware CDC and JTAG`）：

```console
宿主侧波特率扫 6 档（115200 / 460800 / 921600 / 1.5M / 3M / 4M）
  -> 全是 12.3~12.7 KB/s；回读确认 SET_LINE_CODING 已按指令发出
  => 设备不按 CDC 线路编码节流

带时间戳抓 5 s
  -> 中位到达间隔 18.6 ms（p90 21 ms）=> 约 50 Hz，每周期 ~280 B => 13.8 KiB/s
```

⇒ **"把波特率拉高"对这类设备无效：限速在固件自己的循环里。** 换成"不限速大块写"的固件
（4 KB/次）后：

```console
$ timeout 10 cat /dev/ttyACM1 | wc -c
7679488                     # 开发机上（经开发机自己的 hub）；板上单台 845 / 848 KiB/s
```

固件与编译方式见本文档。

## 实测：两台 ESP32-S3，5io（FE1.1s / STT）vs v2（CH334F / MTT）

两台 ESP32-S3 分别插在 hub 的**两个不同下游口**（5io = 口 1/2 = `J10A`/`J10B`；
v2 = 口 3/4 = 白色双层那两个 A 口），宿主侧用 `timeout 10 cat <tty> | wc -c` 量。

**① 5io 板，FE1.1s（Single TT），内核 6.1.118：**

```console
$ sudo /tmp/hub-stt-mtt.sh --secs 10
  sysfs      : 3-1
  产品/VID   : USB 2.0 Hub  1a40:0101
  bDeviceProtocol = 01  -> STT（单 TT，所有下游口共用）
  上游 speed : 480
  3-1.1: speed=12  303a:1001  serial=02:00:00:00:00:03   <- 走 TT
  3-1.2: speed=12  303a:1001  serial=02:00:00:00:00:04   <- 走 TT
  3-1.3: speed=12  0bda:c822  Bluetooth Radio            <- 走 TT（当时挂起，无流量）

  单设备基线: /dev/ttyACM0 = 845 KiB/s   /dev/ttyACM1 = 848 KiB/s
  并发:       /dev/ttyACM0 = 562 KiB/s   /dev/ttyACM1 = 562 KiB/s
  合计并发 = 1125 KiB/s        掉速 = 33% / 33%
```

**`1125 KiB/s` 正好是一个 TT 的 spec 上限的 94.8%**，不是巧合：

USB2.0 对全速 bulk 的规定是 **1 ms 帧内最多 19 × 64 B = 1216 B/ms = 1187.5 KiB/s**（其余留给
控制/中断/等时）。STT 下两个下游口共用这一份额度 ⇒ 合计必然封在这里。

**② v2 板，CH334F（Multi TT），内核 6.1.172：**

```console
$ sudo /tmp/hub-stt-mtt.sh --secs 10
  sysfs      : 6-1
  产品/VID   : USB HUB  1a86:8091
  bDeviceProtocol = 02  -> MTT（每口一个 TT）
  上游 speed : 480
  6-1.1: speed=12  0bda:c822  Bluetooth Radio            <- 走 TT（空闲）
  6-1.3: speed=12  303a:1001  serial=02:00:00:00:00:04   <- 走 TT
  6-1.4: speed=12  303a:1001  serial=02:00:00:00:00:03   <- 走 TT

  单设备基线: /dev/ttyACM0 = 804 KiB/s   /dev/ttyACM1 = 801 KiB/s
  并发:       /dev/ttyACM0 = 811 KiB/s   /dev/ttyACM1 = 813 KiB/s
  合计并发 = 1625 KiB/s        掉速 = 0% / -1%
```

**每个下游口都有自己的 TT ⇒ 并发时完全不互相影响**（并发那组甚至略高于单跑，属测量抖动）。

### 两块板子的对照

| | **5io / FE1.1s** | **v2 / CH334F** |
| --- | --- | --- |
| `bDeviceProtocol` | `01` Single TT | `02` Multi TT |
| 单跑（每台） | 845 / 848 KiB/s | 804 / 801 KiB/s |
| 两台并发（每台） | **562 / 562 KiB/s** | **811 / 813 KiB/s** |
| 合计并发 | **1125 KiB/s** | **1625 KiB/s** |
| 掉速 | **33% / 33%** | **0% / -1%** |
| 合计 / 各自的上限 | 94.8%（被单 TT 封顶） | 68%（每口离自己的 1187.5 还很远） |

⇒ **同一场景下 MTT 的合计吞吐是 STT 的 1.44×**（1625 / 1125），而且各口互不拖累。
单台能力（~800 KiB/s）刚好落在两个门槛之间：**高于 `TT÷2 = 594`（所以 STT 被压）、
低于 `TT = 1187.5`（所以 MTT 每口都还有余量）** —— 这就是两台设备差别如此清楚的原因。

## 为什么掉 33% 而不是 50%

掉速比例**不是固定值**，由「单台能力」与「TT 上限 ÷ 口数」的比值决定：

```console
单台能力 845 KiB/s  >  TT 上限 1187.5 ÷ 2 口 = 594 KiB/s
        => 被压到 594 附近（实测 562，TT 效率 ~95%）
掉速 = 1 - (TT上限 ÷ 口数) ÷ 单台能力 = 1 - 594/845 ≈ 30%   （实测 33%）
```

⇒ **"会不会掉速"的门槛就是 `TT 上限 ÷ 口数`**，两台时 = **594 KiB/s/台**。
单台能力低于它时，STT 与 MTT **不会有任何差别** —— 本项目当前的真实负载
（50 Hz × 280 B ≈ 14 KB/s/台）离这个门槛差 **40 倍**。

## 复现方法

1. **负载设备**：ESP32-S3 刷"不限速大块写 CDC"固件。本次用
   `~/Arduino/s3_usb_throughput/s3_usb_throughput.ino`（开发机上），编译选项
   `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`。
   `USBMode=hwcdc`（内置 USB-Serial-JTAG）与原栈一致，**上传不用按 BOOT**；
   换 `USBMode=default`（TinyUSB）理论上限更高，但要评估上传复位是否还自动。
2. **两台分别插到 hub 的**两个不同**下游口**：5io = `J10A`/`J10B`（= hub 口 1/2）。
   别插 `J9B` —— 那是纯 USB2 HOST0，**不在 hub 后面**，测不出差别。
3. **在板子上跑**：

   ```bash
   sudo ./tests/hub-stt-mtt.sh --secs 10                                     # 自动找
   sudo ./tests/hub-stt-mtt.sh --ttys /dev/ttyACM0,/dev/ttyACM1 --secs 10    # 指定设备
   ```

## 结论与对 BOM 的意义

- 两个传闻都**实测成立**：5io 的 FE1.1s = Single TT、v2 的 CH334F = Multi TT。
- **STT 的实际代价（两台同时灌满）**：各掉 **33%**，合计封在 **1125 KiB/s**
  （= 单 TT spec 上限的 95%）。
- **MTT 的收益**：同一场景下 **0% 掉速**、合计 **1625 KiB/s**，
  是 STT 的 **1.44×**，且各口互不拖累（本文档对照表）。
- 注意（本次对比的混淆项）：两块板**内核不同**（5io = 旧镜像 6.1.118、v2 = 本工程 6.1.172）。
  严格起见应在同一内核上复测；不过 TT 上限是 hub 芯片行为、与内核无关，33% 这个量级不是内核差异
  能解释的。
- 对自研底板：MTT 的价值只在"同一个 hub 上**同时有多台全速设备要往每秒接近 1 MB 走**"时才显现。
  本项目当前负载（ESP32 50 Hz 小包；相机走 VL817/OTG1 那条全高速通路、不经过这个 hub）
  **踩不到**这个门槛。⇒ BOM 结论（单源 CH334F）**不变**，但理由要据实写：TT 模式这条现有实测支撑，
  **"性能"这条在当前负载下不可观测**；选它的真正理由是温度/ESD/供货真伪与资料完备性
  （见 `derived-board-usb-hub.md`）。
- 两个芯片的官方 datasheet **还没入库**（`hardware-docs/modules/` 只有 VL817 / HYM8563 / BQ40Z50）——
  `hardware-docs/README.md` 里那个 `ch334f-datasheet.pdf` 只是命名举例。定 BOM 前补上。
