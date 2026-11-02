# 外部参考：yisding/rock-5b-ysp —— 哪些结论对我们有用

> 上游：<https://github.com/yisding/rock-5b-ysp>（公开仓；本机 checkout 在
> `/home/board/sources/rock-5b-ysp`，29 MB，`main`，最近提交 2026-09-13）。
> 它是 **ROCK 5B(RK3588) 在 Armbian Ubuntu 26.04 上的集成/证据记录仓**，主线恰好是
> 「内核驱动 → vendor 库（MPP/RGA）→ FFmpeg/VA-API → 应用」，**161 条 dated findings**。
> 2026-09-30 用户点名让我看，本文只留"对我们有用"的部分 + 我实测核对的结果。
>
> ⚠️ 它是 **RK3588 通用结论**，但栈不一样（他们跑 mainline/forward-port 6.18 + Ubuntu 26.04；
> 我们跑 Rockchip BSP 6.1.172 + noble）。**版本相关的结论都要按我们的栈复核**——下面
> "我们已经满足"那节就是复核过的。

## TL;DR

- 这是一篇**外部参考仓的交叉对照**（ROCK 5B + Armbian）：哪些结论能直接用（§1）、哪些印证了我们已有的
  结论（§2）、哪些我们已经满足（§3）、哪些是尚未在我们这边验证过的坑（§4）。
- 结论：**没有需要立刻跟进的项**；§6 是从上面提炼的待办候选（按建议顺序）。
- 对方 issue #4 的复现结论见 §7（我们只中一半：**HEVC Main10 硬报错**、非对齐 VP9 不复现）。
- ⚠️ 对方是 Armbian / Ubuntu 26.04，我们是 **Rockchip BSP 6.1** ⇒ 结论不能直接照搬，只能当线索。

## 1. 可以直接用的（按价值排）

### 1.1 板子卡死不是 OOM，是 thrash livelock ⇒ 上 `earlyoom`

外部证据：`findings/2026-07-25-rock5b-zram-thrash-livelock-wedge.md`

- 现象：板子**不会 OOM-kill**（journal 里跨所有 boot 零条 OOM kill），而是 livelock：
  swap 100% 满 ⇒ 唯一可回收的只剩页缓存 ⇒ 反复逐出+重读（实测扫到 **1,800,665 页/s**、
  2,140 major fault/s、load average **114.93**），sar 采样都调度不上 ⇒ **只能断电**。
- 两段机制：① swap 先被"页缓存暴涨"挤满（anon 没涨，`kbanonpg` 稳定）并**整夜钉在 87~98%**
  ⇒ 留下**零 swap 余量**；② **然后一个编译任务到了这台没有余量的板子上** ⇒ 崩。
- 他们的对策：**earlyoom**（该 finding 标 FIX-RUNTIME-VERIFIED）。

⭐ **这跟我们 2026-09-30 那次完全同源**：开发板上 mpv 在软解 4K + 我恢复了 `ninja -j6`
（WPE 构建）⇒ 板子卡到 SSH 连 banner 都交换不完 ⇒ 用户断电重启。
我们**没有 zram**（只有 2 GB swapfile），但"swap 满 → 页缓存反复逐出 → livelock"这条一样成立。

→ **已在开发板落实（2026-09-30）**：装 `earlyoom 1.7-2`（`enabled` + `active`；
`/etc/default/earlyoom` 只有 `-r 3600`，阈值用默认的 `-m 10 -s 10`）。

验证方式（**刻意安全**）——不用默认百分比（那要真把内存吃光才触发，正是我们要避免的状态），
改用绝对值：`-M 4000000`（可用内存 < ~3.9 GiB 算低；当前 6.8 GiB，分配 3 GiB 后 3.8 GiB）
配 `-S 2100000`（可用 swap < ~2.0 GiB 算低；总 2 GiB 且几乎没用 ⇒ 立即满足）。
于是"两个条件同时满足"能在**还剩 3.8 GiB 可用、零 thrash 风险**的状态下复现：

```
· --dryrun： low memory! at or below SIGTERM limits: mem 49.25%, swap 100.14%   ← 检测到
· 真跑（--prefer python3）：分配者被 SIGTERM 杀掉 ✓（内存随即回到 6.9 GiB 可用）
```

