# cage fork：把按键过滤 / 开机 NumLock / 合成器底色做进合成器

> fork 的是 **noble 冻结的 `cage 0.1.5` 源码包**（不是上游 master —— 那要 wlroots 0.20，noble 没有），
> 把按键过滤、开机 NumLock、`--background` 底色三件事做进 wlr_scene 那一层。
> 补丁与预编译 deb 位于 `packages/cage-lsp/`，镜像侧核对安装版本带有 `+board` 标记。

## TL;DR

- 为什么便宜：cage 0.1.5 源码 3048 行、Build-Depends 只有 wlroots 等五个包，板上原生
  `dpkg-buildpackage -b` 实测 **10.5 s**；键盘事件只在 `seat.c:handle_key_event()` 一处汇合。
- 配置**沿用 `/etc/default/lsp-keyfilter`**（现场口径不用改）；`LSP_KEYFILTER_VT` /
  `LSP_KEYFILTER_COMPOSITOR` 对合成器无意义，启动时各打一行 `is ignored`。
- 过滤范围天然正确：合成器只在它拥有会话时收到输入 ⇒ 不再需要"读活动 VT"那条旁路判断。
- 组合键用 **XKB 状态**判（左右 Ctrl 等价、另一只键盘上的 Ctrl 也算），不用自建 uinput。
- ⛔ **NumLock 必须对"成员键盘"合成按键**，不能对"键盘组"：对组合成时组状态是变了，
  但**物理键盘的 numlock LED 不变**；判据就是 `/sys/class/leds/*::numlock/brightness`。
- `--background=#RRGGBB`（现场走 `LSP_KIOSK_BACKGROUND`）：黑屏段 **0.57 s → 0.43 s**；
  剩下的 0.43 s 是 Chromium 窗口自己交的黑 buffer，合成器盖不住。
- kiosk 里的 Chromium 用浅色，只改 kiosk 用户 dconf；构建期通过 `dbus-run-session -- dconf load` 写入，
  裸 `dconf load` 与 `gsettings set` 都需要可用的 session bus。
- ⛔ 别让 `apps/lsp-keyfilter` 守护进程与补丁版 cage 同时跑（两层过滤叠加、NumLock 补按互相抵消）；
  守护进程从 2026-09-30 起默认 `disabled`，留着只为手动排查。

**背景**：用户"按 ① 做试试" —— 就是 [compositor-candidates.md](compositor-candidates.md) 结论里那条路线（fork cage 打补丁），
而不是从零写一个合成器。要搬进合成器的"我们目前的改动"= 按键过滤（`ctrl+w` 黑名单 + 白名单）。

**为什么这条路便宜（先量再改）**：

- Ubuntu noble 的 `cage 0.1.5` 源码 = **3048 行 C/头**（`seat.c` 968、`cage.c` 594、`output.c` 410、
  `xdg_shell.c` 312…）；Build-Depends 只有 `libwlroots-dev (>= 0.17.0)` + wayland-protocols +
  libpixman-1-dev + libxkbcommon-dev + meson/scdoc/pkgconf，而 noble 给的正是 **wlroots 0.17.1**
  ⇒ 板上原生 `dpkg-buildpackage -b` 实测 **10.5 s**。
- ⚠️ 上游 master（0.3.0）要 **wlroots 0.20**，noble 没有 ⇒ fork 的是 **noble 冻结的 0.1.5 源码包**
  （板上 `ubuntu-ports` 那行本来就带 `deb-src`，`apt-get source cage` 直接可用），别跟上游走。
- 渲染走 `wlr_scene` 场景图、键盘事件在 `seat.c:handle_key_event()` 一处汇合 ⇒ 我们要的是"几十行补丁"。

**落地形态（`packages/cage-lsp/`）**：

