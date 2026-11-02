# kiosk 按键过滤与 NumLock 配置

> 默认由 Cage 内建过滤键盘事件；严格白名单包含修饰键，黑名单优先。独立 evdev 守护保留用于手动排查，默认不启用。

## TL;DR

- 配置文件 `/etc/default/lsp-keyfilter`；修改后重启 `lsp-kiosk`。
- `LSP_KEYFILTER_ALLOW` 非空时仅放行列出的键；Ctrl/Alt/Shift/Meta 也要显式列出。
- `LSP_KEYFILTER_BLOCK` 先判定组合或单键；即使目标键在 ALLOW 内，命中 BLOCK 仍被拦。
- `LSP_KEYFILTER_NUMLOCK_ON=1` 由 Cage 在会话启动及键盘热插拔时开启 NumLock。
- 默认 ALLOW 包含主排数字、字母、小键盘数字、回车、退格、锁定键、方向、minus、shift。

## 参数与规则语法

| 参数 | 作用 |
| --- | --- |
| `LSP_KEYFILTER_ALLOW` | 空格分隔的单键、范围或别名组；空值关闭白名单 |
| `LSP_KEYFILTER_BLOCK` | 空格分隔的单键、范围、别名组或 `ctrl+w` 等组合；空值关闭黑名单 |
| `LSP_KEYFILTER_NUMLOCK_ON` | 1 自动开启，0 不自动开启；物理 NumLock 按键仍受 ALLOW 控制 |
| `LSP_KEYFILTER_DEBUG` | 1 打印逐键过滤日志，默认关闭 |
| `LSP_KEYFILTER_VT` | 仅 evdev 守护使用；默认 tty1，空或 0 表示全局过滤 |
| `LSP_KEYFILTER_COMPOSITOR` | 仅 evdev 守护使用；按进程名识别会话重建，空或 0 关闭该补按路径 |

规则名大小写不敏感。范围支持 `a-z`、`0-9` 和 `f1-f12`；
字母与 F11/F12 的 evdev 键码不连续，实现按明确键名表展开。
组合键最后一个元素是目标键，前面是须同时按住的修饰键；左右修饰键均可匹配。
BLOCK 吃掉目标键的按下、重复、抬起，修饰键是否转发由 ALLOW 决定。

| 别名组 | 展开 |
| --- | --- |
| `digits`、`letters`、`alnum` | 主排数字、字母、两者合并 |
| `func` | F1–F12 |
| `keypad` | 小键盘 kp0–kp9，与主排数字不同 |
| `arrows` | up、down、left、right |
| `edit` | enter、backspace、delete、tab、space、esc、home、end、pageup、pagedown |
| `mods` | 左右 Ctrl、Alt、Shift、Meta |
| `ctrl`、`alt`、`shift`、`meta` | 对应左右两个修饰键；meta 也可写 super、win |

常用单键名：

```text
a-z 0-9 f1-f12 esc tab enter space backspace delete insert
home end pageup pagedown up down left right menu capslock numlock
printscreen pause minus equal comma dot slash backslash semicolon apostrophe grave
kp0-kp9（逐个写 kp0 … kp9，或使用 keypad） kpenter kpdot kpplus kpminus kpslash
```

可用同义词包括 escape/esc、return/enter、del/delete、sysrq/printscreen。
符号键使用 `minus` 等键名，不能直接写裸 `-`。

```ini
LSP_KEYFILTER_ALLOW="0-9 a-z keypad enter backspace capslock numlock arrows minus shift"
LSP_KEYFILTER_BLOCK="ctrl+w"
LSP_KEYFILTER_NUMLOCK_ON="1"
```

该配置允许 Shift 输入大写与符号；Ctrl/Alt/Meta 不转发，Ctrl+R、Ctrl+F 无法作为组合到达浏览器。
内部仍追踪物理修饰状态，`ctrl+w` 会继续阻挡 W。若要连 Ctrl+R 的目标 R 也丢掉，须加入 BLOCK。
未列出的 Tab、空格、ESC、功能键等均被拦；HMI 新增输入需求时按需补齐。
不列出 Shift 时，Shift 大写与符号不可用；不列出 Ctrl 时，Ctrl+C/V 不可用。

## Cage 与 evdev 守护的分工

Cage 仅处理自己的会话输入，使用独立客户端 XKB 状态。
物理状态用于组合判定；只有放行按键更新客户端状态，focus enter 与 modifiers 也使用过滤状态，
避免 Ctrl 事件被拦后仍泄露 Ctrl 状态。每个键盘组独立跟踪，NumLock 锁定和 LED 随键盘更新。
实现见 [compositor-customization.md](compositor-customization.md)。

自动 NumLock 补按仅更新物理成员，不作为用户按键转发给客户端。客户端从自动操作完成后的
物理锁定状态初始化，真实 NumLock 按键仍按 ALLOW/BLOCK 执行。仅观察 LED 亮不能证明数字输入可用：
旧过滤实现中，同组第二只键盘的补按会再次切换客户端，物理 LED 和组锁定仍开着，客户端却已关闭。
`+lsp6` 分离这两条路径，并以 KP0–KP9 的数字 keysym 验证热插拔前后行为。

evdev 守护通过 `EVIOCGRAB` 独占键盘，将放行事件转发到 uinput 虚拟键盘；
`LSP_KEYFILTER_VT=1` 将过滤限制在 kiosk 活动 VT，其它 VT 原样转发。
Cage 忽略 VT 与 COMPOSITOR 参数。守护退出时关闭 fd，内核释放 grab。
手动诊断可用 `sudo lsp-keyfilter --check`、`--list`、`--tap-numlock`；
补按模式检查自身 uinput 设备的 LED，确认事件确被合成器接收。

## 验证与测试边界

- `tests/cage-keyfilter.sh`：真实 kiosk 输入与 NumLock A/B。
- `LSP_CAGE_SOURCE=<已应用补丁的源码> bash tests/cage-keyfilter-state.sh`：
  真实 seat 函数和 wlroots/XKB 的隔离回归，核对修饰状态、held keys、功能键范围、
  两个键盘成员的 KP0–KP9 数字输出与真实 NumLock 切换。`+lsp6` 在 noble wlroots 0.17.1 / XKB 1.6.0
  下通过 62 项；旧逻辑仅适配函数签名的负控失败 34 项。此项是隔离行为验证，不代替板上输入验证。
- 独立 evdev 测试须只运行一个实例，确认 grab 成功，并在注入前打开事件节点。
  未取得设备或读到另一个同名虚拟键盘的空输出不能判为拦截成功。

板上验证（2026-10-13，LubanCat-5 V2，新 SD 镜像、Cage `+lsp6`）：两只临时 uinput 键盘
经实际 Cage 会话送到独立 Wayland/XKB 客户端，首次连接、第二只热插后两只键盘共 30 个数字、
60 个按下/抬起事件全部符合 KP0–KP9；设备和客户端回收后原 Cage 进程保留。
现场另确认物理键盘开启 NumLock 后，小键盘可正常输入数字。

NumLock 不通过 kiosk 的 ExecStartPost 补按，相关终端挂断约束见 [cage-sighup.md](cage-sighup.md)。
