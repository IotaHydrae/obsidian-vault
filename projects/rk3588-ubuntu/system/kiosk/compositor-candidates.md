# 合成器选型：cage / weston / sway 实测与其他候选

> 结论：**继续用 `cage`**（够用、迁移成本最低、还能 fork 打补丁）。
> `weston kiosk-shell` 出画面更早（0.67 s vs cage 0.94 s）但迁移成本大；
> `wdm` / `ubuntu-frame` / `Miriway` / `miral-kiosk` 调研与实测后**都否掉**。

## TL;DR

- 三家实测"restart → 出画面"（kmsgrab 数帧，30 fps）：**weston 0.67 s < cage 0.94 s < sway 1.27 s**。
- 取舍：**纯画面体验 weston 更好**（黑屏时长砍半、那段黑还能变成品牌底色）；
  **纯轻量/迁移成本 cage 更好** ⇒ 选 cage + fork（见
  [compositor-customization.md](compositor-customization.md)）。
- 三家都能让"合成器底色"变成页面色，但**都盖不住 Chromium 窗口自己交的那一帧**
  （`--default-background-color=FFEEF2F5` 试过，没用）。
- 否掉的理由一句话各一条：`wdm` 没有 autologin/无 greeter 模式且自带 ~200 ms 黑；
  `ubuntu-frame` 只走 snap、要把整条 Wayland 栈算进去；`Miriway` 不在 noble apt（主要发 snap）；
  `miral-kiosk` 能起但**比 cage 慢 ~0.35 s**、且三件定制要在 MirAL 重做一遍。
- ⚠️ 口径别混：本节是"**restart → 屏幕出画面**"（数帧）；miral-kiosk 那节是
  "**进程起来 → 浏览器请求页面**"（journal 时钟）。两者数值相近但不是一个口径。

## 方案对比

| 方案 | 改动量 | 逃逸面 | 维护成本 | 结论 |
| --- | --- | --- | --- | --- |
| **A. GNOME 里 autostart `--kiosk`** | 最小（一个 `.desktop`） | **大**：Super/Alt+Tab/Ctrl+Alt+T/概览/通知都要逐个关 dconf | 每次 GNOME 升级都要重新对一遍 | 只适合**先给工厂看效果** |
| **B. `cage` + systemd 服务（推荐）** | 中（4 个文件 + mask + 一个包） | **小**：没有 WM/shell/桌面 | `cage` 版本老，升级要测 | ✅ 目标形态 |
| C. `sway -c kiosk.conf` | 中 | 中（sway 有自己的一套快捷键，要清空 bindings） | 配置面比 cage 大 | B 有问题时的备选 |
| D. 出**只有 kiosk 的变体镜像**（去掉 GNOME/桌面包） | 大 | 最小（连桌面都没有） | 镜像要分叉维护 | ⏳ 若工厂机确定不需要桌面，长期最省（体积/启动也受益） |

### 备选合成器实测（2026-09-30，**都只做临时测试、未采纳**）

目标：让"旧 surface 撤掉、新渲染进程首帧未提交"那一下露出**页面底色**，而不是 cage 的 `#3C3C3C`。
同一套 30fps `kmsgrab` 抓帧脚本，量"单位 restart → 出画面"与过渡颜色序列：

| 合成器 | 黑屏起→首个浅色帧（各 2 次） | 过渡颜色序列 | 体积（已安装） |
| --- | --- | --- | --- |
| **cage**（现状） | 0.70 / 0.80 s（中位 0.75） | 黑 0.63~0.77 s → (61,61,61) → 浅 → 暗 → 浅 | **80 KB**（单二进制，无额外依赖） |
| sway + swaybg | 1.27 s（1 次） | 黑 0.57 s → #eef2f5 0.50 s → 黑 0.07 s → 过渡 → 画面 | （sway 1.9 + swaybg，8 个小包） |
| **weston kiosk-shell** | **0.27 / 0.30 s（中位 0.29）** | 黑 0.27~0.30 s → **#eef2f5** → 暗(Chromium 窗口) → 浅 | 整套 **11.9 MB**（weston 6.3 + libweston 1.8 + freerdp/winpr/neatvnc/turbojpeg 等可选后端依赖） |

