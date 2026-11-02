# 品牌化定制：启动 logo / 壁纸 / 锁屏背景

> 记录本镜像里所有可替换的品牌素材位置、**图片格式与尺寸要求**、生效机制，以及踩过的坑。
> 结论均附**证据来源**（包文件清单、打包规则、上游源码），便于日后复核。
>
> **素材不用手工处理** —— 用 `tools/make-assets.sh`（镜像源码：`tools/make-assets.sh`） 一条命令转换到位
> （工具与规格见 [branding-assets-tool.md](branding-assets-tool.md)）。
>
> 本文只讲**素材位与生效机制**（三处 logo 位、Plymouth 开机画面、壁纸/锁屏、ESC 日志、排查顺序）；
> **主机名与登录 motd / 横幅**是另一件事，见 [hostname-and-motd.md](hostname-and-motd.md)。

## TL;DR

- 先分清**三处独立的 logo 位**（开机画面 / 桌面壁纸 / 锁屏），它们**不是同一张图**、改法也不同（§0）。
- 开机画面走 **Plymouth**：默认主题 `bgrt`，但素材实际来自 `spinner` 目录；⚠️ **先确认引擎在**
  （踩过"素材在、引擎不在"）（§1）。
- GNOME 壁纸必须同时覆盖通用与 `:ubuntu` 会话默认值，浅色/深色均指向 example-project 图片（§2）；
  ESC 显示启动日志由内核 `quiet` 参数决定（§3）。
- ⛔ **定制没生效时按 §4 的顺序排查**（从"图有没有进镜像"往"引擎有没有跑"倒推）。
- 证据来源（包清单 / 打包规则 / 上游源码）汇总在文末，便于复核。

## 0. 先分清：一共有三处独立的 logo 位

这是最容易搞错的地方 —— 它们初始内容相同（都源自 Ubuntu 的 `ubuntu-logo.png`），但**是三个独立文件**，换一个不等于全换：

| 位置 | 镜像内路径 | overlay 路径 | 现状 |
| --- | --- | --- | --- |
| **开机画面顶部的 logo** | `/usr/share/plymouth/themes/spinner/watermark.png` | `overlay/usr/share/plymouth/themes/spinner/watermark.png` | 未被替换，仍是 Ubuntu 原生字标 |
| plymouth 构建期的 logo | `/usr/share/plymouth/ubuntu-logo.png` | `overlay/ubuntu-logo.png`（200×100）| 已替换 |
| GNOME「关于」等系统图标 | `/usr/share/pixmaps/ubuntu-logo-icon.png` | `overlay/ubuntu-logo-icon.png`（280×280）| 已替换 |

**证据**：Ubuntu `plymouth` 包的 `debian/rules` 里，三者都是从同一个源文件复制出来的：

```make
install -D -m 0644 debian/local/ubuntu-logo.png debian/plymouth/usr/share/plymouth/ubuntu-logo.png
install -D -m 0644 debian/local/ubuntu-logo.png debian/plymouth/usr/share/plymouth/themes/spinner/watermark.png
install -D -m 0644 debian/local/ubuntu-logo.png debian/plymouth-themes/usr/share/plymouth/themes/spinfinity/header-image.png
```

同时 `-Dlogo=/usr/share/plymouth/ubuntu-logo.png` 是构建期配置，所以**只换 `ubuntu-logo.png` 不会影响开机画面顶部那个 logo**。

## 1. 开机画面（Plymouth） → 见 [plymouth-boot-splash.md](plymouth-boot-splash.md)

引擎前提、`bgrt`/`spinner` 生效机制、各文件规格、多分辨率定尺寸、drop-in 替换、校验命令与位置微调
**已搬成单独一篇** → **[plymouth-boot-splash.md](plymouth-boot-splash.md)**。


## 2. 桌面壁纸与锁屏背景

```bash
# 正常流程：用工具生成，构建脚本自动读取，无需任何变量
./tools/make-assets.sh wallpaper my-wallpaper.png      # → overlay/usr/share/backgrounds/wallpaper.png
./tools/make-assets.sh lockscreen my-lock.png          # → overlay/usr/share/backgrounds/lockscreen.png
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --force
```

