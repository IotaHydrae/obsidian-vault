# 串口启动日志存档（原始文本）

> 这是 [boot-serial-log.md](boot-serial-log.md) 的**原始日志备份**：正常启动那份（全文）
> 与带 BL32（OP-TEE）那份。**分析、逐段注解、可核对的固定数字、以及"怎么自己抓"都在主篇。**
> ⚠️ 下面是**从主篇原样搬过来的两段日志**（一字未改；`## ` 开头那些行是日志内容本身，不是本文标题）。

## TL;DR

- 日志里 `## Checking <节点> … sha256(…) + OK` 的 10 个 hex = **SPL 现场算出哈希的前 5 字节**，
  可与 `dumpimage -l prebuilt/u-boot.itb` 的 `Hash value` 对前 10 位（`base = 0x800`）。
- 两份日志的 `## Verified-boot: 0` 都说明 **OTP 里 Secure Boot 标志没烧**（含义见
  [boot-integrity.md](boot-integrity.md)、[otp-secure-boot.md](otp-secure-boot.md)）。


## 1. 原始日志

```text
DDR 9fa84341ce typ 25/02/06-17:24.09,fwver: v1.18
ch0 ttot10
ch1 ttot10
ch2 ttot10
ch3 ttot10
ch0 ttot16
LPDDR4X, 1848MHz
channel[0] BW=16 Col=10 Bk=8 CS0 Row=16 CS1 Row=16 CS=2 Die BW=16 Size=2048MB
ch1 ttot16
channel[1] BW=16 Col=10 Bk=8 CS0 Row=16 CS1 Row=16 CS=2 Die BW=16 Size=2048MB
ch2 ttot16
channel[2] BW=16 Col=10 Bk=8 CS0 Row=16 CS1 Row=16 CS=2 Die BW=16 Size=2048MB
ch3 ttot16
channel[3] BW=16 Col=10 Bk=8 CS0 Row=16 CS1 Row=16 CS=2 Die BW=16 Size=2048MB
Manufacturer ID:0x13
DQS rds:l0,h3
CH0 RX Vref:30.5%, TX Vref:24.8%,24.8%
DQ rds:h2 h6 h1 h2 h3 h1 h1 h2, h5 h1 h4 h4 h5 h7 h5 h5

DQS rds:l0,h1
CH1 RX Vref:29.7%, TX Vref:25.8%,24.8%
DQ rds:l0 l0 h2 h3 h6 h5 h1 h1, h1 h5 h7 h4 h5 h2 h2 l0

DQS rds:l0,h1
CH2 RX Vref:31.0%, TX Vref:22.8%,22.8%
DQ rds:h1 h4 h7 h4 h6 h4 h3 h1, h1 h3 h3 h3 h3 h3 h7 h3

DQS rds:h1,h1
CH3 RX Vref:31.4%, TX Vref:22.8%,21.8%
DQ rds:h6 h4 h6 h3 h4 h6 h3 h2, h5 h5 h3 h2 h7 l0 h3 h2

stride=0x2, ddr_config=0x4
hash ch_mask0-1 0x20 0x40, bank_mask0-3 0xa00 0x1400 0x2800 0x0, rank_mask0 0x401000
change to F1: 528MHz
ch0 ttot10
ch1 ttot10
ch2 ttot10
ch3 ttot10
change to F2: 1068MHz
ch0 ttot12
ch1 ttot12
ch2 ttot12
ch3 ttot12
change to F3: 1560MHz
ch0 ttot14
ch1 ttot14
ch2 ttot14
ch3 ttot14
change to F0: 1848MHz
ch0 ttot16
ch1 ttot16
ch2 ttot16
ch3 ttot16
out
U-Boot SPL board init
U-Boot SPL 2017.09 (Sep 12 2026 - 04:29:11)
Trying to boot from MMC2
MMC: no card present
mmc_init: -123, time 0
spl: mmc init failed with error: -123
Trying to boot from MMC1
spl: partition error
Trying fit image at 0x4000 sector
## Verified-boot: 0
## Checking atf-1 0x00040000 ... sha256(6a4a192c10...) + OK
## Checking uboot 0x00200000 ... sha256(0d7dbcfbf3...) + OK
## Checking fdt 0x0031aec0 ... sha256(31c141a5e8...) + OK
## Checking atf-2 0xff100000 ... sha256(70505bb764...) + OK
## Checking atf-3 0x000f0000 ... sha256(569ee96047...) + OK
Jumping to U-Boot(0x00200000) via ARM Trusted Firmware(0x00040000)
Total: 110.996 ms

INFO:    Preloader serial: 2
NOTICE:  BL31: v2.3():v2.3-831-g92b1674a5:derrick.huang, fwver: v1.47
NOTICE:  BL31: Built : 16:18:39, Sep  4 2024
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
ERROR:   Error initializing runtime service opteed_fast
INFO:    BL31: Preparing for EL3 exit to normal world
INFO:    Entry point address = 0x200000
INFO:    SPSR = 0x3c9


U-Boot 2017.09 (Sep 12 2026 - 04:29:11 +0000)

Model: Embedfire LubanCat-5
PreSerial: 2, raw, 0xfeb50000
DRAM:  8 GiB
Sysmem: init
Relocation Offset: eda43000
Relocation fdt: eb9f8b98 - eb9fecd8
CR: M/C/I
Using default environment

mmc@fe2c0000: 1, mmc@fe2e0000: 0
Bootdev(atags): mmc 0
MMC0: HS200, 200Mhz
PartType: EFI
DM: v2
No misc partition
boot mode: normal
FIT: No boot partition
No resource partition
No resource partition
Failed to load DTB, ret=-19
No find valid DTB, ret=-22
Failed to get kernel dtb, ret=-22
Model: Embedfire LubanCat-5
CLK: (sync kernel. arm: enter 1008000 KHz, init 1008000 KHz, kernel 0N/A)
b0pll 24000 KHz
b1pll 24000 KHz
lpll 24000 KHz
v0pll 24000 KHz
aupll 24000 KHz
cpll 1500000 KHz
gpll 1188000 KHz
npll 24000 KHz
ppll 1100000 KHz
aclk_center_root 702000 KHz
pclk_center_root 100000 KHz
hclk_center_root 396000 KHz
aclk_center_low_root 500000 KHz
aclk_top_root 750000 KHz
pclk_top_root 100000 KHz
aclk_low_top_root 396000 KHz
Net:   No ethernet found.
Hit key to stop autoboot('CTRL+C'):  0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x1
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x1
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x1
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe Linking... LTSSM is 0x0
pcie@fe150000: PCIe-0 Link Fail

Device 0: unknown device
MMC: no card present
mmc_init: -123, time 0
switch to partitions #0, OK
mmc0(part 0) is current device
Scanning mmc 0:1...
Found U-Boot script /boot.scr
reading /boot.scr
1677 bytes read in 1 ms (1.6 MiB/s)
## Executing script at 00500000
Boot script loaded from mmc 0
reading /ubuntuEnv.txt
4879 bytes read in 1 ms (4.7 MiB/s)
bootargs=rootfstype=ext4 rootwait rw console=ttyS2,1500000 console=tty1 console=ttyFIQ0 cgroup_enable=cpuset cgroup_memory=1 cgroup_enable=memory swapaccount=1 systemd.unified_cgroup_hierarchy=0 quiet splash plymouth.ignore-serial-consoles usbcore.autosuspend=-1 root=LABEL=writable
reading /dtbs/rockchip/rk3588-lubancat-5-v2.dtb
334672 bytes read in 4 ms (79.8 MiB/s)
reading /dtbs/rockchip/overlay/rk3588-lubancat-5-hdmi0-overlay.dtbo
1585 bytes read in 4 ms (386.7 KiB/s)
Applying device tree overlay: /dtbs/rockchip/overlay/rk3588-lubancat-5-hdmi0-overlay.dtbo
reading /dtbs/rockchip/overlay/rk3588-lubancat-5-hdmi1-vp2-overlay.dtbo
1585 bytes read in 3 ms (515.6 KiB/s)
Applying device tree overlay: /dtbs/rockchip/overlay/rk3588-lubancat-5-hdmi1-vp2-overlay.dtbo
reading /dtbs/rockchip/overlay/rk3588-lubancat-5-v2-usbcon-host.dtbo
234 bytes read in 4 ms (56.6 KiB/s)
Applying device tree overlay: /dtbs/rockchip/overlay/rk3588-lubancat-5-v2-usbcon-host.dtbo
reading /vmlinuz
42109440 bytes read in 235 ms (170.9 MiB/s)
reading /initrd.img
18932239 bytes read in 106 ms (170.3 MiB/s)
Fdt Ramdisk skip relocation
No misc partition
## Flattened Device Tree blob at 0x08300000
Booting using the fdt blob at 0x08300000
reserving fdt memory region: addr=8300000 size=62000
'reserved-memory' cma: addr=10000000 size=10000000
'reserved-memory' ramoops@110000: addr=110000 size=e0000
Using Device Tree in place at 0000000008300000, end 0000000008364fff
Adding bank: 0x00200000 - 0xf0000000 (size: 0xefe00000)
Adding bank: 0x100000000 - 0x200000000 (size: 0x100000000)
Adding bank: 0x2f0000000 - 0x300000000 (size: 0x10000000)
Total: 3627.810 ms

Starting kernel ...
```