| 文件 | 说明 |
| --- | --- |
| `0001-lsp-kiosk-keyfilter-numlock.patch` | 对 noble cage 0.1.5 源码的补丁，包含本工程版本标记 |
| `cage_*_arm64.deb` | 当前预编译产物，以仓库文件和包内版本为准 |
| `build-cage-lsp.sh` | 重新生成 deb：`apt-get source` → `patch` → `dpkg-buildpackage` |

镜像侧：`scripts/build-lsp-image.sh` 在装完 `packages/arm64/*.deb` 之后 `dpkg -i packages/cage-lsp/*.deb`
把发行版那版顶掉（`cage` 仍留在 `BASE_PACKAGES` 里**只为拉依赖**：libwlroots12t64 / libseat1 /
xwayland），并断言 `dpkg-query -W -f '${Version}' cage` 带有 `+board` 标记 —— 版本号不带标记就直接 fail，
免得"以为打了补丁其实装的是发行版"。

**设计要点（都是踩过之后才定下来的）**：

1. **配置沿用 `/etc/default/lsp-keyfilter`**（`cage --keyfilter-config` 的默认值）：现场口径不用改，
   注释也不用抄第二份。`LSP_KEYFILTER_VT` / `LSP_KEYFILTER_COMPOSITOR` 对合成器**没有意义**
   （它只看得到自己的会话、也不需要盯另一个进程的 pid），合成器启动时会各打一行 `is ignored`，
   免得有人以为那两条还在生效。
2. **过滤范围天然正确**：合成器只在它拥有会话时才收到输入 ⇒ 旧守护进程那条"读
   `/sys/class/tty/tty0/active` 判断活动 VT"的旁路判断整条消失（board 桌面在别的 VT，cage 不跑就没人过滤）。
3. **组合键用 XKB 状态判**（`xkb_state_mod_name_is_active`）：左右 Ctrl 等价、另一只键盘上的 Ctrl 也算，
   不用像 evdev 版那样自己维护"哪些键按着"、也不用造 uinput 虚拟键盘。
4. ⛔ **NumLock 必须对"成员键盘"合成按键，不能对"键盘组"合成**（这次最大的坑，2026-09-30 实测）：
   - 对**组**合成：组自己的 xkb 锁定掩码确实变了（`numlock tap: locked_before=0x0 → locked_after=0x10`，
     Mod2 就是 NumLock），但**物理键盘的 numlock LED 一直是 0**；
   - 对**成员**合成：成员 xkb 状态锁上 → 成员的 modifiers 事件被 wlroots 的 `handle_keyboard_modifiers`
     同步进组（组的锁定状态照样正确）→ LED 由成员自己的 libinput impl 写进硬件（**LED 0 → 1**）。
   - 判据是物理键盘的 `/sys/class/leds/*::numlock/brightness`。用它之前先验过"LED 是可靠代理"：
     真注入一次 NumLock 能让它 0→1（`tests/cage-keyfilter.sh` 的 A/B 也是这么量的）。
5. 过滤只作用在"要发给客户端的键"上：合成器自己的快捷键（`Alt+XF86Switch_VT_n`、DEBUG 下的 Esc）
   不受影响；被吃掉的键**连 autorepeat 与抬起一起吃**（否则客户端会收到"没按过却抬起"）。

**实测（2026-09-30，开发板 `<HOST_07>`，cage `0.1.5+20240127-2build1+lsp1`）**：

- `sudo bash tests/cage-keyfilter.sh` → **通过 11 / 失败 0**：`Ctrl+W`(17)/`F12`(88) 被吃且 kiosk
  `NRestarts` 不变；`a` / 小键盘 1 / 回车 / 上箭头 / `-` 全放行；**空规则反面对照**下 `Ctrl+W` 把 kiosk
  打了重启（证明注入确实到了 Chromium，前一条不是假通过）；NumLock LED `1`，`NUMLOCK_ON=0` 时 `0`。
