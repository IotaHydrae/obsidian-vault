# 两个裸启动产物：`idbloader.img` 与 `u-boot.itb` 里有什么

> 这两个文件是**一对**（同一次构建产出、一前一后交接），所以放在同一篇里讲：各自装什么、谁加载它、
> 落在介质哪个偏移、`idbloader.img` 里那份 DDR 初始化固件逆向出来是什么、以及"要换 / 要核 / 要刷"时的清单。
> 启动链全貌（BootROM → 内核）见 [rk3588-boot-flow.md](rk3588-boot-flow.md)；BL31（EL3）的机制与 SIP 接口
> 见 [bl31-mechanism.md](bl31-mechanism.md)；校验能证明什么、为什么不是防篡改见
> [boot-integrity.md](boot-integrity.md)。

## TL;DR

- `idbloader.img`（**305,152 B**，@32 KB / 扇区 64）= RKNS 头（2 KB）+ **DDR 初始化固件**（75,320 B）
  + **U-Boot SPL**；**BootROM** 读它。里面那份 DDR 固件是 **AArch64 代码**（跑在 PMU SRAM `0xff001000`），
  **不是 M0 代码**（M0 的固件在 `u-boot.itb` 的 `atf-3` 里，见 4.1）。
- `u-boot.itb`（**1,379,328 B**，@8 MB / 扇区 16384）= external-data **FIT**：`atf-1/2/3`（BL31 的三个段）
  + `uboot` + `fdt`，**每个节点带 sha256**；**SPL** 读它。
- `conf.firmware = "atf-1"` ⇒ SPL 的第一跳是运行在 EL3 的 ATF，之后才轮到 U-Boot proper
  ⇒ ⚠️ **`u-boot.itb` 里缺 ATF 会卡在"SPL 之后、U-Boot 之前"**。
- `atf-3`（24,576 B）= **DDR 子系统那颗 M0 的固件**：向量表在偏移 0，复位后进 `0x2b1c`
  命令分发器（读 `0xfd58a21c` 的**高 8 位**当命令号，只认 `0xF1`/`0xF2` 两条，其余 `wfi`）——
  运行期变频/自刷新就是它干的（§4.3）。
- 两条产物必须**成对更新**（同一构建）；核对用 `dumpimage -l` / `fdtdump -s` / `./tools/check-atf.sh`；
  写入偏移固定为 **32 KB** 与 **8 MB**（见 §5）。

## 1. 一览：谁装什么、谁读它、写在哪

| | `idbloader.img` | `u-boot.itb` |
| --- | --- | --- |
| 装什么 | RKNS 头 + DDR 初始化固件 + U-Boot SPL | FIT：`atf-1/2/3`（BL31 三段）+ `uboot` + `fdt` |
| 谁读它 | **BootROM**（SoC 固化，不可改） | **SPL**（在 idbloader 里） |
| 介质偏移 | **32 KB**（扇区 64） | **8 MB**（扇区 16384） |
| 大小（2026-10-03 的仓库版本） | 305,152 B（尾部零填充，容器大小固定） | 1,379,328 B（节点大小随构建变） |
| 打包来源 | `debian/rules`：`tools/mkimage -n rk3588 -T rksd -d <ddr>:<spl>` | U-Boot 自己的 make（`BL31=<bl31.elf> … u-boot.itb`） |
| 干什么 | 初始化 DDR 控制器（含训练）→ 跑 SPL → 读下一棒 | SPL 跳 ATF（EL3 常驻）→ ATF 交给 U-Boot proper → 加载内核 |

## 2. `idbloader.img`（RKNS 头 + DDR 初始化固件 + SPL）

实测结构（`prebuilt/idbloader.img`，共 305,152 字节）：

| 偏移 | 大小 | 内容 |
| --- | --- | --- |
| `0x0000` – `0x0800` | 2,048 | **RKNS 头**（`header0_info_v2`，见下） |
| `0x0800` – `0x12E38` | 75,320 | **DDR 初始化固件**，与 `rkbin/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin` 逐字节一致 |
| `0x12E38` – 末尾 | 227,784 | **U-Boot SPL**（`spl/u-boot-spl.bin`） |

RKNS 头即 `struct header0_info_v2`（`u-boot/tools/rkcommon.c`），实测字段：

