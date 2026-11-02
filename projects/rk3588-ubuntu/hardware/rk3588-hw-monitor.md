# RK3588 硬件模块占用率：接口地图与 lsp-rk-monitor

> 监控工具现在有**两个**：
> **`lsp-rk-monitor`（Rust + ratatui 的 app，`apps/lsp-rk-monitor/`，镜像里自带，推荐）**，
> 以及仓库里的 `tools/lsp-rk-monitor.sh`（438 行 bash，**保留当兜底**：没编 Rust / 没装二进制时用）。
> 两者**同参数、同 JSON 键名与数值口径**，可以互相 diff。
> 本文记录**每个模块的占用率从哪个接口读、单位是什么、要不要 root**，以及量这些东西时踩过的坑。
> 相关：`notes/system/chromium-rkmpp.md`（VPU/MPP 那一节）、`notes/hardware/cpu-coremark.md`（CPU 算力）、
> `notes/build/apps-examples.md` §2 与 `notes/build/apps.md`（app 怎么编、怎么装、怎么自测）。

## TL;DR

- 监控工具**有两套**：`lsp-rk-monitor`（Rust + ratatui 的 app，**镜像自带、推荐**）与老的 bash 版
  `tools/lsp-rk-monitor.sh`（不进镜像，当"没编 Rust"时的兜底）。
- 接口地图（每个指标的数据从哪来）见 §2；**容易看错的语义**见 §3。
- ⚠️ "GPU 频率总是 200 MHz"是**正常**的（§3.1），也不会拖慢程序启动（§3.2）。
- 写这类工具踩过的 bash 坑见 §4（别的脚本也会碰到）；已知限制见 §5。

## 1. 怎么用

```bash
lsp-rk-monitor                      # TUI，1 秒刷新（默认；stdout 不是终端时退化成纯文本帧）
lsp-rk-monitor -i 0.5 -n 20         # 0.5 秒刷新，20 帧后退出
lsp-rk-monitor --once               # 一份纯文本快照（ssh 一行取数 / 写日志）
lsp-rk-monitor --json               # 一份 JSON（给上位机/脚本用）
lsp-rk-monitor --top 5              # 额外列 CPU 占用最高的 5 个进程
sudo lsp-rk-monitor                 # 加 sudo 才能看到 NPU / RGA 的百分比（debugfs）
ssh -t <板子> sudo lsp-rk-monitor   # 远端看 TUI 的正确姿势（见 §6 的 use_pty 坑）

tools/lsp-rk-monitor.sh             # 老脚本，参数完全一样（板子上有源码就能跑）
```

TUI 长这样（左=外设列表，鼠标点/键盘选；右=被选中外设的曲线；**默认 CPU —— 每个核一张小图**）：

```
RK3588 hardware monitor  15:27:27   dt 0.407s   load 0.11 0.12 0.04   procs 268   win 60s line
┌ devices ───────────────────────┐cpu overall 0%   load 0.11 0.12 0.04   procs 268
│  all   cpu 0% gpu 40% ddr 7%   │┌ cpu0  2.0%  max 9.0% ───────────┐┌ cpu1  0.0%  max 2.0% ───────────┐
│1 cpu     0%  8 cores           ││          ⢀⡠⠴⡀                   ││                                 │
│2 freq  1800-2304MHz            ││⣀⣀⣀⣀⣀⣀⣀⣀⣀⣠⣃⣀⣀⣇⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀││⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀│
│3 gpu    40%  200MHz            ││        ⢠⠃   ⠘⡄ ⢀⠔⢄              ││⢄   ⡠⡀                     ⡠⡀    │
│4 npu   need root               ││⡠⠔⠉⠉⠉⠑⠢⢤⠃     ⢣⠔⠁ ⠈⠢⡠⠔⠊⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉││ ⠣⡠⠊ ⠈⠑⢄⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⡠⠊ ⠈⠢⣀⣀⣀│
│5 rga   need root               │└─────────────────────────────────┘└─────────────────────────────────┘
│6 vpu   idle                    │┌ cpu2  0.0%  max 10.0% ──────────┐┌ cpu3  2.0%  max 2.0% ───────────┐
│7 ddr     7%  1848MHz           ││        ⡜⡄                       ││ ...                             │
│8 temp  max 37.9C               ││⣀⣀⣀⣸⣀⣸⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀││                                 │
│9 mem    20%  1.6G/7.7G         │└─────────────────────────────────┘└─────────────────────────────────┘
│0 irq   169 sum/s               │┌ cpu4  0.0%  max 2.0% ───────────┐┌ cpu5  0.0%  max 2.0% ───────────┐
│                                ││           ... 共 8 张：cpu0..cpu7，2 列 × 4 行              │
└────────────────────────────────┘└─────────────────────────────────┘└─────────────────────────────────┘
1-9,0 device  Tab next  click select  +/- window  s line  t top  space pause  r clear  q quit
```