- 运行期日志前缀 `lsp-cage: keyfilter:`：`… -- 1 block rule(s), allowlist ON, numlock ON` /
  `allowlist has 55 keycode(s)` / `blocked keycode 17 (block rule)`。
  ⚠️ cage 的 stderr 在 journal 里**不挂在 `lsp-kiosk.service` 上**（单元带 `PAMName=login`，
  pam_systemd 把进程挪进了 session scope）⇒ 查日志要按前缀过滤全量 journal，`journalctl -u lsp-kiosk` 是空的。
- 构建可复现：`build-cage-lsp.sh` 重新编出的 `/usr/bin/cage` 与仓库里那个 deb 里的**逐字节一致**。

**独立守护的使用边界**：

- `apps/lsp-keyfilter` 二进制仍在镜像里，但 `lsp-keyfilter.service` **默认 disabled**：
  留着是为了手动排查（`--check` / `--tap-numlock`）与"临时换回发行版 cage"时能顶上。
  ⛔ 别让它和补丁版 cage 同时跑：两层过滤会叠加，而且两边的 NumLock 补按会互相抵消（开了又关）。

### 合成器自己的底色（`--background`，2026-09-30，已上板量过）

**背景**：用户列的"先做 1"——[compositor-candidates.md](compositor-candidates.md) 那两条里能真消掉"每次都要闪一下"的部分。

**做了什么**：给 fork 的 cage 加 `--background=#RRGGBB`（也收 `RRGGBB` / `0xRRGGBB`；空串 = 不画，
等同原版）。实现是在 scene 树**最底层**建一个 `wlr_scene_rect`（必须在任何 view 之前创建 —— wlr_scene
的子节点按插入顺序画，后插的盖在上面），尺寸/位置跟着 `handle_output_layout_change()` 同步成整个
输出布局的包围盒。现场配置走 `/etc/default/lsp-kiosk` 的 `LSP_KIOSK_BACKGROUND`（unit 里
`--background=${LSP_KIOSK_BACKGROUND}`），取值必须是**页面实际底色**：kiosk 版皮肤是上游
`app/static/workstation.css` 的 `body{background:#f3f6fa}`，它**覆盖**了 `styles.css` 里那条
`linear-gradient(180deg,#fcfdfe,#eef2f5)` ⇒ **当前值是 `#f3f6fa`**（2026-10-09 实测：原来写的
`#eef2f5` 已与页面不符，切换时看得出来）。上游一换皮肤就得重新对齐这里。

**实测（开发板，30 fps `kmsgrab` 抓帧，重启 `lsp-kiosk` 看颜色序列；帧 43 = 重启瞬间）**：

| 帧 | 改前（无底色） | 改后（`--background=#eef2f5`） |
| --- | --- | --- |
| 43–45 | `#000000` 黑 | **`#eff3f6` 页面色**（合成器底色立刻顶上） |
| 46–58 | `#000000` 黑 | `#000000` 黑（**Chromium 窗口自己的 buffer**） |
| 59+ | `#eff3f6` 页面色 | `#eff3f6` 页面色 |

⇒ 黑屏段 **0.57 s → 0.43 s**，被盖掉的那 0.14 s 正是"合成器自己清的色"。剩下那 0.43 s 是客户端
（Chromium 窗口创建到首帧之间）自己交的黑 buffer，合成器盖不住 —— 与 [compositor-candidates.md](compositor-candidates.md) 的结论一致
（那次的 `#3C3C3C` 也是客户端 buffer；换合成器/加底色都治不了）。

保留旧 view 的最后一帧只能覆盖顶层 surface 销毁再重建。同一个 Chromium 窗口内的文档导航
不触发 `view_unmap`，不能靠它遮住导航过渡；Cage 整体重启则需要跨进程保存画面。当前没有实现这两条路径。

### 试过但**不可靠**：启动兜底盖层 `--cover-ms`（2026-10-09，+lsp3）