```
magic            = "RKNS"        (RK_MAGIC_V2)
size_and_nimage  = 0x00020180    → [31:16]=2 个镜像；[15:0]=0x180 → hash 字段位于 0x180×4 = 0x600
boot_flag        = 0x00000001    → [3:0]=1，hash 类型 = SHA256
```

生成命令（`debian/rules`）：

```bash
tools/mkimage -n rk3588 -T rksd -d \
  debian/rkbin/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin:debian/build/<platform>/spl/u-boot-spl.bin \
  debian/build/<platform>/idbloader.img
```

`-T rksd` 会自行生成 RKNS 头并把 DDR 固件与 SPL 拼接起来。**注意：`debian/rkbin/` 里的
DDR 固件文件本身以 ARM64 代码开头（`01 00 00 14 ...`），并不含 RKNS 头**，头部是 mkimage 加的。

该阶段的职责：
- DDR 固件：初始化 DRAM 控制器（频率/时序由固件版本决定，本仓库为 `v1.18`）
- SPL：初始化必要外设（MMC/NAND 等），从介质读取 `u-boot.itb`（FIT）到 DRAM，校验后跳转

⚠️ 该文件**尾部有零填充**（实测末尾 512 B 全是 0）⇒ 容器大小恒为 **305,152 B**，里面的 SPL 大小可以变。
它与 `u-boot.itb` 是**同一次构建的一对**：打包来源、落点、刷写偏移与核对清单见本文 §5。


## 3. `idbloader.img` 里那份 DDR 初始化固件（做什么）

它是**跑在 A 核上的 AArch64 DDR 初始化/训练固件**（被 BootROM 载入 PMU SRAM `0xff001000` 执行；
自报 `ddr-v1.18-9fa84341ce`，支持 LPDDR2/3/4/4X/5，末尾导出 `sdram_params_g` / `*_fsp_g` 等符号与参数目录）。
⚠️ **它不是 M0 代码** —— M0 的固件在 `u-boot.itb` 的 `atf-3` 里（见 §4.3）。

**它的逆向（结构/入口/寄存器页/符号表）单独成篇** → [ddr-firmware-reverse-engineering.md](ddr-firmware-reverse-engineering.md) §2。

## 4. `u-boot.itb`（FIT：ATF + U-Boot + DTB）

实测结构（`prebuilt/u-boot.itb`，**2026-10-03 复核**，共 **1,379,328 B** = `0x150C00`），
FIT 描述为 `FIT Image with ATF/OP-TEE/U-Boot/MCU`：

| 节点 | load 地址 | FIT `data-offset`（文件内 = 该值 + **0x800**） | data-size | 说明 |
| --- | --- | --- | --- | --- |
| `uboot` | `0x00200000` | `0x00000000` | 1,158,832 | U-Boot proper |
| `atf-1` | `0x00040000` | `0x0011B000` | 143,216 | ATF BL31 主段（与 ELF 第一个 `PT_LOAD` 同大小） |
| `atf-2` | `0xFF100000` | `0x0013E000` | 36,864 | ATF BL31 的 SRAM 段 |
| `atf-3` | `0x000F0000` | `0x00147000` | 24,576 | DDR M0 固件段 |
| `fdt` | — | `0x0014D000` | 13,304 | U-Boot 控制 DTB（不是内核 dtb） |

⚠️ **`data-offset` 是相对 external-data 区**（本镜像基准 **+0x800**），不是文件绝对偏移 ——
实证：BL31 第一个 LOAD 段落在文件 `0x11B800`（= `0x11B000`+0x800）、M0 blob 落在 `0x147800`。
按上表 +0x800 换算，各 payload 在文件里是**连续**的：`uboot` 0x800、`atf-1` 0x11B800、`atf-2` 0x13E800、
`atf-3` 0x147800、`fdt` 0x14D800，最后结束于 `0x150BF8`（文件 `0x150C00`，尾部 8 B 填充）。
⚠️ **别拿"文件大小"当判据**：同一份源码换个构建就会差 KB 级（`uboot` 节点实测 10-02 版 1,158,832 B；
10-03 那个临时带调试选项的构建 1,162,832 B、整文件 1,383,424 B）⇒ 要比就比节点与哈希。

**怎么自己看（三条命令）**：

