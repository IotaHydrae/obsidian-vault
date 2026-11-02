# kiosk 逃逸面：实测结论与加固

> 关键修正：**"切 VT / Ctrl+Alt+Del 能逃出去"是在 GDM 桌面下量的** —— cage 接手 VT 之后，
> 它把 tty 设成 `KD_GRAPHICS`+`K_OFF`，**内核自己就丢弃** `KT_CONS`(VT 切换) 与 `K_BOOT`(CAD)。
> 加固里那两条因此降级为**兜底**（只在 kiosk 没跑起来时有意义）。

## TL;DR

- 主证据是 **uinput 真注入的 A/B 实验**（同一次注入、只改单元状态），源码只作解释；
  这套注入后来写成了 `tests/kiosk-lockdown.sh`（27 通过 / 0 失败）。
- ✅ cage 跑着时：`Ctrl+Alt+F2..6` 拿不到 getty、`Ctrl+Alt+Del` 不重启。
- ✅ 但 **kiosk 没跑时键盘确实能拿到 getty**；**有 `/dev/tty0` + CAP_SYS_TTY_CONFIG 的人仍能 `ioctl` 切 VT**
  ⇒ 兜底（`NAutoVTs=0` + mask `ctrl-alt-del.target`）仍然值得留。
- 真正要防的是 **Chromium 层**：导航（`URLAllowlist`）、DevTools、下载、打印、无痕。
  ⛔ `URLAllowlist` 尾部写 `/*` 会**把自己也拦在门外**；`DownloadRestrictions=3` **实测不生效**。
- 策略**只对 kiosk 会话生效**（board 桌面不受影响）。
- 验证方法的坑：这台板子上 headless Chromium 不可用、CDP `Target.attachToTarget` 被拒
  ⇒ 只能用"真 kiosk + 本地 HTTP 服务 + `/json/list` 读 title"。

## 逃逸面清单（"ATM 式"= 逐条堵）

> ⚠️ **2026-09-22 重要修正**：表里前两行原来写"`NAutoVTs=6` ⇒ 会拉起 getty / `Ctrl+Alt+Del` 会重启"，
> 那是 **GDM 桌面下**量的。**cage 接手 VT 之后这两条已经被内核自己挡掉了** —— 用户实测
> "Ctrl+Alt+F2..6 没拿到 getty、Ctrl+Alt+Del 也没重启"，查证成立，机制见 [escape-hardening.md](escape-hardening.md)。
> 下面两行的"处置"因此降级为**兜底**（只在 kiosk 没跑起来时才有意义）。

