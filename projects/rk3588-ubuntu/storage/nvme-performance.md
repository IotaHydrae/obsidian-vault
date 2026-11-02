# NVMe 硬盘性能实测（Kingston SNV3S1000G 1 TB）

> 测试日期：2026-09-21；平台：**5io 开发板**（`<HOST_10>`，内核 6.1.118）。
> 目的：这个 M.2 槽到底给多少带宽、这块盘能不能当数据盘用。
> 方法：**文件级** fio（不动裸设备），见 §2、§5。

## TL;DR

- 先看 **lane 数**再比数字：**5io 是 PCIe 3.0 x2、v2 是 x4**（§3 / §4）。
- ⛔ **不能测裸设备**（原因见 §2：绕过文件系统与缓存语义，数字没有意义）。
- 重跑方法见 §5。

## 0. 一句话结论

| 项 | 结论 |
| --- | --- |
| 顺序读写（5io） | **1548 MiB/s 写 / 1572 MiB/s 读**（1.62 / 1.65 GB/s）—— 贴着 PCIe 3.0 **x2** 链路天花板（理论 1.97 GB/s，效率 ~84%）⇒ **瓶颈是槽，不是盘** |
| 4K 随机（5io） | 读 **160.9k IOPS**（628 MiB/s）、写 **92.5k IOPS**（362 MiB/s）；单队列（QD1）读 **12.4k IOPS / 66.6 µs** |
| 槽的差别 | **5io = 2 lane**（DT + 原理图确认）；**v2 = 4 lane**（`pcie30phy` AGGREGATION + SoC 默认 `num-lanes = <4>`）⇒ 同一块盘换到 v2，顺序带宽应翻倍（理论 **3.94 GB/s**） |
| 盘本身 | 控制器**声明 Gen4 x4**，但 RK3588 的 PCIe 控制器只到 **Gen3** ⇒ 在这两块板子上永远拿不到 Gen4 的 5~6 GB/s |

## 1. 被测对象与环境

- 盘：`KINGSTON SNV3S1000G`，固件 `SDU00103`，931.5 GB，单个 ext4 分区 `nvme0n1p1`（**全新盘**：`Mount count: 0`、`Last mounted on: <not available>`）。
- 链路证据（**直接解配置空间**，这块板没装 `lspci`）：

| BDF | 设备 | LnkCap（自己声明的能力） | LnkSta（实际协商） |
| --- | --- | --- | --- |
| `0000:00:00.0` | RK3588 PCIe 控制器（`1d87:3588`） | Gen3 x4 | **Gen3 x2** |
| `0000:01:00.0` | Kingston NVMe（`2646:5028`，class `0x010802`） | **Gen4 x4** | **Gen3 x2** |

- **怎么读这两个数**（别把"盘的能力"当成"实际链路"）：
  - `max_link_speed` / `max_link_width` ← **LnkCap** 寄存器（`drivers/pci/pci-sysfs.c:185-212`，注释就写着 "We read PCI_EXP_LNKCAP"）；
  - `current_link_speed` / `current_link_width` ← **LnkSta** 寄存器（同文件 `:216-250`）；
  - 编码表：`include/uapi/linux/pci_regs.h:534-540`（SLS `3` = 8 GT/s = Gen3、`4` = 16 GT/s = Gen4），文本由 `drivers/pci/probe.c:693,719` 的 `pci_speed_string()` 生成。
- **为什么 Gen3**：链路取双方交集，对端 RK3588 控制器自己只声明 Gen3（`rk3588.dtsi:485 max-link-speed = <3>`）。
- **为什么 x2**：双方 LnkCap 都是 x4，但**根端口的 LnkSta 也是 x2** ⇒ SoC 侧只开了 2 lane。运行 DT `pcie@fe150000 num-lanes = 2`、仓库 `rk3588-lubancat-5iob.dts:1434` 同值，原理图也是 2 lane。lane 数与 phymode 的完整对照见 [../hardware/combphy-mux.md](../hardware/combphy-mux.md)（v2 = AGGREGATION + 4 lane，5io = NANBNB + 2 lane）。
- 带宽自洽核对：`8 GT/s × 2 lane × 128/130 / 8 = 1.97 GB/s` 理论，实测 1.62/1.65 GB/s = 效率 **84%**，典型的链路受限。