```bash
dumpimage -l prebuilt/u-boot.itb    # 节点名/大小/load/哈希（人眼友好）
fdtdump -s prebuilt/u-boot.itb      # 结构块：data-offset / data-size / load / conf 的 loadables
./tools/check-atf.sh                # 逐段 sha256 + ATF 自洽（回归口径，应 rc=0）
```

配置节点：

```
configurations {
    default = "conf";
    conf {
        description = "rk3588-lubancat-5";
        firmware  = "atf-1";                        // SPL 首先跳转 ATF
        loadables = "uboot", "atf-2", "atf-3";      // 由 firmware 负责后续加载
        fdt       = "fdt";
    };
};
```

**读法**：`firmware = "atf-1"` 表示 SPL 的第一跳是 ATF；`loadables` 里的 `uboot` 由 ATF 接力。
⚠️ `conf` 里还有一个 `signature { algo = "sha256,rsa2048"; key-name-hint = "dev"; … }` 节点 ——
**那是 ITS 模板留下的，本工程没开 FIT 验签**（`CONFIG_FIT_SIGNATURE` 没启用、也没有 dev 密钥），
别把它当成"启动会验 RSA 签名"（真正起作用的是各节点的 sha256，机制见
[boot-integrity.md](boot-integrity.md)）。

FIT 由以下脚本生成（`debian/rules`）：

```bash
make O=debian/build/<platform> ARCH=arm BL31=... spl/u-boot-spl.bin u-boot.dtb u-boot.itb
# u-boot.itb 的 ITS 由 make_fit_atf.sh → fit_nodes.sh 生成
```

### 4.1 atf-1 / atf-2 / atf-3 分别是什么

**不是三种固件，而是同一个 BL31 ELF 的三个 `PT_LOAD` 段**。BL31 是 Rockchip 提供的二进制
`debian/rkbin/rk3588_bl31_v1.53.elf`（TF-A v2.3-942-g98eaeb2f3，`fwver: v1.53`）。
段由 ELF 自己定义，`decode_bl31.py` 按 `p_paddr` 拆成 `bl31_0x%08x.bin`：

```bash
# （下面是 readelf -lW 的三个 LOAD 段，行尾 # 注解与右侧 section 是对照 readelf 的
#   "Section to Segment mapping" 加的；原输出里还有 Align 列与 GNU_STACK 段）
# 2026-09-18 按 **v1.53** 重测（v1.47 那份已随升级删除；旧数字是 0x3203c/0x7e000）
$ readelf -lW packages/u-boot-lubancat-rk3588/debian/rkbin/rk3588_bl31_v1.53.elf
Entry point 0x40000
  Type   Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg
  LOAD   0x010000 0x0000000000040000 0x0000000000040000 0x022f70 0x06f000 RWE  # ro .data stacks .bss xlat_table
  LOAD   0x040000 0x00000000000f0000 0x00000000000f0000 0x006000 0x006000 R    # .ddr_m0_bin
  LOAD   0x050000 0x00000000ff100000 0x00000000ff100000 0x009000 0x009000 RWE  # .text_pmusram
```

| 节点 | load | FileSiz / MemSiz | 段内 section | 是什么 |
| --- | --- | --- | --- | --- |
| `atf-1` | `0x00040000` | 143,216 / 454,656 | `ro .data stacks .bss xlat_table` | **BL31 主体**：代码 + 只读数据 + 每核栈（8×2 KB）+ BSS + MMU 页表。`MemSiz` 比文件大（BSS/栈/页表要占内存）。ELF 入口点就是这个 `0x40000` |
| `atf-2` | `0xFF100000` | 36,864 / 36,864 | `.text_pmusram` | **必须跑在 SoC SRAM 里的那部分代码**（段名即 `pmu_sram`）：DRAM 进 self-refresh／断电时能执行的代码只能在 SRAM。内核 DT 的 `syssram@ff001000` 只到 `0xff0f0000`，**与此段不重叠** |
| `atf-3` | `0x000F0000` | 24,576 / 24,576 | `.ddr_m0_bin` | **DDR 子系统那颗 M0 核的固件镜像**（段名 `_m0_` + 内核 DDR 驱动里有 `ROCKCHIP_SIP_CONFIG_MCU_START`，见 3.3.2） |