| 逃逸路径 | 现状（cage 占着 VT 时） | 处置 | 判据（怎么验） |
| --- | --- | --- | --- |
| `Alt+F4` / `Alt+Tab` / `Super` / 活动概览 | GNOME 全都有 | **换 `cage`（无 WM）** | 在 kiosk 里按这三组键，屏幕无变化 |
| `Ctrl+Alt+T`（终端） | GNOME 快捷键 | 同上 + 不在会话里放终端 | 按键无反应 |
| **`Ctrl+Alt+F2..6` 拿 login** | ✅ **cage 持有时按键被内核丢弃**（`K_OFF`）—— 实测拿不到 getty | 可选兜底：logind drop-in `NAutoVTs=0` + `systemctl mask autovt@.service getty@tty2..6`（防"kiosk 没起来时"拿到 login） | 按 F2：屏幕无变化；真要验机制见 [escape-hardening.md](escape-hardening.md) |
| `Ctrl+Alt+Del` | ✅ **cage 持有时按键被内核丢弃**（`K_OFF`）—— 实测不重启 | 可选兜底：`systemctl mask ctrl-alt-del.target` | 按键不重启（cage 不在时才有风险） |
| `Alt+SysRq+{b,o,i}` | ✅ 内核未编（不存在） | 不用做 | `ls /proc/sys/kernel/sysrq` 无此文件 |
| 右键菜单 | Chromium 默认有 | 页面 `oncontextmenu` 屏蔽（浏览器无对应开关） | 右键无菜单 |
| `F12` / `Ctrl+Shift+I`（DevTools） | 默认有 | policy `DeveloperToolsAvailability: 2` | 按键无反应（`chrome://policy` 里显示已生效） |
| 地址栏 / 导航走掉 | `--kiosk` 仍可能 `Ctrl+L` | **policy `URLAllowlist` + `URLBlocklist`**；或 `--app=<URL>`（无地址栏） | 在地址栏输入外网地址被拦；`chrome://policy` 可查 |
| 下载 / 打印 / 无痕 / 登录 | 默认有 | policies：`DownloadRestrictions:3`、`PrintingEnabled:false`、`IncognitoModeAvailability:1`、`BrowserSignin:0` | 菜单/快捷键无效 |
| **`Ctrl+W` / `Ctrl+Shift+Q` 关窗口** | ⛔ **没有 policy/flag 能禁** | **靠服务自愈**：`Restart=always` + **`RestartSec=0`**（立即重启，别给延迟——延迟期间屏上会闪控制台日志）| 按 `Ctrl+W` 后页面自己回来（实测见 [escape-hardening.md](escape-hardening.md) 表第 4 行）|
| 电源键长按 ≥2 s 关机 | `lsp-powerkey` 三级（2/7/10 s） | ✅ **刻意保留** —— 这是现场唯一"能关掉它"的出口（短按已 `ignore`） | 短按无动作；长按 2 s 正常关机 |
| SSH / 串口 | 开着 | ✅ **建议保留**（唯一运维/升级入口，也是出问题时的救命通道） | `ssh board@<设备>` 可用 |
| 插别的 USB 键鼠 | udev 放行 | 物理管控；必要时按 `VID:PID` 只放行指定键鼠（参照用户手写的那 5 条 udev 规则的做法） | `lsusb` 后按规则过滤 |

## "VT 切换键 / CAD 进不来"是**实测出来的**（2026-09-22，附验证方法与坑）

> ⚠️ 这一节的第一版我只"读源码推"，被用户当场指出"没验证就下结论"。现在**主证据是下面这个 A/B 实验**，
> 源码只作解释。

#### A/B 实验（同一个注入工具、同一个按键，只改单元状态）

用 `/dev/uinput` 造一个虚拟键盘直接往内核 input 层注入按键 —— **这条路径与物理键盘是同一条**
（input core → `kbd` handler；`EVIOCGRAB` 只挡 evdev 客户端，不挡 `kbd` handler）。
注入前先确认虚拟键盘确实拿到了 `kbd`：`/proc/bus/input/devices` 里 `Handlers=kbd event13 dmcfreq`。

| # | 状态 | `/dev/tty0`、`/dev/tty1` 模式 | 注入 `Ctrl+Alt+F2` | 注入 `Ctrl+Alt+Del` |
| --- | --- | --- | --- | --- |
| ① | **kiosk 跑着** | `KD_GRAPHICS` + **`K_OFF`** | `tty1 → tty1` **未切换** | `boot_id` 不变 ⇒ **没重启**，journal 里也没有 CAD/reboot 行 |
| ② | **`systemctl stop lsp-kiosk` 后 5 s**（不手工改任何模式） | `KD_TEXT` + **`K_UNICODE`** | `tty1 → tty2` **✅ 切走了** | ⏳ **故意没测**（文本模式下注入 CAD 会真重启；需要时可单独做） |

①→② 唯一的变量是 cage 在不在（连带它设的键盘模式），同一份注入代码在②能切、在①不能 ⇒
**"按 F2 没反应"确实是被 `K_OFF` 挡的，不是我的注入坏了**。
（②还顺带证明：**停掉 kiosk 后模式会还原** —— `KD_TEXT`+`K_UNICODE`。我中途一次在 stop 后 3 s 读到过
`K_OFF`，那是 cage 还没退干净的时机问题，别当结论。）

