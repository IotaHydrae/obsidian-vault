# BL31（TF-A / EL3 常驻固件）机制详解

> BL31 = TF-A 的 **EL3 常驻固件**：RK3588 上它是唯一常驻 EL3 的东西，从 SPL 接过控制权后**永不退出**，
> 内核的 CPU 上下线/复位/关机/深睡全部经 **SMC** 调它。我们出货的是 `rk3588_bl31_v1.53.elf`
> （TF-A `v2.3-942-g98eaeb2f3`）。

## TL;DR

- 常驻位置：DRAM 低端 `0x40000`（`memsz 0x6f000`）起，另有 PMU SRAM 段与 DDR M0 段。
- 提供的服务：**PSCI**（标准，内核直接用）+ **Rockchip SIP**（DDR 变频、VPU 复位、遥控唤醒源、
  虚拟关机、last log…，`0x82000000` 段）+ **opteed**（给 BL32/OP-TEE 的通道）+ 异常处理 EHF + 深睡代码。
- 怎么确认版本：**串口 banner 最直接**；内核侧没有现成出口（见 §5.2）。
- ⛔ 换 BL31 必须换**同一构建的一对** `idbloader.img` + `u-boot.itb`，且核对 DDR/固件兼容矩阵（§3.5）。
- v1.54 起主体段从 `0x40000` 挪到 `0x60000`；版本差异与 changelog 见
  [bl31-version-diff.md](bl31-version-diff.md)。
- BL32（OP-TEE）另见 [bl32-optee.md](bl32-optee.md)；启动链全貌见
  [rk3588-boot-flow.md](rk3588-boot-flow.md)。
- 文中所有"地址 / 字符串 / 反汇编"都出自**我们出货的这一颗** `rk3588_bl31_v1.53.elf`（仓库内）。

---

## 结论速览（表）

| 问题 | 答案 |
| --- | --- |
| BL31 是什么？ | ARM Trusted Firmware（TF-A）的 **EL3 运行时固件**。RK3588 上它是**唯一常驻 EL3** 的东西，从 SPL 手里接过控制权后**永不退出** |
| 出货版本 | `rkbin/rk3588_bl31_v1.53.elf`：TF-A `v2.3-942-g98eaeb2f3`、`fwver: v1.53`、编译 `12:10:56, Aug 25 2025`、365,344 B、`sha256=4969a8d7…e34a2` |
| 它"跑"在哪 | DRAM 低端 `0x40000` 起（`memsz 0x6f000`，到 `0xAF000`），另有 PMU SRAM 段与 DDR M0 段（§2） |
| 提供什么 | ① PSCI（CPU 上下线/复位/关机/休眠）② Rockchip 私有 SIP（DDR 变频、总线、VPU 复位、遥控唤醒源、虚拟关机、last log…）③ BL32(OP-TEE) 的通道 opteed ④ 异常处理 EHF ⑤ 深睡/PMU 域代码 |
| 谁来调它 | normal world（内核）全部经 **SMC 指令**（`conduit = smc`）；EL3 内部还有 PMU MCU 等 |
| 怎么确认版本 | **串口 banner**最直接；内核侧理论上能用 SIP `0x82000001` 问，但**内核没有现成出口**（§5.2） |
| 默认产物里 BL31 是谁 | **v1.53**（当前 `prebuilt/{idbloader.img,u-boot.itb}` 的 sha256 **以 `AGENTS.md` §6 那条为准** —— 那份记录会随每次重建更新，别在这里抄死数字）。判据：`tools/check-atf.sh` → rc=0「全链路都含 ATF 且内部 sha256 一致」。旧版本只能从 git 历史取（`git show <旧提交>:prebuilt/u-boot.itb`） |

---

## 1. 它在启动链里的位置与生命周期

```
BootROM
  └─ idbloader.img（DDR 固件 v1.18 + U-Boot SPL）
        └─ SPL（= TF-A 语境里的 "BL2" 角色，但不是 TF-A 的 BL2！）
              └─ BL31  ← 载入 0x40000，EL3，**常驻，不返回**
                    ├─（可选）BL32 = OP-TEE，S-EL1，常驻
                    └─ BL33 = U-Boot proper（EL2，`SPSR=0x3c9`）
                          └─ Linux（EL1）
```

三个容易搞错的点（细节见 [rk3588-boot-flow.md](rk3588-boot-flow.md) §3.7）：

