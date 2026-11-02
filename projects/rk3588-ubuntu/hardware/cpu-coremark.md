# RK3588 的 CPU：CoreMark 实测（大小核 / 原生 vs qemu 模拟）

> 2026-09-12 实测。**两个用途**：① 说清这块板子的 CPU 拓扑与算力（cpu0-3 是小核、4-7 是大核）；
> ② 为[构建速度对比](../build/build-speed-comparison.md)提供 CPU 与进程启动微基准，
> 用于调查候选开销；这些测量不能单独确定 rootfs 构建瓶颈的占比。
> 所有数字都是板上/服务器上真跑出来的，命令见 §5。

---

## TL;DR

- 两个用途：① 说清这块板的 CPU 拓扑与算力（**cpu0-3 是小核、4-7 是大核**）；
  ② 给 rootfs 性能调查提供进程启动测量（§3），与端到端构建计时分开使用。
- 实测结果见 §2；**这些数字怎么用**见 §4（别当绝对性能看）；复现步骤见 §5。

## 0. 一句话结论

1. **cpu0-3 = Cortex-A55 @1.8 GHz，cpu4-7 = Cortex-A76 @2.304/2.352 GHz**；
   单核 A76 ≈ **2.54×** A55（CoreMark 17292 vs 6800）。
2. **qemu 执行 arm64 单线程 CoreMark ≈ 真 A76 的 0.82 倍**（14100 vs 17200）；
   x86 原生为 55820，模拟路径不能代表服务器的原生算力。
3. 动态小程序启动在服务器 qemu 下为 **9.7 ms**，板子为 **1.2 ms**（**8.2×**）；
   这是该微基准的观测，不能据此推出完整阶段的耗时分布。

---

## 1. 测试对象与方法

| 项 | 值 |
| --- | --- |
| 源码 | EEMBC **CoreMark**，commit `1f483d5`，tar 包 md5 `7220e3892d02e02f6e011b9d9a3a3148`（⚠️ 记录里的路径 `/home/board/sources/coremark` 是当年容器里的，**现已不存在**；见 §7 末条） |
| 板上编译器 | `gcc 13.3.0`（Ubuntu 24.04.4，原生 aarch64） |
| 服务器编译器 | `aarch64-linux-gnu-gcc 13.3.0`（交叉，`-static` 链接） |
| 模拟器 | `qemu-aarch64-static 8.2.2`（Debian 1:8.2.2+ds-0ubuntu1.18） |
| 编译选项 | 官方口径 `-O2`（`make PORT_DIR=linux`，脚本自动带 `-DPERFORMANCE_RUN=1`）；多上下文另加 `-DMULTITHREAD=8 -DUSE_PTHREAD=1` |
| 运行时长 | CoreMark 自己把迭代数校准到 ~10 s（单配置总 wall 约 15~30 s） |
| 校验 | 每次都输出 `Correct operation validated`（种子 CRC 对得上，说明结果有效） |

**板子上的频率/温度**（测试期间实测，未被限频）：

```
cpu0 (A55): 1800000 kHz 恒定          cpu4 (A76): 2304000 kHz 恒定     cpu7 (A76): 2352000 kHz 恒定
温度 thermal_zone0: 37.9 → 44.4 °C（跑满 8 核时）
```
governor 本来就是 `performance`（没改过），所以不需要额外调频；三个 cpufreq policy 分别是
`policy0`(cpu0-3, 1.8 GHz)、`policy4`(cpu4-5, 2.304 GHz)、`policy6`(cpu6-7, 2.352 GHz)。

**拓扑判据**（以后换板子/换内核照这个查）：

```bash
grep -E 'processor|CPU part' /proc/cpuinfo     # 0xd05 = Cortex-A55，0xd0b = Cortex-A76
cat /sys/devices/system/cpu/cpu0/cpu_capacity  # 405（小核）
cat /sys/devices/system/cpu/cpu4/cpu_capacity  # 1003
cat /sys/devices/system/cpu/cpu7/cpu_capacity  # 1024
ls -d /sys/devices/system/cpu/cpufreq/policy*  # policy0 policy4 policy6
```

---

## 2. 结果

### 2.1 板子上原生（RK3588，8 核）

