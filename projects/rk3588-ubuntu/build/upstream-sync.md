# 上游（野火 `ubuntu-ebf-rk3588`）同步：怎么比、取哪些、为什么

> 截至 2026-10-23，已评估到上游 **`88226c2ef`**，本轮较 `3b064db4b` 新增 17 个提交。
> 本轮仅将 RTL8822CE 的两项 TXBD 调试修复纳入源码并完成编译，**尚未安装到开发板运行**；
> 其余更新的取舍见 [逐提交台账 §4](upstream-sync-commits.md)。
> 上次全量同步点仍为 `0c8d06b49`（kernel 6.1.172，对应本工程 `4177ece42`）；
> 上游独立 clone 为 `~/example-project/ubuntu-ebf-rk3588`，分支 `24.04-panfrost`。采纳以内容比对为准。

## TL;DR

- **怎么比**见 §0（三个仓库、`git archive` 展开再 `diff -rq`）；**逐条取舍**见 commits 篇。
- **本轮已纳入源码**：RTL8822CE 关闭 TXBD 调试及配套条件编译；同 `#13` 配置编译通过，
  `.bss` 减少 8968 B。旧模块运行态 A/B 观测与限制见 [专题](../hardware/rtl8822ce-txbd-cost.md)；补丁版运行与最大吞吐尚未验证。
- **此前已采纳**：BL31 v1.53、boot 分区 ext4、`panel-simple` nvmem 修复、`remotectl` IRQ 回退和 CAN 配置。
  journald 当前关闭同步串口转发，由独立服务输出 warning 及以上日志，见 [串口日志](../system/journal-serial.md)。
- **不采纳**：rk3576 / LubanCat-3 全家（§3）、**u-boot 厂商树 1099 文件大更新**（§4，单独评估）、
  gstreamer 降级；**待评估**：mesa 26.2.3、u-boot `dtoverlay`。
- ⛔ **三个必须记住的坑**：① 升 BL32 **不是改两个文件**（§2，光加 `TEE=` 不生效）；
  ② u-boot 里有**我们加的 3 处本地改动**，升整棵厂商树必须重打（§4）；
  ③ defconfig 里 **Kconfig 不存在的符号会被静默丢弃**（commits §2.3 有排查口诀）。
- **改 loader 后必须两板都过一遍回归清单**（§6.1）。

## 0. 怎么比（可复现）

两个仓库是**各自独立的 clone**（对象库不互通，`git cat-file` 拿不到对方对象）⇒ 用**逐文件内容比对**：

```bash
U=~/example-project/ubuntu-ebf-rk3588
git -C $U log --oneline <上次同步点>..HEAD          # 这轮有几个提交
git -C $U diff --name-status <上次同步点>..HEAD     # 改了哪些文件

# 逐目录比：把两版展开到 /tmp 再 diff（以 u-boot 为例）
git -C $U archive <上次同步点> u-boot | tar -x -C /tmp/ub_old
git -C $U archive HEAD         u-boot | tar -x -C /tmp/ub_new
diff -rq <我们>/u-boot        /tmp/ub_old/u-boot    # 我们相对上游改了什么
diff -rq /tmp/ub_old/u-boot   /tmp/ub_new/u-boot    # 上游这一轮改了什么
```

- 2026-09-18 实测：**我们的 u-boot 树 == 上游 `0c8d06b49` 那份**（只差我们自己打的 2 个补丁，§4），
  上游此后换了 **1099 个文件**；`kernel-6.1` 只差 94 个文件、**全是 rk3576**。
- ⛔ 比对时**不要**对 `kernel-6.1/`、`u-boot/` 跑整树 `grep -r`/`find`（会啃十万个文件，见 `AGENTS.md` §2.17）；
  用 `diff -rq`/`rsync -n` 这类"只比元数据或内容哈希"的工具，并且**明确限定子目录**。

## 1. 决策总表（取 / 不取 / 待定）