1. **日志里的 "BL2" 指的是 U-Boot SPL**，不是 TF-A 的 BL2。Rockchip 这套里 TF-A 只提供
   BL31（+ 可选 BL32），BL2/BL33 的角色由 U-Boot 扮演。所以
   `No OPTEE provided by BL2 boot loader` 的意思是"SPL 没给 BL31 传 BL32"。
2. **BL31 不移交给 BL32**：两者是**平行常驻**的服务，normal world 用不同 SMC 号访问
   （PSCI → BL31 自己；TEE 调用 → BL31 的 `opteed` 转发给 BL32）。
3. **BL31 常驻的代价不是内存池**，而是"EL3 里永远有一段活代码 + 每次 SMC 都要陷入 EL3"；
   内核对它的依赖是硬性的（没有 BL31，`cpu_on/cpu_off/reboot/poweroff` 全废）。

---

## 2. 二进制解剖（我们出货的这一颗）

### 2.1 ELF 骨架

```console
$ readelf -hW rk3588_bl31_v1.53.elf | grep -E "Type|Entry"
Type:    EXEC (Executable file)
Entry:   0x40000

$ readelf -lW rk3588_bl31_v1.53.elf
  LOAD 0x010000 0x0000000000040000 ... FileSiz 0x022f70 MemSiz 0x06f000 RWE   ← atf-1
  LOAD 0x040000 0x00000000000f0000 ... FileSiz 0x006000 MemSiz 0x006000 R     ← atf-3
  LOAD 0x050000 0x00000000ff100000 ... FileSiz 0x009000 MemSiz 0x009000 RWE   ← atf-2
```

| FIT 节点 | load | 文件大小 | 里面是什么 | 干什么 |
| --- | --- | --- | --- | --- |
| `atf-1` | `0x40000` | 0x22f70 = 143,216 | `ro` + `.data` + `stacks` + `.bss` + `xlat_table` | **主 BL31**：EL3 运行时服务、SMC 分发、PSCI、驱动 |
| `atf-2` | `0xff100000` | 0x9000 = 36,864 | `.text_pmusram` | **从 PMU SRAM 执行的代码**：掉电/深睡时 DRAM 关掉后还要跑的路径 |
| `atf-3` | `0xf0000` | 0x6000 = 24,576 | `.ddr_m0_bin` | **DDR M0 固件**（PMU 里的 MCU 用，做 DDR 变频/训练） |

> **这三段都是被 SPL 按 FIT 的 `load=` 直接摆到位的**（`atf-2` → PMU SRAM `0xff100000`、
> `atf-3` → DRAM `0xf0000`）；FIT 的 `conf.firmware = "atf-1"`、`loadables = "uboot","atf-2","atf-3"`
> ⇒ **SPL 只跳 `atf-1`，另外两段"只加载、不跳转"**，由 BL31 自己按需使用。
>
> 这三段是**同一个 ELF 的三个 `PT_LOAD`**，不是三个文件。FIT 的生成链：
> `make_fit_atf.sh` → `gen_bl31_node()`（`u-boot/arch/arm/mach-rockchip/fit_nodes.sh:93`）
> → 调 `decode_bl31.py`，它按 `p_filesz` 把每个 `PT_LOAD` 写成 `bl31_0x<物理地址>.bin`
> （文件名里的地址就是 FIT 的 `load=`），再 `sort --key=5 -nr` **按大小降序**编号：
> 最大的当 `atf-1`，其余依次 `atf-2/atf-3`。⚠️ 所以 **`atf-N` 的编号不保证与 ELF 段序一致**，
> 判据永远是 `load=` 地址（串口 `## Checking atf-1 0x00040000` 就是它）。

### 2.2 内部节表（`readelf -SW`）

| 节 | 地址 | 大小 | 说明 |
| --- | --- | --- | --- |
| `ro` | `0x40000` | 0x1f000 | 代码+只读数据 |
| `.data` | `0x5f000` | 0x3f70 | 已初始化数据 |
| `stacks` | `0x62f80` | 0x4000 | **每个核 2 KB × 16 个可能的 CPU**（见下） |
| `.bss` | `0x66f80` | 0x35f60 | 未初始化数据 |
| `xlat_table` | `0x9d000` | 0x12000 | EL3 的 MMU 页表（4 KB 粒度） |
| `.ddr_m0_bin` | `0xf0000` | 0x6000 | DDR M0 blob |
| `.text_pmusram` | `0xff100000` | 0x9000 | PMU SRAM 代码 |
| `.text_sram` / `.data_sram` | `0xff001000` | **0** | 本构建里是空的（预留给"从 SoC 内部 SRAM 跑"的形态） |

