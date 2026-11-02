# lsp-config —— 板级配置工具

`/usr/local/sbin/lsp-config` 提供 USB 主机查询、WiFi、swap、服务、主机名、密码与 `device.yaml` 的统一入口。

它取代的是被我们删掉的厂商 TUI（`/usr/sbin/fire-config`，2225 行 bash + whiptail）。
删它的理由见 `notes/system/system-integration.md` §6；这里只记我们自己这套的设计与边界。

---

## TL;DR

- USB 只提供固定 host/source 状态查询和主机工具；不提供 device 模式或 USB 共享网络入口。
- WiFi、swap、服务、主机名、密码与 `device.yaml` 通过对应子命令管理。
- 设计：**逻辑全在子命令里**，whiptail 只是外壳（§1）；面向运维，菜单要简单（§1.1）。
- 菜单结构见 §2、子命令速查见 §3；WiFi 只连 `example-project` 的约定见 §3.1；操作日志见 §3.2。
- ⛔ 安全边界：有几件事是**刻意不做**的（§4，例如不直接暴露裸 `systemctl`）。

## 1. 设计：逻辑全在子命令里，whiptail 只是外壳

| | 做法 | 为什么 |
| --- | --- | --- |
| 交互层 | `whiptail`（newt） | Debian/Ubuntu 系板级工具的通行做法（raspi-config / armbian-config 都是这套）；本镜像装它**只多 93 KB**——`libnewt0.52` / `libslang2` / `libpopt0` 本来就在（实测 `dpkg -l`） |
| 逻辑层 | 每个功能都是一个**子命令**，纯文本输出 | 没终端、没装 whiptail 也能用；ssh 一行命令、脚本/上位机可直接调；也便于回归测试 |
| 菜单 | 只负责收集参数，然后调同一个子命令 | 界面和逻辑不会各写一份、各改一遍 |

```bash
sudo lsp-config                  # 进菜单（不是 root 会自动 sudo 重入）
lsp-config status                # 状态总览（不需要 root）
lsp-config usb status            # 固定 USB host/source 状态
```

> 不做 `zenity`（图形对话）：板子经常只挂串口/HDMI 控制台，GUI 对话框在那里没用。
> 实测菜单能在 `TERM=linux` 的串口/伪终端里正常渲染（见 §5 实测记录）。

## 1.1 面向运维的设计原则（2026-09-12 用户要求：简单易懂，别像 nmtui 那样绕）

1. **标签说"做什么"，术语放括号里当提示**：写「看 USB 主机状态」，明确这是查询。
2. **层级尽量浅、但按"会不会改东西"分层**：主菜单只有 5 项 + `0 退出`，
   全是"看 / 进下一层 / 重启"这类不动配置的入口；**改配置的动作收进对应的子菜单**
   （WiFi 密码在「网络」里，交换空间/主机名在「系统维护」里）。
   这样运维随手按主菜单不会误改东西。（2026-09-12 用户要求：去掉主菜单改 board 密码和 WiFi 密码，
   WiFi 密码挪进网络菜单。）
3. **一次确认说清后果**：每个会改东西的动作在确认框里写明改哪个文件 / 会不会断连 / 要不要重启。
4. **危险动作二次确认**：删 WiFi 连接、关掉交换空间、重启/关机，都是"选一次 + 确认一次"。
5. **没有惊喜**：改动只发生在你选的那一项上；不改 DT、不重绑 UDC、不碰厂商栈（见 §4）。
6. **每次调用都留痕**（见 §3.2），运维能事后查"谁在哪台机器上改了什么"。

## 2. 菜单结构