| 上游内容 | 结论 | 出处 / 原因 |
| --- | --- | --- |
| RK3588 **BL31 v1.53** | ✅ 已采（2026-09-18） | §2；已写入开发板 eMMC，两板 deb 已验证 |
| **BL32 v1.20（OP-TEE）** | ✅ 做成"默认关、随时可开"的开关 | §2.1；消费者逐个排查后结论是不需要 |
| boot 分区 **vfat → ext4** | ✅ 已实施（2026-09-26） | commits §2.3；动机是掉电可靠性（FAT 无 journal） |
| `panel-simple` nvmem probe 死循环 | ✅ 已采 | commits §2.1 |
| `remotectl` IRQ 回退（`93cdf2c7c`） | ✅ 已采 + A/B | commits §2.2（开机 4 行报错全消失） |
| journald 串口日志（`86da08a2b` / `7faee4cfa`） | 已覆盖当前关闭同步转发的方向 | 早期采纳记录见台账 §2.1；现行策略见 [串口日志](../system/journal-serial.md) |
| 删冗余 BT/USB 服务（`f060c540d`） | ✅ 已清理 + 反向断言 | commits §2.2 |
| `CONFIG_CAN_RK3568=y`（`3b064db4b`） | ✅ 已采（2026-09-30） | commits §3 |
| apt mirror → 华为云 | ✅ 早就是 | commits §2.2 |
| rk3576 / LubanCat-3 全家 | ❌ 不取 | §3 |
| u-boot 厂商树（1099 文件） | 🟡 单独评估、暂不取 | §4 |
| mesa 26.2.3 | 🟡 单独立项（要 libllvm18 + 回归图形栈） | commits §2.2 |
| u-boot `dtoverlay` + uEnv 改法 | 🟡 暂不动（要改写用户手写的 uEnv） | commits §2.2 |
| Realtek WiFi 大更新（`5bed80a74`） | 优先级低；该次未修改 8822CE | commits §2.2/§2.3；本轮 TXBD 修复单列于下 |
| RTL8822CE TXBD 调试（`3a28689bd` / `409968622`） | 已纳入源码，编译通过，未安装运行 | [成本与验证边界](../hardware/rtl8822ce-txbd-cost.md) |
| root / boot 跟随 `boot.scr` 所在介质（`d74ddecdb`） | 值得独立适配，尚未实施 | 不能据此推断 BootROM 或 U-Boot 二进制来源；当前使用 [SD 专用变体](../storage/multi-media-label-conflict.md) |
| CPUID、介质与软件版本总览（`d183e425b` / `88226c2ef`） | 可借鉴只读字段，尚未实施 | 保留 `lsp-config`、硬件监控及定制 MOTD；详见台账 §4 |
| 自动改 MAC、ABC 通用板型与 U-Boot 自动选 DTB | 不直接采纳 | 保留现有网络身份、两板构建及预合并 DTB 契约；详见台账 §4 |
| gstreamer 降到 1.22.9 | ⛔ 不跟（我们 1.24.2） | commits §2.2 |
| gc08a8 Kconfig / MIPI 相机 | ❌ 不取（我们只用 UVC） | commits §2.3 |

## 2. BL31 v1.53 已采纳；BL32 默认关、开关就绪

**上游改了三处**：① `debian/rkbin/` 换 `rk3588_bl31_v1.53.elf`、新增 `rk3588_bl32_v1.20.bin`；
② `debian/targets.mk` 各平台 `_bl31` + 新增 `_bl32`；③ `debian/rules` 多传 `TEE=…_bl32`。

⛔ **光加 `TEE=` / `_bl32` 不会真的把 BL32 打进 FIT**：`TEE=` 由
`u-boot/arch/arm/mach-rockchip/fit_nodes.sh` 的 `gen_bl32_node()` 消费，而我们那版 u-boot 里
`make_fit_atf.sh` 的 `gen_bl32_node` 是**注释掉的**、`fit_nodes.sh` 把 `TEE` 硬写成 `tee.bin`、
`fit_args.sh` 用错拼的 `DARM_BASE` 算加载地址。⇒ 两条路：**① backport 那 3 个文件（不到 20 行，推荐）**
或 **② 整棵 u-boot 升级**（§4）。