**每核栈是算出来的，不是数组索引**（反汇编 `0x589b8`）：

```
589b8: bl 0x58a20           ; x0 = aff0 + aff1        （mpidr → 线性 CPU 号）
589bc: adrp x2, 0x63000
589c0: add x2, x2, #0x780   ; 栈区基址 0x63780
589c4: mov x1, #0x800       ; 每核 2 KB
589c8: madd x0, x0, x1, x2  ; sp = 0x63780 + idx*0x800
```

⇒ `stacks` 段 `0x62f80..0x66f80` 共 16 KB = **8 个核 × 2 KB**，`sp_n = 0x63780 + n*0x800`
（栈向下生长；`sp_0 = 0x63780`、`sp_7 = 0x66f80` 正好是段尾）。

### 2.3 低 2 MB 的内存地图（板级实测 + DTS）

```
0x00000000 ┌──────────────────────────────────────────┐
           │ （SoC 内部 SRAM / 保留）                  │
0x00040000 ├── BL31 主镜像（atf-1，mem 0x6f000）────────┤ ← EL3 常驻
0x000AF000 ├──────────────────────────────────────────┤
0x000F0000 ├── DDR M0 blob（atf-3，0x6000）─────────────┤
0x000F6000 ├──────────────────────────────────────────┤
0x00110000 ├── ramoops（0x110000..0x1F0000，内核保留）──┤ ← 掉电前日志/pmsg
0x001F0000 ├── minidump-smem（0x100，DT 里 disabled）───┤
0x00200000 ├── BL33 = U-Boot proper（banner: Entry point address = 0x200000）
           │   …内核 System RAM 从这里往上…
0xFF100000 ├── BL31 PMU SRAM 代码（atf-2，0x9000，映射到 PMU SRAM）
```

- ⚠️ **v1.54 起主体段从 `0x40000` 挪到了 `0x60000`**（见 [bl31-version-diff.md](bl31-version-diff.md)）；
  下面是 **v1.53**（我们在用）的地图。
- `ramoops` 的地址/大小写死在 `kernel-6.1/.../rk3588-linux.dtsi:92`：
  `reg = <0x0 0x110000 0x0 0xe0000>`，注释就是"0x110000 to 0x1f0000 is for ramoops"。
- `0xff100000`（`atf-2` 的 PMU SRAM）**在固件私有区间**：内核 DT 的 `syssram@ff001000` 只有 `0xef000`
  长（到 `0xff0f0000`），**不覆盖它** ⇒ Linux 永远不碰这块，BL31 可以永久把代码驻留在这里。
  （`0xff001000` 本身是 BootROM 放 DDR 初始化固件的位置，但那是上电阶段、跑完即退出，不冲突。）
- BL31 **没有设备树节点**，内核只能通过"内存从 0x200000 开始"绕开它 ⇒ 别指望
  `/proc/device-tree` 里找到 BL31，也别想用 `/dev/mem` 读它（DRAM 映射在内核里够不到低端）。
- 实测（v2 板，eMMC 启动、无 BL32）：`MemTotal: 8121060 kB`；
  同一块板开 BL32 时是 `8104708 kB`（少的 16,352 kB 是 OP-TEE 的 TEEOS/TA/SHM，见 bl32-optee.md）。

---

## 3. EL3 运行时服务：BL31 到底提供什么

### 3.1 PSCI（标准服务，内核直接用）

板子上实测的内核日志：

```
psci: probing for conduit method from DT.
psci: PSCIv1.1 detected in firmware.
psci: Using standard PSCI v0.2 function IDs
psci: MIGRATE_INFO_TYPE not supported.
psci: SMC Calling Convention v1.2
```