补充口径：**黑屏起 = 抓帧序列里第一帧全黑**（不是"发出 restart 那一刻"，后者含 systemd/PAM 的开销）；
帧率约 30 fps ⇒ 单次量化误差 ~33 ms。常驻内存两家几乎一样（cage 会话 1336 MB vs weston 会话 1334 MB —— 都是 Chromium 占 99%）。
⇒ **纯画面体验 weston 更好（早 ~0.45 s 出画面、黑屏时长砍半、那段黑还变成品牌底色）；纯轻量/迁移成本 cage 更好。**

**结论（含一处对 [startup-timeline.md](startup-timeline.md) 的错误归因的修正）**：

- 三家都能把"合成器底色露出的那段"变成页面色（sway 靠 `exec swaybg -c '#eef2f5'`；Weston 靠
  `[shell] background-color=0xffeef2f5`），**但最后那一下 `(61,61,61)` 灰三家都有** ⇒
  它不是 cage 的底色，而是 **Chromium 窗口自己的底色**（窗口创建到首帧之间）。
  ⇒ **换合成器治不了用户看到的那一下闪**，只能把前面那段黑/灰变成页面色。
- 副作用（值得记）：**weston kiosk-shell 反而是三者里起画面最快的**（0.67 s < cage 0.94 s < sway 1.27 s），
  单进程、不需要额外背景客户端。
- ⚠️ 想彻底消掉那 0.07~0.14 s 只能从 Chromium 侧想办法：已试 `--default-background-color=FFEEF2F5`
  → Chromium 151 **整屏黑**（抓帧 749 帧全黑，已撤）。

真要采纳时的适配清单（都没做）：

| | sway | weston kiosk-shell |
| --- | --- | --- |
| 软件包 | `sway` + `swaybg`（noble/universe，8 个小包） | `weston` 13.0.0（8 个小包，含 libweston） |
| 启动方式 | `--config <ini>`，Chromium 在配置里 `exec` | ⛔ **不接受尾部 client 参数** ⇒ 要包一层 shell：后台起 weston → 等 socket → `exec chromium` |
| 背景色 | `exec swaybg -c '#eef2f5'`（`output * bg #eef2f5 solid/fill` **不认**：颜色被当文件路径） | `[shell] background-color=0xffeef2f5` ✓ 内建 |
| 免黑屏 | 不装 swayidle 即不黑屏 | `[core] idle-time=0`（否则 5 分钟黑屏） |
| 抓屏测试 | `grim` 可用 | ⛔ Weston 不支持 `wlr-screencopy` ⇒ 只能 `ffmpeg kmsgrab` |
| 其它要改 | `lsp-keyfilter` 的 `LSP_KEYFILTER_COMPOSITOR=cage` → `sway`；`tests/kiosk-lockdown.sh` 的 cage 专属断言要重跑 | 同上（环境变量 → `weston`；Weston 自身 VT/快捷键也要重新核对） |

### 其他路线：`wdm`（Wayland display manager）—— 调研后**不适用**（2026-09-30）

`github.com/quinnjr/wdm`（Rust，0.10.x）的卖点是"**自己就是合成器的显示管理器**"：不再额外起一个
kiosk 合成器（cage / weston-kiosk / gamescope）去承载登录界面，而是自己通过 libseat 绑 DRM/KMS、
把 greeter 当普通 Wayland 客户端跑（`wdm_greeter_v1` 协议），登录时**先释放 DRM/renderer/libinput/
libseat 再 fork+exec 用户会话**（用户合成器因此拿到真 DRM master）。

**为什么不采纳**：

- 我们**没有"登录界面"这个需求**：kiosk 自动登录是 systemd + `PAMName=login` 干的，"工号"登录在
  **上位机内部**；wdm 的全部价值（省掉承载 greeter 的那层合成器）对我们不成立 —— **我们本身就是那个 kiosk 应用**。
- 它**没有 autologin / 无 greeter 模式**：`wdm(1)` 只有 `--config` / `--backend` / `-h` / `-V`，
  配置里也没有类似开关（读过 man 与 `crates/wdm/src/config.rs`）。
- 它自己的会话交接**明确有 ~200 ms 黑屏**（README 原话："…a black moment of roughly 200ms, which is
  what every display manager does"）⇒ 比我们现在的路径更差。
- 采纳它等于把 kiosk 单元 + 维护模式（kiosk ↔ gdm3 桌面）整套换掉，**纯亏**。

**可借鉴两点**（记着，不改）：