✅ 好消息：RK3588 的 `CONFIG_SYS_SDRAM_BASE = 0` ⇒ 新树 `DRAM_BASE + 0x8400000` 与老树的
`TEE_OFFSET = 0x8400000` **同一个值**，"老 u-boot + 新 BL32"不存在地址错配，差的只是"节点根本不会被生成"。
⚠️ 我们的 `targets.mk` 里有**自己加的 `UBOOT_PLATFORMS` 过滤块**（`build-u-boot.sh` 依赖它）
⇒ **不能整文件覆盖**，只能按平台逐行改。

**已落地**（2026-09-18，用户选方案 A：只升 BL31）：

| 项 | 值 / 位置 |
| --- | --- |
| BL31 v1.53 入库 | `packages/u-boot-lubancat-rk3588/debian/rkbin/rk3588_bl31_v1.53.elf`（365,344 B，sha256 `4969a8d74dbc…`，`v2.3-942-g98eaeb2f3`）；旧的 v1.47 **已删**（上游也是改名） |
| 五个平台 `_bl31` | `debian/targets.mk` 全部指向 v1.53（保留我们的 `UBOOT_PLATFORMS` 过滤块） |
| 构建验证 | 两板 deb 的 FIT 都是 3 个 atf 节点 + 内嵌 `fwver: v1.53`；`decode_bl31.py` 仍拆出三段（`0x40000`/`0xf0000`/`0xff100000`）⇒ `atf-1/2/3` 判据不变 |
| ⚠️ 判据 | deb **版本号没变**、FIT 大小随构建时间戳浮动 ⇒ **只认 `fwver: v1.xx`** |
| 顺手修的静默坑 | `scripts/build-u-boot.sh` 现在**每次构建都从 `u-boot/` 增量同步**源码副本（原来只在首次拷贝，改了 `u-boot/` 会静默不生效） |

### 2.1 为什么默认不上 BL32（消费者逐个排查 + 开关）

**逐条排查"谁会用到 TEE"**（都查过，结论是没有）：

| 潜在消费者 | 需要 BL32？ |
| --- | --- |
| 镜像用户态 | ❌ 仓库/镜像里**没有任何** `optee-client`/`tee-supplicant`/`libteec` 引用；板上 `/dev/tee*` 不存在 |
| 内核 `CONFIG_OPTEE=y` | ❌ 驱动在，无 BL32 时 probe 失败；装了但不装用户态只是白占内存 |
| `rockchip-secure-otp`（**唯一真走 OP-TEE 的内核驱动**） | ❌ RK3588 内核 DT 里**没有 `secure-otp` 节点**（只有 rk3528）⇒ 永不 probe；我们读的是**非安全 OTP**（见 [../hardware/otp-secure-boot.md](../hardware/otp-secure-boot.md) §1） |
| `CONFIG_TRUSTED_KEYS=m` | ❌ 没有"无 TPM 又要密封密钥"的需求 |
| `optee-rng` | ❌ SoC 有自己的 TRNG（`rockchip_rng`） |
| HDCP2.2（HDMI **RX**） | ❌ 驱动里没有任何 tee/optee/secure 引用，且我们不用 HDMI-IN |
| Secure Boot / 验签 | ❌ 走 **OTP eFuse + FIT 签名 + BootROM/BL31**；反过来**上了 BL32 将来 secure boot 还要多签一个镜像** |
| Widevine / DRM 受保护播放 | ❌ Chromium 的 L3 是软件实现，我们也不播受保护内容 |

**代价（真上的话）**：多一个要跟版本的二进制（477,616 B）、DRAM 多一段保留（默认基址 `0x8400000`）、
启动链多一环、安全世界 TCB 变大、将来 secure boot 多签一个镜像；要用还得装 `tee-supplicant`
（noble 里有 4.1.0，而 BL32 里的 OS 是 **3.13**）。

**什么时候才该上**（命中任一条再动手）：① 受保护内容通路（Widevine L1 / HDCP / secure video path）；
② 要用 TEE 存密钥或做 attestation；③ 厂商某个必需功能/SDK 要求 OP-TEE；
④ 要在 Linux 里读 **secure OTP**（得先给 RK3588 的 DT 加 `secure-otp` 节点）。

