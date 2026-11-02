# `atf-3`（M0 固件）逆向

> 它在启动链里**是干什么用的**、和 `idbloader.img`/`u-boot.itb` 什么关系：见
> [bootloader-blobs.md](bootloader-blobs.md) §4.3；A 核那份 DDR 初始化固件的逆向见
> [ddr-firmware-reverse-engineering.md](ddr-firmware-reverse-engineering.md)。
> ⚠️ **本文不按"DDR 参数"来框**：它的官方身份是 **DDR 子系统那颗 MCU**（内核 SIP 头里
> `RK_DDR_MCU` / `RK_SIP_CFG_DDRMCU_0_ID`），但实测它的寄存器足迹远超"改 DDR 参数" ——
> 还写 **CRU**（时钟）、**`fd8d8000.power-management`**（电源域）、**VOP**（显示）QoS
> 与自己的 FIFO/IPC 块（见 §3 的寄存器清单）。

## TL;DR

- `atf-3` = **Cortex-M0 固件**（24,576 B）：向量表在偏移 0（SP `0x55d8`、复位 `0xc2`、
  默认异常 `0xc0` 是 `b .` 自旋、4 个 IRQ 入口）。
- 复位后进 **`0x2b1c` 命令分发器**：读 `0xfd58a21c` 的**高 8 位**当命令号，只实现 `0xF1`/`0xF2`，
  其它值就 `wfi` 深睡（睡前置 `SCB->SCR |= SLEEPDEEP`）。
- **IRQ2 是 A 核信箱**：`0xfecfc0d8` 的 bit2 表示有命令；处理时先 `NVIC->ICER = 4`（关 IRQ2），
  收尾清状态位并**回写状态码 `0xf30000xx`**（实测 `0xf3000020` / `0xf3000001`）。
- **谁加载它**：SPL 按 FIT 的 `load=` 把它放到 DRAM `0xf0000`（BL31 的只读数据段）→
  **BL31 再搬进 MCU 自己的 SRAM 并启动**（用 `SIP_MCU_CFG` 的子命令告知代码 / SRAM / cache 地址）；
  内核只是 `sip_smc_dram(0, 0, MCU_START)` 触发，**不带代码**（详见
  [bl31-mechanism.md](bl31-mechanism.md) §3.5）。
- 规模很小：5,418 条 Thumb 指令、**无乘除法/无浮点/无字符串**、56 个函数入口、
  最热的是"发命令+等完成"原语 `0x118`（29 处调用）。
- ⛔ 取权威副本：`objcopy --only-section=.ddr_m0_bin` 从 BL31 ELF 取（别照 FIT 的 `data-offset` 硬切，见 §1）。

## 1. 取权威副本（两个坑）

```bash
E=packages/u-boot-lubancat-rk3588/debian/rkbin/rk3588_bl31_v1.53.elf
aarch64-linux-gnu-objcopy -O binary --only-section=.ddr_m0_bin "$E" /tmp/ddr_m0.bin   # 24,576 B，权威
arm-none-eabi-objdump -D -b binary -m arm -M force-thumb /tmp/ddr_m0.bin               # 反汇编（Thumb）
```

⚠️ **坑 1**：FIT 的 `data-offset` 是**相对 external-data 区**的（本镜像基准 **+0x800**），不是文件绝对偏移
（实证：BL31 第一 LOAD 段在文件 `0x11B800`、M0 blob 在 `0x147800`）。
⚠️ **坑 2**：**不同构建的偏移会变**（带 bootstage 那版 atf-3 在 `0x148000`、10-02 版在 `0x147000`）——
照偏移硬切会整体错位（我实测偏了 4 KB、结论作废重做）。想切 FIT 必须先 `fdtdump -s` 读当前这份的偏移。

## 2. 镜像结构与向量表（先看它长什么样）

`atf-3`（`.ddr_m0_bin`，24,576 B，FIT load `0x000F0000`）是一份**正统 Cortex-M0 固件** ——
"运行期 DDR 由 M0 管"里的那个 M0 就是它（上电初始化/训练跑在 A 核，见 §3）。