#### 内核解释（读源码，作为佐证）

`KDGETMODE`/`KDSKBMODE`/`VT_GETMODE` 三个 ioctl 都要传**指针**，传整数会 `EFAULT`。
`K_OFF` 下内核 VT 键盘层有**两道闸门**：

1. `drivers/tty/vt/keyboard.c:1521`
   `if ((raw_mode || kbd->kbdmode == VC_OFF) && type != KT_SPEC && type != KT_SHIFT) return;`
   —— VT 切换键的类型是 **`KT_CONS`**（`defkeymap.c_shipped` 里 `ctrl_alt_map[F1]=0xf500`、`[F2]=0xf501`），
   被这条丢掉 ⇒ **连 `set_console()` 都到不了**。
2. 同文件 `k_spec()`：`if ((VC_RAW||VC_MEDIUMRAW||VC_OFF) && value != KVAL(K_SAK)) return;`
   —— CAD 是 `K_BOOT = K(KT_SPEC,12)`、**不是 SAK** ⇒ 在这里被丢，`fn_boot_it()`/`ctrl_alt_del()` 不执行，
   systemd 收不到那个 `SI_KERNEL` 的 SIGINT。
   （核对过下标：`#define K_HANDLERS` 展开后 index 2=`k_spec`、5=`k_cons`、7=`k_shift`，
   与 `KT_SPEC=2`/`KT_CONS=5`/`KT_SHIFT=7` 一致；`fn_table` 里 `fn_boot_it` 也确实是第 12 个。）
3. 另有 `vt_ioctl.c:1262` 的 `if (vc->vc_mode == KD_GRAPHICS) return;`（"Ignore all switches in
   KD_GRAPHICS+VT_AUTO mode"）作第二重保险。

**⛔ 所以别把"`NAutoVTs=6` ⇒ 能拿到 getty"当结论**：那是 **GNOME/GDM** 下的行为
（mutter 不把 VT 设成 `K_OFF`）。换成 cage 后这条**白捡**了。

#### 边界（实测过/没测过的都标出来）

- ✅ **kiosk 没跑的时候，键盘确实能拿到 getty**（② 实测切走；logind 会拉起 getty —— 另一次
  `VT_ACTIVATE` 实验里看到 `Started getty@tty2.service`，且该单元之后一直 `active`）
  ⇒ **`NAutoVTs=0` + mask `getty@tty2..6` 仍有价值**（防"kiosk 没起来 / 崩了在重启"的窗口期）。
- ⏳ **开机早期（cage 还没启动，约 0~9.5 s）没直接测过**（抓那个窗口要改启动顺序）。它应当是
  `KD_TEXT`+`K_UNICODE`（= ② 的状态，② 已证明该状态下能切）——这是**推断**，不是实测。
- ✅ **有权限的人仍能切 VT**：`ioctl(VT_ACTIVATE)`（要 `/dev/tty0` + CAP_SYS_TTY_CONFIG）**能**切
  （实测 tty1→tty2→tty1）。原因是实测 **`vt_mode = VT_PROCESS`**（`VT_GETMODE` 返回 `VT_PROCESS, relsig=34`），
  而 `set_console()` 那条 KD_GRAPHICS 拦截要求 **`VT_AUTO`** 才生效。这是**运维通道**，不是键盘逃逸。
- ✅ 附带记录：`systemctl stop lsp-kiosk` 时 **cage 是被 SIGABRT 打死的**（journal：
  `Main process exited, code=killed, status=6/ABRT`、`Failed with result 'signal'`），所以单元状态会停在
  `failed`；`systemctl start` 仍能拉起来（必要时先 `reset-failed`）。

#### 这个测试方法的两个坑（都踩过）

- ⛔ **`bytearray` 切片赋值会改变长度**：`dev[0:22] = b'20 字节的名字'` 会把 1116 字节的
  `uinput_user_dev` 缩成 1114，内核直接 `EINVAL`。必须写 `dev[0:len(NAME)] = NAME`。
