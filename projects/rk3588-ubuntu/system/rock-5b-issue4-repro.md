# 复现上游 issue #4（VP9 绿屏 / HEVC Main10）与相关实验

> 本文是 [rock-5b-ysp-crossref.md](rock-5b-ysp-crossref.md) 的**实验篇**：把上游仓库报的 issue #4
> 在我们的板子上逐条复现（哪条中、哪条不中、为什么），以及由此提炼的待办，
> 外加一次 `cma=512M` 的试算。
> 上游那份资料里"能直接用的 / 印证我们结论的 / 要留意的坑 / 待办候选"见主篇 §1~§6。
> ⚠️ 正文**原样搬自主篇的 §7**（节号保留，便于对照引用）。

## TL;DR

- issue #4 的两条 finding：**非对齐宽度 VP9 绿屏我们不中** ✅；**HEVC Main10 我们中，而且更硬** ❌。
- 另一条与我们相关的：上游把问题绑在 6.18 内核上（我们用 6.1 BSP）⇒ 结论不能直接照搬。
- `cma=512M`：能用，但**我们这儿没有可观测收益**。


## 7. 复现他们的 issue #4（2026-09-30）：**我们中一半**

议题：<https://github.com/yisding/rock-5b-ysp/issues/4>（2026-09-02 由 `defcom5-rockchip` 提，
至今 0 回复）。**他的测试台是「Orange Pi 5B (RK3588S) + ubuntu-rockchip BSP 6.1 + noble」**
—— 跟我们的血缘几乎一样，所以值得实测。议题内容：两条硬件验证过的绿屏 finding + 一个问题。

我们用**自己造的素材**复现（板上 GStreamer 有 `vp9enc` / `x265enc` / `videotestsrc`，不需要外网）：

```bash
# 非对齐宽度 VP9（复现他给的 2970，Profile 0 / yuv420p）
gst-launch-1.0 videotestsrc num-buffers=300 pattern=smpte \
  ! video/x-raw,format=I420,width=2970,height=664,framerate=30/1 \
  ! vp9enc ! webmmux ! filesink location=/tmp/vp9-u10.webm
# HEVC Main10（他为 finding #2 的那类）
gst-launch-1.0 videotestsrc num-buffers=300 pattern=smpte \
  ! video/x-raw,format=I420_10LE,width=1280,height=704,framerate=30/1 \
  ! x265enc tune=zerolatency speed-preset=ultrafast ! h265parse ! matroskamux ! filesink location=/tmp/hevc10.mkv
```

⛔ 造素材必须**显式写 `format=I420`**：不写的话 `videotestsrc` 给 `yuv444p12`，`vp9enc` 会编成
12-bit 4:4:4，走 VA-API 时直接触发 ffmpeg 断言崩（那是另一回事，别当成被测对象）。

### 7.1 finding #1（非对齐宽度 VP9 绿屏）：**我们不复现** ✅

他的断言：宽度不是 64 对齐的 VP9（2970，mod16=10、mod64=26）经 VA-API **整屏绿**，
根因是导出 stride 只做了 **16 对齐**，而 Mali/EGL 导入侧要 **64 对齐**。

我们的结果（Firefox 156 + 我们那份驱动，`file://` 播 10 s 素材、**播放中**采样）：

```
VPU：fdc38100/fdc48100.rkvdec-core utilization ≈ 1.7%  ← 确实在硬解
画面：SMPTE 彩条完全正确（中央均值 RGB 117/121/117 = 彩条平均色；绿屏会是 ~0/13x/0）
```

⚠️ 有效信息：**我们驱动的 stride 也是 16 对齐**（上游 v1.0.12 源码原话
`/* image stride matches rk_CreateImage: (width+15)&~15 */`）⇒ 差别不在导出侧，而在**导入侧**：
他那边是 **panfork Mesa 23**，我们是 **Mesa 25.2.8（mainline panthor）**。
⇒ **新增一条"别乱动 Mesa 版本"的理由**（Mesa 一退/一换，这类绿屏可能就回来了）。

### 7.2 finding #2（HEVC Main10）：**我们中，而且更硬** ❌

原文：Main10/HDR10 软解与 MPP 直出都正确、**VA-API 导出绿**（NV15 解 / P010 导出布局不一致）。
他的修复线索是他们 fork 的 `5efe7d4`/`df14bb6`；他们的能力矩阵也把 Main10 标成 **Opt-in**。

我们这边**更硬**——不是绿，是直接报错。三路对照（同一份 HEVC Main10 素材）：

| 路径 | 结果 |
| --- | --- |
| 软解（`mpv --hwdec=no`） | **正常**（无报错）|
| 绕过 VA-API 直连 MPP（`ffmpeg -c:v hevc_rkmpp -i … -f null -`） | **正常**（rc=0，无解码错）|
| **走 VA-API**（`mpv --hwdec=auto-safe`） | **`[ffmpeg] AVHWFramesContext: Failed to sync surface 0x3000013: 23 (internal decoding error)`** + `Error while decoding frame (hardware decoding)!`，且 `rkvdec` 全程 **0%** |

⇒ 失败隔离在 **VA-API 的 Main10 路径**，机制与他的 finding #2 一致。