⚠️ 阈值语义（`earlyoom --help` 原话）：**"both memory and swap must be below minimum for
earlyoom to act"** —— 我们那次卡死（swap 满 + 内存低）正好命中两个条件。

→ **生产镜像要不要装是单独的镜像决定**（它靠 SIGKILL/SIGTERM 杀进程，HMI 场景要先评估
   它会挑谁；开发板上装它没有这个顾虑，因为那是编译机）。

**参数按他们的实测建议配（不是 Debian 默认）**。他们仓里的
`scripts/rock5b-oom-protection-apply.sh`（GPL-2.0-or-later）给了一套有依据的取值，我们
**只取配置值、没有复制脚本**：

```
EARLYOOM_ARGS="-m 12,6 -s 10,5 -r 900 \
  --avoid ^(sshd|systemd|systemd-journald|dbus-daemon|NetworkManager|tailscaled|init|login)$"
```

- **为什么不是只看 swap**：这块板子 swap 常被钉在 **90~97% 而系统完全健康**（页缓存暴涨把冷
  anon 挤进 zram，之后几小时负载 0.13）—— `docs/gotchas.md` 明说"**swap 停在 90~97% 是常态、
  不是告警**；真正的告警是**可用内存下降"且"swap 满**"。earlyoom 的 AND 语义正好对上。
- **`-r 900`**（不是 Debian 默认的 3600）：他们的教训是 **sar 10 分钟一次的采样在 livelock
  期间被整个丢掉**（01:30/01:40/01:50 三条全没），事后没法复盘；900 s 这条是给下一次留的细粒度记录。
- **故意不设 `--prefer`**：earlyoom 会把匹配进程的 badness **乘 10** ⇒ 偏好编译器会让一个 ~1G 的
  `cc1` 盖过多 GB 的真泄漏源，杀了 make 几秒重生、耗子还在。
- **`--avoid` 的作用**是"挂之前还能连上去"（sshd/systemd/journald/dbus/NM/init/login）。

⚠️ **另一个会"误杀"的是 `systemd-oomd`**：它的默认 `ManagedOOMSwap` **只看 swap**，在这块板子上
会在健康空闲时就触发。他们因此用 earlyoom 而不是 oomd。
**我们已核对**：开发板 `systemctl is-enabled systemd-oomd` → **`not-found`**（没装），
slice 上的 `ManagedOOMSwap=auto` 没有守护进程去执行 ⇒ **我们不中这条**。
（记下来：哪天镜像里被 `ubuntu-desktop` 之类带进来了，要显式配 `ManagedOOMSwap=0`。）

### 1.2 他们的 `rockchip-vaapi` fork 比我们集成的上游新

外部证据：`video-libraries/vaapi/README.md`、`docs/source-trees.md` §4、
`findings/2026-08-04-google-chrome-rockchip-vaapi-green-stable-export.md`

| | 我们（`packages/rockchip-vaapi/`） | 他们 |
| --- | --- | --- |
| 来源 | 上游 **`woodyst/rockchip-vaapi`** @ `08559e8a`（v1.0.12） | **`yisding/rockchip-vaapi`** @ `70f26d95`（fork 自同一个 upstream，upstream pin `e8c64dd`） |
| 编码 | 无（`vainfo` 只有 VLD） | **有**：H.264 Main/High、HEVC Main（opt-in） |
| 10-bit / VP9 P2 | 无 | HEVC Main10、VP9 Profile 2（opt-in） |
| **pre-decode export 稳定化** | **没有** | **有**（见下） |

**那个修复为什么重要**：Chromium 家族**在解码前**就把 VA surface 导出成 NativePixmap 并长期保留；
老驱动当时返回的是全零占位 buffer，解码结果写进另一个对象 ⇒ 应用永远显示那个空 buffer ⇒
**整屏绿**（他们的 finding 标题就叫 green-stable-export）。修法是"首次导出即把该分配标成稳定
外部存储，每帧复制进去"。

→ 对我们：**解码路径不受影响**（我们已实测正常）；但只要哪天让 **Chromium 走 VA-API**，
   就会撞上这个 bug。要换 fork 的话，这是主要理由（其次是编码）。

### 1.3 "FFmpeg 8.x + Rockchip" 已经有人做完了

外部证据：`video-libraries/ffmpeg/README.md`

- 他们编的就是 **`nyanmisaka/ffmpeg-rockchip`**——**和我们板上那份同一个上游**（见
  `notes/build/deb-packages.md` §1.1）；