- **左边选外设，右边画它的曲线**：鼠标左键点列表即可（`1`..`9` `0` 直接选、`Tab`/`↑↓` 上下移）。
  列表每行右边是该模块**当前值摘要**（`cpu 0%  8 cores`、`gpu 40%  200MHz`、`temp max 37.9C`…）。
- **默认 CPU 视图**：每个核**一张独立小图**（2 列 × 4 行），量程是**百分比固定 `0~100%`**
  （2026-09-20 改，见 §7 末尾那条），
  标题写 `名字 当前值 max 峰值`；整体占用 / load / 进程数在右上角那行明细里。
- **通道少的模块**画带**完整坐标轴**的图（左侧 Y 刻度 + 底部相对秒数）：GPU 与 DDR 各是**两张**
  （占用 % + 频率 MHz，并排）；只 1 个通道时就是一张占满整块。
- `all`（列表第一行，按 `a`）是所有模块的进度条文本 —— 相当于把 `--once` 搬进 TUI。
- NPU / RGA 的百分比只有 debugfs 有（`/sys/kernel/debug/rknpu|rkrga/load`，**devfreq 的 npu/load 是假的、恒 100%**）。
  debugfs 根目录默认 `0700 root:root` ⇒ 普通用户读不到：此时列表里写 `need root`、右边提示 `run: sudo lsp-rk-monitor`。
  ✅ **镜像默认已把它放开给 `board` 组**（`overlay/etc/systemd/system/sys-kernel-debug.mount.d/10-lsp-debugfs.conf`，
  `Options=…,gid=1000,mode=0750`）⇒ **普通用户跑 `lsp-rk-monitor` 就有 NPU/RGA，不用 sudo**。
  ⛔ 那个 `Options=` 是**整体替换**（不是追加），`nosuid,nodev,noexec` 必须原样重复；内核的 debugfs 只认数字 gid。

按键（**界面文案全英文**）：

| 键 | 作用 |
| --- | --- |
| `1`..`9` `0` | 选外设：cpu / freq / gpu / npu / rga / vpu / ddr / temp / mem / irq |
| `Tab` / `↑` `↓` / `j` `k` | 上下移动选择（最上面那行 `all` = 全模块总览） |
| 鼠标左键 / 滚轮 | 点左侧列表选外设；列表上滚轮 = 移动选择；图表上滚轮 = 缩放时间轴（**上滚 = 放大 = 窗口变短**）|
| `a` | 跳到 `all`（总览） |
| `+` `-`（或 `]` `[`） | 放大 / 缩小：时间窗 15 / 30 / 60 / 120 / 300 / 600 秒（**`+` = 变短**，默认 60s）|
| `s` | 散点 / 连线（默认连线） |
| `t` | 顺带采进程占用（默认关：一帧要扫 300+ 个 `/proc/<pid>/stat`） |
| `space` | 暂停采样（画面冻住，按键仍响应） |
| `r` | 清空曲线历史 |
| `q` / `Esc` | 退出 |
| `LSP_MONITOR_ASCII=1` | 环境变量：用 Block 画点**且不画网格**（字体没有盲文点阵时用） |

