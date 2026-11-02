# RK3588 的 OTP（eFuse）：结构、字段地图、程序里怎么读

⛔ **铁律：只读，不写。** OTP 是 eFuse，**烧写不可逆**（只能把 bit 从 0 变 1），
且 `0x20` 那个 Secure Boot 标志一旦写成 `0xff` 就再也回不去。本文档记录的全是**读取**路径；
涉及写的地方只说明"代码在哪、为什么危险"，**不要执行**。

相关：Secure Boot 在整条启动校验链里的位置见 [`boot-integrity.md`](boot-integrity.md)；
`Verified-boot: 0` 那行的来龙去脉也在那里。

## TL;DR

- ⛔ **铁律：只读，不写** —— OTP 是 eFuse，只能把 bit 从 0 变 1；`0x20` 的 Secure Boot 标志
  一旦写成 `0xff` **再也回不去**。
- 两个 OTP 控制器（NS / S 地址空间分开），字段地图见 §2，板上读数见 §3（只读）。
- 程序里怎么读（SPL / BL31 / Linux 三条路径）见 §4；写入为什么危险有证据（§5）。
- ⛔ **BootROM 不能反汇编**（我们手里没有它的代码，§6）。
- 附录 §7 是本文引用的关键代码（原样摘录）。

## 1. 两个 OTP 控制器（地址空间是分开的）

| | 非安全 OTP（NS） | 安全 OTP（S） |
| --- | --- | --- |
| 地址 / 大小 | `otp@fecc0000`，`0x400` | `secure-otp@fe3a0000`，`0x4000` |
| 谁能访问 | 内核、U-Boot 都能（普通外设） | **只有 SPL / BL31（EL3）**；Linux 侧要经 OP-TEE |
| 绑定 | 内核 DT `rk3588s.dtsi:7548`（`rockchip,rk3588-otp`）| U-Boot DT `u-boot/arch/arm/dts/rk3588-u-boot.dtsi:20`（`rockchip,rk3588-secure-otp`，`u-boot,dm-spl`）|
| 驱动 | U-Boot `drivers/misc/rockchip-otp.c`；内核 `drivers/nvmem/rockchip-otp.c` | U-Boot `drivers/misc/rk3588-secure-otp.S`（+ `CONFIG_SPL_ROCKCHIP_SECURE_OTP`）；内核 `drivers/nvmem/rockchip-secure-otp.c`（走 **OP-TEE**） |
| 里面放什么 | 芯片 ID / CPU 版本 / 漏电与 OPP 参数 / 客户字段 | **Secure Boot 标志、RSA 公钥哈希、防回滚计数** |

> **RK3588 的内核 DT 里没有 `secure-otp` 节点**（只有 `rk3528.dtsi:505` 有 `rockchip,secure-otp`），
> 所以本镜像在 Linux 里**读不到** secure OTP —— 能读它的只有 SPL 与 BL31。就算补上节点，那个内核
> 驱动（`drivers/nvmem/rockchip-secure-otp.c`）也是经 **OP-TEE**（`optee_ctx_match()`）访问的，
> 而本镜像**没有 OP-TEE**。U-Boot 侧 `ROCKCHIP_SECURE_OTP` 的 Kconfig 同样写明
> `depends on MISC && !OPTEE_CLIENT`。

⚠️ **两个"0x400"别混**：

- `otp@fecc0000` 的 `reg = <0x0 0xfecc0000 0x0 0x400>` 是**控制器寄存器窗口**
  （`OTPC_AUTO_CTRL` / `OTPC_AUTO_EN` / `RD_DONE` / `OTPC_DOUT0` 这些寄存器就在里面）——**不是熔丝内容**；
- `/sys/bus/nvmem/devices/rockchip-otp0/nvmem` 那 1024 B 才是**熔丝内容视图**，而且它**不从 OTP
  地址 0 开始**：`rk3588_otp_read()` 先算字地址 `addr_start = rounddown(offset,4)/4`，再
  `addr_start += otp->data->ns_offset`，而 `RK3588_NO_SECURE_OFFSET = 0x300`
  ⇒ **视图偏移 0 = OTP 字地址 0x300**，整个视图覆盖 OTP 字地址 **0x300–0x3FF**（256 字 × 4 B）。
  （`ns_offset` 每个 SoC 不同：PX30S=224、RK3576=0x1C0、RK3588=0x300。）
  DT 里 `cpu-code@2` 的 `2` 是**相对这个视图**的字节偏移。

## 2. 字段地图

### 2.1 非安全 OTP（RK3588 的 nvmem cells，来自内核 DT）