三段都落在**前 2 MiB 之内**（`0x40000 + 0x6f000 = 0xAF000`、`0xf0000 + 0x6000 = 0xf6000` 都 `< 0x200000`），
这正是内核可用内存从 `0x200000` 起算的原因（对照 `boot-serial-log.md` 里的
`Adding bank: 0x00200000 - 0xf0000000`）。

⚠️ **编号规则**：`fit_nodes.sh` 是**按文件大小降序**（`ls -l bl31_0x*.bin | sort --key=5 -nr`）
依次命名 `atf-1/2/3` 的 —— 所以顺序是「主段 143 KB → SRAM 段 36 KB → M0 段 24 KB」，
**与地址高低无关**；只有 `atf-1` 允许压缩，且 `firmware` 永远指向它。

📄 **BL31 的完整机制**（SIP 接口全表、深睡/PMU-SRAM、复位与关机的反汇编、内存地图、
串口日志逐行读法、怎么验/怎么换、10 条坑）单独成篇：**[bl31-mechanism.md](bl31-mechanism.md)**。


### 4.2 BL31（`atf-1`）为什么必须常驻

BL31 是 **EL3 的运行时固件**：启动期初始化 EL3 异常向量与 GICv3，`Preparing for EL3 exit to normal world`
之后**一直留在 EL3**，向上提供两大类服务 —— **PSCI**（CPU 上下电/热插拔、idle/suspend、**reboot/poweroff**）
与 **Rockchip SIP**（**DDR 变频**、拉起 DDR M0、VPU reset 等）。所以 `atf-1` 缺了不只是"少一段固件"，
而是 EL3 这一层没人跑（表现就是卡在 SPL 与 U-Boot 之间）。

完整机制（SIP 接口全表、深睡与 PMU-SRAM、复位/关机反汇编、低 2 MiB 内存地图、串口 banner 逐行读法、
10 条坑）见 **[bl31-mechanism.md](bl31-mechanism.md)**；升级与现场替换见
[bl31-runtime-and-upgrade.md](bl31-runtime-and-upgrade.md)。

### 4.3 `atf-3`（DDR M0 固件，做什么）

`atf-3`（`.ddr_m0_bin`，24,576 B）是 **DDR 子系统那颗 M0 的固件**：上电初始化/训练由 A 核那份做（§3），
**运行期变频/self-refresh 由它做** —— SPL 把它按 FIT `load=` 放到 DRAM `0xf0000`，**BL31 再搬进 MCU 的 SRAM 并启动**（内核只是通过 SMC 触发，不带代码，见 [bl31-mechanism.md](bl31-mechanism.md) §3.5）。
它内部是个命令分发器（A 核把命令写进 IPC 寄存器、M0 处理完回写状态码）。

**它的逆向（向量表/命令分发器/函数图/伪 C/协议）单独成篇** → [mcu-firmware-reverse-engineering.md](mcu-firmware-reverse-engineering.md)。
⚠️ 别把它当成"只改 DDR 参数"的固件：实测它还写 CRU（时钟）、`fd8d8000.power-management`（电源域）
与 VOP（显示）QoS 寄存器。

## 5. 维护与核对：这两块产物从哪来、写哪去

`u-boot.itb` 不是孤立的：它与 `idbloader.img`（结构见
[rk3588-boot-flow.md](rk3588-boot-flow.md) §3.2）是**同一次构建的一对**，必须一起换。

| 项 | `idbloader.img` | `u-boot.itb` |
| --- | --- | --- |
| 装什么 | RKNS 头 + DDR 初始化固件 + U-Boot SPL | FIT：`atf-1/2/3`（BL31 三段）+ `uboot` + `fdt` |
| 谁读它 | **BootROM**（固化） | **SPL** |
| 落在介质 | @32 KB（扇区 64） | @8 MB（扇区 16384） |
| 大小（2026-10-03 的仓库版本） | 305,152 B（**尾部零填充，容器大小固定**；里面的 SPL 大小可以变） | 1,379,328 B（节点大小随构建变） |
| 打包来源 | `debian/rules`：`tools/mkimage -n rk3588 -T rksd -d <ddr>:<spl>` | U-Boot 自己的 make（`BL31=<bl31.elf> … u-boot.itb`，ITS 由 `make_fit_atf.sh`/`fit_nodes.sh` 生成） |