## 6. 带 BL32（OP-TEE）的启动日志（2026-09-18，同一块 v2 板）

对照 §2 那份"不带 BL32"的日志，本例多出的正是 BL32 那几段（loader 从 SD 卡跑、系统仍从 eMMC 起）：

```text
U-Boot SPL 2017.09 (Sep 12 2026 - 04:29:11)
Trying to boot from MMC2
spl: partition error
Trying fit image at 0x4000 sector
## Verified-boot: 0
## Checking atf-1 0x00040000 ... sha256(af59234162...) + OK
## Checking uboot 0x00200000 ... sha256(1bccfcfca4...) + OK
## Checking fdt 0x00319448 ... sha256(31c141a5e8...) + OK
## Checking atf-2 0xff100000 ... sha256(488c757001...) + OK
## Checking atf-3 0x000f0000 ... sha256(b621eea76c...) + OK
## Checking optee 0x08400000 ... sha256(76eeb0b48d...) + OK        ← ★ 新增：BL32 也被逐个校验
Jumping to U-Boot(0x00200000) via ARM Trusted Firmware(0x00040000)
Total: 240.15 ms

INFO:    Preloader serial: 2
NOTICE:  BL31: v2.3():v2.3-942-g98eaeb2f3:derrick.huang, fwver: v1.53
NOTICE:  BL31: Built : 12:10:56, Aug 25 2025
INFO:    spec: 0x1
INFO:    code: 0x88
INFO:    customer demand: 0x0
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
INFO:    BL31: Initializing BL32                                  ← ★ 不再有 "No OPTEE provided…"/"opteed_fast" 报错
I/TC:
I/TC: OP-TEE version: 3.13.0-999-g1e844e4ca #hisping.lin (gcc version <PRIVATE_NETWORK> 20201103 (GNU Toolchain for the A-profile Architecture 10.2-2020.11 (arm-10.16))) #2 Fri May  9 00:41:09 UTC 2025 aarch64, fwver: v1.20
I/TC: OP-TEE memory size: TEEOS 0x200000 TA 0xc00000 SHM 0x200000     ← ★ 2+12+2 = 16 MB，与 MemTotal 少 16,352 kB 对得上
I/TC: Primary CPU initializing
I/TC: Primary CPU switching to normal world boot
INFO:    BL31: Preparing for EL3 exit to normal world
INFO:    Entry point address = 0x200000
INFO:    SPSR = 0x3c9
```

