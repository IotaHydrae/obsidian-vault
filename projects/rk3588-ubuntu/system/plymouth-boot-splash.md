# 开机画面（Plymouth）：文件规格、替换方式与校验

> 本文只讲**开机画面那一个 logo 位**：引擎前提（素材在≠引擎在）、`bgrt`/`spinner` 的生效机制、
> 每个文件的作用与规格、多分辨率怎么定尺寸、drop-in 替换方式、校验命令与位置微调。
> 三处 logo 位的总览、桌面壁纸/锁屏、按 ESC 看日志、以及"定制没生效"的排查顺序见
> [branding-customization.md](branding-customization.md)。
> ⚠️ 正文**原样搬自该篇的 §1**（节号保留）。

## TL;DR

- ⚠️ **素材在 ≠ 引擎在**：`plymouth` 包/`plymouthd` 不在时，图放对了也不会显示（§1.0 有踩坑记录）。
- 生效机制：默认主题名是 `bgrt`，但**资源实际来自 `spinner` 目录**（§1.1）。
- 替换用 **drop-in**（`overlay/usr/share/plymouth/...`），改完要重编镜像或按 §1.4 的路径手放；
  校验命令与位置微调见 §1.5/§1.6。


## 1. 开机画面（Plymouth）

### 1.0 ⚠️ 前提：引擎必须真的在（踩过的坑：素材在、引擎不在）

**症状**：开机没有 spinner，取而代之的是一屏内核日志。

**容易误判的地方**：`/proc/cmdline` 里 `quiet splash plymouth.ignore-serial-consoles` **完全正常**，
`/usr/share/plymouth/themes/spinner/` 目录也**存在**（里面还有我们换过的 `watermark.png`）——
看起来一切就绪。但那个目录是**构建脚本自己 `mkdir -p` 出来的**，不等于主题包装了。

**真正的原因**：镜像里**从来没装过 `plymouth` 包**。它在本镜像里只作为别的包的
Recommends 出现，而我们全程 `--no-install-recommends`，于是被丢掉。
构建脚本会拷素材、设 `update-alternatives`、写 initramfs hook，
**但从来没有人安装渲染引擎本体**。验证：

```bash
systemctl is-enabled plymouth-start.service   # → not-found（单元不存在 = 包没装）
dpkg -l plymouth                              # → un（从未安装，不是被删）
command -v plymouthd                          # → 空
```

对照 `/proc/cmdline`：`quiet`/`splash` 都在 → **缺的不是参数，是引擎**。

**包的分工**（`dpkg-deb -c` 实测）：

| 包 | 内容 |
| --- | --- |
| `plymouth` | `plymouthd`、`plymouth-start.service`，以及 `themes/spinner/watermark.png` |
| **`plymouth-theme-spinner`** | **`themes/bgrt/bgrt.plymouth`**（`PLYMOUTH_THEME` 默认值就是 bgrt）+ 完整的 `themes/spinner/`（`spinner.plymouth`、`throbber.svg`、`animation-*.png`） |
| `plymouth-themes` | 额外的 fade-in / glow / script / solar / spinfinity —— **本镜像不需要** |

所以修复是在 `BASE_PACKAGES` 里显式加 `plymouth plymouth-theme-spinner`
（后者硬依赖 `plymouth` + `plymouth-label`；注意它 `Breaks/Replaces: plymouth-themes`，
别把 `plymouth-themes` 也加进来）。`tools/verify-image.sh` 的「开机画面（plymouth）」组会核对：
`plymouthd` 存在、`plymouth-start.service` 存在、bgrt 主题存在、`spinner.plymouth` 存在、
`default.plymouth` 指向有效主题、initramfs hook 就位。

> 教训：判断"某功能有没有生效"，**要检查功能的执行者，而不是它的素材**。
> 素材是构建脚本从 overlay 拷进去的，包没装照样拷得进去。

### 1.1 生效机制：默认主题是 `bgrt`，但资源来自 `spinner` 目录

`plymouth-theme-spinner` 的 postinst 注册的默认主题是 **`bgrt`**（不是 `spinner`）：

```sh
update-alternatives --install /usr/share/plymouth/themes/default.plymouth default.plymouth \
    /usr/share/plymouth/themes/bgrt/bgrt.plymouth 110
```

而 `themes/bgrt/` 目录里**只有 `bgrt.plymouth` 一个文件、没有任何图片**（包文件清单已确认），
图片资源全在 `themes/spinner/`。所以**替换 `themes/spinner/` 下的图，在默认主题下同样生效**。

