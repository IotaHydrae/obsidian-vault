# 内核升级可行性：6.1.172 BSP 能不能换 / 换到哪 / 代价多大

> 现状：**Rockchip BSP 6.1.172**（= 上游 v6.1.172 + 厂商 delta；2026-09-30 从 6.1.118 升上来，提交 `e4e9c64cb`）。
> "升级版本号"**在同 6.1.y 内是小事**（已经做过一次），但**跨 BSP 大版本或换主线是"换内核"级别的工程**；
> 而且树里没有上游 git 历史，**"自己往上打 patch"是最不划算的一条路**。

## TL;DR

- 现状 **6.1.172**（`kernel-6.1/Makefile` 的 `SUBLEVEL=172`；板上 `uname -r` = `6.1.172`）。
  版本地图（2026-10-03 实读 kernel.org `releases.json` + 分支 `Makefile`）见 §4.1。
- **同 6.1.y 内换小版本 = 小事**（上游 EmbedFire 有现成提交，照搬即可，我们已照搬过一次）；
  **跨 BSP 大版本或换主线 = "换内核"级别的工程**。
- 厂商 delta 实测规模：**888 个上游文件被改 / 185,035 行 / 新增 12,398 文件**（§2，
  ⚠️ 这组数字是**树还是 6.1.118 时**量的，升到 6.1.172 后**未重测**）。
- 为什么不能自己 rebase：冲突面就是那 888 + 154 个核心文件，且大版本间是**成片编译错误**而不是冲突块（§3）。
- 真要升的路线与动作见 §4；**大版本升级的性能影响面**见 §5（回归清单也在那）。
- ⛔ 因为树里没有上游 git 历史，升级只能"**整体换树 + 逐项回归**"，不能 cherry-pick。

## 0. 结论速览（三条路线）

| 路线 | 可行性 | 代价 | 建议 |
| --- | --- | --- | --- |
| 同 6.1.y stable 小步（**6.1.118 → 6.1.172 已完成**，提交 `e4e9c64cb`；上游 LTS 已 **6.1.188**） | ✅ 已做过 | 低：上游 EmbedFire 有现成提交 `0c8d06b`（见 §4.1），可直接照搬 | ✅ **首选**：想再跟就跟 6.1.y（`6.1.188`）；我们的集成点、DTB/overlay、vendor 驱动都最稳 |
| 换新 BSP 大版本（Rockchip `develop-6.6` / `develop-6.12`） | 有官方/半官方分支 | 大：整树替换 + 我们的集成点重做 + 全量回归 | ⚠️ 需要 EEVDF / mTHP / PREEMPT_RT 这类新特性时走这条，按"换内核"立项 |
| 换 mainline | 能力不够 | 极大 | ❌ 我们的核心功能（MPP 硬编、RGA、ISP/CIF/HDMI-IN）主线没有（§4.2） |

## 1. 现状与这棵树的血统

- 版本：`kernel-6.1/Makefile` → `VERSION=6 / PATCHLEVEL=1 / SUBLEVEL=172`；产物 deb = `linux-image-6.1.172-*`（`apt-mark hold`）。
- **基线**：当初导入时是上游 **v6.1.118**（与 `linux-6.1.118.tar.xz` 逐项对比、`SUBLEVEL`/`NAME` 一致）；
  **2026-09-30 已按 EmbedFire 的 `0c8d06b` 升到 6.1.172**（提交 `e4e9c64cb`，含板级与 SDK 同步）。
- **树里没有上游 git 历史**：`kernel-6.1/.git` 不存在，源码是**整棵提交进本仓库**的
  （`git log -- kernel-6.1` 只有 8 个提交，首次导入 2026-03-24；`git ls-files kernel-6.1` = **91,137** 个文件）
  ⇒ **没法 `git merge linux-6.1.y`**，只能"整棵树替换"或"逐个 patch 手工解冲突"。