- ⛔ 注入 CAD 之前先记 `/proc/sys/kernel/random/boot_id`，用它判"有没有重启"比看 ssh 断没断可靠。
- 这套注入可以拿去写 `tests/kiosk-lockdown.sh`：断言"K_OFF 下注入 F2 不切、CAD 不重启"**比让真人按键可靠**。


## 逃逸面加固：每一条都实测过什么（2026-09-22）

| # | 加固项 | 实现 | 实测结论 |
| --- | --- | --- | --- |
| 1 | 切到别的 VT 不再给 login | logind drop-in `NAutoVTs=0`（`overlay/etc/systemd/logind.conf.d/99-lsp-kiosk.conf`） | ✅ 权威开关。`busctl … NAutoVTs` = `u 0`；停掉 kiosk 后（确认 `KD_TEXT`+`K_UNICODE`）注入 `Ctrl+Alt+F2` → VT 切到 tty2 但 **没有** autovt/getty（`getty@tty2 is-active=inactive`、journal 里没有任何 autovt 行） |
| 2 | ~~`mask getty@tty2..6`~~ | **不采用** | ❌ **无效**（实测）：logind 拉的是**别名实例** `autovt@tty2.service`，mask 盖不住 —— 同一实验里 journal 打出 `Started autovt@tty2.service`，且 `getty@tty2 is-enabled=masked is-active=active` 并存 |
| 3 | `Ctrl+Alt+Del` 不重启 | 构建期 `systemctl mask ctrl-alt-del.target` | ✅ 文本模式下注入 CAD：journal `systemd[1]: Received SIGINT.`（= 内核确实把 CAD 交给了 systemd）+ `Failed to enqueue replace-irreversibly job for ctrl-alt-del.target: Unit … is masked`、`boot_id` 未变。**且不误伤**：`reboot.target` 仍 `loaded`、`poweroff.target` 未动 ⇒ `lsp-powerkey` 的优雅关机路径不受影响 |
| 4 | `Ctrl+W` 关窗口 | **当时**不加固（Chromium 侧禁不掉；2026-09-23 已有解法 → **[keyfilter.md](keyfilter.md) evdev 层过滤 `apps/lsp-keyfilter`**），靠 `Restart=always` + **`RestartSec=0`** 自愈 | ✅ 实测（注入 `Ctrl+W`，30 ms 粒度轮询 `cage` PID）：页面确实被关掉但自己回来（PID 变、`NRestarts` +1）⇒ **不是逃逸**。间隙：`RestartSec=2` → **2.6 s**；**`RestartSec=0` → 0.44/0.47 s**（两次复测）|
| 5 | 浏览器层禁止导航出去 | `/usr/share/lsp-kiosk/10-lsp-kiosk.json`（`URLBlocklist:["*"]` + `URLAllowlist`） | ✅ 策略确实被加载并生效：用本地 HTTP 服务做 A/B —— 带策略时 `127.0.0.1:8080`（在表里）放行、`127.0.0.1:9999`（不在）被拦；撤掉策略后**两个都放行** |

#### ⛔ `URLAllowlist` 尾部写 `/*` 会把自己拦在门外（实测模式矩阵）

真机 HMI 是 `http://127.0.0.1:8080/`，所以拿它逐个试写法：

| allowlist 模式 | `http://127.0.0.1:8080/` 能否加载 |
| --- | --- |
| `http://127.0.0.1:8080/*` | ❌ 被拦（**第一版就是这么写的**） |
| `http://127.0.0.1:8080` | ✅ 放行 |
| `http://127.0.0.1:8080/` | ✅ 放行 |
| `127.0.0.1`（只写主机） | ✅ 放行（不限端口/协议） |
| `http://127.0.0.1/*` | ❌（端口不匹配） |
| `localhost` | ❌（它跟 `127.0.0.1` 是两个主机名） |

