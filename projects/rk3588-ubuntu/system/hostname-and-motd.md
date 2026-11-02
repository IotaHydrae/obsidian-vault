# 主机名与登录 motd / 横幅

> 主机名由构建变量 `LSP_HOSTNAME` 统一生成；登录横幅（banner）与 motd 片段是**用户自己手写的文件**，
> 不按通用规范改（见 `AGENTS.md` 第 8 条）。

## TL;DR

- **速查表**：要改什么 → 改哪个文件（见下文第一节）。
- 主机名统一走 `LSP_HOSTNAME`（见下文"主机名由 LSP_HOSTNAME 统一生成"那节），别手工改 `/etc/hostname`。
- 登录字标（`/opt/banner.txt`）用 `tools/make-motd-art.py` 生成（`toilet -f future` + 确定性彩虹色）；
  ⚠️ toilet 的字体除了 7 个内建，还有 `toilet-fonts` 的 `.tlf` 与 figlet 的 `.flf`（在 `/usr/share/figlet/`）。
- ⛔ `overlay/opt/logo.txt` 仍保留在仓库/镜像里，**只是不再显示**；`00-header` 现在只 `cat /opt/banner.txt`。
- 换壁纸/锁屏/logo 的完整例子见本文最后一节。

## 主机名与登录 motd / 横幅

README 的「品牌化」小节只保留命令与速查表，这里给出完整说明。

### 6.1 速查：要改什么放哪里

| 要改什么 | 放哪里 |
| --- | --- |
| 开机画面顶部 logo | `overlay/usr/share/plymouth/themes/spinner/watermark.png`（PNG/RGBA，1:1 绘制不缩放；**按最小屏**算尺寸）|
| 开机画面背景 | `overlay/bgrt-fallback.png`（居中绘制，**高度不得超过屏高的 76.4%** 且**按最小屏**算，不能做整屏）|
| 桌面壁纸 / 锁屏背景 | 用 `./tools/make-assets.sh wallpaper 图.png`；构建脚本默认读它产出的路径，**无需设变量** |
| 设备主机名 | 构建脚本变量 `LSP_HOSTNAME`（默认 `example-project`）|
| 其它 logo 位 | `overlay/ubuntu-logo.png`、`overlay/ubuntu-logo-icon.png` |
| 登录字标 | `overlay/opt/banner.txt`（**字标** "example-project"，彩色；装到 `/opt`，由 `00-header` 打印）。图形标 `overlay/opt/logo.txt` **仍在镜像里、但 00-header 不再打印**（2026-09-26 用户："logo 不好看，去掉吧" —— 指登录横幅里不显示，⛔ **文件别删**，用户专门强调过）；怎么重做见 §6.1.1 |
| 登录 motd | `overlay/etc/update-motd.d/00-header`（横幅 + 一行版本信息）与 `30-sysinfo`（系统信息）；Ubuntu 自带的 `10-help-text`（三行广告）、`60-unminimize`（"系统已被最小化"）、`50-motd-news`、`85-fwupd`、`91-*`、`95-hwe-eol` 由构建脚本删除，`00-header` 也不再打印 Ubuntu 那句 `Welcome to ...`；`/etc/pam.d/{login,sshd}` 里除了 `motd=/run/motd.dynamic` 还有一条裸的 `pam_motd.so noupdate`，它会直接打印 base-files 自带的静态 `/etc/motd`（"Ubuntu 免费软件声明 + sudo_root 提示"），所以构建脚本把 `/etc/motd` 清空成 0 字节 |

**2026-09-26 用户点名改的三处**（都在这两个文件里，改完在开发板 `<HOST_07>` 上实测过）：

1. **`00-header` 只剩字标**：先按"品牌化"给版本行加了品牌前缀
   （`example-project | Ubuntu 24.04.4 LTS | Linux 6.1.172 | aarch64`），**当天晚些用户说这行也去掉**，
   于是版本行整段删除；图形标 `/opt/logo.txt` 同批**从登录横幅里去掉了** —— 但用户紧接着强调
   "别删 /opt/logo.txt，只是不在 header 显示了" ⇒ 文件仍在仓库/镜像里，只是没人 cat 它了。
   现在这个片段只 `cat /opt/banner.txt`（缺就退化成一行 `example-project`）。
   ⚠️ 别再"顺手"把版本行加回来 —— 排障要看发行版/内核就直接 `uname -a` / `lsb_release -a`。