### 1.1 与老脚本（`tools/lsp-rk-monitor.sh`）的关系

- **参数、JSON 键名、单位口径完全一致**（`--json` 可以两边对着 diff）。板上逐字段核对过：
  瞬时量（频率、内存、`npu_load` / `rga_load` 文本、温度文本）**逐字相同**；
  `cpu_pct` / `*_irq` / `mpp_tasks` 是"两次采样做差"，两个进程的采样窗口不重合，差几个点是正常的。
- 老脚本的 TUI 是手工 ANSI（单次写整帧 + 每行 `\033[K`，见 §4），新 app 用 ratatui
  （交替屏幕 + 差量刷新），所以**别再用 `ssh` 抓它的原始输出去解析** —— 要看数就用 `--json` / `--once`。
- 老脚本**不进镜像**（`tools/` 不烤进 rootfs）；镜像里只有 app。


## 2. 接口地图（谁给谁的数据）

| 模块 | 接口 | 单位/语义 | 权限 |
| --- | --- | --- | --- |
| **CPU 每核** | `/proc/stat` 的 `cpuN` 行做差；频率 `/sys/devices/system/cpu/cpuN/cpufreq/scaling_cur_freq` | %（busy = total - idle - iowait）；kHz | 任意用户 |
| **CPU 温度** | `/sys/class/thermal/thermal_zone*/{type,temp}` | 毫摄氏度（脚本换算 °C） | 任意用户 |
| **GPU**（Mali-G610 panthor） | `/sys/class/devfreq/fb000000.gpu-panthor/load` | `负载@频率`，负载是**真百分比**（panthor 用 GPU 时间戳算 busy/idle） | 任意用户 |
| **NPU** | `/sys/kernel/debug/rknpu/load` | `NPU load: Core0: x%, Core1: x%, Core2: x%,` | **root** |
| **RGA** | `/sys/kernel/debug/rkrga/load` | 每个 scheduler（rga3/rga3/rga2）一行 `load = x%` | **root** |
| **VPU 解码** | `/proc/mpp_service/load` | 每核 `load: A% utilization: B%`（见 §3） | 任意用户（但要先设 `load_interval`） |
| **VPU 编码** | 同上（`vepu` / `rkvenc-core`） | 同上 | 同上 |
| **MPP 任务数** | `/proc/mpp_service/<核>/task_count` 做差 | 次/秒（≈ 解码帧率） | 任意用户 |
| **中断速率** | `/proc/interrupts` 做差 | 次/秒；按名字分组：`npu` / `rga` / `rkvdec+vdpu` / `rkvenc+vepu` / `jpeg` / `panthor` | 任意用户 |
| **DDR 带宽** | `/sys/class/devfreq/dmc/load` | `负载@频率`；dmc 驱动用 DDR 带宽计数器算 busy/total，可当带宽占用 | 任意用户 |
| **内存 / swap** | `/proc/meminfo` | KB | 任意用户 |

`devfreq` 的 `load` 单位在 `drivers/devfreq/devfreq.c:load_show()` 里定死：
`busy_time * 100 / total_time`，所以是 0–100 的百分比（`total_time == 0` 时输出空）。

## 3. 关键语义（容易看错的地方）

- ⚠️ **`fdc70000.av1d` 那一行是死值，别按「利用率最高」选核**（2026-09-15 实测）：这块板子上它
  **空闲也一直报 `load: 59.66% utilization: 56.40%`**（其余 12 个核空闲都是 `0.00%`），
  而真正干活的 `rkvdec-core` 空闲 `0.00%`、1080p30 实时播放约 **2%**（满速压测 ~25%）。
  判据：`/proc/mpp_service/av1d/task_count` 不涨就说明没在解 AV1。
  `tests/browser-codec.sh`（只有测 av1 才把 av1d 算进候选）与 `tools/lsp-rk-monitor.sh`
  （只在 av1d 的 task 速率 > 0 时才显示它）都已按这条改过。