- 有 `main` / `ffmpeg-80` / **`ffmpeg-81`（移植到 FFmpeg 8.1.2）** 三条规范分支，
  **28 个 `git format-patch` 的 patch series**、`docs/fix-candidates.md`（14 组 rebase 遗留缺陷）、
  以及两个隔离 PPA：`ppa:yi-ding/rock5b-ffmpeg81-upstream`、`ppa:yi-ding/rock5b-ffmpeg81-rockchip`；
- ⚠️ PPA 是 **Ubuntu 26.04 / arm64**，跟我们 noble（libavcodec 60）**ABI 不同**，不能直接装。

→ 对我们：这就是我上次说"要给别的程序提供 rkmpp 得自己编共享库"的**现成源码级答案**
   （patch + 打包脚本可参考）。

### 1.4 Firefox 沙箱的"正规做法"（不是关沙箱）

外部证据：`video-libraries/vaapi/README.md`（Packaging and browser sandbox boundary）、
`packaging/codec-udev/README.md`

- 原文口径：**"Disabling a sandbox is diagnostic only, never a shipping configuration."**
- 正规路子 = 给 **RDD 进程 broker 访问 `/dev/mpp_service`/`/dev/rga`/`/dev/dma_heap/*`**
  **+ 一份窄 ioctl 白名单（seccomp）**，再配 codec udev 规则。

→ 对我们：`notes/system/vaapi-rockchip.md` §3.4 里 `MOZ_DISABLE_RDD_SANDBOX=1` 那段应补一句
   "这是诊断做法，不是出货做法"。（我们目前**没有**出货 Firefox，所以只是口径问题。）

### 1.5 Chromium 其实也能吃 VA-API

外部证据：`docs/app-enablement.md`（图里 Chromium 是 `build-dependent -> vaapi / v4l2`）、
`findings/2026-08-04-google-chrome-rockchip-vaapi-green-stable-export.md`

- **Chrome 151 会加载 `rockchip-vaapi`** 并列出驱动支持的 profile（H.264 Main/High、
  VP9 P0、HEVC Main，16x16~8192x8192）；
- 但同一份 finding 也说明这条路要同时满足：编译进的 backend、解码器策略、
  **解码前导出稳定**、GPU 呈现、GPU 沙箱。

→ 对我们：现在 Chromium 走的是 **V4L2 VDA**（`/etc/chromium.d/default-flags` 的
   `AcceleratedVideoDecoder`）。切 VA-API 是个可选项，但门槛是 §1.2 那个 fork。

## 2. 独立印证了我们已有的结论（不用重查）

- **Firefox 在 RK3588 上唯一可行的硬解路线就是 VA-API**（他们的架构结论，2026-07-21 finding）：
  mainline 是 V4L2 **stateless request API**，Firefox 没有 request-API 支持；而
  **ffmpeg 的 rkmpp 是"命名 codec"、不是 hwaccel，Firefox 没有选它的代码**。
  ⇒ 与我们 `browser-choice-hardware-decode.md` §6.5 完全一致，也堵掉了"让 Firefox 用 ffmpeg 的 rkmpp"这个念头。
- Chromium 自带 **V4L2VideoDecoder**（不走 ffmpeg）——与我们观测一致。
- VLC 需要 `vaDeriveImage` + `vaAcquireBufferHandle` 才算兼容；
  **headless 输出不算证据**——解释了我们测 Ubuntu 的 VLC 为什么连 `--avcodec-hw=vaapi`
  都没有（它的 libavcodec 没编 vaapi）。
- **`apt-mark hold` ffmpeg**：他们也踩了"新版 Ubuntu ffmpeg 会顶掉本地 rkmpp deb"，做法相同。

## 3. 我们已经满足、不用动的（2026-09-30 逐条实测核对）