**开关（默认关，真机验过）**：二进制已入库（`rk3588_bl32_v1.20.bin`，sha256 `76eeb0b48d3d…`，与上游一致）、
`targets.mk` 备好 `_bl32` 映射、`debian/rules` 里 `ifeq ($(UBOOT_WITH_BL32),1)` 才传 `TEE=`、
`build-u-boot.sh` 透传开关并在"开了但 deb 里没有 `(optee)` 节点"时 `return 1`。

```bash
sudo UBOOT_WITH_BL32=1 ./build.sh -b lubancat-5-v2 -u     # 带 BL32（itb 1,850,880 B，多 (optee) 节点）
sudo ./build.sh -b lubancat-5-v2 -u                        # 默认：不带（与以前完全一致）
```

⛔ **踩到并修掉的坑（arm64 专属）**：上游把"没给 `TEE` 就早退"写在 `if [ "${ARCH}" == "arm" ]` **里面**，
而 arm64 板子（`ARCH="arm64"`）根本进不去 ⇒ 不给 `TEE` 时也会生成 `/incbin/("tee.bin")`，
`mkimage: FATAL ERROR: Couldn't open "tee.bin"` ⇒ **整个 u-boot 构建失败**（上游无条件传 `TEE=`，所以没暴露）。
我们的修法是把判断**挪出 ARCH 分支**：

```sh
if [ -z "${TEE}" ] && [ ! -f tee.bin ]; then
	return          # 默认路径：不产出 optee 节点
fi
TEE="${TEE:=tee.bin}"
```

📄 完整操作手册（开关实现、产物验证判据、**不碰 eMMC 的 SD 卡上板验证法**、回滚、
"真要**用** TEE 还差什么"、8 条坑）见 [../hardware/bl32-optee.md](../hardware/bl32-optee.md)；
各版本对照表见 [../hardware/bl31-version-diff.md](../hardware/bl31-version-diff.md)。

## 3. ❌ 不需要合的（与我们无关）

- **rk3576 / LubanCat-3 / 3io 的全部内容**：`config/boards/lubancat-3*.conf`、`config/uEnv/lubancat-3*.uEnv`、
  `packages/u-boot-lubancat-rk3576/`、`kernel-6.1` 的 `rk3576-lubancat-3*` dts/dtsi/overlay（约 90 个文件）。
  我们只支持 `lubancat-5-v2` / `lubancat-5io`（`AGENTS.md` §1）。
- ⛔ **`kernel-6.1/.../dts/rockchip/Makefile` 和 `overlay/Makefile` 千万不要单独拿**：上游那两处
  **只加了 `rk3576-lubancat-3.dtb` / `-3io.dtb` 两行**，单独取会引用不存在的 dts ⇒ 内核编译**直接失败**。
- `rk3576.dtsi` / `rk3576-linux.dtsi`：只影响 RK3576；**上游没动任何 RK3588 的内核 dts**。
- `config/uEnv/lubancat-{5,5-v2,5io}.uEnv`：纯注释改动，而且是**用户手写文件**（`AGENTS.md` §2.8）⇒ 不动。

## 4. ⚠️ u-boot 厂商树大更新（1099 个文件）——单独评估，**不紧急**

夹在"add rk3576"那两个提交里，是**整棵厂商 U-Boot 树的刷新**（版本号仍是 2017.09）：
`arch/arm` 252、`drivers/video` 136、`drivers/mtd` 35、`drivers/power` 30、`include/configs` 23、
`drivers/misc` 21、`drivers/usb` 20、`drivers/ram` 19、`drivers/clk` 16、`drivers/pinctrl` 14……
含 `rk3588.dtsi` / `rk3588-u-boot.dtsi` / `rk3588s*.dtsi`、`drivers/{usb,phy,pci,mmc,net}` 等
**rk3588 也能用到**的改动。