（取法见 §1：`objcopy --only-section=.ddr_m0_bin`，别照 FIT 的 `data-offset` 硬切。）

⚠️ **两个坑（都实际踩过）**：
① FIT 的 `data-offset` 相对 external-data 区（本镜像 **+0x800**），照它直接切文件会偏 2 KB；
② **不同构建的 `data-offset` 会变**（带 bootstage 那版 atf-3 在 `0x148000`、10-02 版在 `0x147000`）
⇒ 要切 FIT 得先 `fdtdump -s` 读当前这份的偏移（更稳的是直接取 BL31 ELF 的那一段）。

**镜像结构**（偏移即 M0 的 SRAM 视图地址）：

| 偏移 | 内容 |
| --- | --- |
| `0x00` | **向量表**：`[0x00]=0x000055d8`（初始 SP；镜像 `0x6000` 长、栈顶 `0x55d8`）、`[0x04]=0x000000c3`（复位 ⇒ `0xc2`）、`[0x08]/[0x0c]/[0x28]…=0x000000c1`（默认异常 ⇒ `0xc0` = `b .` 自旋）、`[0x40]/[0x44]/[0x54]/[0x60]=0x2afd/0x2b05/0x2b0d/0x2b15`（4 个 IRQ 处理） |
| `0x0000`–`0x2E00` | Thumb 代码 + 数据（≈11.5 KB） |
| `0x2E00` | 一张小表（值多为 `0x2a5e`/`0x2a96` 一类，指向代码区内的数据） |
| `0x2E00`–`0x6000` | 基本全零 ⇒ **BSS / 栈**（代码里持有 `0x4f70`、`0x20002000` 这类缓冲指针） |

**控制流：它是个"命令分发器"**（复位 ⇒ `0x2b1c`）：

```
0x2b1c: push {r4,lr}
0x2b1e: ldr r3, [pc,#28]   ; = 0xfd58a21c（字面量池实证）
0x2b20: ldr r3, [r3]       ; 读该寄存器
0x2b22: lsrs r3, r3, #24   ; 取【高 8 位】= 命令号
0x2b24: cmp r3, #0xF1  →  bl 0x19d4
0x2b28: cmp r3, #0xF2  →  bl 0x1c14
0x2b2c: wfi ; b .          ; 其它 ⇒ 睡等中断
```

两条 IRQ 路径：`0x2b04 → 0x2918 → 0x2868`；`0x2b0c → 0x2920`（读 `0xfecfc0d8`、测 bit2、置位则进 `0x28c0`）。
两个命令实现都较长（`0x19d4` 先读一个 4-bit 状态字段再分支、`orrs #4` 置触发位、再写参数；`0x1c14` 先置同一触发位再取低 4 位参数）。

**它碰的寄存器**（从 PC 相对字面量池提取；次数 = 引用数）：

| 地址 | 次数 | 是什么 |
| --- | --- | --- |
| **`0xfd58a21c`** | 16 | **命令/状态寄存器**；同页 `fd58a208/a20c/a210/a214/a218` ⇒ 一整块 IPC 寄存器 |
| `0xfd7c08d4` / `0xfd7c0594` | 4 / 2 | **CRU**（`fd7c0000`，时钟）⇒ 变频时改时钟 |
| `0xfd59c000` / `0xfd59d000` / `0xfd59e010` / `0xfd59c130` / `0xfd59c134` | 3 | `fd59` 页（`bigcore0_grf` 一带） |
| `0xfd8d8110` / `0xfd8d8118` / `0xfd8d8120` / `0xfd8d8180` | 2 | **`fd8d8000.power-management`**（电源域） |
| `0xfdd90038` … `0xfdd90c00` … | 1~2 | **VOP（`fdd90000.vop`）** ⇒ 变频前后调显示 QoS |
| `0xfe0c0000/4/b8/e04`、`0xfec70008/c`、`0xfecfc058/c0d8`、`0xfe118000/4/10` | 1~3 | 待 TRM 命名（`fecfc0d8` 是 IRQ 里读的状态位） |
| `0x20002000` | 4 | Cortex-M 视图的**共享参数缓冲**（A 核侧同一块 SRAM = `0x000F2000`） |