- 构建方式：`scripts/build-kernel.sh` 把 `kernel-6.1/` **增量同步**到 `build/linux-rockchip/`（rsync 保 mtime）
  再树外构建 → `make rockchip_linux_defconfig` → `make … bindeb-pkg`。换内核树时这里要动。
- 树里还有我们自己的产物（**不要**算进"厂商改动"）：`compile_commands.json`、`.clangd`、
  `.cache/`（clangd 索引，约 1.8 万个文件）、以及早期树内构建留下的 `*.o`/`include/generated/` 等。
- **这棵树的血统（实测）** = **Armbian `linux-rockchip` 的 `rk-6.1-rkr*` 线（它上游才是 Rockchip BSP）
  + EmbedFire 板级支持**。三条证据：
  1. 树根 `kernel-6.1/README.md` 是 **Armbian 团队**的 rockchip 内核仓库说明（上游 6.1.118 只有 `README`，没有 `README.md`）；
  2. `kernel-6.1/Makefile:868` 的 `#KBUILD_CFLAGS-$(CONFIG_WERROR) += -Werror` 被**注释掉**（Armbian 特征），
     Rockchip `develop-6.1` 那边是打开的；
  3. EmbedFire 把树升到 6.1.172 的那次提交（`0c8d06b`）里的 `Makefile` 与 Armbian `rk-6.1-rkr7.2`
     **0 行差异**，`README.md`/`rk3588s.dtsi`/`rk3588-evb1-lp4-v10.dts` 逐字节相同，而 `rk3588s.dtsi` 与 Rockchip 官方树不同。
- 树里有 **409 个**路径含 `lubancat` 的文件（全在 `arch/arm64/boot/dts/rockchip/**`，含 `overlay/`），
  它们**不在** Rockchip 官方树里（`rk3588-lubancat-5-v2.dts` 在官方树是 **HTTP 404**）。
  ⇒ **换内核时必须把这 ~409 个板级文件 + 相关 defconfig 改动一起迁过去。**

## 2. 厂商到底改了多少（实测，⚠️ 6.1.118 口径）

方法（测量时树是 6.1.118）：把上游 `linux-6.1.118` 解压后建成临时 git 仓库，与本仓库**跟踪的** `kernel-6.1/`
按 **blob 哈希**比对（内容寻址 ⇒ 哈希相同即内容相同），不受 `.cache`/`*.o`/`.dtb` 干扰。脚本见 §6。

| 指标 | 数值 |
| --- | --- |
| 上游 6.1.118 文件数 / 本仓库跟踪的 `kernel-6.1/` 文件数 | 78,753 / **91,137** |
| **被修改的上游文件** | **888**（+148,841 / −36,194 行，合计 **185,035 行**） |
| **厂商新增文件** | **12,398**（`drivers/net` 7,082 = Rockchip WiFi 整包、`arch/arm64` 1,354、`drivers/gpu` 1,348…） |
| 厂商删除/改名 | 14 |
| 其中**核心目录**（非 drivers/arch） | **154**：`Makefile`、`MAINTAINERS`、`fs/Kconfig`、`fs/ext4/super.c`、`fs/pstore/*`、`include/linux` 38、`include/drm/**`、`include/dt-bindings/**`… |

被改文件按目录：`drivers/gpu` 116、`drivers/net` 62、`drivers/usb` 62、`arch/arm` 39、`include/linux` 38、
`arch/arm64` 35、`drivers/media` 35、`sound/soc` 29…。改动量最大的文件：
`rockchip_drm_vop2.c` 16,562 行、`rockchip_drm_vop.c` 5,486、`rockchip_vop2_reg.c` 5,458、
`dw_hdmi-rockchip.c` 4,732、`phy-rockchip-inno-usb2.c` 3,804、`rockchip_i2s_tdm.c` 2,781。

### 2.1 那次 bump（`0c8d06b`：6.1.118 → 6.1.172）具体改了什么（实测）

`git diff --numstat 0c8d06b^ 0c8d06b -- kernel-6.1`：

