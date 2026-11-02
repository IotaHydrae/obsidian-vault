# 为什么"别人的 vkmark 6666、我们的 1123"（GPU 基准横比）

> 背景：网上有"RK3588 + 最新版 panvk，vkmark 6666 分"的说法，我们板上 `vkmark` 只有 **1118**；
> 而 `glmark2-wayland` 两边版本相同（2023.01）、分数接近（别人 3571 / 我们 3372）。
> **结论一句话：这两个 vkmark 数字不能直接比** —— 差异按实测权重来自三层：
> **工具版本/运行参数、呈现与分辨率、PanVK 本身**；硬件/频率/散热都没问题。
> GPU 本体（驱动、固件、用户态版本、怎么量占用）见 [gpu.md](gpu.md)。

## TL;DR

- **评分口径两版一样**：评分 = 各场景 FPS 的算术平均、默认 `--size 800x600` + `--present-mode mailbox`；
  差别在 **vkmark 2025.01 重写了窗口系统**（新增 `headless` / `display` 两条绕开合成器的 winsys）——
  我们这版没有。
- **分辨率/呈现模式是数量级变量**：同板同版本 `-b clear`，320x240 → 3908 FPS、800x600 → 1315、
  1080p → 346、`-p fifo` → 59。
- **同参数下 GL 比 Vulkan 快 4.6×**（6120 vs 1334）：GL ≈6120 已经贴近视频里的 6666
  ⇒ "最新版 PanVK 追到 GL 那一档"在数量级上自洽，差距在 Vulkan 栈而不是硬件。
- **记录纪律**：以后任何 GPU 数字都要带
  `vkmark 版本 + winsys + size + present-mode + Mesa 版本 + GPU 频率`，否则不可比。

## 1. 先排除"评分口径"这一层

查过上游源码，**两版的评分公式与默认参数完全一样**：评分 = `total_fps / total_benchmarks`
（各场景 FPS 的算术平均，`src/main_loop.cpp`；1118 正是 13 个场景的平均值，可手算核对），
默认 `--size 800x600`、`--present-mode mailbox`、winsys 自动选（`src/options.cpp`），场景集也相同。

差别在 **2025.01 重写了窗口系统**（上游 `NEWS`）：KMS winsys 改用 `VK_EXT_external_memory_dmabuf` +
显式 format modifier + atomic，**新增 `headless`（`VK_EXT_headless_surface`）与
`display`（`VK_KHR_display`）两条完全绕开合成器的 winsys**，Wayland winsys 改用 xdg-shell。
⇒ **我们这版没有这两条**，跑法本身就受限。

## 2. 呈现模式与分辨率是数量级变量

同板同版本实测（`-b clear` 单场景）：

| 参数 | FPS | FrameTime |
| --- | --- | --- |
| `--size 320x240`（mailbox） | **3908** | 0.256 ms |
| `--size 800x600`（默认，mailbox） | **1315** | 0.760 ms |
| `--size 1920x1080`（mailbox） | **346** | 2.890 ms |
| `-p fifo`（= 垂直同步，800x600） | **59** | 16.949 ms |

- 帧时间 ≈ `0.155 ms（固定开销） + 1.32 µs/kpx`（三点拟合，误差 <5%）。
- `fifo` 直接钉在 59 FPS；`immediate` / `fiforelaxed` 在这个 Wayland surface 上不可用（没出 FPS）。
- ⇒ 只要对方用了 **headless / KMS 直显 + 小分辨率**，分数轻松上 6000，跟驱动好坏无关。

## 3. 驱动：同参数下 GL 与 Vulkan 差 4.6×

| 路径 | FPS | FrameTime | 运行中 GPU |
| --- | --- | --- | --- |
| GL：`glmark2-wayland -b clear -s 800x600`（panfrost gallium） | **6120** | 0.163 ms | 74% @1 GHz |
| Vulkan：`vkmark -b clear -s 800x600`（PanVK） | **1334** | 0.750 ms | **99% @1 GHz** |

- 两者都是"整屏清一遍再呈现"（GL `glClear`+swap；vkmark `vkCmdClearColorImage`+两道 barrier+submit），
  可比性足够；GPU 在 Vulkan 下 **99% 满载** ⇒ **不是 CPU 提交开销**，是 PanVK 在用 GPU 低效地做事。
- 换算有效写带宽：GL 约 **11.8 GB/s**（贴近内存上限），PanVK 约 **2.6 GB/s**（边际 ~3.0 GB/s）。
- ⭐ **6120 几乎就是视频里的 6666** ⇒ "最新版 PanVK 追到了 GL 那一档"在数量级上自洽。
- **为什么 glmark2 反而对得上**：GL 侧是成熟的 panfrost gallium，版本相同、默认 800x600 相同
  ⇒ 3372 vs 3571 只差 5.6%，属环境/温度噪声。**这反证硬件、GPU 频率（全程 1 GHz）、散热都没问题。**

## 4. 还没确认的（别当结论用）

- 对方用的是哪条命令（`--winsys` / `--size` / `-p` / 是否 `--fullscreen`）与哪个 **Mesa 版本**；
  6666 也可能是 `--winsys kms` 直显或 `headless` 跑出来的。
- 我们这版 vkmark 是 Ubuntu 打包的 `2017.08+git20220909`（版本号显示 `2017.08`，
  但**已经用了 xdg-shell**，不是 wl_shell 回退）。
- 内核 panthor 停在**接口 1.5**（厂商 backport，见 [gpu.md](gpu.md) §2）；对方若是 Mesa 25.3+/26.x，
  可能需要更新的 panthor uapi ⇒ **"只升 Mesa"不一定成**。

## 5. 要变成"可比"该怎么做

1. 在板上**从源码编 vkmark 2025.01**，用与对方完全相同的命令跑 —— 这一步把"工具"这一层对齐，
   剩下的差值才是驱动；
2. 拿到对方的 `vulkaninfo --summary` 与完整命令；
3. 试 Mesa 25.3+/26.x 再跑一遍。

⚠️ **记录纪律**：任何 GPU 数字都要带
`vkmark 版本 + winsys + size + present-mode + Mesa 版本 + GPU 频率`，否则不可比。
（这也是 [gpu.md](gpu.md) §7 的待办之一。）