主机名形式也验过（临时把 `hmi.test` 指到 127.0.0.1）：`http://hmi.test:8080` 放行、
`http://hmi.test:8080/*` 被拦 ⇒ **同一条规则**，所以 `https://example.com/*` 那种写法同样会把自己拦掉。
→ 正式策略里一律**不带尾部 `/*`**（`verify-image.sh` 与 `tests/kiosk-lockdown.sh` 都有防回归断言）。

#### 验证方法上踩的坑（下次别再走弯路）

- ⛔ **这台板子上 headless chromium 不可用**：`--headless=new --dump-dom` 对本地 URL（服务自测 200）
  也 `rc=124`/0 字节，基线 `about:blank` 同样超时 ⇒ 别拿它验策略。
- ⛔ **CDP 的 `Target.attachToTarget` 返回 `Not allowed`**（浏览器级 WS 通道本身是通的，
  `Target.getTargets` 正常）⇒ 也走不通。
- ✅ **能用的方法是"真 kiosk 路径 + 本地 HTTP 服务 + `/json/list` 读 title"**：本地页面的标题写成特征串
  （`LSP-HMI-8080-OK` / `LSP-OTHER-9999-OK`），被拦时标题会变成主机名 ⇒ 一眼可判、不需要人看屏幕。
  这套已经写进 `tests/kiosk-lockdown.sh --with-policy-probe`。

#### 占位网址换成 `https://www.baidu.com/`（2026-09-22 晚）与随之补上的验证

`example.com` **从这块板子连不上**（`curl` 8 s 超时，DNS 只出 IPv6）⇒ 占位换成 baidu。
实测（板上）：`https://www.baidu.com/` **http=200 / 0.087 s**（同一个命令里 `bing` 302、`qq.com` 501、
`sohu` 403、`gov.cn` 301）⇒ 选 baidu。换上后 kiosk 进程的 URL 就是它，`ss` 看到 18 条 `:443` 连接
⇒ **页面真的加载了**，顺带把"主机名形式的 `URLAllowlist` 条目"从"间接推断"变成了**直接验证**
（`https://www.baidu.com` 不带 `/*`，放行；带 `/*` 会失败，已由 `hmi.test` 对照实验证明）。

#### 明确**没测**的（别当成已验证）

- ~~策略逐条行为~~ ✅ **2026-09-23 已做，见 [escape-hardening.md](escape-hardening.md)**：`URLBlocklist`/`PopupsAllowedForUrls` 生效；
  **`DownloadRestrictions=3` 不生效**（实测）；`IncognitoModeAvailability` / `PrintingEnabled` /
  `FullscreenAllowed` 三条**判据无效**（脚本里逐条写明为什么）。回归脚本 `tests/kiosk-policy-probe.sh`。
- ~~`DeveloperToolsAvailability=2`~~ ✅ **2026-09-23 已测（[escape-hardening.md](escape-hardening.md)）**：kiosk 里注入 `F12` / `Ctrl+Shift+I`
  **都打不开 DevTools**（`Target.getTargets` 里没有 `devtools://` 目标；阳性对照 `Ctrl+W` 证明按键送得到）
  ⇒ **快捷键不是逃逸路径**。⚠️ 策略关闭时同样打不开 ⇒ 这条**不能**用来证明策略生效。
- `Ctrl+W` 之后那 2.6 s 内屏幕的具体表现（黑屏还是旧帧），以及连续按多次会不会累积副作用 —— 没测。
- ⚠️ 顺带记一个**测试方法**的坑：这台板子上 headless chromium 挂住、被 `timeout` 杀掉时
  **chromium 主进程会漏下来不退出**（我在板子上留了一个跑了 28 分钟的孤儿进程，cgroup 在 ssh 会话里、
  父进程是 `sudo`）⇒ 用完记得 `pgrep -af '/usr/lib/chromium/chromium --password-store'` 查一遍。



## 策略逐条实测（2026-09-23，开发板）：**URL 拦截有效，下载限制无效**

