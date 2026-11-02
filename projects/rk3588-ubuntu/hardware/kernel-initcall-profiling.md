# 内核启动耗时分析：`initcall_debug` + 内核自带 `bootgraph.pl`

> 本文只回答一个问题：**怎么量出"内核那 3.6 秒花在哪"，以及我们量到的结果与可动杠杆**。
> 方法（内核参数 + 内核源码里的工具 + 串口抓包 + 解析脚本）在 §1~§2，
> 本次实测结果在 §3，能省哪几笔在 §4，**口径与坑（尤其"打印开销会污染结论"）在 §5**。
> 全局启动账见 [../system/boot-time.md](../system/boot-time.md)；可优化项清单见
> [../system/boot-time-optimizations.md](../system/boot-time-optimizations.md)（第 12 条）。

## TL;DR

- 开法：cmdline 加 **`initcall_debug ignore_loglevel`**（两个都要 —— `initcall_debug` 的打印是
  `KERN_DEBUG`，而我们的 cmdline 有 `quiet`，不加 `ignore_loglevel` 一条都看不到）。
- 内核源码里有现成工具：**`kernel-6.1/scripts/bootgraph.pl`**（把带 printk 时间戳的启动日志画成
  SVG 启动图）与 `scripts/generate_initcall_order.pl`；本文的图就是前者生成的。
- 实测两轮（开发板 `<HOST_07>`，内核 6.1.172，2026-10-03）：
  · **第一轮（改动前）**：`[0.000]` → `Run /init` = **3.624 s**，initcall 自身合计 **2.007 s**（908 个）。
    最慢三笔：**`rk_gmac_dwmac_driver_init` 618 ms**、**`raid6_select_algo` 510 ms**、`deferred_probe_initcall` 309 ms。
  · **第二轮（砍掉 GMAC 改模块 + 关 btrfs/RAID6 之后）**：内核阶段 **2.939 s（−0.685 s）**、
    initcall 合计 **1.359 s（−648 ms）**，只剩 `deferred_probe_initcall` 320 ms、`of_platform_default_populate_init` 93 ms、
    `rockchip_cpufreq_driver_init` 73 ms、`rockchip_multicodecs_driver_init` 62 ms、`rk3x_i2c_driver_init` 45 ms、`rknpu_init` 29 ms。
- 最便宜的一刀：**关掉 `CONFIG_RAID6_PQ_BENCHMARK`** ⇒ 省 **510 ms**（代价≈0，我们不用 RAID6）。
- ⛔ **最大的坑**：带 `initcall_debug ignore_loglevel` 的那次启动**本身慢 0.80 s**（刷 1500000 波特率串口的开销，
  app 首帧 mono 5.070 → 5.867 s）⇒ **任何"省了多少"的结论都必须在去掉这两个参数后复测**。

## 1. 怎么开

```bash
# 板上（实验；先备份）
sudo cp -a /boot/firmware/ubuntuEnv.txt /boot/firmware/ubuntuEnv.txt.bak-lsp-initcall
sudo sed -i 's/^\(bootargs=.*\)$/\1 initcall_debug ignore_loglevel/' /boot/firmware/ubuntuEnv.txt
sudo reboot
```

- `initcall_debug` 是 `init/main.c` 里的 `core_param`；打印在 `do_one_initcall()` 里，
  **格式是这套树自己的**（注意与 mainline 不同）：
  ```
  calling  <函数>+0x0/0x… @ <pid>
  initcall <函数>+0x0/0x… returned <ret> after <N> usecs
  ```
  `calling` 行里的 `@` 后面是 **PID（通常是 1）**，不是 initcall level —— 别拿它当层级用。
- 为什么必须 `ignore_loglevel`：两条打印都是 `KERN_DEBUG`，而我们的 cmdline 有 `quiet`
  （loglevel=4）⇒ 只看得到"没有输出"。加上 `ignore_loglevel` 后**所有**级别都会进 console，
  日志会变很大（本次 75 s 抓到 4646 行 / 408 KB）。
- 需要 `CONFIG_PRINTK_TIME=y`（本镜像有：日志里本来就带 `[    0.728811]`）。

## 2. 怎么抓、怎么分析