- **VPU 的 `load` 与 `utilization` 不是一回事**（`drivers/video/rockchip/mpp/mpp_common.c:mpp_dev_load()`）：
  - `load` = MPP **任务**占用（从任务开始跑到统计窗口结束，含排队/调度）
  - `utilization` = **硬件真正忙碌**的比例（`mpp_task->hw_time` / 窗口）→ **这个才是 VPU 占用率**
  - 多核时驱动按"100% × 核数"归一化
  - 前提：`load_interval` 非 0（镜像里由 `lsp-mpp-load.service` 开机设成 1000ms；procfs 设置重启会丢）
- ⛔ **NPU 的 devfreq `load` 是假的，别用**：`rknpu_devfreq.c` 在调频时把
  `last_status.total_time = busy_time = 1`，于是 `devfreq` 算出来恒为 **100%**（空闲时也是 100%）。
  要 NPU 百分比只能用 `/sys/kernel/debug/rknpu/load`（root）。
- ⛔ **RGA 没有 devfreq 节点**，百分比只在 `/sys/kernel/debug/rkrga/load`（root）；
  非 root 时脚本用 `rga3`/`rga2` 的中断速率代替（能看出"在不在干活"，但不是占用率）。
- **GPU 那条不是硬解**：Mali 只管合成/渲染，VPU/MPP 解码在独立的 VPU 上。
  实测 4K30 硬解时 GPU 2%、VPU 两个核各 ~15%（见 `notes/system/chromium-rkmpp.md`）。
- **RK3588 有两个解码核**（`fdc38100`/`fdc48100`），MPP 会并行用；
  1080p 只用一个核也能跑，4K 时会看到两个核同时在动。

## 3.1 GPU 频率"总是 200MHz"是正常的（2026-09-20 实测）

板上实测：`lsp-rk-monitor` 的 GPU 频率几乎永远显示 200MHz，但**不是卡住了**。四条证据：

1. `governor = simple_ondemand`，`min_freq = 200MHz`、`max_freq = 1GHz`、`polling_interval = 50ms`；
   驱动里给的阈值是 **`upthreshold = 45`、`downdifferential = 5`**
   （`kernel-6.1/drivers/gpu/drm/panthor/panthor_devfreq.c:271-272`，注释 "chosen based on experiments"）
   ⇒ **负载要超过 45% 才升频**，低于 40% 就落回 200MHz。
2. GPU 是 **runtime-suspend** 的：`power/control=auto`、`autosuspend_delay_ms=50`、
   `runtime_status=suspended` —— 50ms 没有 GPU 任务就挂起。而驱动在**挂起时根本不改时钟**
   （`panthor_devfreq_config_clks()`：`if (!pm_runtime_suspended(dev)) clk_set_rate(...)`）
   ⇒ `cur_freq` 读到的通常就是上次设的 200MHz。
3. 桌面空闲时的负载是**脉冲式**（GNOME 只在有 damage 时才渲染）：连续采样能看到
   `69% → 0% → 68% → 0% → 69% …` 交替 ⇒ governor 就在 200MHz ↔ 1GHz 之间来回跳
   （`trans_stat`：`Total transition: 9999`），但**停留时间 200MHz ≈ 25129 s、1GHz ≈ 295 s
   ⇒ 只有 1.2% 的时间在 1GHz**。1 秒一采样的工具当然几乎永远抓到 200MHz。
4. dmesg 里的 `panthor fb000000.gpu-panthor: [drm] clock rate = 200000000` **只是 probe 时的初始时钟**，
   不是"一直 200MHz"的证据。