> **配色（2026-09-12 用户要求：照内核 menuconfig 来）**：whiptail/newt 默认底色是 **magenta（紫）**，
> 刺眼。脚本里设了 `NEWT_COLORS`，**逐条照搬内核 kconfig 的默认主题** ——
> 源码 `scripts/kconfig/lxdialog/util.c`：`set_classic_theme()`（第 58–88 行）打底，
> 未设 `KCONFIG_THEME` 时用 `set_bluetitle_theme()`（第 133–145 行）覆盖 title/tag 为蓝色。
>
> | 内核元素 | 内核配色（fg/bg，bold） | newt 元素 | 我们设的值 |
> | --- | --- | --- | --- |
> | screen（整屏底） | cyan / blue, bold | `root` `roottext` `helpline` | `brightcyan,blue` |
> | dialog 主体 | black / white | `window` `textbox` `label` | `black,lightgray` |
> | title | **blue / white, bold** | `title` | `brightblue,lightgray` |
> | border | white / white, bold | `border` | `white,lightgray` |
> | **menubox / item** | **black / white** | `listbox` `actlistbox` | `black,lightgray` ← **菜单区是黑字灰底** |
> | item_selected | white / blue, bold | `actsellistbox` `sellistbox` | `white,blue` |
> | **button（焦点）** | white / blue, bold | `button` | `white,blue` |
> | button（非焦点） | black / white | `compactbutton` | `black,lightgray` |
> | inputbox | black / white | `entry` | `black,lightgray` |
> | shadow | black / black | `shadow` | `black,black` |
>
> newt 的颜色名：`lightgray`＝普通白（内核的 `COLOR_WHITE`，终端上呈浅灰）、`white`＝亮白、
> `brightcyan`/`brightblue`＝加粗（对应内核 `DLG_COLOR(...)` 的 bold 标志）。
>
> ⚠️ **踩过的坑 1：别把菜单区也刷成蓝色**。第一版把 `root/window/listbox` 全刷成 cyan，
> 结果菜单区和底色一个色、黑字糊在青底上（用户："连菜单的背景颜色也改了，根本看不清"）。
> 蓝色只用在两处：**整屏底**（配亮青字）+ **选中项/焦点按钮的蓝块**。
>
> ⚠️ **踩过的坑 2：whiptail 对话框按钮的 colorset 是"反"的**。用户反馈"按 TAB 切 Ok/Cancel 不
> 高亮"，实测 + 查 newt 源码得出结论：
> - **获得焦点**的按钮用 **`button`**，**失去焦点**的用 **`compactbutton`**；
>   **`actbutton` whiptail 根本不读**（把它设成红底，屏幕上一点红都没有）；
> - newt 内置默认调色板（`newt.c` 的 `newtDefaultColorPalette`）里
>   `button = lightgray,red`、`actbutton = red,lightgray`、`compactbutton = white,blue`
>   —— 默认界面里那个"红块"就是焦点按钮，来源是 `button`；
> - 所以要让 TAB 切换看得出来：`button` 给**焦点**按钮（醒目色），`compactbutton` 给**非焦点**
>   （与窗口同色，看起来就是普通文字）。
> 实测证据（伪终端抓 SGR，`--yesno` 对话框）：
> ```
> 按 TAB 前:  <Yes>=ESC[44m（蓝底=焦点）  <No>=ESC[47m（灰底）
> 按 TAB 后:  <Yes>=ESC[47m（灰底）        <No>=ESC[44m（蓝底=焦点）
> ```
> 其它实测：`root` 段 `[44m`（蓝底）+`[96m`（亮青字）✓、`[46m`（青底）0 次 ✓、`[45m`（紫底）0 次 ✓、
> 菜单文字 `[30m`（黑字）/`[47m`~`[107m`（浅灰底）✓。
>
> - 想换配色：`sudo NEWT_COLORS='root=brightcyan,blue:button=black,yellow' lsp-config`
>   （脚本只在 `NEWT_COLORS` 为空时用默认值）。可用元素名：`root roottext border window shadow
>   title button actbutton checkbox actcheckbox entry disentry label listbox actlistbox sellistbox
>   actsellistbox textbox acttextbox compactbutton helpline`。
> - 需要 whiptail/newt（镜像里已装，`whiptail 0.52.24`）—— **不需要新装任何包**。

