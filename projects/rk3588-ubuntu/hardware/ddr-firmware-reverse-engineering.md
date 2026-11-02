# DDR 初始化固件（A 核那份）逆向

> `idbloader.img` 里那份 **DDR 初始化/训练固件**（rkbin，**AArch64，跑在 A 核 PMU SRAM**）的逆向。
> 它**是什么、干什么用**见 [bootloader-blobs.md](bootloader-blobs.md) §3；
> **另一份**（`atf-3` 的 M0 固件）单独成篇：[mcu-firmware-reverse-engineering.md](mcu-firmware-reverse-engineering.md)；
> BL31（EL3）机制见 [bl31-mechanism.md](bl31-mechanism.md)。

## TL;DR

- 它是 **A 核上的 DDR 初始化/训练固件**（75,320 B，AArch64，被 BootROM 载入 PMU SRAM `0xff001000`）：
  自报 `ddr-v1.18-9fa84341ce`，支持 LPDDR2/3/4/4X/5，尾部有**符号/参数目录**
  （`sdram_params_g`、`dram_timing_g`、`eye_scan_params`、`*_fsp_g`…），把训练结果与各频率档参数
  导出在固定 SRAM 地址上（BL31/内核的 DDR 变频拿这些档位）。
- 代码区 `0x0`–`0x10FFF` 是 **AArch64**（17,408 条指令，只有 2.3% 解不出），`0x11000` 之后是表与字符串。
- 入口会先判"我是否跑在 `0xff001000`"，主体入口 `0x10530` 先校验芯片 ID（读 `0xfd588080` 比对 `0xef08a53c`）。
- ⚠️ 它与 `atf-3` 那份 **M0 固件不是一回事**（非零窗口零匹配），那份见
  [mcu-firmware-reverse-engineering.md](mcu-firmware-reverse-engineering.md)。

## 1. 取副本与反汇编

```bash
# A 核初始化固件：直接用仓库里那份
F=packages/u-boot-lubancat-rk3588/debian/rkbin/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin
# M0 固件：从 BL31 ELF 的段里取（权威）
E=packages/u-boot-lubancat-rk3588/debian/rkbin/rk3588_bl31_v1.53.elf
aarch64-linux-gnu-objcopy -O binary --only-section=.ddr_m0_bin "$E" /tmp/ddr_m0.bin   # 24,576 B
# 反汇编：A 核 AArch64 / M0 用 ARM Thumb（本机两个交叉 objdump 都有）
aarch64-linux-gnu-objdump  -D -b binary -m aarch64 "$F"
arm-none-eabi-objdump      -D -b binary -m arm -M force-thumb /tmp/ddr_m0.bin
```



## 2. A 核那份：`idbloader.img` 里的 DDR 初始化固件（rkbin，AArch64）

**结论**：这是一份**跑在 A 核上的 DDR 初始化/训练固件**（不是 M0 代码）；M0 只管运行期的变频/self-refresh，
那份固件在 `u-boot.itb` 的 `atf-3` 里（见 4.1 与 §3.3 末尾的分工表）。

**它自报家门**（字符串实证）：

```
ddr-v1.18-9fa84341ce
DDR 9fa84341ce typ 25/02/06-17:24.09,fwver: v1.18      ← git hash + 构建时间 + 固件版本
pd/pu vd_ddr                                            ← 挂在 vd_ddr 电源域
LPDDR2 / LPDDR3 / LPDDR4 / LPDDR4X / LPDDR5             ← 一份固件支持 5 种 DRAM 类型
```

文件名里的 `lp4_1848MHz_lp5_2400MHz` 是两组参数集（LPDDR4 最高 1848、LPDDR5 最高 2400）。

**布局**（75,320 B）：

| 区段 | 内容 | 判据 |
| --- | --- | --- |
| `0x00000`–`0x10FFF` | **AArch64 代码**（约 68 KB） | `aarch64-linux-gnu-objdump` 解出 17,408 条指令，**只有 2.3% 是 `.inst/udf`**；544 个 `bl` |
| `0x11000`–`0x12638` | **表 + 字符串**（约 4 KB，63~75% 是零） | 符号/参数目录，见下 |

**入口语义**（反汇编实证）：

```
0x0000:  b 0x4                          ← 惯例头
0x0004:  stp x0,x4 / stp x29,x30 …       ← 真入口
0x000c:  ldr x4, [0x38] = 0xff001000     ← 期望的代码基址（PMU SRAM）
0x0018:  cmp x0, x4 / b.ne …             ← 判断"我是否跑在 0xff001000"
0x0034:  b 0x10530                       ← 主体入口
0x10530: 序言 → 读 [0xfd588080]，与 0xef08a53c 比较   ← 校验芯片/版本 ID
```

⇒ 它是被 BootROM 载入 **PMU SRAM `0xff001000`** 执行的 A 核固件。

**它摸哪些硬件**（把反汇编里的 `movk` 高 16 位做直方图，再用 RK3588 的 DTSI 对名）：