怎么看它到底动没动：
- 用 lsp-rk-monitor 的 **`3`（GPU）视图**：现在并排两张图（占用 % + 频率 MHz），能直接看到频率尖峰；
  `--json` 里本来就有 `gpu_mhz`，也可以自己 `while :; do cat /sys/class/devfreq/fb000000.gpu-panthor/cur_freq; sleep 0.1; done | sort | uniq -c` 统计。
- 想看累积分布：`cat /sys/class/devfreq/fb000000.gpu-panthor/trans_stat`（每档停留 ms）。
- ⛔ **别指望 `performance` 锁频**（2026-09-20 实测 + 源码核对，我一开始也理解错了）：
  1. `performance` governor **只**在 `DEVFREQ_GOV_START` 时 `update_devfreq()` 设一次最高频，
     它**没有周期 monitor、也没有 SUSPEND/RESUME handler**（`drivers/devfreq/governor_performance.c`）。
  2. GPU 一 runtime-suspend，**devfreq 核心自己**会把频率切到 OPP 表里标了 `opp-suspend` 的那一档：
     `devfreq_suspend_device()` → `if (devfreq->suspend_freq) devfreq_set_target(...)`，
     而 `suspend_freq = dev_pm_opp_get_suspend_opp_freq(dev)`（`devfreq.c:1173` / `:879`）；
     RK3588 GPU 的 OPP 表里 **`opp-200000000` 就带 `opp-suspend`**（`rk3588.dtsi` 的 `gpu-panthor` 节点）。
  3. 所以实测：`echo performance > governor` 的瞬间是 1GHz，**第一次挂起之后就永久停在 200MHz** ——
     5 秒窗口里 `cur=target=200MHz` 一直不动，中间 `runtime_status` 还 `active` 过 3 次也没被拉回去。
  4. 对比 `simple_ondemand`：它的 handler 有 SUSPEND/RESUME（`devfreq_monitor_suspend/resume`），
     resume 后 50ms 定时器继续按负载决策 ⇒ **真干活时反而是它能升到 1GHz**（trans_stat 里那些跳变就是它干的）。
  - 想常驻高频只能动别的旋钮：把 `power/autosuspend_delay_ms` 调大或 `power/control=on`
    （注意挂起时 core clock 本来就被 `clk_disable_unprepare()` 关掉，省的就是这点）、
    改驱动里硬编码的 `upthreshold=45`、或者自己写个按 `load` 调 `min_freq`/`max_freq` 的守护进程。
  - `cur_freq` / `target_freq` 两个属性读的都是 `df->previous_freq`（panthor 没提供 `get_cur_freq`）
    ⇒ 它们是最后一次编程进去的值，不代表 GPU 此刻真的在跑 200MHz。
  - ⚠️ **镜像里原来那个 `gpu-governor-performance.service` 已于 2026-09-20 删除**：它写的是 **kbase**
    的 sysfs 路径（panthor 上不存在 ⇒ 被 `|| true` 吞掉、纯空转），而且按上面第 2/3 条，它真生效
    反而会把 GPU 永久钉在 200MHz。完整来龙去脉（含"为什么上游那套是有效的"）见 [gpu.md](gpu.md) §6。