```
LSP 配置工具（Embedfire LubanCat-5 V2）
提示：常用：1 = 看状态，3 = 网络（WiFi 只连 example-project）。改动都会记到 /var/log/lsp-config.log

 1  看状态（IP / WiFi / USB / 交换空间）
 2  USB 主机状态（固定 source + host）
 3  网络（WiFi 状态 / WiFi 密码）
      WiFi：看状态（连的哪个、IP、有没有多余的连接）
      WiFi：只保留 example-project（删掉其它连接）
      WiFi：设置密码（一般不用改）        ← 密码挪到这里（原在主菜单）
 4  系统维护（交换空间 / 服务 / 主机名 / 设备配置 / 日志）
      交换空间：看状态 / 改大小（立即生效）/ 关掉并删除
      服务：看状态启停（lsp-swapfile / lsp-growroot / lsp-usb-powerfix）
      改主机名 / 设备配置 device.yaml（看 / 改）/ 看操作日志（最近 50 行）
 5  重启 / 关机
 0  退出
```

> **主菜单严格保持"不改东西"**：`1`/`2` 是只读状态，`3`/`4` 进入子菜单，`5` 才有二次确认的
> 重启/关机。改 board 登录密码**从菜单里彻底去掉**（`passwd` 只作为子命令保留，见 §3），
> WiFi 密码也不在主菜单 —— 用户明确要求"没什么事别去改密码"。
> 主菜单编号是稳定的，所以提示行直接写"常用：1 = 看状态，3 = 网络"——
> 运维最常用的两条（看状态、确认只连 `example-project`）分别只要 1 步和 2 步。

> ⚠️ **踩过的坑 3：菜单标签写了就必须有对应的 `case` 分支**。第一版重构时主菜单列了 `0 退出`
> 但 `case "${c}"` 里漏了 `0)`，于是**选"退出"什么也不做、菜单无限重画**
> （用 stub 替掉 whiptail 点菜单时才发现：`/tmp/stub.log` 里主菜单被调用了 1900 多次）。
> 现在写的是 `0|"") return 0 ;;` —— 顺带把"返回空值"也当成退出，免得再出现死循环。
> 复现/回归方法见 §5 最后一条。

> 「交换空间」就是 swap：**不写 swap 字样**是为了让不懂的人也能看懂；括号里保留术语方便对照。
> Type-C 固定 source；`lsp-config power` 只查角色，`power source` 只确认，不执行 role swap。

## 3. 子命令速查

| 子命令 | 作用 | 要不要 root |
| --- | --- | --- |
| `status`（`info`） | 板卡、USB 主机角色、WiFi、swap、启动配置与 ADB 状态 | 否 |
| `usb [动作]` | 转发 `lsp-usb` 的主机查询与重新枚举；不提供 device 模式 | reenumerate 需 root |
| `power [source]` | 查询固定 source 状态或幂等确认；`sink`/`auto` 被拒绝 | 否 |
| `wifi [status\|pass [新密码]\|only]` | WiFi：看状态 / 改 `example-project` 的密码（不带参数则交互输入，不回显；管道则读一行 stdin）/ 删掉非 `example-project` 的连接 | status 否 |
| `swap [status\|size <MiB>\|apply\|off]` | `size` 写 `/etc/default/lsp-swapfile`（菜单「系统维护 → 2」）；`apply` 先 `swapoff`+删文件再跑 `lsp-swapfile.sh`；`off` 关闭并删除并把大小置 0 | 否 / 是 |
| `svc [list\|status\|start\|stop\|restart\|enable\|disable] <单元>` | 只允许 `lsp-swapfile`、`lsp-growroot`、`lsp-usb-powerfix` 三个单元 | list 否 |
| `host [新主机名]` | `hostnamectl` + 同步 `/etc/hosts` 的 `127.0.1.1` | 改要 root |
| `passwd` | `passwd board`（**菜单里刻意不提供**，避免误改登录口令，见 §2） | 是 |
| `device [show\|edit]` | `/home/board/.lsp/config/device.yaml`（edit 用 `sudo -u board`，别把家目录文件改成 root 所有） | show 否 |
| `log [行数]` | 看操作日志（默认最近 50 行）；菜单「系统维护 → 7」里也有入口 | 读日志要 root |
| `reboot` / `poweroff` | 重启 / 关机（菜单第 5 项，带二次确认） | 是 |
| `help` / `--version` | 用法 / 版本 | 否 |