**想法**（用户提出）：+lsp2 只盖住"合成器自己清的色"那 0.14 s；剩下 0.43 s 是 Chromium 窗口
**自己交的**黑 buffer。既然底层盖不住，就用**同一个底色矩形抬到客户端之上**盖一段时间。

**做了**（`packages/cage-lsp/`，默认 0 = 关）：开机即 enable；`view_map()` 里 view 的 scene tree
一挂进 `server.scene->tree` 就 `raise_to_top` 一次（之后没有别人会改顺序 —— ⛔ 只在第一个
`new_surface` 时抬是白抬：那个 tree 是**之后**才追加的），另有 16 ms tick 兜底重申；到点 disable。

**结论：机制成立、端到端不可靠**（kmsgrab 30 fps 逐帧 + 重启 `lsp-kiosk`）：

| 观测 | 结果 |
| --- | --- |
| 盖层建成/尺寸 | ✔ `/tmp/lsp-cover.log`：`raise (view attached) elapsed=941/1032 ms size=3840x2160` |
| 基线（无盖层）帧序列 | 帧 47-58 `#000000` + 59-77 `#1b1b1b`（≈1.0 s 空窗） |
| 有盖层（有时） | 能看到底色段出现（≈11 帧）—— 但**时间窗对不上** |
| 不可靠的根因 | **该次 Chromium 启动观测为 1.4~4 s**（板上同时编译时 >4 s）；时间一到就撤，窗口可能还没画出来（实测整屏 `#1b1b1b` 持续 4 s 以上） |

⇒ ⛔ **调大 `--cover-ms` 解决不了**：窗口"画出内容"的时刻没法用固定时间猜。真要治住只有两条路：
① 盖到"客户端第一帧**有内容**"为止（内容启发式，或首帧前不 map —— 见
[compositor-candidates.md](compositor-candidates.md) 里"留最后一帧"那条）；② 从 Chromium 侧让它
首帧就是页面色（`--default-background-color` 已试过，没用）。镜像里保持 **0（关）**。


### 极简着陆页与底色

`overlay/usr/share/lsp-kiosk/loading.html` 只保留纯色背景、延迟启动提示和连接重试，
不复制上位机的表单、CSS 或时钟。它也必须等 Chromium 能绘制页面后才显示，不能覆盖浏览器自身启动前的空窗。
背景与 `/etc/default/lsp-kiosk` 的 `LSP_KIOSK_BACKGROUND`、上位机实际页面保持一致，当前为 `#f3f6fa`。
探测、导航和恢复边界以 [session-design.md](session-design.md) 为准。

### kiosk 会话里的 Chromium 使用浅色

Chromium 的窗口底色跟随会话配色偏好，页面尚无内容或跨文档导航时可能露出这个底色。
旧着陆页路径曾观察到一帧 `#3C3C3C`，见 [startup-timeline.md](startup-timeline.md)；
这不是 Cage 背景，因此改合成器底色不能消除它。该次历史观察不代表当前每次启动的首帧或导航表现。

构建期只给 kiosk 写入浅色 dconf，维护桌面的深色主题保持独立：

```ini
[org/gnome/desktop/interface]
color-scheme='default'
gtk-theme='Yaru'
```

chroot 中裸 `dconf load` 和 `gsettings set` 都可能因没有 session bus 失败。构建使用：

```bash
runuser -u kiosk -- env HOME=/home/kiosk dbus-run-session -- dconf load / < /tmp/lsp-kiosk-dconf.ini
```

`dbus-run-session` 为这次写入建立临时会话总线；不应为此固定镜像的 `/etc/machine-id`。
写后 `/home/kiosk/.config/dconf/user` 应为 `kiosk:kiosk 0600`，镜像体检检查其中的浅色键值。
目标机上还可在 kiosk 会话中用 `gsettings get org.gnome.desktop.interface gtk-theme` 复核 `'Yaru'`。
这些检查证明配置已安装，不代替真实画面验证。