- ✅ **我们用的两个 defconfig 没变**，`u-boot/arch/arm/dts/rk3588-lubancat-*.dts` 上游**没改**。
- ⛔ **我们在 u-boot 里有 3 处本地改动，升整棵树时必须重新落上去**：
  1. `arch/arm/mach-rockchip/decode_bl31.py`：`python2` → **`python3`**。
     ✅ 上游 HEAD 已自己修好，路线 ② 时这条不用再打。
  2. `arch/arm/mach-rockchip/fit_nodes.sh`：我们加的**硬校验** —— `NUM -eq 1`（只产出 uboot 段、
     没有任何 ATF 段）时 `exit 1`，防"静默生成不含 ATF 的 `u-boot.itb`"（表现为启动卡在 SPL→U-Boot）。
     ⚠️ **上游没有这条**，必须手工加回。
  3. ⭐ `fit_nodes.sh` 的 `gen_bl32_node()` 那个 arm64 早退坑（§2.1）—— 只要我们还"默认关 BL32"，
     这条修复必须重打，否则 `./build.sh -u` 直接挂在 FIT 生成。
- 建议：**先不合**。§2 只需要 3 个脚本的小改动就能拿到 BL31/BL32；等确实需要 u-boot 里的 rk3588 修复
  （或要跟 rk3576 共用一套）时，再单独做一次"升 u-boot + 两板全量回归"。

## 5. 两处顺手项

### 5.1 `load_addr` 的潜伏冲突（现状：**已跟上游对齐**）

`scripts/build-lsp-image.sh` 里现在是 `setenv load_addr "${kernel_addr_r}"`（**已采用**，脚本里带着
原因注释）—— 历史上我们**硬编码 `0x7000000`（112 MiB）**，上游那份把它改成了 `${kernel_addr_r}`。

| 变量 | 值 | 区域（`u-boot/include/configs/rk3588_common.h:62-70`） |
| --- | --- | --- |
| `kernel_addr_r` | `0x00400000`（4 MiB） | 内核 Image（**`load_addr` 现在用它**） |
| `scriptaddr` / `pxefile_addr_r` | `0x00500000` / `0x00600000` | boot.scr / pxe |
| `kernel_addr_c` | `0x05480000`（84.5 MiB） | **"compressed kernel: 84M ~ 130M"**（厂商注释原文） |
| **旧的 `load_addr`** | `0x7000000`（112 MiB） | ⚠️ **正落在这个"压缩内核窗口"里** |
| `fdtoverlay_addr_r` / `fdt_addr_r` | `0x08200000` / `0x08300000` | overlay / DTB |
| `ramdisk_addr_r` | `0x0a200000`（162 MiB） | initrd |

- `load_addr` 在 `boot.cmd` 里**只用来暂存 `/ubuntuEnv.txt`（约 1 KB）**，紧接着 `env import -t`，
  之后才 `load … ${kernel_addr_r} /vmlinuz` ⇒ 用 4 MiB（= `kernel_addr_r`）**安全**：env 导入发生在
  内核装载之前，覆盖同一地址无害。
- 而旧的 `0x7000000` 对我们的启动路径也是**无害**的（我们走 distro boot + 未压缩 Image，
  从不使用 `kernel_addr_c` 那套压缩内核流程），但它是**潜伏冲突**：一旦换成压缩内核，
  112 MiB 处的 env 会被解压过程覆盖。所以跟上游对齐是"消掉一个未来的坑"。
- **验证**（改这两行时做一次）：`/proc/cmdline` 里 `usbcore.autosuspend=-1` 等 bootargs 齐全（§6.1 第 4 项）。

### 5.2 `prebuilt/{idbloader.img,u-boot.itb}` 的现状

- 这两个文件是**我们放的兜底**（上游不跟踪）；`image` 阶段只在**镜像不含 ATF** 时才用它们覆盖镜像头部
  （`build-lsp-image.sh` 里对应那步）。
- **当前（2026-10-02 重建，含"去掉 Android 回退链"那版）**：
  `u-boot.itb` = 1,379,328 B / sha256 `0d0b41d42209…`；`idbloader.img` = 305,152 B / sha256 `729d9ab2405d…`
  —— 权威那一行记在 `AGENTS.md` §6.3（**每次重建后同步它**）。
  ⚠️ 本文早先版本记的 `5c35533540b9…`/301,056 B 是 2026-09-20 那版，**已过时**。