| 偏移/长度 | cell 名 | 用途 |
| --- | --- | --- |
| `0x02` / 2 B | `cpu-code` | SoC 型号码（本板读到 `35 88`，即 RK3588 的编码） |
| `0x05` / 1 bit | `package-serial-number-high` | 封装批号（高位） |
| `0x06` / 3 bit | `package-serial-number-low` | 封装批号（低位） |
| `0x06` / 5 bit | `specification-serial-number` | 规格序列号 |
| `0x07` / 16 B | `id` | **芯片唯一 ID**（ASCII 串，实测见 §3） |
| `0x17`–`0x1b`、`0x28`、`0x29` | `cpub0/cpub1/cpul/log/gpu/npu/codec-leakage` | 各电源域漏电等级（内核用来选 OPP/电压） |
| `0x1c` / 3 bit | `cpu-version` | CPU 版本 |
| `0x22` / 4 bit | `customer-demand` | 客户自定义 |
| `0x3d`–`0x60`，每个 6 B | `cpul/cpub01/cpub23/gpu/npu/dmc-opp-info` | OPP 表（频率/电压）微调参数 |

寻址注意：内核驱动读的时候会先加一个 **`ns_offset = 0x300`**
（`rk3588_otp_read()` 里 `addr_start += otp->data->ns_offset`，`RK3588_NO_SECURE_OFFSET = 0x300`），
所以 nvmem 里的偏移 `0x00` 对应 OTP 全局地址 `0x300`。**dvfs 那套"每颗芯片不同"的参数就出自这里。**

### 2.2 安全 OTP（Secure Boot 相关，定义在 U-Boot）

`u-boot/include/configs/rk3588_common.h:34-40`：

| 偏移 | 长度 | 宏 | 含义 |
| --- | --- | --- | --- |
| `0x20` | 1 B | `OTP_SECURE_BOOT_ENABLE_ADDR` | **Secure Boot 使能标志**：`0xff` = 已使能（读到别的值 = 未使能） |
| `0x9c0` | 32 B | `OTP_RSA_HASH_ADDR` | **RSA 公钥的 SHA-256**（BootROM/SPL 用它确认签名用的公钥是不是烧进去的那把） |
| `0x150` | 2 word = 64 bit | `OTP_UBOOT_ROLLBACK_OFFSET` | **防回滚计数**：一位代表一个版本，只会往上烧，`fls()` 取"最高已烧位的位置"当版本号 |
| —— | —— | `OTP_ALL_ONES_NUM_BITS = 32` | 一个 word 全 1 = 这个 word 的 32 个版本位用满了 |

### 2.3 哪些是出厂烧的、哪些是后来烧的（**别把 OTP 当成"出厂固化、永远不动"**）

OTP = One-Time Programmable：**已经烧过的位不能再改回去**（只能 0→1）——这一半是对的；
但"内容全是出厂写好的"**不对**：

| 什么时候烧 | 烧什么 | 证据 |
| --- | --- | --- |
| 出厂 / 封测 | NS 区的"芯片固有信息"：`cpu-code`、`id`、`cpu-version`、各域漏电、OPP 参数、批号/规格号 | 板上实测这些值已经在了（§3），且 `rockchip-cpuinfo` 开机就打印 `SoC : 35881000` |
| **产线 / 客户后烧** | **secure OTP 的 Secure Boot 字段**：`0x9c0` 公钥哈希（32 B）→ 再 `0x20 = 0xff` 使能 | `u-boot/lib/rsa/rsa-verify.c:594 rsa_burn_key_hash()`：先回读 `0x9c0`，**非零就报 `RSA: The secure region has been written.` 拒绝重写**；然后 `misc_otp_write()` 写哈希、写使能位 |
| **使用过程中持续烧** | `0x150` 防回滚计数：每次固件版本前进就多烧一位，**只增不减** | `arch/arm/mach-rockchip/spl.c:436/470`（`CONFIG_SPL_FIT_ROLLBACK_PROTECT`；本构建没开） |
| 留给整机厂/客户 | NS 区 `customer-demand@22` 等客户字段 | 内核 DT cell |

⇒ 准确说法是：**OTP 不能"修改"，但可以"继续烧"**；"出厂内容"只是其中一部分。
Linux 侧对 NS OTP 是**只读**的（`drivers/nvmem/rockchip-otp.c` 的 `otp_config.read_only = true`，
而且 `rk3588_data` 里**没有 `.reg_write`**），能写的是 U-Boot/SPL（`misc_otp_write()`）与产线工具。

## 3. 板上实测（开发板，**只读**）

