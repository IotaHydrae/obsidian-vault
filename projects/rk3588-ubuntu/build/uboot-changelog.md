# U-Boot（`u-boot/` + 自建 deb + `prebuilt/`）：我们自己的改动账（精确到提交）

> 仓库里的 `u-boot/` 是**整棵 vendored 树**（带厂商历史提交），`prebuilt/` 是我们自己维护的启动产物，
> 打包规则在 `packages/u-boot-lubancat-rk3588/`。所以"U-Boot 有哪些改动"要同时看**三处提交**。
> 截至 **2026-10-03**，**我们自己的改动 = 4 个提交**（U-Boot 树 3 个 + 打包/prebuilt 1 个；厂商历史导入不算）。
> 内核那侧的改动在同目录的 [kernel-changelog.md](kernel-changelog.md)。
> 核对命令见 §0；现状与哈希见 §5；待办见 §6。
> 表与正文**按时间倒序（新→旧）**，与 `git log` 默认一致；每一节的标题里都带日期。

## TL;DR

| # | 提交 | 日期 | 一句话 | 关键实测 |
| --- | --- | --- | --- | --- |
| 1 | `8332879d7` | 10-02 | 删默认 `bootcmd` 里的 **Android 回退链** + 刷新 prebuilt | 新固件**按住 Ctrl+C 也能停在 `=>`**（旧固件会掉进 Android 回退并卡死） |
| 2 | `920e5c4f3` | 09-30 | 去掉默认 **NVMe 启动目标**（驱动与 `nvme` 命令保留）+ 刷新 prebuilt | U-Boot 自报 `Total: 1455 → 639.147 ms`（**−816 ms**） |
| 3 | `1d9a3822b` | 09-26 | 重建 `prebuilt/`（BL31 v1.53）；修 **dpkg ≥ 1.23 不触发构建** | `./tools/check-atf.sh` **rc=0**；eMMC 实测（写前 16 MiB 备份、回读 sha 一致、重启 PSCI 正常、无 `/dev/tee*`） |
| 4 | `2857f174c` | 09-16 | 自建 u-boot deb 打包；ATF **BL31 v1.47 → v1.53**；**BL32 做成开关**（默认不开） | SD 卡验带 BL32 的 loader：`/dev/tee0` + `/dev/teepriv0`、`MemTotal` 少 16,352 kB；默认产物与升级前一致 |

口径：**实测** = 有板上/工程机的命令输出；**已立档** = `notes/` 或 `AGENTS.md` 记着结论、本轮没重跑；
**未验证** = 明确没做/没测。

## 0. 怎么核对

```bash
git log --format='%h %ad %s' --date=short -- u-boot/                      # U-Boot 树（含厂商历史）
git log --format='%h %ad %s' --date=short -- packages/u-boot-lubancat-rk3588/ prebuilt/
git log -1 --format=%B <sha>                       # 为什么 + 判据
./tools/check-atf.sh                               # prebuilt 与镜像里的一致（rc=0 才算好）
sha256sum prebuilt/idbloader.img prebuilt/u-boot.itb   # 当前产物哈希（与 §5、AGENTS.md §6.3 对齐）
```

⚠️ **判 prebuilt 新旧只看串口 `fwver:` 与 FIT 节点哈希，别看 SPL 横幅日期**。
`u-boot/` 共 5 个提交，其中 **3 个是我们的**；`packages/u-boot-lubancat-rk3588/` 与 `prebuilt/` 3 个里 **1 个是我们的**
（厂商历史的 subject 形如 `first commit`）。

## 1. `8332879d7`（2026-10-02）删掉默认 `bootcmd` 里的 Android 回退链

- **改了什么**：`include/configs/rockchip-common.h` 的 `RKIMG_BOOTCOMMAND`
  （`evb_rk3588.h` 把它接给 `CONFIG_BOOTCOMMAND`）由
  `run distro_bootcmd; boot_android …; boot_fit; bootrkp;` 改成**只有 `run distro_bootcmd;`**；
  `boot_android` / `boot_fit` / `bootrkp` 命令本体保留（控制台手动还能用），只是不再进默认 `bootcmd`。
  同一提交刷新 `prebuilt/`，并订正 `AGENTS.md` §6.3 的 prebuilt 哈希行、更新 `notes/handoff.md` 与
  `notes/hardware/boot-serial-log.md` 的现场口径。
- **为什么**：2026-09-24 把 `CONFIG_BOOTDELAY` 改成 0 之后，用户实测：**轻按** Ctrl+C 能停在 `=>`，
  但从加电起**一直按住**会掉进 Android 回退链 —— 串口刷 `Could not find misc partition` /
  `optee api revision is too low` / `### ERROR ### Please RESET the board` 并卡住。
  根因是 `common/cli_hush.c` 的 `had_ctrlc()` 会中止 hush 脚本（我们靠 `boot.scr` 启动），
  持续的 0x03 把 `run distro_bootcmd` 打断，而 `bootcmd` 后面还挂着 Android 那条链。