**与 A 核的协议**（与 `kernel-6.1/include/linux/rockchip/rockchip_sip.h` 对得上）：

- 启动握手：`CONFIG_MCU_CODE_START_ADDR=0x01 … CONFIG_MCU_CACHE_END_ADDR=0x06`（BL31 告诉 M0 代码/SRAM/缓存地址），
  由 `ROCKCHIP_SIP_CONFIG_MCU_START` 放它跑；
- 共享页类型：`SHARE_PAGE_TYPE_DDR / DDRDBG / DDRECC / DDRFSP` ⇒ M0 还管 DDR 调试 / ECC / **FSP 频率档参数**查询
  ——§3 里那份 DDR 初始化固件导出的 `*_fsp_g` 就是给这条链用的；
- 运行期命令只有 `0xF1` / `0xF2` 两条 ⇒ 与内核两个动作 **`DRAM_SET_RATE`（变频）**、
  **`DRAM_SET_AT_SR`（进 self-refresh）** 一一对应（**强推断**：命令数、共享参数页、动 CRU/电源域/VOP-QoS
  的行为都吻合，但没有逐字节坐实）。

**边界**：寄存器语义（`fe0c`/`fec7`/`fe11` 页与 `fd58a2xx` 各偏移）需要 TRM；`0xF1`/`0xF2` 与 SIP 的对应未最终坐实；
共享页结构未解；没有逐函数反编译（M0 代码无符号、**无任何字符串**）。

## 3. 函数图（56 个入口）

入口来源：`push`/`push.w {…,lr}` 序言 25 个 + 镜像内 `bl` 目标 + 向量表目标（`radare2` 在这份裸 Thumb 上
边界不可靠，最终以 objdump 自己读的为准）。带 ★ = 逐条读过并确认。