| 配置 | CoreMark（iterations/s） | 每 MHz | 相对单核 |
| --- | --- | --- | --- |
| **A55 单线程**（`taskset -c 0`，1.8 GHz） | **6799.78** | 3.78 | 1.00 |
| **A76 单线程**（`taskset -c 4`，2.304 GHz） | **17106.69** | 7.42 | 2.52 |
| A76 单线程（`taskset -c 7`，2.352 GHz） | 17292.06 | 7.35 | 2.54 |
| A76 单线程复测（`-c 4`） | 17097.91 | — | （与首次差 0.05%） |
| **4×A55**（`-c 0-3`，M4） | **27130.35** | — | 3.99× 单核 A55 |
| **4×A76**（`-c 4-7`，M4） | **67911.71** | — | 3.97× 单核 A76 |
| 8 核（`-c 0-7`，M8，CoreMark 并行口径） | 84751.75 | — | 见下面的坑 |
| **8 核峰值**（8 个独立单线程进程各钉一核，同时跑） | **95303.4** | — | 4×A55 + 4×A76 = 95042 ✓ |

原始输出（两个代表）：

```
CoreMark 1.0 : 6799.777462 / GCC13.3.0 -O2 -DPERFORMANCE_RUN=1  -lrt / Heap
CoreMark 1.0 : 67911.714771 / … / Heap / 4:PThreads
```

> ⚠️ **`M8` 的 84752 不是 8 核峰值，别拿它当 SoC 性能**。CoreMark 的多上下文是"每个上下文跑
> **同样多**的迭代、一起开始、等最慢的结束"，所以 4 大 + 4 小这种异构配置下，wall time 由
> 落在小核上的那几个上下文决定 → 分数被拉低。实测 84752 / 95303 = **89%**，
> 差的那 11% 不是损失，是**口径问题**。要量峰值就用"8 个独立进程各钉一核再求和"（95303）。
> 顺带排除：跑满 8 核时频率**没有掉**（三个 policy 都钉在 max），温度也只有 44 °C，不是降频导致的。

### 2.2 服务器上 qemu 模拟（x86_64 Ultra 9 285K，24 核）

| 配置 | CoreMark（iterations/s） | 对比板子 |
| --- | --- | --- |
| `qemu-aarch64-static -cpu cortex-a55` 单线程 | 14208.58 | 2.09× 板子 A55 |
| `qemu-aarch64-static -cpu cortex-a76` 单线程 | 14005.60（复测 14123.30） | **0.82× 板子 A76** |
| `-cpu cortex-a76` 4 上下文（宿主 4 核） | 51786.64 | 0.76× 板子 4×A76 |
| `-cpu cortex-a76` 8 上下文（宿主 8 核） | 103492.88 | 1.09× 板子 8 核峰值 |
| **服务器本地 x86_64 原生** 单线程（参考） | 55820.56 | 3.26× 板子 A76 |
| 服务器本地 x86_64 原生 8 上下文（参考） | 418290.71 | 4.39× 板子 8 核峰值 |

**这张表最容易看错的地方**：qemu 的 `-cpu cortex-a55` / `cortex-a76` 只是选**指令集模型**，
不是模拟 A55/A76 的微架构和频率 —— 所以两者分数几乎一样（14208 vs 14005）。
它代表的是"**这台 x86 用 TCG 翻译执行 arm64 代码**"的速度，**不能**当成"服务器算力"。
真正的服务器算力看最后两行（x86 原生）。

---

## 3. 进程启动微基准与构建观察

同一份源码（`int main(void){return 0;}`）编译成静态/动态链接的小程序，
两边各 `exec` 300 次取平均（这条命令基本只测"起一个进程要多久"）：

| 环境 | 静态链接 | 动态链接 |
| --- | --- | --- |
| **板子上原生** | 0.919 ms | **1.176 ms** |
| **服务器 qemu-aarch64-static** | 5.700 ms | **9.693 ms** |
| 服务器 x86_64 原生 | 0.213 ms | 0.275 ms |

（qemu 跑动态链接的程序要多翻译一遍 `ld.so` 和 libc，所以比静态又贵了 70%。）

对上 `rootfs` 阶段的实测差距：

```
起一个动态进程：  qemu 9.693 ms  ÷  板子 1.176 ms  =  8.2×
`rootfs` 阶段总耗时：   46m57s (服务器 qemu) ÷ 6m04s (板子) =  7.7×
```

两个倍率接近，提示进程启动值得进一步测量，但缺少真实构建的 exec 计数、CPU / I/O 剖析，
不能确定它解释了多少端到端差距。内核交叉编译器在宿主原生运行，不涉及 QEMU 开销摊薄。
包清单、缓存、网络和存储的边界见[构建测量](../build/build-speed-comparison.md)。

---

## 4. 数字怎么用