2. **`30-sysinfo` 的 IP 行**：原先抄的是 Armbian 的接口名前缀匹配
   `SHOW_IP_PATTERN="^[ewr].*|^br.*|^lt.*|^umts.*"`，而这两块板实际承载局域网的接口叫 **`p2p0`**
   （USB WiFi 8822CE 起的名字；有线 `eth0/eth1` 常常 `unavailable`）⇒ 匹配不到，
   **登录时那一行永远是空的**（最该显示的信息恰好不显示）。现在改成
   `ip -4 -o addr show scope global`（`scope global` 天然排除 `lo`，不依赖接口命名），
    `SHOW_IP_PATTERN` 默认留空＝不过滤，仍可用 `/etc/default/armbian-motd` 覆盖。
   板上实测渲染出一行：`CPU: 37°C | GPU: 38°C | IP: <HOST_07>`。
3. **`30-sysinfo` 去掉 "System information as of <date>" 那一行，把时间并进信息块**：
   时间戳（`YYYY-MM-DD HH:MM:SS`）加在**第二行末尾**（CPU/GPU/IP 之后），不是第一行开头 ——
   串口按 80 列排版，第一行（Up time/Memory/Usage）本来就有 ~70 列，前面再插 19 字符会折行。
   现在的样子：
   ```
   Up time: 29 min | Memory: 5% of 7930MB | Usage of /: 25G of 58G (46%)
   CPU: 37°C | GPU: 38°C | IP: <HOST_07> | 2026-09-26 12:11:08
   ```

> 回归断言：`tools/verify-image.sh` 的「登录 motd」组 —— `00-header` 里不许再出现 `uname`（版本行）、
> `30-sysinfo` 里不许再出现 `System information as of`、信息块里必须能抓到 `YYYY-MM-DD HH:MM:SS`、
> 有 scope-global IPv4 时必须打印 IP 行、`/opt/logo.txt` 必须不存在。

### 6.1.1 登录字标（banner）怎么重做

2026-09-26 重做过一次，目标是"**小、短**"（用户原话"不要太大太长"），图形标改成**描线**
（用户建议"是不是可以用 `-/\`"），字标改成 **toilet 出的彩色字标**；**随后用户说图形标不好看、
从登录横幅里去掉了**（版本行也是同批去掉的，见上面的速查表；⚠️ 文件 `/opt/logo.txt` 保留）。
当前产物：

```
overlay/opt/banner.txt   374 B / 17 列 × 3 行     ← toilet -f future "example-project" + 脚本上彩虹色
（`overlay/opt/logo.txt` 仍在仓库/镜像里，只是 00-header 不再打印；改它用脚本的 `--logo`）
```

（重做前是 6785 B/42×16 的图形标 + 859 B/40×6 的字标，旧版给**每个字符**都套了一对 SGR 颜色码；
现在只剩字标，字形由 toilet 出、颜色由脚本上，只有 3 行 17 列。）

**素材**（唯一事实来源，都在仓库里）：

| 素材 | 是什么 |
| --- | --- |
| `overlay/ubuntu-logo-icon.png`（280×280）| **品牌 lockup**：左边图形标 + 右边 "example-project" 字标，白字透明底 |
| `overlay/ubuntu-logo.png`（200×200）| 同一个 lockup 的另一尺寸（GNOME 侧用）|
| ⛔ `overlay/opt/oem-host/app/static/example-project.svg` | **不是品牌标** —— 那是上位机（HMI）自己的图标（`aria-label="example-project"`），别拿它做登录横幅 |