| 指标 | 数值 |
| --- | --- |
| 变更文件 | **7,918**（修改 7,163 / 新增 489 / 删除 182 / 改名 78） |
| 行数 | **+360,571 / −126,828**（≈48.7 万行） |
| 其中**纯上游 stable** 6.1.118→6.1.172 | 改 6,044 个文件、**+95,110/−48,187 = 143,297 行** |
| **厂商侧**（上游 stable 没碰过的路径） | **2,002 个文件、+258,760/−77,714 ≈ 69%** |

- 厂商侧按目录：`arch/arm64/boot` 720（DTS/overlay 为主）、`drivers/gpu/arm` 209、`drivers/media/platform` 107、
  `drivers/media/i2c` 88（一大批新 sensor）、`drivers/crypto/rockchip` 43、`arch/arm64/configs` 32（defconfig）…
- 最大改动：`mali_csffw.h` +17,666、`vehicle-mcu-bin.h` +8,872、`os08g10.c` +7,671、
  `dw_hdmi_qp-rockchip.c` +5,064（新）、`phy-rockchip-samsung-hdptx.c` +2,609/−1,030、`rk3538.dtsi` +2,670（新 SoC）。
- **一句话分类**：约 1/3 是上游 6.1.119–6.1.172 的 stable 修复；约 2/3 是厂商同步（显示/视频链 VOP2、
  DW-HDMI-QP、Samsung HDPTX PHY；摄像头新 sensor + CIF/ISP；Mali 固件头；新 SoC rk3538；PHY/以太网；
  rockchip crypto），外加删旧驱动；剩下才是野火板级（`arch/arm64/boot/dts/rockchip/**` 里 302 个 `lubancat` 文件）。
- **厂商 delta 规模几乎没变**（都是 vs 上游同版本、按 blob 比）：

  | | 被改的上游文件 | 新增 | 删除/改名 | 其中核心目录 |
  | --- | --- | --- | --- | --- |
  | 我们当时的 6.1.118 | 888 | 12,398 | 14 | 154 |
  | 野火 bump 后的 6.1.172 | **916** | **12,784** | **16** | **156** |

  ⇒ 这次 bump 是"跟着上游 + Rockchip SDK 一起走"，不是"厂商重写"；但也因为同步了较新 SDK，
  带进了新驱动/新 SoC/驱动重构 ⇒ 升级后要重测**显示、摄像头、PHY、GPU**这几条链（§4.4 已做一轮）。

## 3. "自己往上打 patch" 为什么痛

1. **冲突面就是那 888 + 154 个文件**：stable 补丁改的正是 `fs/`、`mm/`、`kernel/`、`include/`、`drivers/net`、
   `drivers/usb` —— 厂商在这 154 个核心文件里都动过手，cherry-pick 几乎必然 conflict。
2. **更常见的是"编译期 API 漂移"**：厂商驱动（VOP2、HDMI、sensor、PHY、WiFi）跟着内核内部 API 走，
   大版本之间 API 一变就是**成片编译错误**，而不是一个冲突块。
3. **典型样本**（`kernel-6.1/init/main.c`）：厂商用 `#ifdef CONFIG_ARCH_ROCKCHIP` 包了一段
   靠 `pr_notice()` 的**返回值**当"打了多少字符"来分段打印 cmdline 的 hack —— 这是个脆弱假设。
   实测 **上游 v6.1～v6.18 都还是原来的单行 `pr_notice(...)`** ⇒ 这段能干净套上；但**主线 master 已用
   正规实现**（`print_kernel_cmdline()` + `CONFIG_CMDLINE_LOG_WRAP_IDEAL_LEN`）⇒ 将来并到那个版本就既冲突又多余。
   同一文件里厂商还塞了 `CONFIG_INITCALL_ASYNC`（并发 initcall，约 180 行）—— 那才是大版本升级时最难搬的东西。
4. 结论：**别在这棵树上手工 rebase**。要新内核就让 Rockchip 去做（`develop-*` 分支就是"把 BSP 移到新
   stable/新大版本"的产物），我们做"**整树替换 + 回归**"。