> 菜单只暴露运维常用的那些；`power` 查询、`svc status` 之类的细节留在命令行，
> 免得菜单越堆越长。菜单项选完就是调**同一个子命令**（通过 `dispatch`），行为完全一致、也照样记日志。

## 3.1 WiFi：约定设备只连 `example-project`

**为什么**：设备连到别的 SSID 就可能落到别的网段 —— 内网里找不到它（SSH 上不去、上位机连不上）。
所以本工程把"只连 `example-project`"当成约定，而不是"顺手能连上的都连"：

- 构建期由 `WIFI_SSID`（默认 `example-project`）写死一份 NM profile（`id`/`ssid` 同名、
  `autoconnect-priority=100`，keyfile 0600 root:root）。
- 运行期 `lsp-config wifi pass` 改的是**同一份 profile** 的 `psk`，不新建别的 SSID：
  `nmcli connection modify example-project wifi-sec.key-mgmt wpa-psk wifi-sec.psk <新密码>`，
  改完 best-effort `nmcli connection up`（AP 不在范围内会失败并说明，密码仍然存下了）。
  若那份 profile 不存在，才按同样的结构 `nmcli connection add` 建回来。
- `wifi status` / 主菜单的状态总览会**标出任何非 `example-project` 的 WiFi 连接**并给出
  `wifi only` 的建议；`wifi only` 会删掉它们（`example-project` 那份一定保留）。
- `tools/verify-image.sh` 也加了检查项：镜像里只要出现非 `example-project` 的 WiFi profile 就判失败。
- ⚠️ **改密码会重新关联**：如果此刻正是通过 WiFi 连进板子的（本开发板的管理地址就是走 WiFi 的），
  这个连接会断一下 —— 命令会先打印告警，菜单里也会提示。要避免的话走串口/有线再改。
- 想换 SSID 是**构建期**的事（`WIFI_SSID=`），不提供运行期改 SSID 的子命令 —— 免得有人随手
  把设备连到别的网络上。

### 为什么连上后接口叫 `p2p0` 而不是 `wlan0`（2026-09-12 查清，**不要"修"它**）

本板无线是 **RTL8822CE**（PCIe `10ec:c822`），用的是 Rockchip BSP 里的 Realtek 外挂驱动
`8822ce.ko`。它以 **Android 平台配置**编译（`include/autoconf.h` 与 `Makefile` 里的
`CONFIG_CONCURRENT_MODE` + `CONFIG_PLATFORM_ANDROID`），于是**一个射频注册两个 vif**：

```c
/* os_dep/linux/os_intfs.c */
char *ifname  = "wlan%d";                 // 第一个接口（主接口）
#ifdef CONFIG_PLATFORM_ANDROID
    char *if2name = "p2p%d";              // 第二个接口 → p2p0
#else
    char *if2name = "wlan%d";             // 非 Android 平台会是 wlan1
#endif
```
板上实测：`/sys/class/ieee80211/` 只有 `phy0`，两个口挂在**同一个** PCI 设备下
（`0004:41:00.0`）⇒ 同一颗卡、同一根天线；MAC 只差**本地管理位**
（`wlan0 02:00:00:00:00:07` / `p2p0 02:00:00:00:00:08`，`dc|0x02=de`）。
`nmcli dev status` 里另两个 `p2p-dev-p2p0` / `p2p-dev-wlan0` 是 wpa_supplicant 给每个口
建的 P2P-device 接口，属上层记账。

