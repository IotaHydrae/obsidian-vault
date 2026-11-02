# 品牌素材转换工具：`tools/make-assets.sh`

> 一条命令把源素材（PNG / JPEG / **PDF** / SVG）按各部位规格转好，并直接放进 `overlay` 对应位置 ——
> 不用手工量尺寸、也不用记每处的格式要求。

## TL;DR

- 用法、内置规则（各部位的尺寸/格式/命名）与依赖见下文；跑完直接重编镜像即可。
- ⚠️ **踩过的坑：SVG 渐变会被渲染成黑色** —— 用 PDF/PNG 源，或先把渐变栅格化。
- 工具输出后会打印"放进去了哪些文件"，可直接对照 [branding-customization.md](branding-customization.md)
  的三处 logo 位核对。

## 素材转换工具：`tools/make-assets.sh`

把源素材（PNG / JPEG / **PDF** / SVG 等）按各部位的规格转换好，并直接放进 `overlay` 对应位置。

```bash
# 先看规格与当前状态
./tools/make-assets.sh list

# 单个
./tools/make-assets.sh watermark my-logo.pdf        # 开机画面顶部 logo（PDF 会渲染成透明背景）
./tools/make-assets.sh wallpaper my-photo.png       # 桌面壁纸
./tools/make-assets.sh lockscreen my-lock.png       # 锁屏背景
./tools/make-assets.sh background my-bg.png         # 开机画面背景
./tools/make-assets.sh icon my-icon.png             # 系统图标 280x280
./tools/make-assets.sh wordmark my-word.pdf         # plymouth 构建期 logo

# 一次多个
./tools/make-assets.sh watermark logo.pdf wallpaper photo.png

# 常用选项
--screen 1920x1080      # 最小目标屏幕（分辨率敏感素材按它算）
--max-screen 3840x2160  # 最大目标屏幕（壁纸/锁屏按它算）
check                   # 检查现有素材在目标区间内是否可用
--width 500 --height 200   # 直接指定目标尺寸（覆盖默认）
--mode fit|contain|cover|stretch   # 覆盖默认缩放方式
--trim                # 先裁掉透明/纯色边框（适合导出时留白很多的 logo）
--out PATH            # 自定义输出路径
--dry-run             # 只打印计划
```

### 工具内置的规则

| 部位 | 模式 | 默认尺寸 | 输出格式 |
| --- | --- | --- | --- |
| `watermark` | fit（按宽度）| 最小屏屏宽 × 20%（默认 1080p → 384px）| PNG32 / RGBA |
| `background` | fit 到 最小屏 屏宽×屏高×0.74 | 默认 → 1920×799 内 | PNG24 / RGB |
| `wallpaper` | cover | 最大屏分辨率（默认 3840×2160）| PNG24 / RGB（→ `usr/share/backgrounds/wallpaper.png`）|
| `lockscreen` | cover | 最大屏分辨率（默认 3840×2160）| PNG24 / RGB（→ `usr/share/backgrounds/lockscreen.png`）|
| `icon` | contain（补透明边）| 280×280 | PNG32 / RGBA |
| `wordmark` | fit（按宽度）| 200px | PNG32 / RGBA |

工具会主动提示的三类问题：

- **源图没有 alpha**（如 JPEG）而该部位需要 → 会盖出不透明方块（建议用带透明背景的 PNG，或直接用 PDF）
- **源图偏小**（放大 >1.5 倍）→ 会偏糊，建议换更大的图或矢量文件
- **有 alpha 但完全不透明** → logo 会带一块底色，可用 `--trim` 处理

### 依赖

| 用途 | 依赖 | 说明 |
| --- | --- | --- |
| 必需 | `imagemagick` | 提供 `magick` / `identify` |
| PDF（推荐）| `poppler-utils` | 提供 `pdftocairo`，能渲染**透明背景**；缺失则退回 IM + Ghostscript |
| SVG（推荐）| `librsvg2-bin` | 提供 `rsvg-convert`。**缺失时含渐变/CSS 的 SVG 会被直接拒绝**（见下） |
| 兜底 | `ffmpeg` | IM 解码失败的格式再试一次；可选 |

### 踩过的坑：SVG 渐变会被渲染成黑色

ImageMagick 的 SVG delegate 本身调用的就是 `rsvg-convert`；**没装 librsvg 时会退回内置渲染器**，
而内置渲染器不支持渐变与 CSS。实测（`fill="url(#g)"` 的矩形）：

```
渐变区最左 (30,100)  = (0, 0, 0, 255)   ← 应为 #1155b6，实际渲染成黑色
绿色小方块 (630,160) = (0, 255, 0, 255) ← 纯色正常
文字区     (100,100) = (255,255,255)    ← 文字正常
```

即：**纯色与文字正常，渐变会被渲染成黑色**。所以工具在这种情况下会**直接报错拒绝**（而不是产出一个
颜色错误的 logo），并提示安装 `librsvg2-bin`。装好后由 rsvg 渲染，渐变正常。

> 顺带：PDF 输入通常比 SVG 更省事 —— 用 `pdftocairo -transp` 能直接得到**透明背景**的位图，
> 且矢量放大不糊，不受渐变问题影响。

### 工具输出后

```bash
sudo ./scripts/build-lsp-image.sh --board=<BOARD> --force
```

`rootfs` 阶段会重建 rootfs 与 initrd。壁纸/锁屏的路径已与构建脚本默认值对齐，**不需要再设变量**。