## 2. 为什么不测裸设备

⛔ 往 `/dev/nvme0n1p1` 裸写（fio 的 `--rw=write` 就是往扇区写数据）会把 ext4 **直接毁掉**。
所以测的是**文件系统层**：临时挂载该分区 → 在挂载点里建 `fio-lsp-test.bin` → 跑完**删文件 + 卸载**。
分区测试前是未挂载状态，测试后核对：`Filesystem state: clean`、`Free blocks` 正常、`findmnt` 无该分区。
这样测到的是"应用实际能拿到的速度"（含 ext4 元数据开销），也正是要回答的问题。
⚠️ 若把盘插到已经有数据的板子上，跑之前先确认可用空间（脚本要求 ≥ 12 GiB，否则直接放弃）。

## 3. 实测数据（5io，PCIe 3.0 x2）

口径：`fio --ioengine=libaio --direct=1 --group_reporting`，**20 s 稳态 + 3 s 预热**，单 job（4K 用 QD32），盘上 4 GiB 测试文件。

| 测试 | 带宽 | IOPS | 平均延迟 | p99 | 20 s 内 IO 量 |
| --- | --- | --- | --- | --- | --- |
| 顺序写 1M QD32 | **1548.4 MiB/s** | 1547 | 20.60 ms | 24.77 ms | 30.3 GiB |
| 顺序读 1M QD32 | **1571.5 MiB/s** | 1570 | 20.26 ms | 23.20 ms | 30.7 GiB |
| 4K 随机读 QD32 | 628.5 MiB/s | **160 882** | 193.7 µs | 222.2 µs | 12.3 GiB |
| 4K 随机写 QD32 | 361.5 MiB/s | **92 542** | 336.2 µs | 415.7 µs | 7.1 GiB |
| 4K 随机读 QD1 | 48.3 MiB/s | **12 370** | **66.6 µs** | 75.3 µs | 0.7 GiB |

温度 38 ℃ → 46 ℃（一轮 5 项、约 2 分钟）。

读法：
- 1M 顺序的 20 ms 平均延迟**不是**慢 —— `bs=1M` 且 QD32，延迟 ≈ `QD × 1 MiB / 带宽`，与 1548 MiB/s 自洽。
- **4K 随机读 161k IOPS 时只用了 628 MiB/s**，离 1.97 GB/s 链路上限还远 ⇒ 随机性能由**盘/主控**决定（SNV3S 是 DRAM-less 盘），换到 x4 的槽上也**不会**翻倍。
- 单队列 66.6 µs 属正常消费级 NVMe 水平（比这块板子的 SD 卡 6.01 ms 快 ~90×，见 [sd-card-performance.md](sd-card-performance.md)）。

## 4. v2（PCIe 3.0 x4）

⏳ 待测：把同一块盘插到 v2 的 M.2 M-Key。
预期：顺序读 ~3.2–3.7 GB/s（3.94 GB/s 理论的 85–95%）、顺序写同级或略低；4K 随机 IOPS 与 5io 同级。
判据（跑之前先看这三行，别急着看跑分）：

```bash
cat /proc/device-tree/pcie@fe150000/num-lanes | od -An -tu4 --endian=big   # 应为 4
cat /sys/bus/pci/devices/*/current_link_width                              # NVMe 那个应是 4
cat /sys/bus/pci/devices/*/current_link_speed                              # 应是 8.0 GT/s PCIe（Gen3）
```

### 4.1 空槽时两侧的探测代价（2026-09-30 实测，槽里没盘）