| 入口 | 大小 | 被调 | 角色（实测依据） |
| --- | --- | --- | --- |
| `0x00c0` ★ | 2 B | — | 默认异常：`b .`（自旋） |
| `0x00c2` ★ | 10 B | 0 | **复位**：`push {r4,lr}; bl 0x2b1c; pop {r4,pc}` |
| `0x2afc/0x2b04/0x2b0c/0x2b14` ★ | 8 B 各 | 0 | 4 个 **IRQ 入口**（两个空转、两个转发到 `0x2918`/`0x2920`） |
| `0x2b1c` ★ | 36 B | 1 | **命令分发器**：读 `0xfd58a21c` 高 8 位 → `0xF1`/`0xF2` → 否则 `wfi` |
| `0x19d4` ★ | 576 B | 1 | **命令 `0xF1` 处理**（IPC 位段、`SCR\|=SLEEPDEEP`、`0xfd59e010`、`0xfd58a2xx`、回写状态） |
| `0x1c14` ★ | 48 B | 1 | **命令 `0xF2` 处理**开头（读 4-bit mode、`mode>4` 分支） |
| `0x1c44` | 646 B | 7 | `0xF2` 的工作函数（碰 `0x20002000` 共享缓冲、`0xfd800300`、`0xfe0c00b8`） |
| `0x2868` ★ | 88 B | 1 | **IPC 收尾**：`NVIC->ICER=4` → 清 `0xfecf805c` 位 → 查表 → **回写 `0xf3000020`** |
| `0x28c0` ★ | 88 B | 1 | 另一条 IPC 收尾（`0xfd58a21c`、`0xfecfc058`） |
| `0x2920` ★ | 24 B | 1 | IRQ 过滤：读 `0xfecfc0d8` 测 **bit2**，置位才进 `0x28c0` |
| **`0x0118`** ★ | 80 B | **29** | **通用原语**：写 `[r0+0x10080]/[r0+0x10084]` → 置 bit31 → 等 `[r0+0x10090]` bit0 清零 → 再等完成 |
| `0x0234` ★ | 24 B | 13 | 原语 2：`[r0+0x10c80]=1` → 等 `[r0+0x10c84]` bit0 == 1 |
| `0x024c` ★ | 72 B | 13 | 原语 3：**多阶段等待**（3~5 个状态位串行等；参数 `==15` 时走完整流程） |
| `0x0684`/`0x0698` | 20/10 B | 7/7 | 小工具（碰 `0xfec70008/0x0c`） |
| `0x2a2c` ★ | 12 B | 1 | **写全局**：`*(u32*)0x5154 = r0`（BSS 里的选择子） |
| `0x2a38` | 2 B | 7 | 空函数 |
| `0x0518`/`0x0570` | 88/80 B | 2/2 | 电源域操作（`fd8d8110/8118/8120`） |
| `0x05c0` | 164 B | 3 | **CRU**（`fd7c08d4`）+ **VOP**（`fdd9xxxx`）：变频前后的时钟/显示 QoS |
| `0x098a` | 2466 B | 8 | 最大工作函数之一（`fd59c130/134`、`fd59e010`、`fd7c0594`、`fd800300`） |
| `0x1336` | 1196 B | 3 | 大工作函数（`fd59d000`、`fe0c00b8`），内部多次调 `0x118`（`r0` 由它算） |
| `0x1f3c` | 1996 B | 7 | 大工作函数（`fd586300`、`fd58a21c`、`fd59xxxx`、`fdd90038`） |
| `0x029c`/`0x06a2`/`0x17e2`/`0x2b40`/`0x2c54` | 636/530/498/256/428 B | 少 | 其余工作函数（`0x20002000` 共享页、`fe0c0004`、`fd59c000`…） |

**已确认的架构细节**：`0xe000e180 = NVIC->ICER`（写 4 = 关 **IRQ2**）、`0xe000ed10 = SCB->SCR`
（写 4 = 置 **SLEEPDEEP**，配合分发器的 `wfi` 进深睡）⇒ 确认是 Cortex-M 核、用 IRQ2 做 A 核信箱。

## 4. 伪 C（核心部分，逐条读出来的）

