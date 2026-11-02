# BL31 版本差异：changelog（v1.47 → v1.56）与 v1.47↔v1.53 二进制对比

> ⚠️ 口径：**发布日期** = rkbin 仓库里"把该版本提交进去"那次提交的日期；厂商提交说明基本只有一行
> `rk3588: bl31: update version to v1.xx`，**不带变更内容** ⇒ 下面的 changelog 是从二进制与上游
> TF-A 提交反推出来的，**别当厂商 release note 用**。

## TL;DR

- v1.47 → v1.56 的全版本清单（含我们能确认的差异）。
- ⚠️ **v1.54 起 BL31 主体段从 `0x40000` 挪到了 `0x60000`** ⇒ 换版本时内存地图会变，必须一起看。
- v1.47 ↔ v1.53 的二进制级对比（大小 / 段布局 / 字符串差异）。
- 换 BL31 要换**同一构建的一对产物**（`idbloader.img` + `u-boot.itb`），并核对 DDR/固件兼容矩阵 ——
  见 [bl31-mechanism.md](bl31-mechanism.md) §3.5 与 §6；升级实录见其 §6.3。

## 按版本号的 changelog（v1.47 → v1.56）

**口径（先看，别当厂商 release note 用）**

- **发布日期** = rkbin 仓库里"把该版本提交进去"那次 git 提交的日期；厂商的提交说明基本就是一行
  `rk3588: bl31: update version to v1.xx`，**不带变更内容**。
- **TF-A 版本 / 构建时间 / 大小 / sha256 / 段表** = 把该版本 ELF 抓下来**实测**（`readelf` + `decode_bl31.py`）。
- **"变化"** = 相邻两版二进制里的字符串自动 diff，只列有意义的（函数名/打印文案/寄存器名）。
  ⇒ 这是"**能看见的**变化"，不是完整改动清单（纯逻辑改动可能一个新字符串都没有）。
- ⚠️ rkbin **每个平台只留最新版**：v1.49~v1.52/v1.54/v1.55 在当前树里已被删除，
  这些二进制是按"新增那次提交"的 sha 从 git 历史取的（复现命令见本节末尾）。

| 版本 | 发布 | TF-A（rockchip 分支） | 构建时间 | ELF | atf-1 主体 | 主体 load / MemSiz | ELF sha256 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| v1.47 | 2024-09-04 | `v2.3-831-g92b1674a5` | `16:18:39, Sep  4 2024` | 430,880 | 204,860 | `0x40000` / 0x6f000 | `226567ab56b5d5ba…` |
| v1.48 | 2024-12-19 | `v2.3-868-g040d2de11` | `15:02:44, Dec 19 2024` | 430,880 | 204,860 | `0x40000` / 0x6f000 | `ff717807d873ce95…` |
| v1.49 | 2025-03-03 | `v2.3-893-g1653f2a08` | `14:27:47, Mar  3 2025` | 365,344 | **139,324** | `0x40000` / 0x6f000 | `28ab81ddacceb2a4…` |
| v1.50 | 2025-03-13 | `v2.3-905-gb23ff75bb` | `10:41:09, Mar 14 2025` | 365,344 | 139,364 | `0x40000` / 0x6f000 | `283982d93cb07f0f…` |
| v1.51 | 2025-04-25 | `v2.3-924-gbc6030fac` | `17:48:01, Apr 25 2025` | 365,344 | 143,480 | `0x40000` / 0x6f000 | `2847c82a44aff28f…` |
| v1.52 | 2025-06-27 | `v2.3-937-g6c8bcef24` | `11:07:15, Jun 27 2025` | 365,344 | 143,216 | `0x40000` / 0x6f000 | `64228e2f054c1608…` |
| **v1.53** ⬅ 我们在用 | 2025-08-25 | `v2.3-942-g98eaeb2f3` | `12:10:56, Aug 25 2025` | 365,344 | 143,216 | `0x40000` / 0x6f000 | `4969a8d74dbcf09d…` |
| v1.54 | 2025-12-26 | `v2.3-964-g8319968f5` | `17:20:23, Dec 26 2025` | 365,344 | 149,020 | **`0x60000`** / 0x71000 | `4d111f0c793035db…` |
| v1.55 | 2026-03-17 | `v2.3-973-g396907b8c` | `17:45:05, Mar 17 2026` | 365,344 | 149,044 | **`0x60000`** / 0x71000 | `bafe76c6dcfa2453…` |
| v1.56 ⬅ 当前 upstream | 2026-03-27 | `v2.3-975-g6c06d3bad` | `15:06:47, Mar 27 2026` | 365,344 | 149,196 | **`0x60000`** / 0x71000 | `2864c2fc36333e03…` |

（`atf-2` = 36,864 B 在 `0xff100000`、`atf-3` = 24,576 B 在 `0xf0000`，**十版全都一样**，没列进表。）

**逐版本能看见的变化**