> 回归脚本 **`tests/kiosk-policy-probe.sh`**（自己起探针 → A/B → 跑完自动恢复；`sudo bash tests/kiosk-policy-probe.sh`）。
> 本次结果：**通过 5 / 失败 0 / 警告 2 / 跳过 4**（警告=已知缺口，跳过=判据无效，脚本里逐条写明）。

**先记三个方法上的坑**（上次在这里走过弯路）：

1. ⛔ **上次"C DP 走不通"的真因是 `Origin` 头**：Chrome 111+ 会对 CDP 的 WebSocket 校验 `Origin`，
   而 python 的 `websocket-client` 默认按 URL 自动带上 `Origin: http://127.0.0.1:9222` ⇒ 握手直接
   **403**（`Rejected an incoming WebSocket connection … Use --remote-allow-origins=…`）。
   ⇒ `create_connection(..., suppress_origin=True)` 之后 **browser 端全通**（`Target.getTargets` 正常）。
2. **`Target.attachToTarget` 返 `Not allowed` 属实**，但**不需要它** —— 页面是我们自己的，
   让页面用 `fetch('/report?...')` **自报**结果即可，完全绕开 CDP 求值。
3. ⚠️ **`Browser.setDownloadBehavior(behavior:'allow', downloadPath:…)` 会覆盖被测策略**（观察者效应）
   ⇒ 只能拿它把"下载通路"固定住，不能拿它观测"默认行为"。
4. ⛔ **注入按键必须 root**：`/dev/uinput` 只有 root 能开，用普通用户跑会 `PermissionError`；
   若把它的 stderr 重定向掉，就变成"注入了但没效果"的**假象**（这次在这上面白跑了一轮）。
   做键盘类判据**必须带阳性对照** —— 注入 `Ctrl+W` 应把窗口关掉，不成立就说明按键没送进去。

逐条结论：

| 策略 | 实测 | 判据 / 说明 |
| --- | --- | --- |
| `URLBlocklist` / `URLAllowlist` | ✅ **生效** | 8080 放行、9999 被拦（A/B；kiosk-lockdown 与本次脚本都验） |
| `PopupsAllowedForUrls` | ✅ **生效** | 8080 页面 `window.open` 成功（在表里，符合预期） |
| `DeveloperToolsAvailability: 2` | ⚠️ **只能证明"快捷键不是逃逸路径"** | 注入 `F12` / `Ctrl+Shift+I`（root）后 `Target.getTargets` 里**没有** `devtools://` 目标；阳性对照 `Ctrl+W` 能把窗口关掉 ⇒ 按键确实送到了。⚠️ **策略关闭时同样打不开** ⇒ 结论是"kiosk 下这两个快捷键打不开 DevTools"，**不等于**策略生效 |
| **`DownloadRestrictions: 3`** | ❌ **不生效** | 显式下载目录下，**策略开/关都是 `completed` 且文件落盘**（2×2 复跑 3 次一致）⇒ 浏览器层"禁止下载"这条**是空的** |
| `IncognitoModeAvailability: 1` | ⚠️ 判据无效 | CDP `Target.createBrowserContext` **不受它管**（策略开启时照样返回 contextId）⇒ 要另找判据 |
| `PrintingEnabled: false` | ⚠️ 判据无效 | `window.print()` 立即返回、也没出现 `chrome://print/` 目标，**开/关一个样** |
| `FullscreenAllowed: true` | ⏳ 未测 | `requestFullscreen()` 需要用户手势（无手势抛 `TypeError`）⇒ 得用 `/dev/uinput` 注入点击再测 |
| `BrowserSignin: 0` / `BackgroundModeEnabled: false` | ⏳ 未测 | 本次没设计判据 |

⇒ **对安全结论的影响**：`URLBlocklist` 那条"锁住导航"仍然成立（最重要的一条）；
但**"禁止下载"不能算锁住了** —— HMI 页面（或任何被放行的页面）触发的下载会真的落盘。
要么 HMI 侧不提供下载入口，要么换别的机制堵。

#### 顺带发现：默认下载目录解析不出来（与策略无关，但影响产品）