1. **交接纪律**：切换会话前先释放 DRM / libinput / libseat 再 exec —— 与本工程"维护模式切到桌面"
   那条链是同一类问题，值得对照着看。
2. 将来若要**自己写一个最小 kiosk 合成器**（而不是给 cage 打补丁），它的 Rust + libseat/DRM 代码是现成参考。

### 其他路线：`ubuntu-frame`（Canonical 的 kiosk 合成器）—— 评估后**不采纳**（2026-09-30）

**是什么**：Canonical 的 "simple fullscreen shell"（基于 **Mir**），就是为 kiosk / 工业显示 / 数字标牌做的：
应用拿到一个全屏窗口，触摸/键盘/鼠标由它统一处理；**自带背景客户端**，背景色可配 ——
`frame.config` 里的 `--wallpaper-top=0xRRGGBB` / `--wallpaper-bottom=0xRRGGBB`（默认是灰渐变
`0x7f7f7f → 0x1f1f1f`）⇒ "合成器底色露出的那段"可以配成我们的页面色 ✓。
snap 配置项只有四个：`daemon` / `launcher` / `config` / `display`（在 classic 系统上 `daemon` 默认 false，
要显式 `snap set ubuntu-frame daemon=true`）。

**为什么不采纳**：

1. **只有 snap 形态**（`snap/snapcraft.yaml`，base `core24`）。本镜像是纯 deb 构建 —— 引入它要：
   装 snapd + 内置 `.snap`（离线 `snap install --dangerous`）+ `snap refresh --hold` 防自动刷新 +
   连 `desktop-launch` / `wayland` 接口。为一件事（改背景色）开一条新的打包通道不划算
   （Mir 本身在 Ubuntu 有 deb，但 ubuntu-frame 这个 shell 没有 deb；拿源码自编就是又一套自维护构建）。
2. **治不了用户看到的那一下闪**：[compositor-candidates.md](compositor-candidates.md) 已实测，那 0.07~0.14 s 的 `(61,61,61)` 是 **Chromium 窗口自己的底色**，
   cage/sway/weston 三家都有 ⇒ 换谁来都一样。
3. 换合成器还要重做 kiosk 生态：`lsp-keyfilter` 的合成器名、`tests/kiosk-lockdown.sh` 的断言、显示配置。

**结论：记档，不采纳。** 若将来工位要变成"多窗口 / 多 app + 应用切换侧栏"（`launcher=true` 那类形态），
它比 cage 更对口，到时候可以再翻出来评估。

### 其他路线：`Miriway`（基于 Mir 的通用合成器）—— 评估后**不采纳**（2026-10-02）

**是什么**（`github.com/Miriway/Miriway`，C++，**GPL-3.0**，108 star，最近提交 2026-09-22）：
自称 "Mir based Wayland compositor" —— 基于 **Mir** 的**通用**合成器，定位是"可定制成各种桌面环境"：
浮动窗口策略、动态工作区、**panel/dock 这类 shell 组件**、经 Xwayland 跑 X11 应用；
配置在 `miriway-shell.config` / `~/.config/miriway-shell.settings`。它的默认快捷键里就有
**`ctrl+alt+BackSpace` = `@exit`**（退出）、`meta`+方向键 = 停靠/切工作区。

**可用性（2026-10-02 在开发板上查的）**：

- **Ubuntu noble 的 apt 里没有 `miriway`**（`apt-cache policy miriway` 无候选）；
- 但 **Mir 本身在 noble 有**：`libmiral-dev 3.9.0.2.14.1-4.1ubuntu3`、`libmirserver-dev 2.14.1`、
  `mir-platform-graphics-gbm-kms20`、`mir-platform-graphics-wayland20` ⇒ 想用就得**自己编 miriway-shell**；
- 它的**主要分发渠道是 snap**（另有 Fedora）—— 与 [compositor-candidates.md](compositor-candidates.md) 的 `ubuntu-frame` 同一个问题：
  本镜像是纯 deb、**不带 snapd**。

**为什么不采纳**：

1. **它面向的是"桌面形态"**（多窗口 + 工作区 + 面板/停靠），我们要的是**单全屏、零逃逸**；
   它的默认快捷键本身就是逃逸面（`ctrl+alt+BackSpace` 直接退出、`meta`+方向键停靠/切工作区），
   要全部清掉才能谈 kiosk。