| 版本 | 相对上一版 |
| --- | --- |
| v1.48 | 新增 `(arch timer reset`、`(ext-32k invalid`、`(sleep_pin: 0x%x 0x%x`（深睡/ext-32k/定时器相关打印） |
| v1.49 | **主体大瘦身**：atf-1 204,860 → 139,324 B、ELF 430,880 → 365,344 B；新增深睡容错 `uart always busy, don't save register this time!` |
| v1.50 | 新增启动打印 `(customer demand: 0x%x` —— **就是 banner 里 v1.53 多出来的那一行** |
| v1.51 | DDR 调试接口重做：`ddr_debug_func` → **7 个 `ddrdbg_*`**（读 regs/deskew/drvodt/powersave + 回写 deskew/drvodt/powersave）；加 DDR 类型名表 `LPDDR2/3/4/4X/5/UNKNOWN` |
| v1.52 | 没有新字符串（只有编译期常量/数据变化）；主体 143,480 → 143,216 B |
| v1.53 | 新增 `E: %s-%d: timeout! vop_post=%d, scan_line=%d, line_flag=%d`（VOP 超时诊断） |
| v1.54 | **主体 load `0x40000` → `0x60000`**（MemSiz 0x6f000 → 0x71000）；新增 `(%s: save mcu bin`、`(pmumcu vad wkup`、`pmumcu_init`（PMU MCU / VAD 唤醒） |
| v1.55 | **深睡唤醒源与 PMU 寄存器打印整段重写**：新的 `(CPU0 interrupt`、`(GPIO%d_INT:`、`(PMU0_PWR(0x%x) PMU0_WKUP_INT(0x%x)`、`(PMU1_PWR/PMU1_CRU_PWR/PMU1_WKUP_INT`、`(wake_up_st: 0x%x`、`(PMU0_DDR_RET`、`(virtual pwroff` 取代旧的 `(… interrupt wakeup`、`(GPIO%d_INTEN`、`(PMU0_PWR_CON…` 系列 |
| v1.56 | 新增 SCMI 时钟名 `scmi_aclk_vop_r` |

**对我们意味着什么**

- 我们停在 **v1.53**（= v1.49 瘦身后、v1.54 改地址前的那一档），比当前 upstream 落后 **3 版**。
- ⚠️ **将来要上 v1.54+ 时，`atf-1` 的 `load=` 会变成 `0x60000`**（FIT 的地址取自 `decode_bl31.py`
  按段文件名生成的名字，所以会自动跟着变，**别在脚本/文档里硬编码 `0x40000`**），同时主体
  MemSiz 从 0x6f000 涨到 0x71000（+8 KB）——低 2 MiB 内存地图要按 §2.3 重算。
- 对外接口面没变：v1.53→v1.56 没有新增 SIP 号（`scmi*` 62→63 只是多了个时钟名）；
  DDR↔BL31 兼容串（`current loader need match with trust bl31 V1.41 or newer`、
  `… loader ddr bin V1.13 or newer`）在 v1.53~v1.56 里都在 ⇒ 我们的 DDR v1.18 仍满足要求
  （注意 upstream 的 DDR bin 已经到 **v1.24**，将来一起升级时要重新核）。
- v1.54/v1.55 的改动集中在 **PMU MCU / 深睡唤醒源**，v1.56 更像收尾；我们不用深睡的话，
  升级的动机不强（**这也是"停在 v1.53"的一个理由**）。

**怎么复现 / 更新这张表**

```bash
# 需要代理时先 export https_proxy=… ；GitHub API 未认证限 60 次/小时，raw 直链不吃限流

# 1) 找"新增该版本"那次提交（拿 sha 与日期）
curl -s "https://api.github.com/repos/rockchip-linux/rkbin/commits?path=bin/rk35/rk3588_bl31_v1.56.elf&per_page=100"
# 2) 按该 sha 取 ELF（⚠️ 必须用"新增那次"提交；用"删除那次"（下一个版本发布的提交）会 404）
curl -sfL -o rk3588_bl31_v1.56.elf \
  "https://raw.githubusercontent.com/rockchip-linux/rkbin/<add-sha>/bin/rk35/rk3588_bl31_v1.56.elf"
# 3) 版本串 / 段表 / 哈希
strings -a rk3588_bl31_v1.56.elf | grep -m1 'v2\.3-'
BL31=./rk3588_bl31_v1.56.elf python3 u-boot/arch/arm/mach-rockchip/decode_bl31.py && ls -l bl31_0x*.bin
readelf -lW rk3588_bl31_v1.56.elf | grep LOAD ; sha256sum rk3588_bl31_v1.56.elf
# 4) 与上一版比"能看见的变化"（LC_ALL=C 保证排序一致，否则 comm 会报 not in sorted order）
for f in old.elf new.elf; do LC_ALL=C strings -a -n6 $f | LC_ALL=C sort -u > $f.str; done
LC_ALL=C comm -13 old.elf.str new.elf.str      # 只在 new 里 = 新增
LC_ALL=C comm -23 old.elf.str new.elf.str      # 只在 old 里 = 消失
```

> 本次抓下来的 10 个版本缓存在 `.scratch-boot/bl31-versions/`（gitignored，约 3.8 MB），
> 生成脚本是 `.scratch-boot/rkbin-bl31-diff.py`（便签性质，逻辑就是上面 4 步）。