**抓**：优先串口（`/dev/ttyUSB0`，1500000 8N1），因为它能覆盖"内核第一行之前"的 U-Boot 段，
而且 initcall 打印量很大时不会丢行。抓包要**从重启前就开始**（否则丢掉前半段）：

```bash
# 开发机上（仓库自带 pyserial 在 .venv 里；脚本在 .scratch-session-digest/lv-exp/ 那种临时目录）
./.venv/bin/python serial-cap.py --secs 75 --out serial-initcall.txt &   # 被动读，带时间戳前缀
sleep 4; ssh board@<板子> 'sudo systemctl reboot'                          # 抓包期间触发重启
```

**转格式**：`bootgraph.pl` 要的是 dmesg 那种 `[    4.178285] …` 开头的行；把抓包的自定义前缀剥掉即可
（本次脚本：正则 `^\[\s*[\d.]+s\]\s*` 去掉 `[  10.346s]`，再保留 `^\[\s*[\d.]+\]` 的行）。
⚠️ **一个抓包里通常含两次 boot**（重启前旧 boot 的尾巴）⇒ 先按最后一个 `Starting kernel` 切开，只喂新 boot。

**画图（内核源码自带）**：

```bash
perl kernel-6.1/scripts/bootgraph.pl initcall-dmesg-new.txt > bootgraph.svg    # 12 KB
# 另一个：perl kernel-6.1/scripts/generate_initcall_order.pl <log> <out>       # 生成 initcall 顺序
```

**统计**（本次用的脚本 `initcall-stats.py`，逻辑很短，可直接抄）：把 `returned … after N usecs`
按 N 排序出 Top-N；按函数名前缀聚合子系统；再算**相邻内核日志行的时间差**找"非 initcall 的空档"。

## 3. 本次结果（2026-10-03，开发板，6.1.172）

内核阶段：`[0.000]` → `[3.623533] Run /init as init process` = **3.624 s**；initcall 合计 **2.007 s**。

| 耗时 | 时刻 | initcall | 解读 |
| --- | --- | --- | --- |
| **618 ms** | 3.010 s | `rk_gmac_dwmac_driver_init` | 两路 GMAC 同步 probe；日志里 `fe1b0000 … DMA host/device` 143.8 ms、`fe1c0000 … User ID` 132 ms，余下是 PHY 复位/DMA 配置 |
| **510 ms** | 2.229 s | `raid6_select_algo` | RAID6 算法基准：`neonx8/x4/x2`、`int64x8/x4`、`int8` 各 ~57 ms —— **我们不用 RAID6** |
| **309 ms** | 3.515 s | `deferred_probe_initcall` | 依赖未就绪的驱动重试（PHY/DSI/PCIe 那类） |
| 93 ms | 1.636 s | `of_platform_default_populate_init` | 按 DT 建平台设备 |
| 71 ms | 3.586 s | `rockchip_cpufreq_driver_init` | 调频 |
| 63 ms | 3.168 s | `rockchip_multicodecs_driver_init` | 音频 codec |
| 46 ms | 3.080 s | `rk3x_i2c_driver_init` | I2C 控制器 |
| 27 ms | 3.613 s | `rknpu_init` | NPU（本产品不用） |

**非 initcall 的空档**（相邻内核日志行间隔，说明时间不在 initcall 里）：

| 空档 | 时刻 | 那一行 |
| --- | --- | --- |
| 187 ms | 1.111→1.298 | `rcu: srcu_init: Setting srcu_struct sizes based on contention` |
| 165 ms | 0.742→0.908 | `OF: reserved mem: initialized node cma, compatible id shared-dma-pool` |
| 144 ms | 2.705→2.849 | `rk-pcie fe150000.pcie: PCIe Linking... LTSSM is 0x0`（与 U-Boot 那次同源） |
| 144 ms | 2.863→3.007 | `rk_gmac-dwmac fe1b0000.ethernet: Using 32/32 bits DMA host/device` |
| 109 ms | 3.326→3.436 | `dw_hdmi_qp-rockchip fde80000.hdmi: final tmdsclk = 533280000` |
| 72 ms | 0.998→1.070 | `software IO TLB: mapped …` |