2. **我们的按键需求它给不了**：我们要的是"**只放行白名单里的按键**"（[keyfilter.md](keyfilter.md)（原文档误编号为 [integration-pitfalls.md](integration-pitfalls.md).2） 那类语义），
   这是**输入层过滤**，不是"快捷键配置"；cage 那条路是直接在 `seat.c:handle_key_event()` 里拦的
   （[compositor-customization.md](compositor-customization.md)），Miriway 的配置面覆盖不到。
3. **治不了用户看到的那一下闪**：同 [compositor-candidates.md](compositor-candidates.md) 的结论 —— 那段 `(61,61,61)` 是 Chromium 窗口自己的底色，
   换谁来都一样。
4. **代价更高**：snap 通道我们不用 ⇒ 要么自己编 miriway-shell（并跟着 Mir 走），要么等发行版打包；
   而 cage fork 已经上板验过、只有 80 KB（[compositor-customization.md](compositor-customization.md)）。
5. 顺带：许可证从 **MIT（cage）变成 GPL-3.0**。

**结论：记档，不采纳。** 至此"合成器候选"清单：
`cage`（在用）→ `weston kiosk-shell` / `sway`（实测过，[compositor-candidates.md](compositor-candidates.md)）→ `wdm` / `ubuntu-frame` / `Miriway`
（调研后否掉，[compositor-candidates.md](compositor-candidates.md) / [compositor-candidates.md](compositor-candidates.md) / 本节）→ **`miral-kiosk`**（[compositor-candidates.md](compositor-candidates.md)，这批里最接近 cage 的一个）。

**另**：正在评估的 WPE 路线（`notes/system/wpe-webkit.md`）用的是 `WPEDisplayDRM`
（**直接出屏、不要合成器**）⇒ 那条路上也不需要 Miriway。

### 其他路线：Mir 自带的 `miral-kiosk`（含被误会的 `miral-app -kiosk`）—— 评估后**不采纳**（2026-10-02）

⛔ **先纠正一个容易误会的点：`miral-app -kiosk` 不是"kiosk 模式"，它是个 demo 启动脚本。**
读 noble 的 `/usr/bin/miral-app`（来自 `mir-demos 2.14.1-4.1ubuntu3`）全文可见：`-kiosk` 只是把
`miral-shell` 换成 **`miral-kiosk`**，然后等它把 Wayland socket 建出来、**启动 `miral-terminal`
（终端模拟器）**当那个 kiosk 应用，终端一退出就 kill 掉服务器。
**真要对标 `cage` 的是 `miral-kiosk` 本身。**

**`miral-kiosk` 是什么**：Mir 的 **kiosk 壳**（二进制 133 KB；依赖 `libmiral5t64` +
`libmir{server,platform,wayland,core,common,cookie}`，运行库合计约 **4.4 MB**、共 13 个包，
**noble 里都有**）。读上游 `examples/miral-kiosk/kiosk_main.cpp`（106 行）可见它全部内容就是
`KioskWindowManager`（单窗口全屏）+ `Keymap{}`（只设键盘布局），**没有任何快捷键**
（对比 `miral-shell` 那堆 `Ctrl-Alt-*`）⇒ **默认就没有逃逸面**。

它提供、而 cage 没有的东西（都值得记）：

| 选项 | 作用 |
| --- | --- |
| `--startup-apps=<冒号分隔>` | **声明式**地拉起 kiosk 应用（不用自己写 exec 包装） |
| `--kiosk-startup-apps-only` | `KioskAuthorizer` **只允许"启动期"连上来的应用**，之后的连接被拒 |
| `--show-splash`（**默认 true**） | 合成器自带启动 splash，可在浏览器页面就绪前显示画面；本工程当前使用 Cage 背景 |
| `--cursor=null` | 隐藏鼠标指针 |
| `--idle-timeout`（默认 0） | 永不熄屏（weston 要显式 `idle-time=0`） |
| `--display-layout`（`miral-kiosk.display`） | 显示布局配置 |
| `--compositor-report` / `--display-report` / `--input-report` / `--scene-report` / `--shell-report` | 各子系统诊断报告（排障用） |
| `--keymap` / `--enable-key-repeat` / `--app-env*` | 键盘布局 / 重复 / 启动应用环境变量 |

**为什么不采纳**：