| 他们的坑（外部出处） | 我们的实测结果 |
| --- | --- |
| `CONFIG_DMABUF_DEBUG`（DMA-BUF debug 的 scatterlist 处理）会**搞坏 rockchip-vaapi**，因为驱动直接调 `DMA_BUF_IOCTL_SYNC`（`findings/2026-07-28-dmabuf-debug-…`、`…-production-kernel-debug-option-audit.md`） | ✅ 板上 `CONFIG_DMABUF_DEBUG` 与 `CONFIG_DMA_API_DEBUG` **都没开**（`/proc/config.gz`，内核 6.1.172） |
| 只给 `/dev/mpp_service` 权限不够：**必须连 `/dev/dma_heap/*` 一起给 `video` 组**，否则编码在 `MppBufferService get_group failed … type 1` 挂掉（`packaging/codec-udev/README.md`） | ✅ vendor 的 `/etc/udev/rules.d/99-rk-device-permissions.rules` 已把 `mpp_service`/`rga` 设成 `root:video 0660`，dma_heap 也是 `root:video 0660`（6 个 heap 全给）；`board` 与 `kiosk` **都在 `video` 组** |
| — | 顺带：我们`overlay/etc/udev/rules.d/` 里**没有**自己的 mpp/rga/dma_heap 规则，靠的是 vendor 默认 ⇒ 如果哪天 vendor 那套变了，要自己补一条（照他们那份写） |

## 4. 要留意的坑（还没在我们这边验证过）

- **Mali-G610 上基于 BLIT 的 texture transfer 不可靠**（`docs/gotchas.md`、`video-libraries/mesa/`）：
  texel 坐标经 **`LD_VAR_IMM` 有损插值**，**整数格式转换会取错 texel**；还有 AFBC 约束
  （compute 不能写 AFBC 目的）。他们给了复现程序 `video-libraries/mesa/reproducers/interp_probe/`。
  → 这是我们那个"**色彩不对**"的一条正经候选假设（mpv 做缩放/格式转换走的就是 blit）。
  ⚠️ 他们的结论建立在**他们那份 Mesa**上，我们是 noble 的 Mesa 25.2.8，要单独验。
- **一堆 RGA/MPP 内核驱动缺陷**（UAF / KASAN / double-free，集中在 `findings/2026-07-1[7-8]-*`、
  `2026-07-2[0-9]-*`、`2026-08-*`）。他们在 6.18 forward-port 上修的；**我们跑 BSP 6.1.172**，
  值得对照看有没有同源问题。其中 **`2026-07-17-mpp-procfs-session-teardown-oops.md`**
  直接对着我们轮询的 `/proc/mpp_service/load`。
- **`docs/gotchas.md` 是整仓 trap 索引**，建议通读一遍再挑（我们只挑了对得上的）。

### 4.1 通读 `docs/gotchas.md` 挑出来的（2026-09-30）

**良性启动噪声（不是故障，别再查）**——`docs/gotchas.md` 的 Runtime 段明确列了这几条：

- **`mpp_platform: client N driver is not ready!`** —— MPP 在枚举**未移植的 legacy codec 块**
  （VPU / VDPU1 / jpeg 之类）。⚠️ **这行我们到处都见**：`ffprobe`、`vainfo`、`gst-inspect`、
  `mpv`、ffmpeg 全都打它，只说明"链接/加载了 librockchip_mpp"，**不代表任何故障**。
- `rkvdec2_init: No niu aclk/hclk reset resource define`（DT 里没有可选的 NIU reset）；
- `failed to init_opp_table` / `failed to add venc devfreq`（DVFS 是第二档、没开）。

**驱动静默失败的一个通用坑（与栈无关，值得记）**：`really_probe()` 里 **`-ENXIO` / `-ENODEV` /
`-EPROBE_DEFER` 走的是 `dev_dbg`**，只有其它错误才 `dev_err` ⇒ **"dmesg 里没有错误"不等于驱动
bind 成功了**（缺中断、match 被拒都是静默的）。判据要用
`/sys/bus/platform/drivers/*/` 下的设备符号链接。
（特例：`platform_get_irq()` 自己走 `dev_err_probe()`，会打出 `IRQ index N not found`。）

**librga 的开源来源**（`docs/gotchas.md` 的 Userspace 段）：官方 `airockchip/librga` **只发预编译
`.so` + 头 + 示例（没有库源码）**，容易误以为闭源；真实现在 `JeffyCN/mirrors:linux-rga-multi` /
`tsukumijima/librga-rockchip` / `madisongh/rockchip-librga` 这条 lineage 里（Apache-2.0）。
⚠️ 他们提醒：**别假设预编译那份带 P010/P210 的修复**。另外 **ffmpeg 的 `rkrga` 是可选的**
——`h264_rkmpp`/`hevc_rkmpp` 不依赖它，只是会丢掉硬件缩放/色彩转换。