## 4. 真要升，走哪条、要做什么

### 4.1 Rockchip BSP 分支（推荐）

- 分支：`github.com/rockchip-linux/kernel` 的 `develop-6.1`（我们在用）、`develop-6.6`、`develop-6.12`。
- ⚠️ **版本号的出处与可信度**：这是**读该镜像分支 HEAD 的 `Makefile`** 得到的（`raw.githubusercontent.com`），
  **这个镜像没有版本 tag** ⇒ "6.1.17x"是**分支 HEAD 的快照、会随时间变**；Rockchip 正式 SDK 走自己的渠道，
  GitHub 镜像**可能滞后**。⇒ 读成"这条线跟到哪了"，**不要**当成可 pin 的发布版本号。
  （2026-09-16 实读：`develop-6.1` = 6.1.172、`develop-6.6` = 6.6.89、`develop-6.12` = 6.12.69；
  2026-10-03 复查时 `raw.githubusercontent.com` 取不到，**这三个数未复测**。）
- ✅ **现成的升级来源：EmbedFire 上游仓库里已经有这一步**（`~/example-project/ubuntu-ebf-rk3588`）：

  | | |
  | --- | --- |
  | 提交 | `0c8d06b4965436aa0584f69e42be8f995339fd4c` —— `kernel: bump 6.1 to 6.1.172`（2026-09-11） |
  | 形态 | **单亲提交**（parent = 6.1.118），**不是 merge**：做法是"整棵 `kernel-6.1/` 换掉"（7,918 文件） |
  | 内容来源 | **主体 = Armbian `linux-rockchip` 分支 `rk-6.1-rkr7.2`**（`Makefile` 相同），**不是原样搬**。2026-10-16 按完整 Git tree 与 `a794e888f8eb3feafcf4020f77e968e965426600` 核对：共同路径 84,431 个，其中修改 51 个；野火独有 7,013 个（主要为无线驱动和 DTS），Armbian 独有 368 个。行为问题与复现方法见 [差异审查](armbian-kernel-delta.md)。 |
  | 板级处理 | 这次提交动了 **302 个 `lubancat` 文件**，bump 后树里仍有 391 个 ⇒ 板级是"在新树上重放"的 |
  | **非内核改动（别漏）** | 还改了 **5 个 `config/uEnv/*.uEnv`** + **1 个 `packages/arm64/camera_engine_rkaiq_rk3588_arm64.deb`**。uEnv 实质改动：`-hdmi0-overlay` → **`-hdmi0-vp0-overlay`**、`overlays=` 新增 **`dp0-vp1-overlay`**、注释按新 VP 规则重写。那个 deb 从 `oid 296a88bd…`(2,829,076 B) 换成 `oid 3b277443…`(2,861,064 B)，版本号仍 `6.30.4` |
  | 对我们 | 导入前的 `kernel-6.1/` 与 `0c8d06b^` 的 **91,137 个跟踪文件逐条完全相同**。本工程 `e4e9c64cb` 已导入这份内核；相对野火内核快照另有 `.clangd`、自研 `rk3588-lsp-evb.dts` 及 DTS Makefile 注册，所有驱动 blob 相同。`compile_commands.json` 是按需生成的本地产物，不计入跟踪文件差异。 |

- ⚠️ **别直接把 Rockchip 的分支当成我们的内核**：我们的树 = Rockchip BSP + **EmbedFire 板级支持**（§1）。
  "最优一步"的正确说法是：**换到 EmbedFire 发布的新 BSP 内核树**（最省事），或者
  **取 Rockchip 新树 + 迁移那 ~409 个板级 DTS/overlay 与 defconfig 改动**。
- **上游侧的版本水位（2026-10-03 实读 kernel.org `releases.json`）**：
  latest stable **7.2.8**、mainline **7.3-rc5**、longterm **6.18.54 / 6.12.111 / 6.6.157 / 6.1.188**。
  **6.1 仍是受支持的 longterm** ⇒ "留在 6.1 但跟上 6.1.y（现在差到 6.1.188）"完全站得住。
  （SDK 侧的 U-Boot/ATF/rkbin 也要一起看版本匹配。）