1. **我们要的三件定制它一件都不提供**：合成器底色（cage 那边我们 fork 了 `--background`）、
   **按键白名单过滤**（fork 在 `seat.c:handle_key_event()` 里）、开机 NumLock（fork 里补按）。
   换过去等于把 `packages/cage-lsp/` 那 787 行补丁**在 MirAL 上重做一遍**（Mir 侧得写
   window-management policy / input filter），还要多背 13 个包的 Mir 全家桶。
2. **治不了用户看到的那一下闪**：[compositor-candidates.md](compositor-candidates.md) 已定死，那段 `(61,61,61)` 是 **Chromium 窗口自己的底色**，
   换谁来都一样。
3. **已补测，没有收益且更慢**：同判据下 miral-kiosk 比 cage **慢约 0.35 s**（见下面实测），
   而 cage fork 已上板验过、只有 80 KB。

**已实测（2026-10-02，开发板 `<HOST_07>`）：`miral-kiosk` 确实能起、能出画面，但更慢。**
先纠正之前"没测就卸掉"的做法 —— 这次把 `mir-demos` + 三个平台包装回去真跑了一遍：
临时单元是 active 的，Chromium 以 Wayland 客户端连上并成功取到上位机页面（不是"起不来"）。

**判据（全用 journal 单调时钟，不抓帧）**：⛔ 用户明确"截图定时不准"，所以这次不数帧。
两个临时单元与生产 `lsp-kiosk.service` **同骨架**（`User=kiosk` / `PAMName=login` / `TTYPath=/dev/tty1`），
只换 `ExecStart`，浏览器命令逐字相同（同一份 wrapper，先打一行 `[trial-marker] exec chromium`
再 exec 同一串 Chromium 参数）：

- cage：`/usr/bin/cage -- /tmp/chromium-trial.sh`
- miral-kiosk：起 `miral-kiosk --show-splash=false &`，等 Wayland socket 建出来，再跑同一个 wrapper
  （照抄 `/usr/bin/miral-app` 的 `-kiosk` 分支，只把 `miral-terminal` 换成 Chromium）

取三个时间点：**T0** = `Started <unit>`；**T1** = `[trial-marker]`；**T2** = 上位机第一条
`GET / HTTP`（用临时 drop-in 去掉 `oem-host.service` 的 `--no-access-log` 才拿得到这行访问日志），
全程只在当前 boot 里取（⛔ 不加 `-b` 会把上次开机的 `short-monotonic` 混进来，第一版就踩了）。

| 轮次 | cage 合成器就绪 / 浏览器→GET / / **合计** | miral-kiosk 合成器就绪 / 浏览器→GET / / **合计** |
| --- | --- | --- |
| 1 | 0.26 / 0.67 / **0.93** | 0.66 / 0.72 / **1.38** |
| 2 | 0.28 / 0.67 / **0.95** | 0.64 / 0.69 / **1.33** |
| 3 | 0.28 / 0.69 / **0.97** | 0.59 / 0.69 / **1.29** |

⇒ **miral-kiosk 慢 ~0.35 s，而且慢的全在"合成器就绪"那一段**（0.27 s → 0.63 s）；
浏览器自己那段两边一样（~0.68 s），说明差的是合成器本体（MirAL 初始化 + 平台探测），不是应用。
再算上多背 13 个包（运行库 4.4 MB）和"三件定制得在 MirAL 侧重做一遍"⇒ **维持不采纳**。

⚠️ **口径区别（别混着引用）**：本节量的是"**进程起来 → 浏览器开始请求页面**"；
[compositor-candidates.md](compositor-candidates.md) 里 cage 的 0.94 s 是"**restart → 屏幕出画面**"（kmsgrab 数帧）。两者数值相近但不是同一个口径。

**结论：记档，不采纳** —— 但它是这批候选里**最接近 cage 的一个**（真 kiosk 语义、noble 有包、
自带 splash 与声明式 startup-apps）。**哪天真要换合成器，先看它**，别从 Miriway / weston 看起。

**可借鉴两点（记着，不改）**：

1. **`--startup-apps` + `--kiosk-startup-apps-only`** 这种"**声明式启动 + 连接白名单**"的组合，
   比我们现在"unit 里写死 `ExecStart` + 靠 systemd 管生命周期"表达力更强；
2. **自带 splash**（`--show-splash`）说明"合成器自己先画一帧品牌页"是主流做法 ——
   与 [compositor-candidates.md](compositor-candidates.md) 的结论一致（那段黑只能由合成器或应用自己填）。