| 页 | 次数 | 是什么 |
| --- | --- | --- |
| `0xfd5f0000` | **31** | **`ioc`（IOMUX）** —— DTSI 实锤；DDR PHY 训练前后大量引脚复用/驱动强度设置 |
| `0xfe050000` | 11 | ⚠️ RK3588 的 DTSI/驱动里**没有**这个节点（其他 SoC 的 `fe050000` 是 `coregrf`/uart）⇒ 按位置与用法判断属 DDR 控制器/PHY 一侧，**要 TRM 才能坐实**（本仓库 `hardware-docs/` 只有索引、没存 `rk3588/` 的 TRM 原件） |
| `0xfd590000` | 3 | `bigcore0_grf`（GRF） |
| `0xfd7c0000` | 1 | `cru`（时钟控制器） |
| `0xfd58xxxx`/`0xfd8cxxxx`/`0xfe01xxxx`/`0xfe03xxxx`/`0xff00xxxx` | 各 1~4 | syscon / PMU / 电源域一类（`fd58c000`、`fd8d8000.power-management` 这几个在开机日志里都出现过） |

**尾部那张表 = 符号/参数目录**（最有信息量）：记录形如 `(id:u32, flags:u32, SRAM 地址:u64, 长度:u64)`，
步长 24 B；跟着 12 个符号名：

```
dram_timing_g      timing_config_fsp_g   eye_scan_params    sdram_params_fsp_g
final_sdram_params dram_timing_fsp_g     phy_vref_tab       printf_buffer
timing_config_g    boot1_parm            dram_info          sdram_params_g
```

读法：固件把**训练结果与各频率档参数**导出在这些 SRAM 地址上（实测记录如
`(id=1, 0xff015f88, len=0x2ac)`、`(id=0x0f, 0xff016238, len=0x94)`…）。`*_fsp_g` 就是
**FSP（frequency set point）各档参数** —— 正对应 BL31 启动日志里那句
`dfs DDR fsp_params[0..3].freq_mhz = 1848/528/1068/1560`。

**能力字符串**（它自己会打的日志）：`WR Training result:`、`TX Vref:%d.%d%%`、`%s eye:`、
`scan ch[%d]:`、`%s dq%d,`、`vref`（眼图/Vref 训练）、`unsupported dram type`、
`spec timing unsupported`、`stride=0x%x, ddr_config=0x%x`、`Unit: %d fs`。

**与 M0 的分工（重要）**：

| 阶段 | 谁在跑 | 代码在哪 |
| --- | --- | --- |
| 上电初始化 + DDR 训练 | **A 核**（PMU SRAM `0xff001000`） | 就是这份 rkbin DDR 固件（75,320 B，AArch64） |
| 运行期 DDR 变频 / self-refresh | **DDR 子系统里的 M0** | `u-boot.itb` 的 `atf-3`（`.ddr_m0_bin`，24,576 B，FIT load `0x000F0000`） |（**逆向见 §4.3**）

依据：① 这份固件 `0x0`–`0x11000` 是 AArch64（97.7% 可解），**没有 Cortex-M 向量表候选**，
也没有 `m0/mcu/pmu/sram` 之类字符串；② 它与 BL31 的 `atf-3` 做**非零** 16 B 窗口比对是 **0/733 命中**
（若把零字节也算上会得到"52% 相同"的假象，别用那个口径）；③ 内核 DTS 里的 `dmc` 节点**没有 `reg`**
（`rk3588s.dtsi` 的 `dmc: dmc { … }`，纯虚拟节点）⇒ Linux 不直接碰 DDR 寄存器，只走 SMC
（`ROCKCHIP_SIP_CONFIG_MCU_START` 拉起 M0、`…DRAM_SET_RATE`、`…DRAM_SET_AT_SR`），接口见
[bl31-mechanism.md](bl31-mechanism.md)。

**复现命令**：

```bash
F=packages/u-boot-lubancat-rk3588/debian/rkbin/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin
strings -n 4 "$F" | head -40                       # 版本串 / 训练日志 / DRAM 类型
aarch64-linux-gnu-objdump -D -b binary -m aarch64 "$F" | head -40    # 入口（AArch64）
# 「是不是 AArch64 代码」的量化判据：0x0~0x11000 里 .inst/udf 占比（实测 2.3%）＋寄存器页直方图（movk 高 16 位）
aarch64-linux-gnu-objdump -D -b binary -m aarch64 --stop-address=0x11000 "$F" \
  | awk '/^ +[0-9a-f]+:/{n++; if ($0 ~ /\.inst|udf/) b++} END{printf "%d 条指令, %.1f%% 解不出\n", n, b*100/n}' 
```

⚠️ **本节的边界**：只做了"结构 + 入口 + 寄存器页 + 字符串 + 导出表"这一层，**没有反编译逐函数**；
`0xfe05xxxx` 未能坐实；`atf-3`（M0 固件）本身**还没逆**。

## 3. 复现命令与边界

复现命令分散在各节末尾（§1 取副本与反汇编、§2 版本串/入口/寄存器页直方图、§3 向量表与函数图）。

**边界（没做完的）**：

  `movs r0, r4`，`r4` 由上层大函数（`0x1336`/`0x98a`）算出，要把那两段读透才能坐实；
- 寄存器**字段语义**：`0xfd59e010`、`0xfd58a208/a20c/a210/a214`、`0xfe0c*`、`0xfec7*`、`0xfe11*`
  ——需要 TRM（本仓库 `hardware-docs/` 只有索引、没有 `rk3588/` 原件）；