**不干预时（不用 CDP 指定目录）下载根本不落盘**；只有用 CDP 显式指定下载目录才成功。

- kiosk 下 `xdg-user-dir DOWNLOAD` = **`/home/kiosk`**（不是 `~/Downloads`），因为 kiosk 没有
  `~/.config/user-dirs.dirs`（`xdg-user-dirs` 包本身装了）。
- ⚠️ 但即使把 profile 的 `download.default_directory` 写死成 `~/Downloads`，不干预时**仍然不落盘**
  ⇒ **根因未查清**（细节与复现步骤见脚本第 2 节与本次会话记录）。
- 影响：HMI 里任何"导出 / 下载"按钮都可能**静默失败**。⏳ 待查。

## 禁翻译（`TranslateEnabled: false`，2026-10-09 现场报回）

**症状**：操作员在工号/编号/密码输入框里敲字母或组合键时，屏幕上可能弹出 Chromium 的
"是否翻译此页面?" 气泡 —— 那层 UI 不在我们控制里，还会盖住工位界面（键盘过滤放行字母与修饰键，
所以组合操作能触发浏览器功能/刷新，随后才冒出这个气泡）。关掉翻译后，正常的工号/编号/密码输入
与回车操作都保留。

**两层一起关**（少一层都可能从别的路径被触发）：
1. **kiosk 企业策略**：`overlay/usr/share/lsp-kiosk/10-lsp-kiosk.json` 加 `"TranslateEnabled": false`。
   运行期由 `lsp-kiosk.service` 的 `ExecStartPre` 装进 `/etc/chromium/policies/managed/`，
   kiosk 一停就由 `ExecStopPost` 撤掉（这样 board 桌面的浏览器不受影响）。
2. **页面自己声明**：极简着陆页 `overlay/usr/share/lsp-kiosk/loading.html` 与上位机模板
   `app/templates/index.html` 都写 `translate="no"` + `<meta name="google" content="notranslate">`。
   ⚠️ 上位机那份在**上游 oem-host 仓库**里（本仓库只有 `overlay/opt/oem-host/` 快照）⇒ 改完必须
   `tools/sync-oem-host.sh` 刷新快照，否则重编镜像会把它丢掉。

⚠️ **别指望"兼容启动脚本"**：仓库里那个启动脚本带着 `--disable-features=...Translate`，但现场走的是
`lsp-kiosk.service` 的 ExecStart（`/usr/bin/chromium` 包装脚本）⇒ 那个参数**根本不会作用到屏幕上**。
这也是这次一开始"看起来已经关了、其实没关"的原因。

**判据**：`tools/verify-image.sh` 检查策略里的 `"TranslateEnabled": false`、着陆页的两项
禁翻译声明，以及 `/opt/oem-host/app/templates/index.html` 的 notranslate 声明。
现场复核：`grep TranslateEnabled /usr/share/lsp-kiosk/10-lsp-kiosk.json` 与（kiosk 在跑时）
`grep TranslateEnabled /etc/chromium/policies/managed/10-lsp-kiosk.json` 都应为 `false`。

**这条残留风险已堵（2026-10-09，同一轮）**：键盘白名单改成**严格**语义 —— 修饰键也要写进 `ALLOW`
才转发给会话，所以 `Ctrl`/`Shift` 不再"永远放行" ⇒ 浏览器**收不到任何组合键**（`Ctrl+R` 只会变成
一个光秃秃的 `r`，无害）。过滤器内部仍跟踪修饰键 ⇒ `BLOCK` 里的组合规则照旧命中（`ctrl+w` 依旧
吃掉 W）。代价：`ALLOW` 里不写 `shift` 就打不出大写/符号 —— **现场写了 `shift`**（大写/符号可用），
Ctrl/Alt/Meta 没写。语义与实测见
[keyfilter.md](keyfilter.md)。

## 策略**只对 kiosk 会话生效**（2026-09-23 用户要求）