- DT：`/sys/firmware/devicetree/base/psci/method = "smc"`（不是 hvc）。
- 内核用的是 **PSCI v0.2 的 function ID**（`psci_0_2_set_functions()`），BL31 报 **v1.1**。
- 实际用到/可用的：
  | 功能 | function ID | 我们这边的证据 |
  | --- | --- | --- |
  | `PSCI_VERSION` | 0x84000000 | 内核打印 `PSCIv1.1` |
  | `CPU_SUSPEND` / `CPU_OFF` / `CPU_ON` | 0x84000001/2/3 | **实测**：`cpu7` offline→`0-6`、online→`0-7`（PSCI 正常） |
  | `AFFINITY_INFO` | 0x84000004 | 热插拔/`psci_affinity_info` |
  | `MIGRATE` / `MIGRATE_INFO_TYPE` | 0x84000005/6 | BL31 返 `NOT_SUPPORTED`（`psci.c:552`）⇒ **没有多核 Trusted OS**，与我们没上 BL32 一致 |
  | `SYSTEM_OFF` | 0x84000008 | `poweroff` 走它 → BL31 写 PMU/GPIO（§4） |
  | `SYSTEM_RESET` | 0x84000009 | `reboot` 走它 → BL31 写 CRU `GLB_SRST_FST`（§4） |
  | `SYSTEM_RESET2` | 0xC4000009 | BL31 支持；只有 `reboot=warm/soft` 时才用（`psci.c:310`） |
  | `PSCI_FEATURES` | 0x8400000A | 内核用它探 `SYSTEM_RESET2` 是否可用 |
- `/sys/power/state = "freeze mem"` ⇒ 内核注册了 PSCI 的 suspend ops；`mem` 这条深睡路径
  在 BL31 里对应 PMU 域那一套（[suspend-and-deep-sleep.md](suspend-and-deep-sleep.md)），
  **我们没有实测过 `mem` 睡眠**（怕醒不过来）。

### 3.2 SIP（Rockchip 私有 SMC，0x82000000 段）

内核侧定义在 `kernel-6.1/include/linux/rockchip/rockchip_sip.h`，**这是 BL31 暴露给 normal world 的私有接口全表**：

| SIP 号 | 名字 | 谁在用（内核驱动） |
| --- | --- | --- |
| `0x82000001` | `SIP_ATF_VERSION` | `sip_smc_get_atf_version()` **已导出但无人调用**（§5.2） |
| `0x82000002` | `SIP_ACCESS_REG` | `drivers/nvmem/rockchip-efuse.c`（**经 EL3 读写 eFuse 控制器**；我们板子上这个驱动不绑定，见 §3.5） |
| `0x82000003` | `SIP_SUSPEND_MODE` | `drivers/soc/rockchip/rockchip_pm_config.c`（虚拟关机）、深睡配置 |
| `0x82000004` | `SIP_PENDING_CPUS` | 深睡时"哪些核还醒着" |
| `0x82000005` | `SIP_UARTDBG_CFG` | 串口调试（`fiq_debugger`） |
| `0x82000006` | `SIP_MCU_EL3FIQ_CFG` | `sip_smc_mcu_el3fiq()`：PMU MCU ↔ EL3 的 FIQ |
| `0x82000007` | `SIP_SECURE_MEM_CONFIG` | 共享内存配置 |
| `0x82000008` | `SIP_DRAM_FREQ` | **DDR 变频**（子功能见下），被 `clk-ddr.c`/`rockchip_dmc.c`(devfreq)/`rockchip_edac.c`/`rk_system_heap.c`/`rockchip_bus.c` 调 |
| `0x82000009` | `SIP_SHARE_MEM` | `sip_smc_request_share_mem()`：内核↔EL3 的共享页（DDR addr map、ECC 统计…） |
| `0x8200000a` | `SIP_SIP_VERSION` | SIP 接口自身版本 |
| `0x8200000b` | `SIP_REMOTECTL_CFG` | `drivers/input/remotectl/rockchip_pwm_remotectl.c`：**把遥控/电源键配成唤醒源**（EL3 在深睡里管它） |
| `0x8200000c` | `SIP_VPU_RESET` | `drivers/video/rockchip/mpp/*`：**解码器硬复位**由 EL3 做 |
| `0x8200000d` | `SIP_BUS_CFG` | `drivers/devfreq/rockchip_bus.c`：总线空闲/自动门控 |
| `0x8200000e` | `SIP_LAST_LOG` | `sip_smc_lastlog_request()`：取 **BL31 的 last log**（掉电前日志，用于死机定位） |
| `0x8200000f` | `SIP_ACCESS_MEM_OS_REG` | 深睡时保存/恢复"内存 OS 寄存器" |
| `0x82000010` | `SIP_SCMI_AGENT0` | SCMI 代理（RK3588 上给 PMU MCU 用） |