构建脚本里用 `PLYMOUTH_THEME`（默认 `bgrt`）显式固定主题，避免将来包默认值变化导致定制静默失效。

### 1.2 各文件的作用与规格

| 文件 | 作用 | 绘制方式 | 尺寸/格式 |
| --- | --- | --- | --- |
| `themes/spinner/bgrt-fallback.png` | 启动画面**背景** | **居中**绘制，垂直中心固定在屏幕 38.2% 处 | PNG/RGBA；**高度不得超过屏幕高的 76.4%**（见 1.3）|
| `themes/spinner/watermark.png` | **顶部的 logo 叠加层** | 按**原始像素 1:1** 绘制 | PNG/RGBA；原生约 200×100 |
| `themes/spinner/throbber-*.png`、`animation-*.png` | 转圈/结束动画 | 序列帧 | PNG；可选替换 |

**背景是居中绘制的证据**（上游 two-step 插件 `view_set_bgrt_fallback_background`）：

```c
x_offset = (screen_width - width) / 2;
y_offset = screen_height * 382 / 1000 - height / 2;   /* 黄金分割点 */
```

也就是说当前 618×420 的背景在 1080p 下的效果是「**黑底上居中一个小色块**」，不是整屏背景。
要满屏就得按屏幕分辨率出图（如 1920×1080）。

**背景图不能做成整屏尺寸（重要）**

背景的垂直中心被固定在屏幕高度的 **38.2%**（黄金分割）处：

```c
x_offset = (screen_width  - width) / 2;
y_offset = screen_height * 382 / 1000 - height / 2;   /* 38.2% */
```

于是「完整可见」的条件是 `height <= 0.764 * screen_height`。姐妹函数（BGRT 分支）在绘制前还有
`if (x_offset >= 0 && y_offset >= 0)` 守卫 —— 图太大时偏移为负，**背景直接不绘制（只剩黑屏）**。
按这个公式算出来的边界：

| 分辨率 | 宽度上限 | 高度上限 | 618×420（仓库现有）| 1236×840（现有 ×2）| 整屏尺寸 |
| --- | --- | --- | --- | --- | --- |
| 1920×1080 | 1920 | **825** | ✓ y_off=202 | ✗ y_off=−8 | ✗ y_off=−128 |
| 3840×2160 | 3840 | **1650** | ✓ y_off=615 | ✓ y_off=405 | ✗ y_off=−255 |

注意 1080p 下把现有背景放大一倍就已经踩线了。所以：

- 想「居中放个 logo/图形」→ 保持小幅图（Ubuntu 原生的 fallback 也是一张小图）
- 想「横向铺满」→ 用 `屏宽 × 0.764×屏高`（4K 即 3840×1650），上下留黑边
- **想要真正满屏背景，这个主题做不到** —— 它是「黄金分割处居中放图」的设计，不是壁纸引擎；
  真要满屏得自己写 script 主题（超出本文范围）

### 多分辨率共存：按「最小屏」定尺寸

如果镜像可能用在多种分辨率的屏上（如 1080p / 2K / 4K），**分辨率敏感的素材必须按最小的那块屏算**：

| 素材 | 为什么敏感 | 按哪块屏算 | 按最大屏算的后果 |
| --- | --- | --- | --- |
| `background` | 高了会算出负偏移 → **完全不绘制（黑屏）** | **最小屏** | 插到小屏上开机背景消失 |
| `watermark` | 按原始像素 1:1 绘制、不缩放 → 比例随屏幕变 | **最小屏** | 插到小屏上 logo 占满大半屏（40%）|
| `wallpaper` / `lockscreen` | GNOME 会自行缩放 | **最大屏** | 按小屏算的话大屏上会被放大而糊 |

工具已经按这个原则分成两套参考分辨率（都有默认值，可用 `--screen` / `--max-screen` 覆盖）：

```bash
# 最小屏（默认 1920x1080）：background / watermark 按它算
# 最大屏（默认 3840x2160）：wallpaper / lockscreen 按它算
./tools/make-assets.sh check     # 检查现有素材在目标区间内是否可用
```

`check` 会逐项给出结论，例如：

```
background  2917x1641 RGB
            ✗ 超出最小屏上限（高 ≤825，宽 ≤1920）→ 在 1920x1080 上会算出负偏移而【不绘制】
watermark   768x768 RGBA
            · 占屏宽：最小屏 40%，最大屏 20%
            ✗ 在小屏上偏大（>35%）
```