- **评估"换块更快的板子有没有意义"**：A76 单核 17.2k、A55 6.8k 是基线；
  板子 8 核峰值 95.3k，和一台 8 核 x86 原生（418k）差 4.4 倍 —— 差距主要在**单核 IPC/频率**。
- **评估 rootfs 构建位置**：结合[历史构建记录](../build/build-speed-comparison.md)选择原生 ARM64 路径，
  新环境按相同包清单和缓存条件重测；微基准不承诺当前构建时间。

---

## 5. 复现步骤

```bash
# ---------- 板子上（原生 aarch64） ----------
scp coremark.tgz board@<板子IP>:~/ && cd ~ && tar xzf coremark.tgz -C ~/coremark --strip-components=1 && cd ~/coremark
make PORT_DIR=linux                                     # 单线程
make PORT_DIR=linux XCFLAGS="-DMULTITHREAD=8 -DUSE_PTHREAD=1"   # 多上下文（pthread）

taskset -c 0   ./coremark.exe            # 小核 A55 单线程
taskset -c 4   ./coremark.exe            # 大核 A76 单线程（cpu7 是 2.352 GHz 那档）
taskset -c 0-3 ./coremark.exe M4         # 4 个小核
taskset -c 4-7 ./coremark.exe M4         # 4 个大核
taskset -c 0-7 ./coremark.exe M8         # 8 核（注意 §2.1 的口径问题）

# 8 核峰值（每个进程钉一个核，再求和）
for c in 0 1 2 3 4 5 6 7; do ( taskset -c $c ./coremark.exe > /tmp/one_$c.log 2>&1 ) & done; wait
grep -h 'Iterations/Sec' /tmp/one_*.log | awk '{s+=$3} END{print s}'

# ---------- 服务器上（交叉编译 + qemu 模拟 arm64） ----------
make PORT_DIR=linux CC=aarch64-linux-gnu-gcc XCFLAGS="-static"
qemu-aarch64-static -cpu cortex-a55 ./coremark.exe
qemu-aarch64-static -cpu cortex-a76 ./coremark.exe
qemu-aarch64-static -cpu cortex-a76 ./coremark.exe M8     # 多上下文版要另编（-DMULTITHREAD=8 -DUSE_PTHREAD=1）
# x86 原生参考直接在同一目录再 make 一次即可（不带 CC= 就是本机 gcc）
```

**进程启动开销**（§3 那张表）：

```bash
printf 'int main(void){return 0;}\n' > /tmp/hi.c
gcc -O2 -static -o /tmp/hi-s /tmp/hi.c && gcc -O2 -o /tmp/hi-d /tmp/hi.c
for b in /tmp/hi-s /tmp/hi-d; do S=$(date +%s.%N); for i in $(seq 1 300); do $b; done; E=$(date +%s.%N); \
  awk -v s=$S -v e=$E -v b=$b 'BEGIN{printf "%s 每次 %.3f ms\n", b, (e-s)*1000/300}'; done
# 服务器上把 $b 换成 qemu-aarch64-static [-L /usr/aarch64-linux-gnu] /tmp/hi-* （aarch64 版）
```

---

## 6. 注意 / 别踩

- ⛔ **别把 qemu 的分数当"服务器性能"**：它测的是 TCG 翻译速度（§2.2 的说明）。
- ⛔ **别用 `M8` 的 84752 当 8 核峰值**：异构集群 + CoreMark 的等迭代口径 → 用 8 个独立钉核进程（95303）。
- 频率/温度会影响数值：这块板子跑 CoreMark 时**没降频**（实测三个 policy 全程钉在 max、最高 44 °C），
  但如果外面还压着别的负载（比如同时在编译），数字会偏低 —— 测之前先 `uptime` 看一眼。
- CoreMark 官方要求跑两次（`run1.log`/`run2.log`）并报告编译选项；这里只取
  `CoreMark 1.0` 那一行的 iterations/sec（单次内部已经跑 ~10 s 取平均，复测差 < 0.1%）。
- 源码**不在本仓库里**（当年放在 `/home/board/sources/coremark`，那个容器路径**现已不存在**）：
  现在板子上放在 `~/coremark`、服务器上放在 `~/ubuntu-lsp-rk3588/build/coremark`
  （`build/` 被 gitignore，重启不丢，可直接复跑）；要重取就按 commit `1f483d5` 拉 EEMBC CoreMark
  （tar 包 md5 `7220e3892d02e02f6e011b9d9a3a3148`）。
  两边都已编好 `coremark.st.exe`/`coremark.mt.exe`（板子）与 `cm-aarch64-{st,mt}.exe`/`cm-x86-{st,mt}.exe`（服务器）。