- **结果**：新固件**按住也能停在 `=>`**；现场口径从"要轻按别按住"改成"按住也可以"。
- **出处**：`notes/hardware/boot-serial-log.md`（含 U-Boot 里哪些"报错"是正常的）、`AGENTS.md` §6.3。
- ⛔ 相关铁律：`CONFIG_BOOTDELAY=0` **别改成 `-1`/`-2`**；环境是 `CONFIG_ENV_IS_NOWHERE=y`
  ⇒ `fw_setenv` 与 `ubuntuEnv.txt` 都改不动 U-Boot 环境，只能重编 deb。

## 2. `920e5c4f3`（2026-09-30）去掉默认 NVMe 启动目标

- **改了什么**：`include/configs/rockchip-common.h` 的 `BOOT_TARGET_DEVICES` 里去掉
  `BOOT_TARGET_NVME(func)`（附原因注释）⇒ 默认 `boot_targets` 从 `nvme0 mmc1 mmc0 …` 变成
  `mmc1 mmc0 mtd…`；**NVMe 驱动与 `nvme` 命令保留**（控制台仍可手动 `nvme scan`）。
  同时刷新 `prebuilt/`（`idbloader.img` 303104 → **305152 B**，与 deb 同一次构建的一对）。
- **为什么**：`CONFIG_NVME=y` 时 `nvme0` 排第一 ⇒ distro boot 每次都先扫 NVMe ⇒ 触发 PCIe 枚举；
  **M.2 空槽时驱动重试 50 次 × `msleep(10)`**（`u-boot/drivers/pci/pcie_dw_rockchip.c:574`）
  = 白等 **517 ms** 并刷 50 行 `PCIe Linking... LTSSM is 0x0`。我们只从 eMMC/SD 启动、NVMe 是数据盘；
  内核侧（`CONFIG_PCIE_DW_ROCKCHIP=y` / `BLK_DEV_NVME=y`）完全不受影响。
- **结果与实测**：开发板（eMMC，同一套系统）U-Boot **自报 `Total: 1455 → 639.147 ms`（−816 ms）**。
- **出处**：`notes/system/boot-time-optimizations.md` §3、`notes/storage/nvme-performance.md` §4.1
  （含"尾部备份 GPT 仍会被扫"的注意事项）。

## 3. `1d9a3822b`（2026-09-26）重建 prebuilt（BL31 v1.53）+ 修 deb 打包

- **改了什么**：`prebuilt/{idbloader.img,u-boot.itb}` 重建为 ATF **BL31 v1.53**（取自**同一次构建**的
  `u-boot-lubancat-rk3588` deb；DDR 仍 v1.18）；`packages/u-boot-lubancat-rk3588/debian/rules`
  修 dpkg ≥ 1.23 不再触发构建的问题，并把 BL32(OP-TEE) 的开关**挪出 ARCH 分支**
  （否则 arm64 不给 `TEE=` 时会 `incbin` 不存在的 `tee.bin`，`mkimage` 直接失败）。
- **为什么**：BL31 从 v1.47 升到 v1.53 以对齐厂商；开发板 eMMC 上要能直接烧、能回读验证。
- **结果与实测**：`./tools/check-atf.sh` **rc=0**（全链路都含 ATF 且内部 sha256 一致）；
  当时 `u-boot.itb` sha `5c35533540b9…` 与板上已验那份**逐字节相同**；eMMC 实测（写前 16 MiB 备份、
  回读 sha 一致、重启 PSCI 正常、无 `/dev/tee*`）。
- **出处**：`notes/hardware/bl31-mechanism.md`、`notes/build/upstream-sync.md`（ATF/打包相关的历史说明）。
- ⛔ **`prebuilt/{idbloader.img,u-boot.itb}` 必须成对更新**（同一次构建），别只换一个。

## 4. `2857f174c`（2026-09-16）自建 deb 打包 + ATF v1.53 + BL32 开关

- **改了什么**（11 文件，+106/−25）：
  1. 自建 `packages/u-boot-lubancat-rk3588`（`debian/rules` 修 dpkg ≥ 1.23 不跑 build 的问题）；
  2. ATF：`targets.mk` 里**五个平台**的 `_bl31` 从 `rk3588_bl31_v1.47.elf` 换成 **v1.53**
     （TF-A `v2.3-942-g98eaeb2f3`），并补 `_bl32 = rk3588_bl32_v1.20.bin`；
  3. `debian/rules`：只有 `UBOOT_WITH_BL32=1` 时才把 `TEE=<rkbin/rk3588_bl32_v1.20.bin>` 传给 u-boot 构建
     ⇒ **默认产物与以前完全一致**（FIT 里 3 个 `atf` 节点、无 `optee` 节点）；
  4. `make_fit_atf.sh`：恢复 `gen_bl32_node` 调用（老树里那行被注释掉 ⇒ 光传 `TEE=` 根本不生效，
     是从上游 HEAD backport 的）；
  5. `fit_nodes.sh`：没给 BL32 时明确**不**产出 `optee` 节点；BL31 段缺失时报错并提示查什么；
  6. `fit_args.sh`：`DARM_BASE` 拼写修正为 `DRAM_BASE`，并显式算出 TEE/MCU 加载地址
     （不再靠"未定义变量 = 0"）。