构建脚本的默认值就是工具产出的镜像内路径，**重新构建镜像时自动读取这些文件**：

```bash
DESKTOP_WALLPAPER="${DESKTOP_WALLPAPER-/usr/share/backgrounds/wallpaper.png}"
LOCKSCREEN_WALLPAPER="${LOCKSCREEN_WALLPAPER-/usr/share/backgrounds/lockscreen.png}"
```

显式给了变量就用给的路径；**目标文件不在镜像里时会逐级回退**（构建日志会打印提示）：

| 情况 | 结果 |
| --- | --- |
| `wallpaper.png` 存在 | 用它 |
| 不存在，但 `overlay/warty-final-ubuntu.png` 在 | 回退到 `/usr/share/backgrounds/warty-final-ubuntu.png`（兼容早期做法）|
| 都不存在 | 不写壁纸设置，沿用系统默认 |
| 锁屏文件缺失 | 锁屏跟随桌面壁纸 |

实现方式：

1. 生成 `/usr/share/glib-2.0/schemas/99-lsp-branding.gschema.override` 并 `glib-compile-schemas`
   —— 这是**系统级默认值**的标准做法（Ubuntu 自身也是用它设默认壁纸的），只依赖 `libglib2.0-bin`，
   不需要 `dconf-cli`。桌面同时写入 `[org.gnome.desktop.background]` 与
   `[org.gnome.desktop.background:ubuntu]`，两段的 `picture-uri`、`picture-uri-dark`
   均指向选中的壁纸，`picture-options='zoom'`
2. `overlay/warty-final-ubuntu.png` 若存在，仍会占系统默认壁纸槽位（兼容早期做法）
3. 系统级 dconf（收藏夹等）放 `overlay/etc/dconf/`（`profile/user` + `db/local.d/`），构建时 `dconf update`

Ubuntu 的 `10_ubuntu-settings.gschema.override` 为 `:ubuntu` 会话单独设置了壁纸。
`ubuntu-wayland.desktop` 声明 `DesktopNames=ubuntu;GNOME;`；会话专用默认值优先于通用值，
所以只给通用段换成 `99-` 文件名仍不足以覆盖。深色桌面读取 `picture-uri-dark`，两种模式都要设置。

校验须读取编译后的实际值，而不是只检查 override 文件存在：

```bash
GSETTINGS_SCHEMA_DIR="$PWD/build/rootfs/usr/share/glib-2.0/schemas" \
GSETTINGS_BACKEND=memory XDG_CURRENT_DESKTOP=ubuntu:GNOME \
gsettings get org.gnome.desktop.background picture-uri-dark
# 'file:///usr/share/backgrounds/wallpaper.png'
```

`tools/verify-image.sh --root build/rootfs` 同时检查 GNOME 与 Ubuntu 会话的浅色/深色默认值，
期望值来自生成的品牌配置，支持自定义壁纸路径。memory backend 不读取或修改个人 dconf。
系统默认值不覆盖用户已保存的壁纸选择；需要恢复默认时，在该用户桌面中分别执行
`gsettings reset org.gnome.desktop.background picture-uri` 与
`gsettings reset org.gnome.desktop.background picture-uri-dark`。

> 若镜像内没有 `dconf` 命令（`dconf-cli` 未安装），构建会提示 `/etc/dconf/db/local.d` 下的设置不生效。

## 3. 开机画面按 ESC 显示日志

**ESC 就是显示日志的键**，由 two-step 插件处理（上游源码 `validate_input`）：

```c
if (strcmp (add_text, "\e") == 0) {      /* \e = ESC */
        toggle_console_messages (plugin);
```

会在启动画面上叠加一个**滚动日志视图**（新式 console viewer，替代 fbcon），再按一次 ESC 收起。
启用条件是 `ply_console_viewer_preferred()`，以下情况会失效：

- 内核参数带了 `plymouth.prefer-fbcon`
- 字体渲染不可用（正常桌面环境都满足）

### `quiet` 决定"能看到多少"