### 3.1 第二轮：砍掉 GMAC（改模块）+ 关 btrfs/RAID6 之后（2026-10-03）

| 指标 | 第一轮（改前） | 第二轮（改后） | 差 |
| --- | --- | --- | --- |
| 内核阶段 `[0.000]` → `Run /init` | 3.624 s | **2.939 s** | **−0.685 s** |
| initcall 自身合计 | 2007 ms | **1359 ms** | **−648 ms** |
| `calling` 次数 | 908 | 884 | −24（btrfs 那批 initcall 没了） |
| `Freeing initrd memory` | 18288 K | 14492 K | initrd 小了 ~3.8 MB |

**剩下的 Top（都跟我们的改动无关，全是"用不到就关"那一类）**：

| 耗时 | initcall | 说明 / 能不能动 |
| --- | --- | --- |
| 320 ms | `deferred_probe_initcall` | 依赖没齐的驱动重试（PHY/DSI/PCIe 那类）；要动得从 DT 关设备 |
| 93 ms | `of_platform_default_populate_init` | 按 DT 建平台设备，属基础设施 |
| 73 ms | `rockchip_cpufreq_driver_init` | 调频，不能省 |
| 62 ms | `rockchip_multicodecs_driver_init` | 音频 codec；不用音频可关 |
| 45 ms | `rk3x_i2c_driver_init` | I2C 控制器；现场要用，不能关 |
| 29 ms | `rknpu_init` | NPU；本产品不用，可关/改模块 |

⇒ 结论：**这两轮之后，内核阶段里已没有"白花的大钱"**（其中 deferred probe 那 320 ms 的构成见 §3.2） —— 剩下最大的一项是 deferred probe（320 ms，
本质是若干驱动在等依赖），继续压就得按设备逐个决定"要不要"；相比继续抠内核，
`initramfs` 那 −1.7 s 才是下一个数量级（见 [../system/boot-time-optimizations.md](../system/boot-time-optimizations.md) 第 2 条）。

### 3.2 `deferred_probe_initcall` 那 320 ms 到底是什么（2026-10-03 追问）

结论先说：**其中约 86% 是"显示子系统的 bring-up"**，不是"若干驱动在排队等依赖"那么泛。
窗口 2.500 → 2.820 s，里面 25 个 probe（累计 457 ms > 墙钟 320 ms ⇒ 有一部分是 async 并行跑的）：

| 耗时 | 设备 | 是什么 |
| --- | --- | --- |
| **274.6 ms** | `fdd90000.vop` | 显示控制器 VOP2 的 probe **+ 整条 `rockchip-drm` 组件 bind**：日志里依次是 `[drm:vop2_bind] vp0 assign plane mask` → `dw_hdmi_qp… final tmdsclk = 533280000`（含 HDMI EDID/模式设定）→ `[drm] fb0: rockchipdrmfb frame buffer device`。**任何要出画面的方案都依赖这一段** |
| 72.0 / 71.6 ms | `3-1` / `3-1:1.0` | **USB hub**（日志 `usb 3-1: Product: USB HUB`）。注意别把 `3-1` 当成 i2c-3 上的 0x01 地址 —— 这是 USB 总线-端口命名 |
| 14.3 ms | `mmc0:0001` | eMMC 卡设备（HS400 卡初始化） |
| 7.0 ms | `fb000000.gpu-panthor` | GPU（panthor） |
| 4.2 ms | `dmc` | DDR 调频设备 |
| 1.0~1.7 ms × 9 | `fdc70000.av1d`、`fdbb0000.iep`、`fdba{0000,4000,c000}.jpege-core`、`fdb50400.vdpu`、`fdb50000.vepu`、`0004:40:00.0`(PCIe) | VPU 家族 + PCIe：2.24 s 集体 -517，到这里才重试成功 |
| <1 ms 若干 | `combophy-avdd1v8`、`vcc-3v3-s0`、`vcc-3v3-sd-s3`、`hdmi0-sound`、`fe170000.pcie`、`dw-hdmi-qp-hdcp.8.auto` | 三路 regulator（1.69 s 就 -517）、HDMI 声卡等 |

**-517（`EPROBE_DEFER`）的链条**（同一份日志里 grep `probe of … returned` 就能拿到）：