- **为什么**：厂商 prebuilt 的 BL31 是 v1.47，要对齐厂商新版本（**后面几次 prebuilt 刷新都建立在这一笔之上**）；BL32（OP-TEE）做成"随时可开"
  但**默认不开**（我们不需要，且开着会让 `MemTotal` 变小）。
- **结果与实测**：不碰 eMMC、走 SD 卡验证带 BL32 的 loader ⇒ 出现 `/dev/tee0` + `/dev/teepriv0`、
  optee 驱动 initialized、`MemTotal` 少 **16,352 kB**；默认（不带 BL32）与升级前一致；
  开发板 eMMC 的 `u-boot.itb` 也单独写过并回读验过 sha。
- **出处**：`notes/hardware/bl31-mechanism.md`、`notes/hardware/bl32-optee.md`、
  `notes/hardware/bl32-optee-enable-and-verify.md`（怎么开、怎么验、怎么回滚）。

## 5. 现状快照（2026-10-03）

| 项 | 值 | 怎么核 |
| --- | --- | --- |
| ATF | **BL31 v1.53**（TF-A `v2.3-942-g98eaeb2f3`）常驻 EL3；**BL32 默认不开**（开关 `UBOOT_WITH_BL32=1`） | 串口 banner `fwver:`；`notes/hardware/bl32-optee*.md` |
| `prebuilt/idbloader.img` | **305,152 B**，sha256 `729d9ab2405d95ed…` | `sha256sum`（本轮实测，与 `AGENTS.md` §6.3 一致） |
| `prebuilt/u-boot.itb` | **1,379,328 B**，sha256 `0d0b41d42209ac08…` | 同上；FIT 里 5 个节点的 sha256 都验过 |
| 自动启动 | `CONFIG_BOOTDELAY=0`：零等待，但**按住 Ctrl+C 仍能进 `=>`** | 板上串口；`AGENTS.md` §6.3 |
| U-Boot 环境 | `CONFIG_ENV_IS_NOWHERE=y`（不持久化）⇒ `fw_setenv` 无效 | `notes/hardware/rk3588-boot-flow.md` |
| 启动耗时 | U-Boot 段 **639 ms**（去 NVMe 后） | `notes/system/boot-time-optimizations.md` §3 |
| 这几处的工作区 | 无未提交改动 | `git status --short -- u-boot prebuilt packages/u-boot-lubancat-rk3588` |

## 6. 待办与"容易漏掉的相关改动"

待办：

1. **每次重建 `prebuilt/` 后要同步三处**：`AGENTS.md` §6.3 的哈希行、本文 §5、
   并重跑 `./tools/check-atf.sh`（改了 prebuilt 就要重验镜像侧的一致性）。
2. **NVMe 是"默认不扫"而不是"不能用"**：控制台 `nvme scan` 仍可用；若哪天要支持从 NVMe 启动，
   得把 `BOOT_TARGET_NVME` 加回来并重新量那 517 ms（`notes/storage/nvme-performance.md` §4.1）。

不在 `u-boot/` 树里、但同属"启动侧"的改动（找"U-Boot 改动"时别漏）：

| 东西 | 在哪 | 备注 |
| --- | --- | --- |
| 设备树 overlay（`*.dtbo`） | `overlay/boot/firmware/dtbs/rockchip/overlay/` | ⛔ **文件名不许改**（`config/uEnv` 按名引用）：Type-C `-usbcon-fixedsrc`/`-dwc3-rolehost`、combphy 切换、CAN overlay 都在这里。见 `notes/hardware/combphy-mux.md`、`notes/usb/typec-rollout-and-decision.md` |
| U-Boot 环境变量 | `config/uEnv/*.uEnv` | ⛔ 用户手写；改了要重编镜像才生效 |
| 启动参数 `bootargs` | 构建期生成的 `ubuntuEnv.txt` | 面板不做内核控制台（2026-09-24 起，`image` 阶段有断言拦着） |
| `/boot/firmware` 里各文件的来源 | 启动分区（p1，ext4） | 内核/initrd/dtbs 是 `overlay/etc/initramfs/post-update.d/zz-update-firmware` 搬的，**不是 `flash-kernel`**（镜像没装它） |