```bash
$ ls -l /sys/bus/nvmem/devices/*/nvmem
-r--r--r-- 1 root root 1024 Sep 16 15:58 /sys/bus/nvmem/devices/rockchip-otp0/nvmem   # 全局可读
$ head -c 32 /sys/bus/nvmem/devices/rockchip-otp0/nvmem | od -An -tx1 -v
 52 4b 35 88 12 fe 21 4e 59 38 34 50 00 00 00 00
 00 00 00 00 05 06 08 08 08 09 21 0d 08 00 00 00
```

按 §2.1 的 cell 解释（偏移都是上面这份 dump 的字节下标）：

| 偏移 | 值 | 解释 |
| --- | --- | --- |
| `0x00` | `52 4b` | `"RK"`（固定前缀，不属任何 cell） |
| `0x02` | `35 88` | `cpu-code`（模型码） |
| `0x05`/`0x06` | `fe` → 批号高位=2；`21` → 低 3 位=1、低 5 位=1 | 封装批号 + 规格序列号 |
| `0x07`（16 B） | `4e 59 38 34 50 "NY84P"` + 11 个 0 | 芯片唯一 ID（**每颗不同**，对外可读，不是密钥） |
| `0x17`–`0x1b` | `08 08 08 09 21` | cpub0/cpub1/cpul/log/gpu 漏电等级 |
| `0x1c` | `0x08 >> 3 & 0x7 = 1` | `cpu-version` |

（`/proc/iomem` 里能看到 `fecc0000.otp` 与两个 `sram` 窗口，**没有 ROM 区** —— 见 §6。）

`rockchip-cpuinfo` 开机读的就是这些 cell（`drivers/soc/rockchip/rockchip-cpuinfo.c`），板上实测：

```
[    5.193333] rockchip-cpuinfo cpuinfo: SoC		: 35881000
[    5.193339] rockchip-cpuinfo cpuinfo: Serial		: EXAMPLE_ID_04
```

- `cpu-code` 按**大端两字节**解释成数值（`efuse_buf[0] << 8 | efuse_buf[1]`）：`35 88` → `0x3588`
  ⇒ `SoC` 打成 `35881000`（低 16 位是 SoC 码）——印证了 §2.1 里 `0x3588` 的解释；
- `Serial` **不是**那 16 字节原样：驱动先对 `id` 做**字节两两交换**
  （`buf[i] = id[1+(i<<1)]`、`buf[i+8] = id[i<<1]`），再取 `crc32()` 拼成高低 32 位
  ⇒ 想要原始 16 字节就直接读 nvmem。

## 4. 程序里怎么读

### 4.1 U-Boot / SPL（Secure Boot 字段只有这条路）

```
misc_otp_get_device(OTP_S)          # include/misc.h:25 有 OTP_S / OTP_NS 能力位；drivers/misc/misc_otp.c
        └── misc_otp_read(dev, offset, buf, size)
                └── 驱动读到哪个看 capability：
                    · OTP_NS → drivers/misc/rockchip-otp.c   （普通 OTPC 寄存器 + ECC/SBPI 命令）
                    · OTP_S  → drivers/misc/rk3588-secure-otp.S（CONFIG_SPL_ROCKCHIP_SECURE_OTP）
```

⚠️ `rk3588-secure-otp.S` 是**签入仓库的"生成汇编"**：文件头 `.file "drivers/misc/rk3588-secure-otp.c"`，
里面是 `secure_otp_ioctl` / `secure_otp_read` / `secure_otp_write` 三个函数，配 `secure_conf` /
`mask_addr` / `cru_rst_addr` 这些属性 —— **对应的 `.c` 源码不在树里**（Rockchip 的老做法，
`px30/rk3308/rk3568/...-secure-otp.S` 一整排都这样）。所以想弄清寄存器级细节只能读这段汇编。

谁在用它读什么：

| 调用点 | 读哪个字段 | 干什么 |
| --- | --- | --- |
| `arch/arm/mach-rockchip/fit_misc.c:218` | `0x20`（1 B） | 就是串口日志 `## Verified-boot: N` 的来源；非 0 时 FIT 签名校验变成**必需** |
| `arch/arm/mach-rockchip/spl.c:436` / `:470` | `0x150`（2 word） | 防回滚（`CONFIG_SPL_FIT_ROLLBACK_PROTECT`；**本构建没开**） |
| `lib/rsa/rsa-verify.c:612` | `0x20` | **烧写例程** `rsa_burn_key_hash()`（`:594`）的第一步：已经是 `0xff` 就直接返回，"别重复烧" |
| `lib/rsa/rsa-verify.c:695` | `0x9c0`（32 B） | 同上例程：回读公钥哈希核对写成功没有 |

⚠️ **`rsa_burn_key_hash()` 里 `:705` 会 `misc_otp_write(dev, 0x20, 0xff, 1)` —— 这就是"烧
Secure Boot 使能位"，不可逆。本文不执行、也不建议在没有完整方案（签名工具链 + 私钥 + 可回滚的
验证流程）前执行。**