- **"锁 1GHz 空转"到底多花多少电**（2026-09-20 实测：`power/control=on` + `userspace` 固定频率，
  GPU 上电但 `load=0`、CPU `loadavg` 0.05~0.2，两种状态各保持 45s）：

  | 状态 | `vdd_gpu_s0` | gpu-thermal | soc-thermal |
  | --- | --- | --- | --- |
  | 200 MHz | **675 mV** | 37.0 °C | 37.0~37.9 °C |
  | 1 GHz | **850 mV** | 37.0 °C | 37.0~37.9 °C |

  - 电压来自 OPP 表（`rk3588s.dtsi` 的 `gpu_panthor`：200~600MHz=675mV、700=700、800=750、900=800、
    1000=850）⇒ **+26% ⇒ V² = 1.58×**；
  - 这颗 thermal 传感器只有 ~1 °C 分辨率（读数只在 37.0/37.9 两档跳），45s 空转**看不出差别**
    ⇒ **空转/轻载时锁频多花的是"漏电 + 时钟树/电源域常开"，量级在百毫瓦以内**，不是 EM 表那个数。
  - 满载才有量级差：内核 EM 表（`P = C·V²·f`，C = DT `dynamic-power-coefficient = 2982`）给的是
    **100% 利用率**的值 —— `/sys/kernel/debug/energy_model/fb000000.gpu-panthor/`：
    200MHz **272mW**、400MHz 543mW、600MHz 815mW、800MHz 1342mW、1GHz **2154mW**
    （≈每 100MHz +136mW）；1GHz 满载是 200MHz 的 7.9×。
  - 同一份活的**能量** E ≈ C·V²·W（只跟电压平方有关，1.58×），但 1GHz 跑得快、漏电时间短
    ⇒ 对突发负载，锁频的能耗影响远小于 1.58×，对纯空转的影响是百毫瓦级。
  - ⛔ 想拿**绝对瓦数**只能外接功率计（RK806 不上报电流：`regulator_summary` 电流列恒 0mA；EM 表是模型值）。
    `power/control=on`（锁频的配套动作）会让 GPU 电源域 + 4 个时钟常开，这部分代价同样在百毫瓦量级。
- 真要让 GPU 长时间跑满（比如压测）再观察：`glmark2-es2-wayland` 之类；桌面点来点去也会看到 1GHz 的尖峰。

## 3.2 GPU 程序"启动时会不会被 200MHz 拖慢"：实测不会（2026-09-20）

用 `vkmark`（Vulkan / panvk，1080p `effect2d` 场景）在板上量，三种配置：

| 配置 | FPS | 帧时间 |
| --- | --- | --- |
| `simple_ondemand`（默认，从 GPU 挂起/200MHz 起跑） | **231** | 4.33 ms |
| 固定 1GHz（`power/control=on` + `userspace`） | **234** | 4.27 ms |
| 固定 200MHz（同上） | **67** | 14.93 ms |

- **默认策略的稳态性能和钉在 1GHz 只差 1.3%**；完全不升频才是 3.5× 慢 —— 但它没发生。
- devfreq tracepoint（`/sys/kernel/debug/tracing/events/devfreq/devfreq_{frequency,monitor}`）实测升频过程
  （`t=` 是相对 app 启动的秒数）：
  ```
  t=+0.038  FREQ  1GHz   prev=200MHz  load=92   ← 负载一上来，第一个 50ms 窗口就升到 1GHz
  t=+0.149  FREQ  700MHz              load=29   ← 启动阶段负载间隙，掉两档
  t=+0.202  FREQ  200MHz              load=1
  t=+0.255  FREQ  1GHz                load=95   ← 0.25s 后稳定在 1GHz，之后 load=99 一直 1GHz
  ```
  ⇒ **升频延迟 ≈ 38ms（≤1 个 polling 窗口）**，抖动窗口约 0.15~0.25s，折算下来只有一两帧；
  app 启动耗时的大头是 Vulkan/EGL 初始化 + 着色器编译（CPU），跟 GPU 频率无关。
- 什么时候**会**真慢：① 负载是短脉冲（每 50ms 窗口里 GPU 忙不到 ~22ms = <45%）⇒ governor 不升频，
  一直 200MHz，最坏就是上面那个 3.5×；② 负载在 45% 阈值附近来回穿越 ⇒ 频率抖动（观察得到，观感轻微）。
  要压掉这两条：把 `upthreshold` 调低（驱动里硬编码 45）、或该 app 启动前把
  `min_freq` 抬到 max（`echo 1000000000 > /sys/class/devfreq/fb000000.gpu-panthor/min_freq`，退出后还原）、
  或直接 `power/control=on` + 钉频（代价见上面那节的实测：空闲多百毫瓦级、温度看不出来）。