**更要紧的一点**：我们的驱动 **`vainfo` 里把 `VAProfileHEVCMain10` 当成可用能力 advertise 出去**了
—— 应用会选中它然后失败（而不是干净地退回软解）。ysp fork 把 Main10 定为 **Opt-in（默认不 advertise）**
正是这个道理：**"只 advertise MPP 能真正履约的那些能力"**。

### 7.3 他的问题（ysp13 是否绑 6.18 内核）：与我们直接相关 ⚠️

他把 ysp 的 PPA 源码（librga git20260725、mpp git20260805、ysp13）**backport 到 noble/BSP-6.1**
后：能编、但**所有硬件解码都回退软解**，症状就是那行
`mpp_platform: client 12 driver is not ready!` —— 而那行我们**到处都见**、而且我们这边解码是好的
（他们自己也把它归为"良性噪声"，见 [rock-5b-ysp-crossref.md](rock-5b-ysp-crossref.md) §4.1）。

⇒ **对我们最实用的一条**：**别轻率把 `librockchip-mpp1` / `librga2` 升到更新的 git 快照**。
我们现在的组合（mpp `git20260811` + librga 2.2.0 + 我们那份 vaapi 驱动）**硬解是通的**；
升级前必须先按这个症状验一遍（`vainfo` + 一个真实素材的 `rkvdec>0`）。

### 7.4 待办（从 7.1–7.3 提炼）

1. `[x]` **把 Main10 从 advertise 里去掉**（fail-closed）—— **2026-10-02 用户拍板做了**：
   `packages/rockchip-vaapi/0001-no-hevc-main10-advertise.patch`，deb 版本 `1.0.12-1+lsp1`。
   板上实测：打之前是 `Failed to sync surface … internal decoding error`（rkvdec 0%）；
   打之后只剩一行 `hevc: Failed setup for format vaapi: hwaccel initialisation returned error.`
   然后 **rc=0 干净退回软解**；4K30 H.264 硬解不回归（`tests/media-hardware-decode.sh` 5 通过/0 失败）。
2. `[ ]` 评估换 `yisding/rockchip-vaapi`（[rock-5b-ysp-crossref.md](rock-5b-ysp-crossref.md) §1.2）时，把 **Main10 与 64 对齐**这两条一起当作验收项。
3. `[ ]` **不要动 Mesa / mpp / librga 的版本**，除非按 §7.1/§7.3 的判据重验。
4. `[ ]` （可选）把我们这两条结果回帖给那个 issue —— 他们明确说想要"RK3588S/noble 之外的覆盖"，
   而我们正好提供：**同血缘的 BSP-6.1/noble 上，VP9 非对齐宽度用 Mesa 25.2.8 不复现、
   但 Main10 是硬报错**。**回帖是对外动作，得用户点头才做。**

### 7.5 试 `cma=512M`（2026-10-02）：**能用，但我们这儿没有可观测收益**

起因：fork 的 `INSTALL.md` 写"4K 需要 `cma=512M`，默认 256M 时 Firefox 播 4K 几秒后
`NS_ERROR_DOM_MEDIA_FATAL_ERR` 掉回软解"，而我们板子正好是默认的 **256 MB**
（`CmaTotal: 262144 kB`，cmdline 里没有 `cma=`）。

**怎么改**：内核 cmdline 来自 U-Boot，板上的落点是 **`/boot/firmware/ubuntuEnv.txt` 的
`bootargs=`**（`sed -i 's|^bootargs=|bootargs=cma=512M |'`，然后 `systemctl reboot`）。
改完核对：`grep -o cma=[0-9A-Za-z]* /proc/cmdline` → `cma=512M`，
`CmaTotal: 524288 kB` ✓，kiosk 正常起，**可用内存没少**（CMA 是可回收的，7930 MB 里 available 6987 MB）。

**A/B 结果（Firefox + VA-API 播 4K30 素材，逐秒采 `rkvdec`）**：

| CMA | 解码期 rkvdec（每秒） | `NS_ERROR_DOM_MEDIA_FATAL_ERR` | `CmaFree` |
| --- | --- | --- | --- |
| **512 M** | 稳定 8.8~10.4% | **0** | 517548 kB |
| **256 M** | 稳定 8.9~10.8% | **0** | 255404 kB |

⇒ **完全一样，没有可观测差别**。再看 CMA 到底用了多少（256M 那次）：

- Firefox 4K 播放全程：`CmaFree` 262144 → **255404 kB**，只用了 **~6.7 MB**；
- 换 **MPP 直连**（`ffmpeg -c:v h264_rkmpp` 全速解同一个 4K，rkvdec 38~42%）：
  `CmaFree` 只从 255404 掉到 **253428 kB**（~2 MB）。

**为什么我们不吃 CMA**：fork 的 INSTALL.md 自己写了 —— "**driver 自己的 per-surface buffer
来自 system dma-heap（不是 CMA）**，CMA 压力来自 MPP 的解码 DPB + GPU 合成器"。
我们两条路径的缓冲都走 dma-heap，所以 256 MB 远远够。

**结论：维持 256M（已回退）**。512M 无害但也无益，按"没证据就不改"的口径不动 cmdline。
（真要上，判据在这里：拿一个"4K 播 >20 s + 逐秒采 rkvdec + 数 `NS_ERROR_DOM_MEDIA_FATAL_ERR`"
 的用例做 A/B，看不到差别就别改。）
