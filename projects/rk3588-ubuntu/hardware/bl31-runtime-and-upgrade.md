# BL31 的运行行为、验证与升级

> 本文是 [bl31-mechanism.md](bl31-mechanism.md) 的**后半篇**：
> 复位/关机/重启到底是谁做的（反汇编 + 真机实证）、怎么验证产物里嵌的是哪颗 BL31（构建侧 + 运行侧）、
> 记录过的真实 banner，以及**怎么换/升级**（含 eMMC 升级实录）。
> BL31 在启动链里的位置、二进制解剖（ELF/节表/内存地图）、EL3 提供了哪些服务（PSCI/SIP/opteed）
> 见主篇 §1~§3。
> ⚠️ 正文**原样搬自主篇的 §4~§7**（节号保留）。

## TL;DR

- 复位/关机/重启最终由 **BL31（PSCI）**落地：`SYSTEM_OFF` 写 IOC/GPIO0、`SYSTEM_RESET` 走复位寄存器（§4）。
- 验证"产物里是哪颗 BL31"：构建侧看 `prebuilt/`/*.deb 嵌的段，运行侧看串口 banner（§5）。
- 换/升级 BL31 必须**成对更新** `prebuilt/{idbloader.img,u-boot.itb}` 并核对 DDR/版本兼容矩阵（§6、主篇 §3.5）。


## 4. 复位/关机/重启是谁做的（反汇编实证）

> 完整对比与内核侧链路见 [shutdown-reboot-path.md](shutdown-reboot-path.md) 与
> [rk3588-boot-flow.md](rk3588-boot-flow.md) §3.7（EL 总表），这里只讲 BL31 内部。

| 动作 | 内核侧 | BL31 函数 | BL31 干的活 |
| --- | --- | --- | --- |
| `reboot` | `psci_sys_reset`（优先级 **129**，压过 `dw_wdt` 的 128）→ SMC `SYSTEM_RESET` | `0x50d28`（打印 `(system reset......`） | 预置一批 PMU/CRU 寄存器 → 写 **`CRU+0xc08 = GLB_SRST_FST ← 0xfdb9`** → `dsb sy` → WFI 循环（不返回） |
| `poweroff` | `pm_power_off = psci_sys_poweroff` → SMC `SYSTEM_OFF` | `0x50de4`（打印 `(system poweroff......`） | 写 `IOC 0xfd5f0000 ← 0x0f000000`、`GPIO0 0xfd8a0000/0xfd8a0008 ← 0x00040004`（PMU 域引脚）→ WFI 循环 |
| 兜底 | SMC 若返回 → `dw_wdt`(128) 把 WDT 超时设到最小再踢一脚 | — | 只有 PSCI 失败才会走到 |

- `0xc08` = `RK3588_GLB_SRST_FST`（`u-boot/arch/arm/include/asm/arch-rockchip/cru_rk3588.h:118`），
  `0xfdb9` 与 U-Boot `drivers/sysreset/sysreset_rockchip.c:27` 的魔数**完全相同**
  ⇒ 重启是 **CRU 全局软复位**，不是 watchdog。
- 镜像里还有 `wdt_config`（`0x52e04`，写 `0xfd586000`，一个 DT 里都没有的 PMU 域 WDT），
  但**全镜像没有任何一处调用**（`bl 0x52e04` 命中 0）= 死代码；Linux 那个
  `watchdog@feaf0000`（`dw_wdt`，`/dev/watchdog0`）是**另一个块**。
**串口实测（2026-09-18，v2 板，用户抓的原始输出）**：

```console
$ sudo reboot
[ 3171.232690] watchdog: watchdog0: watchdog did not stop!
[ 3171.641613] reboot: Restarting system

$ sudo poweroff
[   30.676204] reboot: Power down
```

**怎么一眼区分 v1.47 / v1.53**（两块介质上各有一份 loader 时很有用）：

| 行 | v1.47（2024-09-04 版） | v1.53（`rkbin/` 里这颗，2025-08-25） |
| --- | --- | --- |
| `BL31: v2.3():…` | `v2.3-831-g92b1674a5` | `v2.3-942-g98eaeb2f3` |
| `BL31: Built :` | `16:18:39, Sep  4 2024` | `12:10:56, Aug 25 2025` |
| `code: 0x88` 的下一行 | 直接是 `ext 32k is not valid` | 多一行 `INFO: customer demand: 0x0` |

> ⚠️ **同一块板子上"哪份 loader 在跑"取决于从哪个介质启动**。开发板（`<HOST_07>`）的历史状态：
> 2026-09-18 之前 eMMC 是 **v1.47**、SD 卡是 **v1.53 + BL32**；**2026-09-18 已把 eMMC 的 `u-boot.itb`
> 换成 v1.53（BL31-only）**——只写 itb（BL31 在里面），**没动 idbloader（SPL + DDR v1.18）**，
> 详见 [bl31-version-diff.md](bl31-version-diff.md)。

逐行读法：

- ⚠️ **`watchdog0: watchdog did not stop!` 不是"重启由看门狗触发"**，而是
  `drivers/watchdog/watchdog_dev.c:946` 的 **release 路径**：关机时 systemd 按
  `RebootWatchdogSec=10min`（systemd 自带的默认值，`/etc/systemd/system.conf` 里是注释行）
  打开 `/dev/watchdog0` 当"关机卡死兜底"，随后**没有写 magic 关闭字符 `V`** 就关掉 fd；
  `dw_wdt` 声明了 `WDIOF_MAGICCLOSE`（`dw_wdt.c:344`）而 `nowayout=n`
  （defconfig 里没有 `CONFIG_WATCHDOG_NOWAYOUT`）⇒ 内核**拒绝停狗**，打这句 `pr_crit`，
  并**立刻补一次 keepalive ping**（就是为了别让狗在关机中途咬）。
  日志实证（板子 boot -3）：

  ```
  systemd[1]: Using hardware watchdog 'Synopsys DesignWare Watchdog', version 0, device /dev/watchdog0
  systemd[1]: Watchdog running with a timeout of 10min.
  kernel: watchdog: watchdog0: watchdog did not stop!
  systemd-shutdown[1]: Using hardware watchdog '…', device /dev/watchdog0
  systemd-shutdown[1]: Watchdog running with a timeout of 10min.
  ```

  顺带：这块板的狗**本来就是 running**——`sudo wdctl` 显示
  `Timeout: 44 seconds / Timeleft: 44 seconds`（U-Boot 留下的 `WDOG_HW_RUNNING`，
  内核 probe 时接手继续喂，`dw_wdt.c:662`）。所以"没有 systemd 看门狗"≠"狗没在跑"。
- `reboot: Restarting system` = `kernel_restart()` 打印；紧接着就是 PSCI `SYSTEM_RESET` →
  BL31 写 CRU `GLB_SRST_FST`（上表）⇒ **复位仍由 EL3 完成**。
- `reboot: Power down` = `kernel_power_off()` 打印；紧接着 PSCI `SYSTEM_OFF` → BL31 写 IOC/GPIO0。
- 实测确认：**`reboot` 之后除了上面两条内核日志，控制台没有任何其它输出，直接进入下一轮**
  （`DDR …` → SPL → BL31 banner）—— 也就是说 BL31 的 `(system reset......` **确实没打出来**。
  原因见下条（打完就复位，UART 被清），不要据此认为"EL3 没参与"。
- ⚠️ **BL31 自己那两行 `(system reset......` / `(system poweroff......` 很可能看不到**：
  打完马上就复位/断电，UART FIFO 未必发得完。**抓到这两行就是"EL3 真的执行了"的直接证据**；
  这两段输出里没有，所以该结论目前由反汇编 + ops 表支撑：
  `rk3588_bl31_v1.53.elf` 里 `0x59f30`/`0x59f38` 两个相邻指针槽就是
  `system_off`/`system_reset` 两个 wrapper（PSCI 层经这张平台 ops 表调用它们）。

- ⚠️ 两个"关机流程"的坑：
  1. `SYSTEM_OFF` 的 PMIC 侧写如果不生效，helper（`0x589a8`）的 WFI 循环最终会
     **跳回 `soc_system_reset`（`0x58a34` → `b 0x50d28`）** ⇒ 表现为"关机变成重启"。
  2. 板子上 RK806 是 **SPI**，内核驱动只把 `pmic-power-off`（`rk806_dvs1_pwrdn`）pinctrl
     **lookup 出来而没人用**，也没注册 `sys_off`/syscore 回调，DT 里也没有
     `rockchip,system-power-controller`；开机日志还打 `rk806 spi2.0: shutdown-sequence missing!`
     ⇒ **断电是 EL3 那条路完成的，内核 PMIC 驱动不参与**。

---

## 5. 怎么验 BL31（构建侧 + 运行侧）

### 5.1 构建侧（产物里到底嵌了哪颗 BL31）

1. **FIT 节点**：`u-boot.itb` 里应当有 `atf-1/atf-2/atf-3` 三个 `os = "arm-trusted-firmware"` 节点，
   `load=` 分别是 `0x40000 / 0xff100000 / 0xf0000`；哈希是**每个段**的 sha256。
2. **一条命令核对**（会解析 FIT/RKNS 内部 sha256 并现场重算）：
   ```bash
   tools/check-atf.sh <u-boot.itb 或 idbloader.img>
   ```
   （用法与判据见脚本头；它同时能查 SPL/DDR 固件那一半。）
3. ⚠️ **判新旧只看 `fwver:` 与 FIT 节点哈希，别看 SPL 横幅的构建日期**：增量编译不会重建
   `spl/u-boot-spl.bin`，日期会骗人（我们踩过）。

### 5.2 运行侧

| 手段 | 能拿到什么 | 备注 |
| --- | --- | --- |
| **串口**（115200） | `NOTICE: BL31: v2.3():v2.3-942-g98eaeb2f3:derrick.huang, fwver: v1.53` + 一整段启动诊断 | **最直接**；SPL/BL31 那段只有串口有 |
| 内核日志 | `psci: PSCIv1.1 detected` / `Using standard PSCI v0.2 function IDs` / `SMC Calling Convention v1.2` | 只能证明"PSCI 活着" |
| 行为探针 | `cpu7` offline/online（PSCI）、`reboot`（SYSTEM_RESET）、`poweroff`（SYSTEM_OFF） | 前两个安全；**`poweroff` 之后要人工上电**，别远程试 |
| SIP `0x82000001` | BL31 的 ATF 版本号 | ⚠️ 内核里 `sip_smc_get_atf_version()` **没有任何调用者**（无 sysfs/procfs/debugfs 出口）⇒ 想从 Linux 读得自己加个模块；**"BL31 版本只能看串口"这条结论要这么修正** |
| DT / `/dev/mem` | ❌ 都没有 | BL31 **没有设备树节点**；内核也读不到低端 DRAM |

### 5.3 我们记录过的真实 banner（v2 板，v1.53）

```
NOTICE:  BL31: v2.3():v2.3-942-g98eaeb2f3:derrick.huang, fwver: v1.53
NOTICE:  BL31: Built : 12:10:56, Aug 25 2025
INFO:    spec: 0x1
INFO:    code: 0x88
INFO:    ext 32k is not valid
INFO:    ddr: stride-en 4CH
INFO:    GICv3 without legacy support detected.
INFO:    ARM GICv3 driver initialized in EL3
INFO:    valid_cpu_msk=0xff bcore0_rst = 0x0, bcore1_rst = 0x0
INFO:    l3 cache partition cfg-0
INFO:    system boots from cpu-hwid-0
INFO:    bypass memory repair
INFO:    idle_st=0x21fff, pd_st=0x11fff9, repair_st=0xfff70001
INFO:    dfs DDR fsp_params[0].freq_mhz= 1848MHz
INFO:    dfs DDR fsp_params[1].freq_mhz= 528MHz
INFO:    dfs DDR fsp_params[2].freq_mhz= 1068MHz
INFO:    dfs DDR fsp_params[3].freq_mhz= 1560MHz
INFO:    BL31: Initialising Exception Handling Framework
INFO:    BL31: Initializing runtime services
WARNING: No OPTEE provided by BL2 boot loader, Booting device without OPTEE initialization. SMC`s destined for OPTEE will return SMC_UNK
INFO:    BL31: Preparing for EL3 exit to normal world
INFO:    Entry point address = 0x200000
INFO:    SPSR = 0x3c9
```

逐行读法：

| 行 | 谁打的 / 含义 |
| --- | --- |
| `BL31: v2.3():…:derrick.huang, fwver: v1.53` | TF-A 版本 + 构建者 + Rockchip 的 `fwver`（**判版本认这个**） |
| `Built : 12:10:56, Aug 25 2025` | 厂商构建时间 |
| `spec: 0x1` / `code: 0x88` | SoC 规格/型号编码（本板实测值；0x88 是这颗芯片在 BL31 里的 code） |
| `ext 32k is not valid` | 外部 32.768 kHz 晶振没接/无效 ⇒ 用内部 RC 做低频计时（正常现象） |
| `ddr: stride-en 4CH` | DDR 交给 BL31 的"分块交错"参数 |
| `GICv3 without legacy support detected.` / `ARM GICv3 driver initialized in EL3` | **GIC 由 EL3 初始化**（这也是为什么 EL3 常驻：中断路由/虚拟化要在 EL3 配） |
| `valid_cpu_msk=0xff bcore0_rst=0x0, bcore1_rst=0x0` | 8 个核都可用、两个 cluster 无复位残留 |
| `l3 cache partition cfg-0` | DSU/L3 分区配置 |
| `system boots from cpu-hwid-0` | 启动核 = CPU0（big.LITTLE 里也可 boot from A76，看 HWID 引脚） |
| `bypass memory repair` | 跳过内存修复流程（DRAM 无坏块记录时正常） |
| `idle_st/pd_st/repair_st` | 低功耗状态寄存器快照（调试用） |
| `dfs DDR fsp_params[i].freq_mhz=` | DDR 的四档频率点（DFS 表），来自 DDR bin/BL31 匹配结果 |
| `BL31: Initialising Exception Handling Framework` | EHF 初始化 |
| `BL31: Initializing runtime services` | PSCI/SIP 服务上线 |
| `WARNING: No OPTEE … SMC_UNK` | **我们默认不带 BL32**，TEE 号段的 SMC 会返回 `SMC_UNK`（正常） |
| `BL31: Preparing for EL3 exit to normal world` | 准备把控制权交给 BL33 |
| `Entry point address = 0x200000` | BL33（U-Boot proper）入口 |
| `SPSR = 0x3c9` | 交给 normal world 时的状态：`M[3:0]=0b1001` = **EL2h**（U-Boot 跑在 EL2） |

---

## 6. 怎么换 / 升级 BL31

### 6.3 eMMC 升级实录（2026-09-18，开发板 `<HOST_07>`）

1. 备份：`dd if=/dev/mmcblk0 bs=1M count=16 of=…`（16 MiB 覆盖 GPT + idbloader + itb），
   落到 `.scratch-boot/dev238-emmc-boot-16M-<时间>.bin`，sha256 `5b9db3145bf6573a…`
   （与 11:11 那份**逐字节相同** ⇒ 备份有效、loader 区没被别的东西动过）。
2. 只写 itb（**不动 idbloader**，SPL 与 DDR v1.18 保持原样）：
   `dd if=/tmp/u-boot-v153.itb of=/dev/mmcblk0 bs=512 seek=16384 conv=fsync,notrunc`
   （1,373,696 B = 2683 扇区，落在 8.00~9.31 MiB，**p1 从 16 MiB 起，不碰任何分区**）。
3. 回读校验：`dd if=/dev/mmcblk0 bs=512 skip=16384 count=2683 | sha256sum` =
   `5c35533540b94436d8aa4ef5de3194571003a2531d9e7a829908224fbf651b16`（与源文件一致）；
   分区表/文件系统完好。
4. 生效验证（工件级，已闭合）：itb 内三段 sha256 与 `rk3588_bl31_v1.53.elf` 解码结果**逐段相同**
   （`af59234162…` / `488c757001…` / `b621eea76c…`），也正是串口里
   `## Checking atf-1/2/3 … + OK` 打的三个哈希。重启后板子 20 s 内回来，PSCI 正常、
   `/dev/tee*` 不存在（BL31-only）、`MemTotal` 8,121,060 kB 不变。
   ⚠️ **"运行时跑的确实是 v1.53"只能从串口 banner 确认**（内核侧读不到 BL31 版本，见 §5.2）。

---

## 7. 与其它组件的关系（交叉链接）

| 组件 | 关系 |
| --- | --- |
| BL32 / OP-TEE | 平行常驻在 S-EL1；BL31 的 `opteed` 转发 SMC，并替 secure world 管时钟/电源域 → [bl32-optee.md](bl32-optee.md) |
| U-Boot（BL33） | BL31 把 EL3 之下的世界交给它（EL2）；FIT 里 `atf-*` 与 `uboot` 是平级 loadable → [bootloader-blobs.md](bootloader-blobs.md) |
| 内核 PSCI 驱动 | `drivers/firmware/psci/psci.c`：所有 PSCI 号都从这里出发 → 主篇 [bl31-mechanism.md](bl31-mechanism.md) §3.1 |
| 内核 SIP 驱动 | `drivers/firmware/rockchip_sip.c` + `include/linux/rockchip/rockchip_sip.h` → 主篇 [bl31-mechanism.md](bl31-mechanism.md) §3.2 |
| OTP / Secure Boot | BL31 只读 OTP（PVTPLL 校准、HDCP key）；写路径在 U-Boot/BL32 → [otp-secure-boot.md](otp-secure-boot.md) |
| 重启/关机 | 本文 §4 + [shutdown-reboot-path.md](shutdown-reboot-path.md) |

---

## 8. 坑（都踩过或有实测依据）

1. ⛔ **`atf-1/2/3` 不是三个固件、也不是 BL32**：它们是同一颗 BL31 ELF 的三个 `PT_LOAD`
   （主镜像 / PMU SRAM 代码 / DDR M0）。BL32 是 FIT 里另开的 `(optee)` 节点。
2. ⛔ **`atf-N` 的编号按段大小降序**（`sort --key=5 -nr`），**不是**按 ELF 段序；判据看 `load=`。
3. ⛔ **别把 `prebuilt/` 当当前固件**：`prebuilt/{idbloader.img,u-boot.itb}` 上游不跟踪，
   里面仍是 **BL31 v1.47**（2026-09-18 实测），只有新构建的产物才是 v1.53。
4. ⚠️ **SPL 横幅的构建日期不刷新**（增量编译不重建 `spl/u-boot-spl.bin`）⇒ 判新旧认
   `fwver:` 与节点哈希。
5. ⛔ **"给 `TEE=` 才打进 FIT"这条判断上游在 arm64 上是坏的**（早退写在 `ARCH == arm` 分支里），
   不给 TEE 会去 incbin 不存在的 `tee.bin` 导致 `mkimage` 直接失败；我们已修，
   **整棵升级 u-boot 时必须重新打**（[../build/upstream-sync.md](../build/upstream-sync.md) §2.1）。
6. ⚠️ **"BL31 版本只能看串口"要修正**：SIP `0x82000001` 能问，但内核没有出口
   （`sip_smc_get_atf_version()` 无人调用）⇒ 实用结论仍是"看串口"。
7. ⛔ **`poweroff` 会让板子真的断电**（远程试过就回不来了）；`reboot` 安全。
   两条路都经 EL3，见 §4。
8. ⚠️ **低端 2 MB 是"共享地带"**：BL31 `0x40000`、DDR M0 `0xf0000`、ramoops `0x110000`，
   内核从 `0x200000` 起。往这片区域放东西（比如自定义 blob）前先对表。
9. ⚠️ **别用 watchdog 解释重启**：`wdt_config` 在 BL31 里是死代码，内核的 `dw_wdt` 只是
   PSCI 失败的兜底；重启是 CRU `GLB_SRST_FST`（§4）。
10. ⚠️ **banner 里的版本对应的是"这次从哪块介质启动"**：eMMC / SD 卡可以各有一份 loader
    （我们这儿 eMMC 现在是 v1.53、SD 卡上是 v1.53+BL32）。看到别的版本先确认启动介质；
    `cfg/boards` 里没有"当前 loader 版本"这种信息，只有串口能告诉你。
11. ⚠️ **EL3 与内核共用同一个串口**：EL3 的 `PANIC at PC : 0x…` 会和内核日志混在一起，
    排查时先看是哪一层的格式。

---

## 9. 相关文档

- [rk3588-boot-flow.md](rk3588-boot-flow.md) —— 启动链全貌（§3.7 EL 总表）；FIT/BL31 结构见 [bootloader-blobs.md](bootloader-blobs.md)、重启/关机见 [shutdown-reboot-path.md](shutdown-reboot-path.md)
- [bl32-optee.md](bl32-optee.md) —— BL32（OP-TEE）开关、实测、TA 加载、OTP 排查
- [../build/upstream-sync.md](../build/upstream-sync.md) §2.1 / §6 —— BL31 v1.47→v1.53 的升级记录与上板实测
- [otp-secure-boot.md](otp-secure-boot.md) —— OTP/eFuse 字段地图与"只读"铁律
- [boot-serial-log.md](boot-serial-log.md) §1/§6 —— 我们归档的两份完整串口启动日志（v1.47 / v1.53，含带 BL32 的那次）
- `tools/check-atf.sh` —— 产物侧的逐段 sha256 核对
- `u-boot/arch/arm/mach-rockchip/{make_fit_atf.sh,fit_nodes.sh,decode_bl31.py}` —— FIT 打包链