| 时刻 | 谁 defer | 等什么 |
| --- | --- | --- |
| 1.690 s | `combophy-avdd1v8`、`vcc-3v3-s0`、`vcc-3v3-sd-s3` | 上游 PMIC / regulator |
| 2.238–2.243 s | 3 个 PCIe + VPU 家族 12 个（vdpu/vepu/jpege×4/iep/jpegd/rkvdec×2/rkvenc×2/av1d） | 电源域 / 时钟 / regulator |
| 2.270 / 2.280 s | `fdd90000.vop`、`fb000000.gpu-panthor` | 电源域 |
| 2.46 → 16.04 s | `mtd_vendor_storage`（**60 次**，每次 0.0x ms） | 厂商 vendor-storage 设备始终没 probe 成功；`/sys/kernel/debug/devices_deferred` 里最终只剩它 |

**怎么复现这两张表**（不用重启、不用额外抓包）：

```bash
# 1) 当前仍挂着 deferred 的设备（debugfs，6.1 就有）
sudo cat /sys/kernel/debug/devices_deferred

# 2) 每个设备 probe 的耗时与返回码 —— 只有"开了 initcall_debug 的那次启动"日志里才有
#    （drivers/base/dd.c 的 really_probe_debug() 打的 "probe of %s returned %d after %lld usecs"）
grep -oE 'probe of .* returned -?[0-9]+ after [0-9]+ usecs' <抓包> | awk '{print $7, $3, $5}' | sort -rn | head -20
grep -E 'probe of .* returned -517' <抓包>          # 只看 defer 过的

# 3) 无驱动的平台设备（大多是 *.syscon / *.debug / *.cspmu，本来就不需要驱动，别当故障）
for d in /sys/bus/platform/devices/*; do [ -e "$d/driver" ] || basename "$d"; done
```

**能压吗**：那 274 ms 是"VOP2 + rockchip-drm + HDMI + fb0"，**要出画面就得付**（它发生在 2.53–2.81 s，
而 LVGL 单元 2.89 s 才启动、3.03 s 出首帧 ⇒ 已经在关键路径上被用掉）。
可动的只有边角（合计 ~30 ms 量级）：eMMC 卡那次 14 ms、VPU 家族 8 个各 ~1 ms（确认不用 AV1/IEP/JPEG 可从 DT 关）、
`mtd_vendor_storage` 的 60 次重试（省不了时间，但能让日志干净）。
再往下抠显示这段的方向只有 `CONFIG_DRM_FBDEV_EMULATION`（去掉 `fb0` 创建）与 HDMI EDID 读取；
⚠️ 前者会影响 plymouth / 控制台行为，**必须先量有没有收益再动**。

## 4. 可动的杠杆（含代价）