### 4.2 mainline 能力盘点（按主线源码实测）

| 能力 | 主线现状 |
| --- | --- |
| 显示（VOP2 / HDMI TX） | ✅ 有 |
| NPU | ✅ **v6.18 起**（`drivers/accel/rocket`） |
| 视频**解码**（H.264/HEVC，VDPU381/383） | ⚠️ **仅 master（未发布，预计 6.19）** |
| 视频**编码**（VEPU/venc） | ❌ 没有 |
| RGA（2D，RK3588 的 RGA3） | ❌ 没有（主线 `rockchip/rga` 只覆盖老 RGA 的 v4l2 路径） |
| ISP / CIF / HDMI-IN | ❌ 没有（主线只有老的 `rkisp1`） |
| MPP 用户态 / `librga` / RKNN | ❌ 不在主线（厂商栈） |

⇒ 我们镜像的卖点（Chromium RKMPP 硬解、三路相机录制、RGA）**换主线直接丢** ⇒ mainline 只适合做实验/评估。

### 4.3 替换内核时的动作清单

1. 取新 BSP 的整棵树：替换 `kernel-6.1/`（或新增 `kernel-6.6/` 之类目录）。
2. 改 `scripts/build-kernel.sh` 里同步 `kernel-6.1/` 那段；确认新树的 defconfig 名（现在写死
   `rockchip_linux_defconfig`）与 `KBUILD_IMAGE`。
3. 重新出内核 deb（`./build.sh -b lubancat-5-v2 -k`）；镜像构建脚本按 `linux-image-*` 通配取包、不写死版本号。
4. 重新生成 clangd 索引（`tools/gen-compile-commands.sh kernel`），`kernel-6.1/.clangd` 的 `Remove` 三条按新树复核。
5. **回归清单**（都在板子上跑）：启动（dtbs/overlay/`overlays=`）、`tests/browser-codec.sh`（RKMPP 硬解）、
   三路相机录制、`tests/rga-bench.sh`、USB/Type-C 角色、`tools/verify-image.sh` 全套。

### 4.4 实测：6.1.172 已上板回归（2026-09-16 首次；2026-09-30 提交）

流程：**服务器交叉编译出 deb → 板子 `dpkg -i` → 重启 → 回归**（24 核服务器约 3 分钟出包）：

- 产物：`linux-image-6.1.172_6.1.172-1_arm64.deb`（38.9 MB，含 **1995 个 `.ko`**）+ headers（8.9 MB）；
  板上 `dpkg -i` → `zz-update-firmware` 重建 `/boot/firmware/dtbs/`（overlay 257 → 323 个）→ 重启 →
  `uname -r` = **6.1.172** ✓。
- ⛔ **overlay 改名（升级时必须跟着改，否则会静默丢掉全部 overlay）**：新 BSP 把
  `rk3588-lubancat-5-hdmi0-overlay` 改成 **`…-hdmi0-vp0-overlay`**（反编译对比：3 个 fragment 逐条相同，纯改名），
  另新增 `hdmi1-vp0`/`hdmi-rx`。原因在 U-Boot 的 `boot.cmd`：**任一名字找不到就 `overlay_error=true`
  → 最后重新加载原始 DTB、把全部 overlay 丢掉**（连带 `usbcon-host` 失效、PD 退回 sink）。
  `config/uEnv/lubancat-5-v2.uEnv` 已同步改成新名字。