开发板（v2，M.2 空槽）实测，**U-Boot 和内核都会为"空槽"白等一段时间**，量级差 10 倍以上：

| 阶段 | 代价 | 出处 |
| --- | --- | --- |
| U-Boot | **517 ms** + 50 行 `pcie@fe150000: PCIe Linking... LTSSM is 0x0` —— **2026-09-30 已去掉**：实改后同一块 eMMC 上 U-Boot 自报 `Total: 1455 → 639.147 ms`（**省 816 ms**） | 串口抓包（`notes/hardware/boot-serial-log.md` §5.2）；根因 = `CONFIG_NVME=y` ⇒ `boot_targets` 里 `nvme0` 排第一；驱动是 50 次 × `msleep(10)`（`u-boot/drivers/pci/pcie_dw_rockchip.c:574`）。改法与验证（含 SD 卡验证法）见 `notes/system/boot-time-optimizations.md` 第 3 条 |
| 内核 `fe150000`（M.2 M-Key，空） | ~2.3 s（dmesg 2.68 → 4.99） | `PCIe Link Fail … hw_retries=0` 后停 |
| 内核 `fe170000`（M.2 E-Key，空） | ~6.6 s（dmesg 3.41 → 10.04） | LTSSM 0x3 ⇒ 每轮还多一次掉电重上电 |

内核那段为什么这么长（`kernel-6.1/drivers/pci/controller/dwc/pcie-dw-rockchip.c`）：
`RK_PCIE_ENUM_HW_RETRYIES = 2` 轮；每轮 = `rockchip,perst-inactive-ms`（默认 **200 ms**）
+ 轮询 `wait_for_link_ms / 20` 次、每次 `usleep_range(20000, 21000)`；
而 `wait_for_link_ms` 在驱动里被夹到 **≥ 2000 ms**（`pcie-dw-rockchip.c:710` 的
`max_t(u32, value, 2000)`）⇒ 每轮 ~2.2 s；LTSSM ≥ 3 时还要再 `msleep(1000)` 掉电重来。
⇒ ⛔ **想靠 DT 把 `wait-for-link-ms` 调小是没用的**（被 clamp 在 2 s），只能改驱动常量/改 clamp，
或者干脆把空槽的节点在板级 DT 里关掉（代价：用户插卡就认不到了）。

⚠️ **但内核这段不在启动关键路径上**：dmesg 里它从 2.68 s 一直重试到 10.04 s，
而 userspace 在 3.9 s 就起来了、cage 4.9 s —— 都是异步 probe，只是刷屏 + 白耗 PCIe PHY 的电。
**唯一在关键路径上的是 U-Boot 那 517 ms**（见 `notes/system/boot-time-optimizations.md` 第 3 条）。

## 5. 怎么重跑

```bash
# 从开发机喂进去跑（脚本自己找带 ext4 的 nvme 分区；也可显式指定）
ssh -F /dev/null <用户>@<板子> 'bash -s' < .scratch-apps/nvme-fio-test.sh
ssh -F /dev/null <用户>@<板子> 'bash -s -- /dev/nvme0n1p1' < .scratch-apps/nvme-fio-test.sh
```

脚本（`.scratch-apps/nvme-fio-test.sh`）自带：链路能力/DT lane/理论上限打印、分区已挂载则复用（不卸载用户的挂载）、可用空间 < 12 GiB 直接放弃、跑完删文件并卸载。手工等价命令：

```bash
sudo mount /dev/nvme0n1p1 /mnt/nvmeperf
sudo fio --name=seqread --filename=/mnt/nvmeperf/fio-lsp-test.bin --ioengine=libaio --direct=1 \
         --rw=read --bs=1M --iodepth=32 --size=4G --runtime=20 --time_based --ramp_time=3
sudo rm -f /mnt/nvmeperf/fio-lsp-test.bin && sudo umount /mnt/nvmeperf
```