- ⛔ **量"固定频率"时必须先 `power/control=on`**（2026-09-20 踩过）：`userspace` governor 写进去的值，
  在 GPU 处于 runtime-suspend 时**不会**真正写进时钟（`panthor_devfreq_config_clks()` 里
  `if (!pm_runtime_suspended(dev)) clk_set_rate(...)`）⇒ 我第一次量到"200MHz 和 1GHz 一样快"（428 vs 414 FPS）
  就是这个原因；关掉 runtime PM 后才是真实的 67 vs 234 FPS。

## 4. 写这个工具时踩到的坑（bash 相关，别的脚本也会碰到）

1. **`set -u` + 没赋过值的数组 = "unbound variable"**：`declare -A a` 之后 `${#a[@]}`、
   `${a[k]:-x}` 在 bash 5.2 上都会报错退出。必须写成 `declare -A a=()`（`declare -a a=()` 同理）。
2. **关联数组的键在算术里要写 `${pid}`**：`$(( P_NOW[pid] ))` 里的 `pid` 会被当成**字面量键** `pid`
   （永远取不到），必须 `$(( P_NOW[${pid}] ))`。
3. **别每个进程 fork 一次 `sed`/`awk`**：300+ 进程时一帧要 4.3 s（实测）。
   改成纯 bash 解析 `/proc/<pid>/stat`（comm 在括号里，括号后第 12/13 个字段是 utime/stime）后，
   `--top` 那一帧总共 2.7 s 里绝大部分还是 `sleep`。
4. **TUI 不要整屏清屏**：`\033[J` 每帧清屏 + 分多次写会让画面闪。
   现在是：开始时 `\033[2J\033[H` 清一次 → 之后每帧只 `\033[H` + **一次性写出整帧** +
   每行行尾 `\033[K` 擦掉上次残留；运行期间隐藏光标（`\033[?25l`），退出/ Ctrl-C 时恢复。
5. **长行会折行**，折行后 `\033[K` 擦不干净 → VPU 行只列**非零**的核，并把设备地址缩成
   `rkvdec-core(fdc38100)` 这种短形式。
6. **Ctrl-C 只恢复终端不 `exit` = 要按好几次**：`trap 'restore' INT` 里如果只是恢复光标然后
   return，bash 会回到 `while` 循环接着刷，用户得连按（而且半帧状态下还会刷出错）。
   正确做法：INT/TERM 的 handler 里**自己 `exit`**（130/143），恢复终端放在幂等的函数里、
   EXIT trap 再兜一次。实测（真 pty 里写 `\x03`）：启动 0.5s 打断、跑起来后单次打断、
   连按两次、SIGTERM 四种情况都是**一次就退、无报错、光标恢复**。
7. **`sleep` 要能被信号打断**：直接 `sleep 1` 时 trap 得等它睡完才执行（最长一个刷新间隔的延迟）。
   写成 `sleep N & wait $! || true`，信号一到立刻进 handler。

## 5. 已知限制 / 可以再加的

- NPU / RGA 的**百分比**需要 root（debugfs 权限），非 root 只有中断速率。
  如果希望普通用户也能看，可以在镜像里放一条 udev 规则把这两个节点的组/权限放开（尚未做）。
- 没有进程级归因（"是哪个进程在用 VPU/RGA"）。`/proc/mpp_service/sessions-summary`
  与 `/sys/kernel/debug/rkrga/mm_session` 里有会话信息，但格式偏调试用，还没接进工具。
- 采样间隔最小 0.1 s 级别；更细的突发（单帧解码几十毫秒）会被平均掉。
- app 的 `t`（进程占用）只列**这一采样窗口里 utime+stime 增量 > 0** 的进程 —— 板子闲着时列表是空的
  （界面会写 `top processes / none used CPU in this interval`，老脚本则是干脆不打印这一段）。