**和 §2 那份的差异（只有这几处）**：

| 位置 | 不带 BL32 | 带 BL32 |
| --- | --- | --- |
| SPL 逐节点校验 | 5 行（`atf-1/uboot/fdt/atf-2/atf-3`） | **6 行**，多 `## Checking optee 0x08400000 … + OK`（哈希前 5 字节 = `rk3588_bl32_v1.20.bin` 自身 sha256 的前 5 字节） |
| BL31 的 runtime services | `WARNING: No OPTEE provided by BL2 boot loader …` + `ERROR: … opteed_fast` | **没有这两行**，改成 `INFO: BL31: Initializing BL32` |
| OP-TEE 自己 | — | 4 行 `I/TC:` banner（版本 3.13.0-999-g1e844e4ca / `fwver: v1.20` / 内存布局 / Primary CPU 进出正常世界） |
| 启动耗时 | `Total: 199.68 ms` | `Total: 240.15 ms`（+40 ms，多初始化一个安全世界） |
| 其它（DDR 训练、BL31 的 GICv3/EHF/变量、`Entry point`/`SPSR`） | — | **完全一样** |

> 📌 内核起来之后（同一份日志的下一段）应能看到 `optee: revision 3.13 (1e844e4ca)` 与
> `initialized driver`、`/dev/tee0`；`MemTotal` 从 8,121,060 kB 降到 8,104,708 kB。
> 完整操作与判据见 [bl32-optee.md](bl32-optee.md)。