- 回归结果：

  | 项 | 结果 |
  | --- | --- |
  | `tests/rga-bench.sh` | **39 通过 / 0 失败**；4K copy 2827 MP/s、rga2 resize 1348 MP/s（与 6.1.118 的 2761/1246 基本一致） |
  | `tests/browser-codec.sh`（Chromium + RKMPP） | **19 通过 / 0 失败**；h264 1080p 硬解 35.3 fps，GPU 13% + 渲染 8% |
  | MPP / VPU | `mpp_service`/`mpp_vepu2`/`mpp_rkvdec2`/`mpp_rkvenc2` 全部 probe 成功，`/dev/video-dec0`、`/dev/video-enc0` 在 |
  | GPU | 内核侧 **panthor**（`card1`），Mesa 25.2.8 报 `Mali-G610 (Panfrost)` |
  | overlay 判据 | `try-power-role=source`、HDMI0/HDMI1 `okay`、Type-C `data_role=[host]`/`power_role=[source]` ✓ |
  | `tools/verify-image.sh` | 185 通过 / 1 失败 / 9 跳过（唯一失败是 `wireless-regdb`，旧 rootfs 已知项、与内核无关） |
  | 温度 / 网络 | 38.8 °C；WiFi 仍走 `p2p0` 连 `example-project` ✓ |
- **第二轮**（把 bump 的非内核部分也补上）：同步两份 uEnv（含新增 `dp0-vp1`）+ 换 rkaiq deb 后重启，
  **4 个 overlay 全部生效**（`try-power-role=source`、两个 HDMI `okay`、**`dp@fde50000` = okay**、`dp@fde60000` disabled），
  DRM 出现 `card0-DP-1`（disconnected）、新驱动 `dw-hdmi-qp-hdcp` probe success、`camera-engine-rkaiq 6.30.4` 已更新。
  ⇒ **内核 + uEnv + rkaiq 三部分都合进来并验证过**。
- **新增的 dmesg 警告**（非致命、未处理）：`rockchip-thermal … Missing rockchip,grf property`、
  `dw-pcie fe190000.pcie: invalid resource / Failed to initialize host`、`fiq_debugger … IRQ fiq not found`、
  `debugfs: Directory 'vcc_5v0' … already present!`、`Early cacheinfo failed, ret = -22`。
- **回滚办法**（板子上备份在 `/root/kernel-backup-6.1.118/`）：把 `/boot/vmlinuz-6.1.118`、
  `initrd.img-6.1.118`、备份的 `firmware/dtbs/.` **和 `ubuntuEnv.txt`** 一起拷回 `/boot/firmware/` 再重启
  —— ⛔ `ubuntuEnv.txt` 必须一起回滚（旧 dtbs 里没有 `-hdmi0-vp0-overlay` 这个名字）。

## 5. 大版本升级的性能影响面（概览，不逐行读源码）

前提：这几代内核的性能改动几乎都是**渐进 + 强场景相关**的，没有哪版能让板子"快一倍"。对我们影响最大的
往往不是内核本身，而是**厂商 BSP 能不能把 VOP2 / MPP / RGA / WiFi / ISP 这些 vendor 驱动带过来**
（带不过来 = 功能没了，性能无从谈起）。下面只列"大致会动到哪"：