因为预置 keyfile 没写 `connection.interface-name`，NM 抓到哪个口就用哪个，**实测落在 `p2p0`**
（`device (p2p0): Activation: successful`，`p2p0` 拿 `<HOST_07>/22`，`wlan0` 保持 DOWN）。

⛔ **不要改成 `wlan0`、不要把 `interface-name` 写死**（2026-09-12 用户明确："我们之前的设备用的
都是 p2p0，别改了"）。速度上也没有理由不同：两个 vif 共用同一套 HALMAC、TX/RX ring、速率自适应和
天线 —— 具体到 `CONFIG_PCI_TX_POLLING`（本驱动已开）这条唯一的"主/次接口差异"：
`rtw_tx_poll_init()` 只对 **primary** 生效、且 TX-done 中断被从中断掩码里去掉，
但 `hal/hal_halmac.c:_drv_enable_trx()` 里 `adapter = dvobj_get_primary_adapter(d)`
**总是拿 primary 的 adapter 对象**去 `rtw_intf_start()`，定时器随后轮询的是 `dvobj` 里**全部 6 个
HW 队列**（`rtl8822ce_tx_poll_handler`）—— 所以哪怕 `wlan0` 是 DOWN、流量全在 `p2p0` 上，
收 TX 完成的还是那个设备级定时器，不存在"次接口没人收包"的问题。
`lsp-config` 的 `wifi_iface()` 因此写成"先挑**已连接**的那个口，再退到任意 wifi 口"，
两种命名都不会踩空。

## 3.2 操作日志（溯源：谁在哪台机器上改了什么）

**位置**：`/var/log/lsp-config.log`，权限 **0640 root:adm**，超过 512 KB 自动轮转成 `.log.1`（只留一份，
不依赖 logrotate/定时器）。可用环境变量 `LSP_CONFIG_LOG` 改路径（测试用），`LSP_CONFIG_NO_LOG=1` 关掉。

**记什么**：

| 类型 | 什么时候写 | 例子 |
| --- | --- | --- |
| `调用` | 功能调用（命令行与菜单都算；help/version 只读、不记日志） | `调用 user=board from=<HOST_06>(pts/0)  wifi pass <密码已隐藏>` |
| `修改` | 只在该动作**真的改了东西**时，带上前后值 | `修改 user=board from=…  SWAP_SIZE_MB 2048 → 3072（/etc/default/lsp-swapfile）` |
| `失败` | 参数非法 / 没权限 / 命令返回非 0（`die` 里也会写一行） | `失败 user=board from=…  wifi pass <密码已隐藏> → 需要 root：sudo lsp-config wifi pass` |

**谁 / 从哪来**：`user=` 取 `SUDO_USER`（sudo 场景）→ `USER` → `id -un`；
`from=` 取 `SSH_CONNECTION` 的来源 IP，**sudo 会清掉它**，所以再退到 `who am i`
（形如 `board pts/0 2026-09-12 12:36 (<HOST_06>)` → 记成 `from=<HOST_06>(pts/0)`）；
串口/本地登录没有括号时记 `from=本地(ttyS2)`。

⛔ **密码永远不记**：`wifi pass` 与 `passwd` 只记"密码已更新"，参数里出现密码时也会被
`redact_args()` 换成 `<密码已隐藏>`（实测 grep 日志 0 命中）；密码本身也**不走命令行参数**
（菜单里经 stdin 传给子命令，`ps` 里看不到）。

**已知限制**（写清楚免得误判）：日志是 0640 root:adm，**非 root 的调用写不进去**（写失败静默跳过）。
不用 sudo 就改不动任何配置（改配置的动作全部 `need_root`），所以"到底有没有被误改"这条线是完整的；
但"有人试了没成功/只是看了看"只在你用 sudo 跑时才有记录。