⚠️ 口径：表的"谁在用"列来自对内核源码的 grep；**少数几项（`PENDING_CPUS` / `UARTDBG_CFG` /
`SECURE_MEM_CONFIG` / `SCMI_AGENT0` 等）在内核里没有直接调用者**，含义按 SIP 头文件命名 + BL31
侧字符串给出，不做过度推断。

`SIP_DRAM_FREQ` 的子功能（`ROCKCHIP_SIP_CONFIG_DRAM_*`，0x00~0x13）：
`INIT / SET_RATE / ROUND_RATE / SET_AT_SR / GET_BW / GET_RATE / CLR_IRQ / SET_PARAM /
GET_VERSION / POST_SET_RATE / SET_MSCH_RL / DEBUG / MCU_START / ECC / GET_FREQ_INFO /
ADDRMAP_GET / GET_STALL_TIME / ECC_POISON / TREFI_UPD`
⇒ **DDR 的变频/训练/ECC 全在 EL3 侧执行**，内核只是下命令。

BL31 二进制里能对上的处理函数名（`strings`）：

```
rockchip_sip_svc, rockchip_plat_sip_handler, ddr_plat_smc_handler, suspend_mode_handler,
access_mem_os_reg_handler, pvtpll_config_handler, hdmirx_config_handler,
hdcp_storage_handler, rk_sip_hdcp_config_handler, last_log_request
```

### 3.3 BL32（OP-TEE）的通道：opteed + secure 侧资源

- `opteed_fast` / `opteed_std` = BL31 里的 **SPD 层**：把 normal world 的 TEE SMC 转给 BL32，
  并把返回值搬回来。没有 BL32 时它们的返回是 `SMC_UNK`，串口会打
  `WARNING: No OPTEE provided by BL2 boot loader … SMC's destined for OPTEE will return SMC_UNK`。
- 反向的：BL31 还替 **secure world** 提供时钟/电源域控制
  （`rockchip_opteed_clk_enable/clk_get_info/clk_get_rate/clk_get_status/clk_set_rate`、
  `rockchip_opteed_pd_enable/pd_get_info/pd_get_status`）——
  OP-TEE 没有自己的 CRU/PMU 驱动，要靠 EL3 代劳。
- 打开 BL32 的完整流程/实测/回滚见 [bl32-optee.md](bl32-optee.md)。

### 3.5 DDR / 版本兼容（⚠️ 换 BL31 时必须一起看）

BL31 里带 DDR M0 固件（`atf-3`），而且**它和 SPL 用的 DDR bin 有版本匹配要求**，字符串写得很直白：

```
current loader need match with trust bl31 V1.07-V1.37
current loader need match with trust bl31 V1.38-V1.40
current loader need match with trust bl31 V1.41 or newer
current trust bl31 need match with loader ddr bin V1.13 or newer
dfs DDR fsp_params[0].freq_mhz= 1848MHz     ← 板子上实际打印（v1.53）
dfs DDR fsp_params[1].freq_mhz= 528MHz
dfs DDR fsp_params[2].freq_mhz= 1068MHz
dfs DDR fsp_params[3].freq_mhz= 1560MHz
ddr_fsp_init, ddr_round_rate, ddr_plat_smc_handler, ddr_ecc_info, ddr_ecc_poison
```

**M0 固件是谁加载的**（2026-10-03 查证）：

1. **SPL**：按 FIT 的 `load=` 把 `atf-3`（`.ddr_m0_bin`）放到 **DRAM `0xf0000`**（BL31 的只读数据段），
   此时它只是"躺在 DRAM 里的一份数据"；
2. **BL31**：把它**搬进 DDR MCU 自己的 SRAM**，并用 `SIP_MCU_CFG`（`0x82000028`）下的
   `CONFIG_MCU_CODE_START_ADDR`(0x01) … `CONFIG_MCU_CACHE_END_ADDR`(0x06) 这一组子命令告诉 MCU
   "你的代码 / SRAM / cache 在哪"，然后放它跑（BL31 字符串里有 `Waiting DDR_MCU work done timeout!`、
   `DDR_MCU init status error!`，佐证生命周期归它管）；