按最小屏算出来的背景尺寸与三种屏的可见性（16:9 源）：

| 屏幕 | 背景尺寸 | x_off | y_off | 结果 |
| --- | --- | --- | --- | --- |
| 1920×1080 | 1420×799 | 250 | 13 | ✓ |
| 2560×1440 | 1893×1065 | 333 | 18 | ✓ |
| 3840×2160 | 2840×1598 | 500 | 26 | ✓ |

> 工具内部用 `0.74` 作为安全系数（理论上限是 `0.764`），留一点余量避免整数取整后贴边。

**logo 不缩放的证据**：插件对这类贴图直接用图片自身像素尺寸作为绘制区域，没有缩放逻辑
（`image_area.width = ply_image_get_width (plugin->corner_image);` 等）。因此：

> **图片的像素尺寸 = 屏幕上的显示像素尺寸**

### 1.3 图片规格建议

| 项 | 要求 |
| --- | --- |
| 格式 | **PNG**（plymouth 用 libpng 加载）。SVG 运行时不会渲染 —— 包里的 `throbber.svg` 只是设计源文件 |
| 透明通道 | **必须带 alpha**（RGBA）。插件按 ARGB32 直接 blit，透明区透出背景；不透明的图会盖出一个方块 |
| 位深/色彩 | 8-bit/通道、sRGB 最稳；避免 16-bit 与调色板 PNG |
| 体积 | 几十 KB 以内（要打进 initrd）|
| logo 尺寸 | 宽度取屏幕宽的 **20%** 左右；**多分辨率下按最小屏算**（见下） |
| 背景尺寸 | **不能做整屏**！高度必须 ≤ 屏幕高的 76.4%，且**按最小屏算**（见下） |

### 1.4 替换方式（drop-in）

```bash
# logo（顶部）
cp my-logo.png overlay/usr/share/plymouth/themes/spinner/watermark.png
# 背景
cp my-bg.png   overlay/bgrt-fallback.png

sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --force
```

- `overlay/usr/share/plymouth/themes/spinner/` 是整目录 drop-in 点，放什么都会被拷进镜像
- 构建顺序已确认：**资源拷贝在 `update-initramfs -u` 之前**（构建脚本里 `themes/spinner/` 那几行拷贝
  在 `update-initramfs -u` 之前执行 —— 用符号定位，别记行号，那两处会随脚本演进而漂），
  所以新素材会自动打进 initrd —— **这一步不做就不会生效**（plymouth 在 initramfs 阶段就要渲染）

### 1.5 校验命令

```bash
# 当前默认主题是否被固定
readlink -f /usr/share/plymouth/themes/default.plymouth
# 主题的资源目录与 watermark 设置
grep -i -E "imagedir|watermark" /usr/share/plymouth/themes/bgrt/bgrt.plymouth
# 量现有图片尺寸（无需 imagemagick）
python3 -c "import struct;d=open('/usr/share/plymouth/themes/spinner/watermark.png','rb').read();print('%dx%d'%struct.unpack('>2I',d[16:24]))"
```

### 1.6 调整位置

位置由主题配置里的对齐参数控制（插件内为 `watermark_horizontal_alignment` / `watermark_vertical_alignment`，
上游有提交 *"themes: spinner: Add watermark alignment settings"*）。要改位置需在 overlay 里覆盖当前生效的
那份 `.plymouth` 配置 —— **注意默认生效的是 `bgrt` 那份**，别改错主题。

### 1.7 Plymouth 与 Cage 的交接

当前由 `lsp-kiosk.service.d/10-lsp-quit-plymouth.conf` 的 `ExecStartPre` 在 Cage 接管 DRM/VT 前执行
`plymouth quit`。Plymouth 的画面到此结束；随后由 Cage 背景和 Chromium 客户端 buffer 决定显示内容，
极简着陆页要等浏览器能绘制后才会出现，不能遮住整个浏览器冷启动过程。
单元排序与无 HDMI 行为见 [session-design.md](kiosk/session-design.md)。

后端监听就绪、HTTP 响应与浏览器绘制是不同事件。不能仅把退出动作移到后端“启动完成”时，
就宣称 logo 能覆盖到页面首帧；任何延后交接方案都需要另行验证 DRM 接管与失败退出。
本工程当前没有由上位机加载后调用 `plymouth quit` 的机制。

LVGL 直出 DRM 的实验不沿用 kiosk 排序；它与 Plymouth 曾出现相反的启动顺序，
见 [lvgl-drm-direct-boot.md](lvgl-drm-direct-boot.md) §5。