> 起因（用户 2026-09-23 报回来）：切到 **board 的 GNOME 桌面**后，Chromium 里 **F12 打不开 DevTools**、
> **浏览器只能上白名单里那 3 个网址**。查下去发现是 [escape-hardening.md](escape-hardening.md) 第 5 条那份策略 —— 它当时被**预置**进了
> `/etc/chromium/policies/managed/`。

**根因**：Chromium 的**强制**策略只有**系统级**目录这一条路（`/etc/chromium/policies/managed/`），
**没有"按用户 / 按会话"的策略目录** ⇒ 桌面上的 Chromium 读的是同一份 ⇒
`DeveloperToolsAvailability=2`（F12 禁掉）与 `URLBlocklist:["*"]`（只能上白名单）**一并作用到桌面**。
（对照组：**按键过滤器 `apps/lsp-keyfilter` 是 VT 限定的，桌面上实测放行** —— 所以那次"桌面被拦"与它无关。）

**做法**：既然 kiosk 与桌面**互斥**（`lsp-kiosk.service` 有 `Conflicts=gdm.service`），就让策略
**只在 kiosk 会话存在的那段时间装在 /etc**：

| 位置 | 干什么 |
| --- | --- |
| `overlay/usr/share/lsp-kiosk/10-lsp-kiosk.json` | 策略文件**放这儿**（镜像里**不再**预置到 `/etc/chromium/policies/`） |
| `lsp-kiosk.service` `ExecStartPre=+install …` | 会话起来时装进 `/etc/chromium/policies/managed/` |
| `lsp-kiosk.service` `ExecStopPost=+rm -f …` | 会话结束（含被 gdm 挤掉）时撤掉 |
| `gdm3.service.d/10-no-kiosk-policy.conf`（新增） | 桌面启动前再 `rm -f` 一次，兜住"kiosk 崩溃留下残留" |

⛔ **那三条必须带 `+` 前缀**：systemd 会把 `User=kiosk` **也应用到 `Exec*Pre`/`Exec*Post`** ——
不带 `+` 时它们是 kiosk 身份跑的，往 root 的 `/etc/chromium/...` 写会
`install: … status=1/FAILURE` ⇒ **整个 kiosk 单元起不来**（日志里 `Start request repeated too quickly`）。
`+` = "这一条以完整权限运行、忽略 `User=`"。2026-09-23 实测踩到，`verify-image.sh` 已加断言要求那个 `+`。

**板上实测（2026-09-23，开发板 `<HOST_07>`）**：

| 动作 | 结果 |
| --- | --- |
| 切到 kiosk（`disable --now gdm3` + `reset-failed` + `start lsp-kiosk`） | `kiosk=active`、`cage` 起来、活动 VT `tty1`；**策略文件已自动装上** ✓（内容确是 `DeveloperToolsAvailability=2` + `URLBlocklist=['*']`） |
| 切回桌面（`enable --now gdm3`） | `gdm3=active`、活动 VT `tty2`；**策略文件已自动撤掉** ✓ |

> 注：上面两行动的是这次 A/B 实验用的**手工**命令。正式路径是 `sudo lsp-maintenance on|off`
> （[maintenance-mode.md](maintenance-mode.md)），它只 `start/stop gdm3`、**不 enable**；`enable` 会让重启也进桌面，别在产品机上用。

⚠️ **桌面上"已经在跑"的浏览器仍用旧策略** —— Chromium 只在**启动时**读策略 ⇒ 改完要**重启浏览器**才生效
（这也是用户报"还是不行"的一个可能原因）。

📌 顺带记：`systemctl stop lsp-kiosk` 会把 cage 打成 SIGABRT、单元停在 `failed`（老坑 [session-deployment.md](session-deployment.md) 第 10 条（原文档误编号为 §7）），
回 kiosk 要 `reset-failed` 再 `start`：

```bash
sudo systemctl disable --now gdm3 && sudo systemctl reset-failed lsp-kiosk && sudo systemctl start lsp-kiosk
```