- 自检：`./tools/check-atf.sh` 应 rc=0（prebuilt 与 deb 两侧 `atf-1/2/3` 齐全、内部 sha256 自洽）。
- 判"BL32 有没有打进去"看 `dumpimage -l` 里有没有 `(optee)` 节点 —— `(atf-` 计数启用 BL32 后**仍是 3**
  （BL31 被切成三段，BL32 是另一个节点）。

## 6. 合并 bootloader 后的回归清单（RK3588 boot 关键路径，**两板都要**）

### 6.0 先编出 deb，并验证它真的是新版本

✅ **不需要手工清编译缓存**：`u-boot.its` 的规则带 `FORCE`，`.its` 一变新 `u-boot.itb` 的 `if_changed`
就会重建 —— 换 `debian/rkbin/` + `targets.mk` 后直接重跑 `build.sh -u` 即可（实测 v1.47 → v1.53 生效）。

```bash
sudo ./build.sh -b lubancat-5-v2 -u
t=$(mktemp -d); dpkg-deb -x build/u-boot-lubancat-5-v2_*.deb "$t"
dumpimage -l "$t"/usr/lib/u-boot/u-boot.itb | grep -E 'FIT description|Image [0-9]+ \('
strings   "$t"/usr/lib/u-boot/u-boot.itb | grep -oE 'fwver: v1\.[0-9]+' | sort -u   # 期望 v1.53
rm -rf "$t"
```

- ⚠️ **大小不是判据**：同一版 BL31、同一棵树，两次构建的 `u-boot.itb` 会差 1~2 KB
  （FIT 里嵌的 U-Boot 版本串带构建时间戳）。
- ⚠️ ⛔ **看 SPL 横幅的时间戳会误判**：增量编译**不会重建 `spl/u-boot-spl.bin`**，它保持上次的时间戳；
  这次真正重建的是 FIT 里的 BL31/BL32 与 `u-boot-nodtb`。**判断新旧只看 `fwver:` 与各节点哈希。**
- **改 `u-boot/` 源码时的同步行为**（2026-09-18 三段实验）：只改仓库侧 `u-boot/` 不同步副本 ⇒ **改动不生效**；
  已修为"每次构建都增量同步"（`rsync -rlptD -c -i`，**必须带 `-c`**：默认只比大小+时间戳，
  实测"字节数相同 + 同一秒"的改动会漏掉；排除 `debian`/`.cache`/`compile_commands.json`，**不加 `--delete`**，
  `UBOOT_NO_SYNC=1` 可跳过）。兜底：`sudo rm -rf build/u-boot-lubancat-rk3588 && sudo ./build.sh -u`。

### 6.1 上板测试（⚠️ 会写启动介质，先跟用户确认介质）

| # | 测什么 | 判据 |
| --- | --- | --- |
| 1 | 串口启动日志的 ATF 版本 | `NOTICE: BL31: v1.53`（`tools/check-atf.sh` 也会打印） |
| 2 | BL32（若启用） | 日志里 OP-TEE 初始化；`dumpimage -l` 里有 `(optee)`。**默认没上 BL32 时 `WARNING: No OPTEE` 属正常** |
| 3 | **SD 启动 + eMMC 启动**各一遍 | 都能进系统；`/proc/cmdline` 完整（`bootargs` 没丢） |
| 4 | U-Boot 环境导入 | `ubuntuEnv.txt` 的 `overlays=` 生效（dtbo 起作用，例如 Type-C 的 `try-power-role`） |
| 5 | Type-C / PD | `sudo lsp-usb status`；`power_role`/`vbus_typec`；`lsp-usb-powerfix` 自动纠正仍工作 |
| 6 | USB | 三台相机 + 蓝牙 + ESP32（`tools/lsp-usb-map.sh` 过一遍）、自动挂起仍是 `on` |
| 7 | 显示 | HDMI / DP 正常出图（VP 分配不变） |
| 8 | 温度 / CPU | 空载与满载温度、8 核跑分不异常（确认安全世界改动没引发降频） |
| 9 | 时钟 | `date` / RTC 正常（ATF 负责的部分变了） |
| 10 | 镜像体检 | `tools/verify-image.sh` 全绿 |