**`scale_rkrga` 默认保持宽高比**：`force_original_aspect_ratio` 默认是 `decrease`，
`scale_rkrga=w=640:h=480` 对 16:9 源会得到 **640x360**；要精确尺寸得加
`:force_original_aspect_ratio=disable`。

**DT 相关（我们用的是内核内置 DT，不是 overlay，所以多数不中）**：

- **别名缺失 ⇒ `core_id` 错 ⇒ 崩**：`of_alias_get_id(np, "rkvenc"/"rkvdec")` 必须有
  `aliases { rkvenc0/rkvdec0; }` 才解析得出来。
- **configfs overlay 里的别名会解析到错路径**（`/fragment@0/__overlay__/…` 而不是合并后的节点）
  ⇒ 他们因此走内核内置 DT。**另外：绝不要 `rmdir` 一个正在用的 configfs DT overlay**
  （configfs 死锁 + `rtnl_lock` 卡死，只能重启）。
- **`fdc40000` vs `fdc48000`**：vendor BSP 写 `fdc48000`，TRM/mainline 是 `fdc40000`
  （BSP 那个是 64 KB 窗口里的 `+0x8000` 镜像地址）。**我们板上 `/proc/mpp_service/load` 里
  就同时有 `fdc38100.rkvdec-core` 与 `fdc48100.rkvdec-core`** ⇒ 我们跟着 vendor BSP 走，
  这是地址约定差异，**不是故障**。

## 5. 其他可复用的（启动链 / 平台）

- plymouth/initramfs **启动卡死**：`findings/2026-07-22-rock5b-boot-hang-plymouth-initramfs-daemon.md`、
  `2026-07-23-rock5b-boot-hang-recurred-with-patched-plymouth.md` —— 我们的 `boot-time.md` 那摊活儿相关。
- U-Boot：**FIT dtb race**（`2026-07-13-…`）、**console options**（`2026-08-06-…`）、
  SPI/SD 启动链（`2026-07-07-…`、`2026-07-09-…`）—— 我们改过 bootdelay/Ctrl+C、做过 initramfs 裁剪。
- **崩溃取证**：`ramoops` + `systemd-pstore`（`findings/2026-07-28-…`）——
  注意要看 `systemd-pstore` 的 journal 与 `/var/lib/systemd/pstore/`，`/sys/fs/pstore` 常态是空的。
- **CPU 电压分档 / PVTM**：`2026-07-25-rk3588-cpu-voltage-binning-bsp-vs-mainline.md`、
  `2026-07-27-rk3588-pvtm-volt-sel-measured.md`。
- 打包：**combined(`=y`) 内核与 DKMS 互斥**（`packaging/dkms/`）。

## 6. 待办候选（从上面提炼，按我建议的顺序）

1. `[x]` **开发板装 `earlyoom`**（§1.1）—— 已装 `1.7-2`、按他们的实测建议配置好参数，
   并用绝对阈值做了**安全的**触发验证（检测 + 真杀都验过）。生产镜像要不要装见第 5 项。
2. `[ ]` 评估把 VA-API 驱动换成 **`yisding/rockchip-vaapi`**（§1.2：拿到"解码前导出稳定化"
   与编码）。**目前不急**：我们只走解码、且实测正常；只有要让 Chromium 吃 VA-API 才需要它。
3. `[x]` **通读 `docs/gotchas.md`** 并挑出与我们相关的（§4.1）—— 顺带把那条到处都见、
   一直没人敢确认的 `mpp_platform: client N driver is not ready!` 确认成**良性噪声**。
4. `[ ]` 决定 Chromium 要不要从 V4L2 VDA 切到 VA-API（§1.5，依赖第 2 项）。
5. `[ ]` **镜像级决策**：生产镜像装不装 earlyoom（它会杀进程，HMI 场景要先评估挑谁）、
   要不要自带一条 codec udev 规则（我们现在靠 vendor 默认，见 §3）。

## 7. 复现他们的 issue #4（VP9 绿屏 / HEVC Main10）与相关实验 → 见 [rock-5b-issue4-repro.md](rock-5b-issue4-repro.md)

逐条复现（含"哪条中/不中"、`cma=512M` 试算）**已搬成单独一篇** →
**[rock-5b-issue4-repro.md](rock-5b-issue4-repro.md)**（§7.2 那条 HEVC Main10 仍是被引用的锚点）。