| 子系统 | 6.1 之后的代表性变化 | 对**我们这台板子用法**的预期影响 |
| --- | --- | --- |
| **调度器** | **EEVDF** 取代 CFS（6.6 起）；`sched_ext`（6.12+）；**PREEMPT_RT 主线化**（6.12）；PELT/`uclamp`/EAS 持续修 | 影响最大的是**抖动与尾延迟**：三路 GStreamer + ROS2 这类混合负载的帧间隔稳定性会变；大小核选核/负载均衡行为变了，`lsp-rk-monitor.sh` 看到的每核占用会不一样；要 RT 不必再打 patch |
| **内存管理** | folio 化持续；**mTHP**（6.7 引入、逐步默认化）；swap/zswap；MGLRU | 页缓存/匿名内存开销下降 ⇒ 大文件读写、`apt` 解包顺一点；⚠️ **大页与"物理页必须 <4 GB"是交互的**（RGA2 的隐患面）⇒ 升级后**必须**用 `tests/rga-v4l2-probe.sh` 复测 |
| **块层 / 文件系统** | io_uring 大改；writeback/iocost；**ext4 转 iomap**（6.13+ 分批）；erofs/f2fs 改进 | 顺序写路径（MP4 录制 ~5.4 MB/s、写卡、镜像解压）的吞吐/延迟会有变化；历史上这类重构出现过回归 ⇒ **只看数字不看版本号** |
| **网络** | page-pool→netmem、Big TCP、`SO_REUSEPORT` 扩展；mac80211/cfg80211 演进 | 管理口（`p2p0`）带宽基本不变；真正的门槛是**厂商 rtw88/8822ce 驱动能不能编过** |
| **图形 / 多媒体** | DRM `dma-fence`/scheduler/`dma-buf` 重构；Mali `panfrost`→`panthor`；`dma-heap` 行为调整 | GPU/显示的同步效率与稳定性；**但更关键的是 vendor 的 MPP/RGA/ISP/rk_hdmirx 还能不能用**；`dma-heap` 变化会间接影响 RGA2 的 <4G 约束 |
| **电源 / 热 / 实时** | `cpufreq`/`cpuidle`/`thermal`/EM 改进；suspend/resume | 待机功耗可能降、唤醒更快（对"随时要用"的设备是好事）；EM/OPP 数据来自 vendor DTS，**必须和新内核匹配**，否则调频/功耗跑偏 |
| **加固与新默认** | CFI、RANDSTRUCT、`init_on_alloc/free` 等与默认策略变化 | 新 vendor defconfig 若顺手打开，会有**个位数百分比**开销 —— "升级后感觉变慢"最常见的原因之一，先 diff `.config` |

**优先级（按投入产出）**：

1. **跟上 6.1.y**（我们已是 6.1.172，LTS 到 **6.1.188**）：最大收益来自 stable 里的性能与回归修复，
   同 major.minor ⇒ 集成点、DTB/overlay、vendor 驱动全都最稳。**留在 6.1 不丢人。**
2. **跳 6.6 / 6.12**：只有出现明确诉求时才值（"三路相机抖动要更低（PREEMPT_RT）"、"内存密集场景要 mTHP"、
   "要 EEVDF 的延迟特性"）。代价是整棵 vendor 树换代 + 全量回归。
3. **7.x mainline**：性能讨论没意义（硬编/RGA/ISP/HDMI-IN 都没有）。
4. 无论走哪条，**用现成尺子量**：`tests/rga-bench.sh`、`tests/browser-codec.sh`、
   `tests/opt-flags-bench.sh`、`tools/lsp-rk-monitor.sh`，写盘用 `dd`/`fio`，要评估调度抖动上 `cyclictest`/`hackbench`。

## 6. 复现本节数字

```bash
# 1) 上游 6.1.118
mkdir -p /tmp/kc && cd /tmp/kc
curl -sSLO https://mirrors.edge.kernel.org/pub/linux/kernel/v6.x/linux-6.1.118.tar.xz
tar -xf linux-6.1.118.tar.xz && mv linux-6.1.118 up
cd up && git init -q . && git add -A && git -c user.email=a@b -c user.name=a commit -qm up
git ls-files -s | awk -F'\t' '{split($1,a," "); print $2"\t"a[2]}' > /tmp/kc/up.txt

# 2) 本仓库跟踪的 vendor 源码（不含构建产物/缓存）
git -C <repo> ls-files -s -- kernel-6.1 \
  | awk -F'\t' '{split($1,a," "); p=$2; sub(/^kernel-6.1\//,"",p); print p"\t"a[2]}' > /tmp/kc/ven.txt

# 3) 比 blob 哈希：同路径不同哈希 = 被改；只在 ven = 新增；只在 up = 删除
#    （§2 的 888 / 12,398 / 14 与 185,035 行就是这样得到的）
```

> ⚠️ 不要用 `diff -rq kernel-6.1 <upstream>` 直接统计：本地的 `.cache/`（clangd 索引 1.8 万文件）、
> `scripts/dtc/include-prefixes/`（构建生成的符号链接农场）、`*.dtb`/`*.o`/`include/generated` 等
> 都会混进"厂商新增"，把数字抬高好几千。