**写 loader 的安全做法**：板载 loader 区最后一个非零字节在 9.37 MiB，而 p1 从 **16 MiB** 开始
⇒ 写 loader 只动"32 KB ~ 16 MiB 预留区"，**不碰任何文件系统**。两种方式：

- **方式 A（零 eMMC 风险，推荐）**：把新 loader 写到**备用 SD 卡**前 16 MiB，插卡重启 —— BootROM 优先 SD，
  新的 ATF/U-Boot 从卡上跑、**系统仍从 eMMC 启动**（卡上没系统时 U-Boot 落到 eMMC）；验完拔卡即恢复。
- **方式 B**：直接写 eMMC 预留区（**写前必须备份前 16 MiB**）：`dd … seek=64`（32 KB）+ `dd … seek=16384`
  （8 MiB），写完**回读核对介质里的 `fwver:` 与 sha256**。⛔ 工程机 eMMC 不要动（`AGENTS.md` §2.13）。

### 6.2 实测记录（2026-09-18，方式 A：SD 卡带新 loader、系统走 eMMC）

- 卡上原 loader 先备份（16 MiB）；写新 loader 后**板上回读**校验 itb/idbloader 哈希与文件一致；
  再把卡上 `boot.scr` 改名 ⇒ U-Boot 在卡上找不到脚本、落到 eMMC 的 `system-boot` ⇒ loader 从 SD 跑、系统仍从 eMMC 起。
- **结果**：连续两次冷启动都在 ~20 s 内回网（`lubancat-5-v2`、eMMC 系统、内核 6.1.172）；
  cmdline/USB 自动挂起/Type-C/USB 树/温度/时钟全部正常；**串口坐实** `fwver: v1.53`
  （`v2.3-942-g98eaeb2f3`，构建时间 `Aug 25 2025`）。显示一项因现场没插屏没验。
  ⚠️ 同时看到 `NetworkManager-wait-online.service` failed（开机 3 s 时 WiFi 还没关联）—— **与 loader 无关**
  （loader 改不了 systemd 单元），NM 连通性是 `full`。
- **"loader 来自 SD"的三条独立证据**：① SPL 自报 `Trying to boot from MMC2`（SD 槽）；
  ② FIT 里 `atf-*`/`uboot` 四个节点 sha256 与上一次 eMMC loader 的**全都不同**；
  ③ FIT 里加载地址与离线 `dumpimage` 分析逐项一致。
- ✅ eMMC 随后也换成了 v1.53（方式 B，只写 `u-boot.itb`，idbloader/SPL/DDR 未动，写前备份）；
  **`lubancat-5io` 当时未测**（需要板子）。
- 现场完整串口原文与逐段注解 → [../hardware/boot-serial-log.md](../hardware/boot-serial-log.md) §5.2/§5.3；
  升级过程与 BL31 机制 → [../hardware/bl31-runtime-and-upgrade.md](../hardware/bl31-runtime-and-upgrade.md) §6.3（机制见 [../hardware/bl31-mechanism.md](../hardware/bl31-mechanism.md)）。

## 7. 相关

- 逐提交明细（已评估到 `88226c2ef`）：[upstream-sync-commits.md](upstream-sync-commits.md)
- BL32/OP-TEE 操作手册：[../hardware/bl32-optee.md](../hardware/bl32-optee.md)；BL31 版本对照：[../hardware/bl31-version-diff.md](../hardware/bl31-version-diff.md)
- 启动链路与 ATF 兜底：[../hardware/rk3588-boot-flow.md](../hardware/rk3588-boot-flow.md)、[../hardware/atf-missing-troubleshooting.md](../hardware/atf-missing-troubleshooting.md)
- 内核升级可行性（跨 BSP 大版本）：[kernel-upgrade-feasibility.md](kernel-upgrade-feasibility.md)