**怎么看**：`sudo lsp-config log`（最近 50 行）或菜单 → 系统维护 → 看操作日志。

服务启停、swap 重建/配置写入、hosts 同步和编辑器失败均返回非零，不记成功。
隔离回归见 `python3 tests/sbin-config.py`，只操作临时配置与服务桩。

出厂恢复用 `lsp-reset-config [--dry-run]`：先备份现场配置，再全部暂存后逐个原子发布。
备份或暂存失败不发布；发布中途失败会报已恢复数量，并保留 `/var/backups/lsp-reset-config/` 下的备份。
它不提供跨文件事务，udev reload/trigger/settle 失败也返回非零。

## 4. 安全边界（刻意不做的事）
- **不切换 USB device 或供电角色**：Type-C 固定 source + host；主机工具说明见
  [usb-gadget.md](../usb/usb-gadget.md)。
- **不做 SD↔eMMC 的 `dd` 备份/烧录**（厂商 TUI 的 P5）：误操作就是毁数据，需要时手动做。
- **不做基于 `xrandr` 的屏幕旋转**（厂商 TUI 用的是它）：本镜像是 **Wayland**（`ubuntu-wayland`
  会话 + GDM3），`xrandr` 改不动；真要旋转得走 GNOME 的显示配置或内核 cmdline。
- **不做引脚复用/设备树**：那要改启动分区 `ubuntuEnv.txt` 的 `overlays=`，属于下一步（见 §6）。
- **不启动厂商 device 栈**：`svc` 有白名单，`usbdevice.service` 不在其中，并被构建期 mask。

## 5. 验证入口

- 隔离回归：`python3 tests/sbin-config.py`，检查服务失败、配置写入、审计与交互分支。
- USB 主机接口回归：`python3 tests/usb-host-tool.py`，检查只读角色查询、旧 device 命令拒绝与重新枚举恢复。
- `lsp-config --help` 与 `lsp-config usb --help` 应只列当前主机接口；旧 device 和共享网络入口拒绝执行。
- 镜像体检：`tools/verify-image.sh` 检查主机工具、厂商 device 服务屏蔽、WiFi 配置与旧配置残留。
- 菜单改动需用 whiptail 桩核对标签和分支对应，尤其返回 / 退出与危险操作的确认路径。
- 重新枚举会中断外设；隔离回归不能代替板上相机或 Quest 恢复验证。

## 6. 加一个新条目要动哪里

1. `overlay/usr/local/sbin/lsp-config` 里加一个 `cmd_xxx()` + 在 `dispatch()` 的 `case` 里挂子命令；
2. 需要交互的再加一个菜单项（`ui_menu` + 调 `dispatch xxx`，**不要直接调 `cmd_xxx`**，
   否则绕过日志）；
3. **会改东西的动作必须在成功路径写一行 `log_event 修改 "<改了什么，带前后值>"`**，
   密码类一律不记（见 §3.2）；
4. 若引入**新文件/新包**，按 `AGENTS.md` §7「四处同步」：`overlay/` → `scripts/build-lsp-image.sh`
   的显式 `install -m`（包则进 `BASE_PACKAGES`）→ `tools/verify-image.sh` 检查项 →
   `notes/system/system-integration.md` 登记；
5. 本文件 §2/§3 的表格同步更新。

## 7. 下一步（候选，未做）

- **设备树 / overlays 开关**（厂商 P1/P2 对应的部分）：读写启动分区 `ubuntuEnv.txt` 的 `overlays=`，
  列出可用 dtbo 并开关。注意我们已经踩过"改错就静默回退原始 DTB"的坑，所以要做就得先备份、
  校验 dtbo 存在、并提示重启 —— 实现前先定这个流程。
- **zram**：比磁盘 swap 更适合 eMMC（优先级给高值、swapfile 给 0，做成"先压内存再落盘"）。
- **屏幕旋转**：Wayland 下要另找办法（GNOME `monitors.xml` / 内核 cmdline），先确认目标显示设备。