### 6.2 v1.47 ↔ v1.53 到底差在哪（二进制级对比）

把两个版本的 **BL31 主体段**都抠出来比对（v1.47 从**当时**的 `prebuilt/u-boot.itb` 的 `atf-1`
取 —— 该文件已于 2026-09-20 更新为 v1.53，复现请用 `git show <旧提交>:prebuilt/u-boot.itb`；
v1.53 从新构建的 `u-boot.itb` 取；抠取用 `.scratch-boot/fit-extract.py`，靠 FIT 自带的 sha256 自校验）：

| | v1.47 | v1.53 |
| --- | --- | --- |
| TF-A 版本串 | `v2.3-831-g92b1674a5` | `v2.3-942-g98eaeb2f3`（**+111 commit**） |
| `fwver` / 构建时间 | `v1.47` / `16:18:39, Sep  4 2024` | `v1.53` / `12:10:56, Aug 25 2025` |
| atf-1（主体） | 204,860 B · sha256 `6a4a192c104cc98c…` | **143,216 B** · sha256 `af59234162f3ddc5…`（−30%） |
| 主体内存占用 MemSiz | 516,096 B | **454,656 B**（−61 KB） |
| atf-2（PMU-SRAM）/ atf-3（DDR M0） | 36,864 / 24,576 B | 同（36,864 / 24,576 B） |
| 可打印字符串条数 | 410 | 429 |

**新增的东西**（`comm` 逐条比对，只列有意义的）：
- `ddr_debug_func`（1 个）→ **`ddrdbg_get_{regs_info,deskew_info,drvodt_info,powersave_info}` +
  `ddrdbg_update_{deskew,drvodt,powersave}`（7 个）**：DDR 调试接口被拆细并加了"可回写 deskew/drvodt/powersave"；
- DDR 类型名表：`LPDDR2 / LPDDR3 / LPDDR4 / LPDDR4X / LPDDR5 / UNKNOWN`（v1.53 会报 DDR 类型）；
- 新启动打印：`(customer demand: 0x%x`、`(ext-32k invalid`、`(sleep_pin: 0x%x 0x%x`、`(arch timer reset`；
- 深睡容错：`uart always busy, don't save register this time!`。

**没变的东西（很重要）**：对外接口面**完全一致** —— `scmi*` 65 条、`pvtpll` 6 条、`hdcp` 7 条、
`armoff*` 4 条、`opteed*` 10 条、ECC、`last_log`、`Magic(`、`boot from resume`、DDR↔BL31 版本兼容串
（`current loader need match with trust bl31 V1.41 or newer` 等）在两版里逐项相同。
⇒ **v1.53 不是"给 normal world 加了新 SIP/PSCI"，而是内部重构 + DDR 侧增强 + 打印**；
PSCI 版本（1.1）、SMC conduit、重启/关机行为都没变（§3/§4）。

**实测**：v1.53 在我们板子上启动 2/2、CPU 热插拔、`reboot`、`poweroff` 全部正常；
banner 比 v1.47 多一行 `INFO: customer demand: 0x0`。

> ⚠️ 我们**没有** Rockchip 的版本变更说明（rkbin 仓库只有二进制和用法 README，提交说明也只是一行
> `update version to v1.xx`），上面这些是从两个二进制里 diff 出来的**客观差异**，不是厂商的 release note。
> **按版本号排的全版本清单（v1.47 → v1.56）见 §6.1。**

我们**不自己编 ATF**（源码链、DDR 匹配、签名都要厂商配套），只换厂商给的预编译 ELF：

```bash
# 1) 把新 ELF 放进 deb 的 rkbin 目录（与旧文件并存，便于回滚）
cp rk3588_bl31_v1.xx.elf packages/u-boot-lubancat-rk3588/debian/rkbin/
# 2) 改五个平台的映射
grep -n "_bl31" packages/u-boot-lubancat-rk3588/debian/targets.mk
# 3) 编 u-boot（不要删 build 目录里的 u-boot；缓存行为见下）
sudo ./build.sh -b lubancat-5-v2 -u
# 4) 验证产物：3 个 atf 节点 + 哈希（用编出来的那个 u-boot.itb，或直接核对 prebuilt/ 里的对照件）
tools/check-atf.sh <编出来的 u-boot.itb>
# 5) 上板（不碰 eMMC 的 SD 卡法，或全新镜像）
```

- 实测经验（见 [../build/upstream-sync.md](../build/upstream-sync.md) §2.1/§6）：
  **改了 rkbin 后不需要删 `u-boot` 的构建缓存**（v1.47→v1.53 只改 rkbin + targets.mk，产物从
  1,434,112 B / `fwver: v1.47` 变成 1,372,160 B / `fwver: v1.53`）；
  但**改 `u-boot/` 源码**要考虑同步（`scripts/build-u-boot.sh` 会自动 rsync，且必须用 `-c`）。
- 兼容性：**BL31 与 DDR bin 有版本配套要求**（§3.5），换的时候把 DDR bin 一起核。
- 回滚：`rkbin/` 里保留旧 ELF + `targets.mk` 改回 + 重编；上板用 SD 卡法回滚不需要动 eMMC。