## 6. app（Rust / ratatui）版的坑

1. ⛔ **`echo 密码 | sudo -S lsp-rk-monitor` 起 TUI，按键完全进不去**。Ubuntu 24.04 的 sudo 默认
   `Defaults use_pty`（`/etc/sudoers:15`）：sudo 会给命令**新开一个 pty**，而它自己的 stdin 是那根管道
   ⇒ 你在终端上敲的键不会被转发进去。表现非常有迷惑性：**画面在正常刷新，按键却被 tty 回显在屏幕上
   （散落在画面上），app 一点反应没有**。正确姿势：
   - 本地/远端交互看 TUI：`sudo lsp-rk-monitor` / `ssh -t <板子> sudo lsp-rk-monitor`（sudo 的 stdin 就是 tty）；
   - 自动化（脚本里）：别起 TUI，用 `--json` / `--once`，它们根本不需要键盘。
   实测这个坑时还额外确认了：`stty -a -F <pane tty>` 里 `echo` 是开的（因为 raw mode 只作用在
   **sudo 内层那个 pty**），所以"按键被回显"和"按键不生效"是同一个原因。
2. **X 轴会先缩到"已有数据的跨度"**：数据不满所选窗口（比如刚启动、窗口选了 60 s 但只有 3 s 数据）时
   如果不缩，所有点会全挤在右边缘成一坨实心块。所以标题会写 `window 60s (showing 5s)`，
   等数据攒够就自动变成整个窗口。刻度在跨度 < 4 s 时带一位小数（否则会出现 `-2s -2s -1s -1s`）。
3. **量程**：**百分比通道固定 `0~100%`**（有天然上界），其余单位（`MHz` / `C` / `/s`）按窗口自适应 ——
   所以"irq 速率几千/s 把百分比压成一条线"这种事不会再发生：它们本来就是不同的图。
   ⛔ 曾是"每张小图各自自适应"（百分比只锚 0，上界 = 窗口内峰值），结果**峰值按定义必然贴着顶画**，
   空闲核（峰值 2~3%）的图看着像满载（2026-09-20 用户报的"很低的占用，折线也会跑到顶"）——
   别改回去。判据：`tui.rs` 的 `axis_bounds()`，单测 `pct_axis_is_always_0_100` /
   `other_units_keep_autoscale`。
   CPU 视图**只画 8 个核**：整体占用在右上角明细行里，让它也占一张图会把 8 个核挤成 5 行
   （每张只剩 4 行画布）。
4. ⛔ **小图网格的 rect 必须裁到区域里，否则 ratatui 直接 panic**
   （`index outside of buffer: … but index is (1, 12)`）：把 8 张图塞进 9 行时行高被抬到最小 3 行，
   第 4 张起就落到区域外了 —— 板上 40x12 / 60x20 实测踩到，app 当场崩。
   现在一律 `min(区域下边界)` 裁剪、放不下就整张丢掉；另外 2 列的门槛降到 52 列
   （60x20 也能 2 列 4 行放下 8 个核）。
5. **`LSP_MONITOR_ASCII=1`**（字体没有盲文点阵时）：实测三种画法，只有 **Block + 不画网格**能看 ——
   `Dot` + 网格 = 数据点看不清、网格反而最显眼；`Block` + 网格 = 5 条网格线被画成整格实心条，
   跟数据一样粗。所以 ASCII 模式下网格是**故意关掉**的（Y 刻度数字还在）。
6. **别用 Block/Dot 之外的 `Marker::Bar` 画多通道**：它画的是"从 0 填到该点的实心条"，
   多通道会互相盖住（只剩最后画的那条）。
7. 曲线几何（稀疏连线 / 密集按列取 min-max 再首尾相连 / NaN 真断口）与 `lsp-mcap` 共用同一套实现和
   单测；注释里的 ⛔ 是踩过的：**只画"每列一条竖线"会看起来线断了**。