**为什么必须成对**：两者有交接约定（FIT `conf` 的 `firmware = "atf-1"`、各段 load 地址、BL31 版本与
地址布局）；只换一个就可能卡在"**SPL 之后、U-Boot 之前**"——`u-boot.itb` 里缺 ATF 是同一个症状
（排查见 [atf-missing-troubleshooting.md](atf-missing-troubleshooting.md)）。换 BL31 还要核对
DDR/固件兼容矩阵（v1.54 起 BL31 主体段从 `0x40000` 挪到 `0x60000`）。

**这两块产物在本工程里有三处落点**（改一处就要同步另两处的口径）：

1. 内核旁的 **u-boot deb**：`/usr/lib/u-boot/{idbloader.img,u-boot.itb,rkspi_loader.img}`
   （`rkspi_loader.img` 是同一个 idbloader 的 **SPI NOR 变体**：它是个带分区表的小镜像，
   `idbloader` @扇区 64、`u-boot.itb` @扇区 16384；从 SPI 启动才用它，见 `notes/build/flashing.md`）；
2. 仓库 **`prebuilt/`**：镜像 `image` 阶段的兜底覆盖源（镜像里缺 ATF 时用它盖头部 32 KB/8 MB 两处）；
3. **已烧录的板子**：裸区那两处 —— 增量刷写要显式带 `tools/update-emmc.sh --bootloader`，
   否则它只同步 p1/p2、不碰裸区。

**核对与刷写的惯用做法**（2026-10-03 实刷一次，全程有效）：

```bash
./tools/check-atf.sh                                   # 先核：FIT 五段哈希 + ATF 三节点自洽，应 rc=0
dumpimage -l prebuilt/u-boot.itb | head                # 再核：节点与版本串对不对得上

# 刷前先确认偏移里放的确实是这两块（结构判据，别只看 sha256：不同构建的哈希本来就不同）
sudo dd if=/dev/mmcblk0 of=/tmp/cur-itb bs=512 skip=16384 count=2702 status=none
strings /tmp/cur-itb | grep -m1 'U-Boot 2017'          # 版本串 ⇒ 8 MB 处确实是 u-boot.itb
sudo dd if=/dev/mmcblk0 of=/home/board/uboot-backup-<日期>/region-32k-16m.bin \
        bs=512 skip=64 count=32704 conv=fsync          # 回滚备份：32 KB ~ 16 MiB 整段（16 MB）

sudo dd if=idbloader.img of=/dev/mmcblk0 bs=512 seek=64    count=596  conv=fsync
sudo dd if=u-boot.itb   of=/dev/mmcblk0 bs=512 seek=16384 count=2702 conv=fsync
sudo dd if=/dev/mmcblk0 of=/tmp/rb-itb bs=512 skip=16384 count=2702 status=none
cmp /tmp/rb-itb u-boot.itb                              # 读回比对必须一致再重启
```

⛔ 别跳过"读回比对"和"整段备份"这两步；刷的是裸区，写错位置没有文件系统兜着。
（`prebuilt/` 换新后，按 `AGENTS.md` §6.3 同步那行 sha256，并重跑 `check-atf.sh`。）

## 6. 相关文档

- **两份固件的逆向**：A 核那份 DDR 初始化固件 → [ddr-firmware-reverse-engineering.md](ddr-firmware-reverse-engineering.md)；
  `atf-3` 那份 M0 固件 → [mcu-firmware-reverse-engineering.md](mcu-firmware-reverse-engineering.md)
- 启动链全貌（BootROM → SPL → ATF → U-Boot → 内核 → systemd）：[rk3588-boot-flow.md](rk3588-boot-flow.md)
- BL31（EL3）机制、SIP 接口全表、DDR 变频与 M0 的接口：[bl31-mechanism.md](bl31-mechanism.md)
- 校验证机制（FIT 里的 sha256 能证明什么、为什么不是防篡改）：[boot-integrity.md](boot-integrity.md)
- 缺 ATF / 卡在 SPL 与 U-Boot 之间时的排查：[atf-missing-troubleshooting.md](atf-missing-troubleshooting.md)
- 整盘/增量烧录（含 SPI 变体 `rkspi_loader.img` 与 `update-emmc.sh --bootloader`）：`notes/build/flashing.md`