3. **内核只是"喊开始"**：运行期 `drivers/devfreq/rockchip_dmc.c` 调
   `sip_smc_dram(0, 0, ROCKCHIP_SIP_CONFIG_MCU_START)` —— **三个参数全是 0，不带任何代码或地址**，
   驱动里那个 `addr_mcu_el3` 字段其实**从未被使用**；DT 里也没有 M0 代码区
   ⇒ **M0 的代码来源是 BL31 里那份 `atf-3`，不是内核、也不是 DT 给的**。

⚠️ "BL31 是上电初始化时就搬并启动 M0，还是等内核第一次 `MCU_START` 才搬" —— 从无符号的 BL31
二进制里读不出来，要靠 Rockchip 的 TF-A 源码。**固件本身的逆向**（向量表 / 命令分发器 / IRQ2 信箱 /
状态码 / 伪 C）见 → [mcu-firmware-reverse-engineering.md](mcu-firmware-reverse-engineering.md)。
⇒ 串口开头那句 `DDR 9fa84341ce … fwver: v1.18` 与 `BL31 … fwver: v1.53` 是**要配套看的一对**；
我们只换 BL31（v1.47→v1.53）而 DDR bin 没动，实测能正常启动并正确打印 4 档 fsp_params，
但**将来换 SoC/大幅升 BL31 时要同时核 DDR bin 版本**，否则可能出现上面这些报错。

### 3.6 HDCP / OTP / 密钥（与"OTP 只读"铁律的关系）

BL31 里与密钥/OTP/HDCP 相关的字符串（**只有读，没有写**）：

```
rk_keylad_read_otp_key, rk_keylad_flush_otp_cache, rk_otp_init fail,
otp read failed!, otp read word failed!, scmi_otpc_rd, scmi_otpc_arb,
(adjust gpu/npu/cpul/cpub01/cpub23 pvtpll by otp: min=%uM, max=%uM, length=%u
dptx_load_hdcp_key, hdcp_decrypt_key, hdcp_storage_handler, rk_sip_hdcp_config_handler
```

- **OTP 在这里只被"读"**：① 读 PVTPLL 校准值（按 OTP 里的 freq/电压表调 PLL）；
  ② 读 HDCP key（DP 加密用）。`rk_keylad_flush_otp_cache` 是清缓存，不是写 eFuse。
- ⛔ **BL31 不是 OTP 写入路径**：`strings` 里没有任何 otp/efuse 写命令；写 OTP 的路径只有
  U-Boot 的 `rsa_burn_key_hash()`（要签名 FIT，我们没开）和 BL32 里厂商 TA 的
  `rk_write_oem_otp`（要显式调用），逐条排查见 [bl32-optee.md](bl32-optee.md) §9 与
  [otp-secure-boot.md](otp-secure-boot.md)。
- `SIP_ACCESS_REG`（0x82000002）能读写"安全寄存器"，`drivers/nvmem/rockchip-efuse.c`
  就是用它操作 **eFuse 控制器**（含写）。⚠️ 这是"经 EL3 写 OTP"的一条潜在通道，
  但它只在 **`rockchip,efuse` 节点存在**时才绑定；我们板子上是 `rockchip-otp`（NS 只读驱动），
  没有 `rockchip,efuse` 节点 ⇒ 不通。

### 3.7 异常处理（EHF）与死后现场

```
(BL31: Initialising Exception Handling Framework
exception reason=%u syndrome=0x%llx
Unhandled External Abort received on 0x%lx from %s
No EL3 exception handler for priority 0x%x
PANIC at PC : 0x
last_log_request
```

- 串口里那行 `BL31: Initialising Exception Handling Framework` 就是这个（EHF）。
- BL31 有自己的 **last log 环形缓冲**，normal world 可以用 `SIP_LAST_LOG`(0x8200000e) 取回；
  崩溃/掉电前 EL3 里发生了什么靠它。
- EL3 的 panic 会直接打在同一个串口上（`PANIC at PC : 0x…`）——**开机后串口只剩这些才是 EL3 的错**，
  内核 panic 是另一套格式。

---

## 4.~7. 复位/关机路径、怎么验、怎么换/升级、与其它组件的关系 → 见 [bl31-runtime-and-upgrade.md](bl31-runtime-and-upgrade.md)

"复位/关机/重启是谁做的"（反汇编实证）、BL31 的构建侧/运行侧验证、真实 banner 记录、
换/升级流程（含 eMMC 升级实录）与交叉链接**已搬成单独一篇** →
**[bl31-runtime-and-upgrade.md](bl31-runtime-and-upgrade.md)**。