Ubuntu 开发者 Steve Langasek 在 plymouth 的 bug 报告里明确指出（[Launchpad #1664066](https://bugs.launchpad.net/ubuntu/+source/plymouth/+bug/1664066)）：

> "That's the 'Esc' key. If you hit Esc and you see no boot messages still, that's because you're booting with
> the 'quiet' kernel option, which is the default option in Ubuntu by design."

即 **`quiet` 让内核几乎不产生日志，按 ESC 后主要只看到告警与错误**。本镜像默认保留 `quiet`（与 Ubuntu 一致）。

- 想让 ESC 看到**完整启动日志** → 去掉 `quiet`、保留 `splash`（代价：早期启动阶段可能短暂看到文字）
- 串口控制台不受影响：`plymouth.ignore-serial-consoles` 让串口始终有完整日志，这是最省事的调试通道

## 4. 定制没生效时的排查顺序

0. **先确认引擎在不在** —— `command -v plymouthd` / `systemctl is-enabled plymouth-start.service`。
   没有任何 spinner、只有内核日志时，**十有八九是 `plymouth` 包根本没装**（见 §1.0），
   而不是主题或参数的问题。素材和 cmdline 都会"看起来没问题"，别从那里开始查。
1. **确认改的是当前生效的主题** —— `readlink -f /usr/share/plymouth/themes/default.plymouth`（默认应是 `bgrt`）
2. **确认文件真的进了 initrd** —— 资源拷贝必须早于 `update-initramfs -u`；可 `lsinitramfs /boot/firmware/initrd.img | grep watermark`
3. **确认换对了文件** —— 顶部 logo 是 `watermark.png`，不是 `ubuntu-logo.png`（两者初始内容相同，极易混淆）
4. **确认图片规格** —— 是否 PNG、是否带 alpha
5. **确认尺寸预期** —— 不缩放，所以"图太小/太大"是尺寸本身的问题，不是没生效
6. **开机画面被提前关掉** —— 检查 DRM 连接器检测脚本（见下）

### 附带修复：无显示时误关开机画面

`overlay/usr/local/bin/check-hdmi-plymouth.sh` 原先写死 `/sys/class/drm/card0-HDMI-A-1/status` 等路径。
但内核 `rockchip_linux_defconfig` 里 **`CONFIG_DRM_ROCKCHIP=y` 与 `CONFIG_DRM_PANTHOR=y` 都是内建**，
谁先注册谁就是 `card0`；若 `card0` 是 Mali GPU（没有任何连接器），就会误判为"无显示" →
**开机画面被提前关掉且毫无提示**。且原脚本只认 HDMI/DP，不认 DSI（本板 uEnv 里列了 11 个 DSI overlay）。

已改为遍历 `/sys/class/drm/card*-*/status`，任一 `connected` 就保留 plymouth。


## 7. 证据来源汇总

| 结论 | 来源 |
| --- | --- |
| 默认主题是 `bgrt` | `plymouth-theme-spinner.postinst`（Ubuntu noble 打包）|
| `themes/bgrt/` 无图片资源 | `plymouth-theme-spinner` 包文件清单 |
| `watermark.png` 与 `ubuntu-logo.png` 同源 | `plymouth` 包 `debian/rules` 的 `install -D` 两行 |
| 背景图居中绘制（黄金分割点）| 上游 two-step 插件 `view_set_bgrt_fallback_background` |
| 贴图按原始像素绘制、不缩放 | 上游 two-step 插件绘制逻辑（`ply_image_get_width/height` 直接作为绘制宽度）|
| ESC 切换日志视图 | 上游 two-step 插件 `validate_input` / `toggle_console_messages` |
| `quiet` 导致 ESC 看不到日志 | Ubuntu 开发者答复（Launchpad #1664066）|
| 图片规格（200×100 / 618×420 / RGBA）| 本仓库 `overlay/` 下文件的实测值 |
| IM 的 SVG delegate 是 rsvg-convert | `magick -list delegate` 输出 |
| 无 librsvg 时渐变会渲染成黑色 | 实测采样（见 5 节）|
| PDF 可用 pdftocairo -transp 得到透明背景 | 实测（poppler-utils）|