另外：**本构建的 U-Boot 没有 `misc` 命令**（`.config` 里 `# CONFIG_CMD_MISC is not set`），
所以 U-Boot 控制台里没有现成的 "读 OTP" 命令，只能看代码里的调用。

### 4.2 Linux（只能读非安全 OTP）

```
/sys/bus/nvmem/devices/rockchip-otp0/nvmem     # 1024 B，全局可读（实测非 root 也能读）
        └── drivers/nvmem/rockchip-otp.c : rk3588_otp_read()
                · 每次 4 字节一个"字"（RK3588_NBYTES = 4）
                · 写 OTPC_AUTO_CTRL = (地址 << 16) | (burst 1 << 8)
                · 写 OTPC_AUTO_EN 触发 → 轮询 RD_DONE → 读 OTPC_DOUT0
                · 先 clk_bulk_prepare_enable()（otpc/apb/phy 三个时钟）
```

DT 里那些 cell（§2.1）是给内核驱动消费的：`rockchip-thermal`/`nvmem` 消费者、OPP 微调、
漏电等级、芯片 ID（`cat /sys/bus/nvmem/devices/rockchip-otp0/nvmem` 或经 cell 接口）。
**secure OTP 在 Linux 侧没有节点**（§1），所以 `0x20` / `0x9c0` / `0x150` 在系统里读不到。

## 5. 写入为什么危险（证据）

- secure OTP 驱动里有现成的字符串：`"The region has been written.\n"`、`"Write secure otp fail.\n"`
  ⇒ 它自己就会拒绝重复写；
- 防回滚计数器是**单调**的：`fit_write_otp_rollback_index()` 只在 `otp_index < fit_index` 时写，
  每次把新的位数烧上（`fls()` 取最高已烧位），**减不回去**；
- 烧写例程会先读 `0x20`，`0xff` 就早退（`rsa-verify.c:612`）—— 这既是"幂等"也是"没有回头路"的体现；
- `OTP_ALL_ONES_NUM_BITS = 32`：一个 word 烧满就是 32 个版本位耗尽。

## 6. BootROM 能不能反汇编？—— 不能（我们手里没有它的代码）

- RK3588 的 BootROM 是 **SoC 内部的掩膜 ROM**：不在 `rkbin/`、不在本仓库、Linux 也不映射它
  （板上 `/proc/iomem` 只有 `otp@fecc0000` 与两个 `sram` 窗口，没有 ROM 区），公开也没有可用的 dump。
  ⇒ **没有可反汇编的对象**，不是"有但没看"。
- 我们手里与 BootROM 有关的**只有接口**：`u-boot/arch/arm/mach-rockchip/bootrom.c` 的 BROM helper
  （`back_to_bootrom()` 传 `BROM_BOOT_NEXTSTAGE` / `BROM_BOOT_ENTER_DNL` 回 ROM 继续启动或进下载模式，
  外加 `BROM_BOOTSOURCE_*` 启动介质编号、`SAVE_SP_ADDR`）。它靠**固定地址/约定**去调 ROM，
  不是 ROM 的代码；`arch/arm/mach-rockchip/Makefile:7-11` 的注释还说明它"可能从 ATF 进来、
  此时栈指针指向不可访问/受保护的内存"。
- 能反汇编的是我们手上的 aarch64 二进制：`prebuilt/u-boot.itb` 的各段（**2026-09-20 重建后内含 v1.53**
  的 BL31；要看 v1.47 那版用 `git show <旧提交>:prebuilt/u-boot.itb`）、
  `rkbin/rk3588_bl31_v1.53.elf`（2026-09-18 从 v1.47 升级，见 [../build/upstream-sync.md](../build/upstream-sync.md)）、
  idbloader 里的 SPL。所以 `boot-integrity.md` 里"BootROM 会按 RKNS 头的 sha256 校验
  idbloader"那条，依据是**头里字段的语义 + Rockchip 的设计约定**（`boot_flag[3:0]` 的含义写在
  `u-boot/tools/rkcommon.c` 的注释里），**不是反汇编 BootROM 得到的结论**。

## 7. 附录：本文引用的关键代码 → 见 [otp-secure-boot-code.md](otp-secure-boot-code.md)

U-Boot 的 OTP 字段宏/读取入口、`Verified-boot: N` 的来源、防回滚计数、**烧写例程（⛔ 不要执行）**、
安全 OTP 驱动的形态、内核读非安全 OTP 的寄存器级代码、`cpu-code`/`id` 的解释、DT 写法
**已搬成代码存档篇** → **[otp-secure-boot-code.md](otp-secure-boot-code.md)**。