```c
#define IPC_CMD    (*(volatile u32*)0xfd58a21c)  /* 高8位=命令号；写回 0xf30000xx = 完成状态 */
#define IPC_IRQ    (*(volatile u32*)0xfecf805c)
#define IPC_STATUS (*(volatile u32*)0xfecfc0d8)  /* IRQ2 里读它，bit2 = 有命令 */
#define SCR        (*(volatile u32*)0xe000ed10)  /* |4 = SLEEPDEEP */
#define NVIC_ICER  (*(volatile u32*)0xe000e180)  /* =4 关 IRQ2 */
#define GLOBAL_SEL (*(volatile u32*)0x00005154)
#define SHM_BUF    ((u8*)0x00004f70)

/* 通用原语（0x118，29 处调用）：写参数 → 置启动位 → 等硬件清掉 → 再等空闲 */
static int hw_cmd(u32 base, u32 sel, u32 hi, u32 lo) {
    while (*(volatile u32*)(base + 0x10090) & 1) ;
    *(volatile u32*)(base + 0x10080) = sel << 4;
    *(volatile u32*)(base + 0x10084) = (hi << 8) | (lo & 0xFF);
    *(volatile u32*)(base + 0x10080) |= 0x80000000u;
    while ((*(volatile int*)(base + 0x10080)) >= 0) ;
    while (*(volatile u32*)(base + 0x10090) & 1) ;
    return 0;
}
/* 典型调用形态：hw_cmd(r4, 3, 16 /*或 13*/, r5) */

static void hw_trigger(u32 base) {              /* 0x234，13 处调用 */
    *(volatile u32*)(base + 0x10c80) = 1;
    while (!(*(volatile u32*)(base + 0x10c84) & 1)) ;
}

void main(void) {                               /* 0x2b1c：命令分发 */
    u32 cmd = IPC_CMD >> 24;
    if (cmd == 0xF1)      cmd_f1();             /* 0x19d4 */
    else if (cmd == 0xF2) cmd_f2();             /* 0x1c14 */
    for (;;) __asm__("wfi");                    /* 睡等 IRQ2 */
}

void cmd_f1(void) {                             /* 0x19d4 / 主体 0x19de */
    u32 mode = (IPC_CMD >> 12) & 0xF;
    if (mode == 0)      GLOBAL_SEL = 0xfd890000;
    else if (mode == 1) GLOBAL_SEL = 0xfeb50000;                  /* 调试串口基址 */
    else                GLOBAL_SEL = 0xfd890000 & 0xffffae2f;
    SCR |= 4;                                                     /* SLEEPDEEP */
    *(volatile u32*)0xfd59e010 = 0x02000200;
    /* 随后用 0xfd58a208/a20c/a210/a214、SHM_BUF、0xfeb40000、0xfe000008、
       掩码 0xffffae2f、0x0000a140，最后回写状态 0xf3000001（后半段未逐条读完） */
}

void cmd_f2(void) {                             /* 0x1c14（48 B 全读完） */
    *(volatile u32*)REG_1f90 |= 4;               /* 清/关 IPC 中断 */
    u32 mode = *(volatile u32*)REG_1f94 & 0xF;
    *(volatile u32*)(TABLE_1f98 + 0x198) = mode;
    if (mode > 4) { /* 0x1c52 起按 mode 分支 → 0x1c44 一带的工作函数 */ }
}

void irq2(void) {                               /* 0x2920 → 0x28c0 */
    if (!(IPC_STATUS & 4)) return;
    ipc_finish_a();
}
void ipc_finish_b(void) {                       /* 0x2868 */
    NVIC_ICER = 4;                                        /* 关 IRQ2 */
    *(volatile u32*)0xfecf805c &= ~0x20u;                 /* 清状态位 */
    u32 n = SHM_BUF[0x198];
    u32 arg = ((n * 33 * 4 + n) << 4) + 0x00002e30;       /* 查表得参数 */
    helper_0698(arg);
    IPC_CMD = 0xf3000020;                                 /* 回写完成状态 */
    helper_0684();
}
```

## 5. 协议总结（从伪 C 反推出的规格）

1. **A 核 → M0**：命令写进 `0xfd58a21c`，**高 8 位是命令号**（只实现 `0xF1`/`0xF2`）；
   `0xF1` 用 bits[15:12] 当 mode，`0xF2` 用另一个寄存器低 4 位当 mode。
2. **M0 → A 核**：状态码 `0xf30000xx`（实测 `0xf3000020` = 收尾路径、`0xf3000001` = `0xF1` 路径）。
3. **同步**：IRQ2 通知（`0xfecfc0d8` bit2）；处理时先 `NVIC_ICER=4` 关 IRQ2，收尾清状态位 + 回写状态码；
   空闲置 `SLEEPDEEP` 再 `wfi` 深睡。
4. **共享数据**：`0x00004f70` 起的 BSS 缓冲、`0x00002e30` 的参数表、`0x20002000`（Cortex-M 视图）
   都在传参链上。

## 6. 边界（没做完的）

- M0 里 `[r0+0x10080]/+0x10084/+0x10090`、`+0x10c80/…` 这些**块基址 `r0` 是谁**：调用点都是
- 两个大工作函数（`0x098a` 2466 B、`0x1f3c` 1996 B、`0x1c44` 646 B）只到"碰哪些寄存器"这一层；
- `0xf30000xx` 各码位的含义要靠 A 核侧（BL31 / 内核 `rockchip_dmc.c`）对照。

（其余寄存器字段语义需要 TRM：本仓库 `hardware-docs/` 只有索引、没有 `rk3588/` 原件。）