**图形标**：`tools/make-motd-art.py` 会把 lockup 按"非透明像素的列间隙"切成左右两块，
取左边那块（65×50px），再用 `- | / \ +` **描线**（**不用 Unicode 半块 ▀ █**：
串口 console 字体不保证有 U+2580–259F，旧文件也是纯 ASCII）。
描线怎么算：格子自身的覆盖率决定"这格画不画"，方向则取一个以格子为中心的**正方形窗口**
（边长 ≈1.6 格）里的墨迹点做 PCA —— 窗口必须方形，否则笔画方向会被拉偏；
方向明显就画 `- | / \`，各向同性（拐角/交叉）画 `+`。密度色阶模式还留着（`--style ramp`），
但几何线稿明显是描线好看。

```bash
python3 tools/make-motd-art.py --cols 24                  # 写 logo.txt + banner.txt（要 figlet）
python3 tools/make-motd-art.py --cols 30 --preview        # 先看效果，不落盘
python3 tools/make-motd-art.py --no-banner                # 只重做图形标
```

`--cols` 是字符列数（行数按素材比例自动算，约 = 列数 × 高/宽 / 2）；嫌糊就加大列数
（`--cols 30` → 12 行），嫌大就减小。

**字标**：默认交给 **toilet**（`-f future -F gay` → 3 行 × 17 列，彩虹色）；也可以退回
figlet 出字形 + 脚本自己上色（`--banner-tool figlet --font small`，`--banner-color blue|none`）：

```bash
python3 tools/make-motd-art.py --font letter               # toilet 的纯 ASCII 字体（6 行 × 38 列）
python3 tools/make-motd-art.py --font emboss               # 另一种 3 行风格
python3 tools/make-motd-art.py --banner-tool figlet --font small --banner-color blue
```

**toilet 的字体有三个来源**（2026-09-26 实测，⚠️ 我第一次只试了内建字体就下错结论，记在这里防再犯）：

1. **编进二进制的内建字体**：`mono9 mono12 smmono9 smmono12 bigmono9 bigmono12 term`；
2. **`toilet-fonts` 包的 `.tlf`：21 个，装在 `/usr/share/figlet/`**（⛔ 不是 `/usr/share/toilet/`，
   那个目录根本不存在 —— 我第一次就是查错这里才误判"只有内建字体"）；
3. **figlet 的 `.flf` 也能读**：把 figlet 的字体放进任意目录，`toilet -d <目录> -f <名字>` 即可
   （实测：拷 `small.flf` 过去后 `toilet -d /tmp/fonts -f small example-project` 直接出图）。
   ⛔ 板上没装 figlet 时 `/usr/share/figlet` 里只有 `.tlf`，`-f small` 会报 `could not load font small`，
   那是"没有这个字体文件"，不是"不支持 flf"。

`toilet-fonts` 里几个候选（都带 `-F gay`；表里的字节数是 **toilet 自带色码**（每个 12 字节）
的大小，我们实际用的是自己的 5 字节色码 ⇒ 现用的 `future` 落盘 374 B。列宽按**字符**数不是字节数 ——
框线字符 UTF-8 占 3 字节，用 `wc -c` 量列宽会虚高）：

| 字体 | 尺寸 | 大小 | 备注 |
| --- | --- | --- | --- |
| `future`（现用）| 3 行 × 17 列 | 497 B | 框线风格，最紧凑 |
| `emboss` | 3 行 × 17 列 | 499 B | 同尺寸，另一种字形 |
| `smbraille` | 2 行 × 19 列 | 367 B | Braille 盲文块，最矮 |
| `circle` | 1 行 × 7 列 | 73 B | 圈字 ⓁⓘⓑⓔⓡⒶⒾ，只有一行 |
| `smblock` | 4 行 × 19 列 | 631 B | |
| `letter` | 6 行 × 38 列 | 1137 B | **纯 ASCII**（其它多是框线/块字符）|
| `smascii9` | 8 行 × 28 列 | 991 B | 纯 ASCII |

> 框线/块字符在 SSH 和串口登录都没问题（串口那侧是**对端终端**渲染，不是内核字体）；
> 只有本地 fbcon 才依赖内核字体，而内核默认字体是带 box-drawing 的。

改完两个文件都不用动构建脚本（`copy_overlay_dir "opt"` 整目录带进镜像）；
`tools/verify-image.sh` 的「登录 motd」组会检查它们存在且**没有又变大**（>2 KB 或 >12 行就告警）。

### 6.2 主机名由 `LSP_HOSTNAME` 统一生成

`/etc/hostname` 与 `/etc/hosts` 的 `127.0.1.1` 行同源，不会不一致：

```bash
sudo LSP_HOSTNAME=my-board ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=rootfs --force
```

默认小写（RFC 1123 惯例，DHCP/avahi 最省事）；大写功能上也合法，想用 `example-project` 直接传即可。

### 6.3 换壁纸 / 锁屏 / logo 的完整例子

```bash
# 例：换壁纸与锁屏（工具直接写到 overlay 对应位置，构建脚本默认就会读它们）
./tools/make-assets.sh wallpaper my-wallpaper.png
./tools/make-assets.sh lockscreen  my-lock.png

# 例：换开机画面顶部 logo
./tools/make-assets.sh watermark my-logo.pdf

# 重新构建使其生效
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --force
```

> 多分辨率共存时（1080p / 2K / 4K），工具会**按最小屏**算开机背景与 logo、**按最大屏**算壁纸，
> 保证各屏都能正常显示；`./tools/make-assets.sh check` 可核对现有素材。
>
> 构建脚本的壁纸/锁屏默认值就是工具的产出路径，所以**跑完工具不需要再设任何变量**。
> 也可以用 `DESKTOP_WALLPAPER` / `LOCKSCREEN_WALLPAPER` 显式指定其他路径；
> 指定的文件若不在镜像里会自动回退（构建日志会提示）。

开机画面按 **ESC** 可显示启动日志（`quiet` 下主要只看到告警与错误，这是 Ubuntu 的默认设计），
机制见第 3 节。