| 改法 | 预期省 | 代价 / 判据 |
| --- | --- | --- |
| **关 `CONFIG_BTRFS_FS`**（2026-10-03 **已做**）—— btrfs 是唯一把 `RAID6_PQ` 拉成 `=y` 的（`fs/btrfs/Kconfig:18`）；关掉后 `RAID6_PQ` 降为由 `DM_RAID=m → MD_RAID456=m` select 的 **`=m`**，基准只在 dm-raid 加载时跑 | 基准那 **510 ms** 从启动消失（实测 `dmesg \| grep -c raid6` = **0**）；本板 net kernel 3.425 → **2.556 s**（含 ETH 那笔合计 −0.87 s） | 代价：内核不再支持 btrfs（用户 2026-10-03 拍板：启动介质只有 eMMC/TF（ext4），NVMe 数据盘也用 ext4）。md/dm-raid 的 RAID6 能力**保留为模块**。判据：`modinfo btrfs` 失败、config 里 `# CONFIG_BTRFS_FS is not set`、`CONFIG_RAID6_PQ=m`；kernel deb 小 446 KB |
| （备选，未做）`# CONFIG_RAID6_PQ_BENCHMARK is not set` | 510 ms | 若**保留** btrfs 又想去掉基准时用这招：arm64 算法表顺序 `neonx8 → … → intx1`，跳过基准时取第一个 valid 的 = **neonx8**（正是基准选出的最快那个 6358 MB/s）⇒ 本板零代价。判据：日志出现 `raid6: skipped pq benchmark and selected neonx8` |
| **`CONFIG_STMMAC_ETH=m` + `CONFIG_STMMAC_PLATFORM=m` + `CONFIG_DWMAC_ROCKCHIP=m`（`DWMAC_GENERIC=n`）** —— 2026-10-03 **已做** | **实测 −0.63 s**（app 首帧 mono 3.996 → 3.367 s；`systemd-analyze` kernel 3.425 → 2.640 s） | 本产品不用 RJ45：模块由 udev 在 `mono 4.99 s`（**app 首帧之后 1.6 s**）自动加载，`eth0/eth1` 照旧出现、插线即用 ⇒ 代价≈0。⛔ 三件套必须同为 `=m`（`dwmac-rk.c` 调 `stmmac_probe_config_dt()`，在 `stmmac_platform.c` 里）；判据：`lsmod` 有 dwmac_rockchip/stmmac、日志里 `rk_gmac-dwmac …` 出现在用户态段 |
| `CONFIG_PCIE_DW_ROCKCHIP=m` | ~144 ms + 连带 deferred probe | NVMe 是数据盘；改模块后 NVMe 上电可用时间推后 |
| `rknpu` / `multicodecs` / 部分 `rk3x_i2c` 关掉或改模块 | ~130 ms | 用得到就不能关（NPU 本产品不用） |
| 组合（乐观） | **~1.0–1.3 s**（内核 3.62 → ~2.4 s） | ⛔ 必须在**去掉 `initcall_debug ignore_loglevel`** 的启动上复测才算数 |

## 5. 口径与坑

1. ⛔ **打印本身就是开销**：带 `initcall_debug ignore_loglevel` 的启动比正常启动慢 **0.80 s**
   （同板同日：app `first_frame` mono 5.070 → 5.867 s）。这个数只能用于"归因"，
   **不能用于"优化前后对比"**；优化收益要另开一次干净启动量。
2. `calling … @ N` 的 N 是 **PID**，不是 initcall level（这套树的 `do_one_initcall_debug` 用的是
   `task_pid_nr(current)`）—— 想要"层级/顺序"用 `scripts/generate_initcall_order.pl`。
3. **initcall 之和 ≠ 内核阶段**：本次 2.007 s vs 3.624 s，差的 ~1.6 s 在早期架构/内存/SMP 初始化
   与上面那些空档里（所以 §3 两张表要一起看）。
4. **同步 probe 会算进 initcall**：`builtin_platform_driver()` 注册时会就地 probe 已存在的设备
   （GMAC 那 618 ms 主要就是这个），所以"某个 initcall 很慢"往往等于"某个驱动在同步 probe"。
5. **一个抓包含两次 boot**：重启前后都在抓时，先按最后一个 `Starting kernel` 切开（§2）。
6. ⛔ **`returned … after N usecs` 会把该窗口里的"异步事件"一并算进去**。实例（2026-10-03 第二轮）：
   `init_blk_tracer` 报 **461 ms**（第一轮同一项只 15 µs），看原始行才发现它的窗口里夹着
   `Freeing initrd memory: 14492K`（initramfs 的异步释放，第一轮那行落在 PCIe/网卡 probe 之间）。
   ⇒ 见到某个 initcall 突然"变慢一个数量级"，**先看它窗口里有没有别的 kernel 输出**，别急着去动那个驱动。
   反面教材的正解：`init_blk_tracer` 本身是微秒级，那 461 ms 不是它的开销。

## 6. 待办 / 下一步

1. ✅ 已做：GMAC 改模块（第 14 条）、关 btrfs 连带去掉 RAID6 基准（第 15 条）——
   `notes/system/boot-time-optimizations.md` 有实测与判据。
2. 剩下可动的都在 §3.1 那张表里，且都是"设备要不要"的产品决策（NPU / 音频 codec / deferred probe 背后的设备），
   不是纯启动优化。
3. `initramfs` 那 −1.7 s 是下一个数量级，与本文正交（见第 2 条）。
4. 本文的 `bootgraph.svg` 是单次样本；要下结论应各量 3 次取中位数（口径同
   [../system/boot-time.md](../system/boot-time.md) §2）。
