# 另一类"六路全断"：单路外设偶发断连被程序侧放大

> **一句话**：屏幕上那"六路全断"**不是六路都坏了** —— 是"一路外设短暂断连/静默"被程序侧四条逻辑
> （错误位 OR 锁存、单路超时判整机、ERROR 态长按=关机杀脚本、位不会自愈）放大成整机故障，
> 再叠加"六路外设全走 USB、一路线缆能带掉一整组"。**与 logind `RemoveIPC` 那条是两回事**，见
> [collection-dds-shm-fault.md](collection-dds-shm-fault.md)。

## TL;DR

- 四条放大点（都在 `lcd_voice_bridge` / `data_collection_trigger` 里，§2）：
  ① 下发给面板的 0x80 错误快照是 **OR 锁存**，只有**显式关机（进 OVER）**才清零；
  ② 错误位只能靠"新鲜且健康的消息"清除，**没有"源消失就清位"的路径**；
  ③ 触发节点**任何一路超时 3 s 就整机进 ERROR**（没有单路降级档）；
  ④ ERROR 态长按 = **关机**，`tmux.kill_all()` 把四个外设脚本一起杀掉 ⇒ 六路真没了。
- 硬件侧：六路外设**全是 USB**，一路线缆/供电异常可能把同一个 hub/控制器带下去（§3）。
- **现场怎么区分**（DDS/共享内存被删 vs 某一路真掉线）见 §4 口诀；可复现验证（不用拔线、可逆）见 §5。
- ⚠️ 本文是**静态代码排查**（读板子上装的厂商 release `v1.0.20`，只读，**没改一行**）。
  给厂商的最小改动建议（按性价比）见 §6。

## 1. 直接回答："一路相机 3 s 没数据会导致**六路**全断吗？"—— **程序上不会**

把一路相机的 `image_compressed` 停掉 3 s，程序里只会发生：

1. 触发节点 `_check_health` 报 `TOPIC_TIMEOUT` → `_enter_error()` → 状态 `ERROR`（并 `_publish('error')`）。
   **它不杀脚本**（`keyboard_trigger_node.py:1312-1336` 只改状态 + 通知收尾）。
2. LCD 桥收到 `command='error'` → 相位 `ERROR_RUN`，下发 `阶段=故障运行(6) | 语音=出现错误(5)`。
3. LCD 桥的**错误位**来自它自己的 `error_monitor`：只有**那一路**（相机最多三路）会置位，
   其余外设只要还在发数据就是健康的 ⇒ **位保持 0**。

代码层面**没有任何"一路错 → 置其它路"的路径**（`apply()` 是每个源各自映射到自己那一位）；
唯一会"批量置位"的是 `phase_fsm.check_startup()` 的启动窗口超时（§2.6），而且置的也只是"没在窗口内报到"的那几路。
旁证：12:58 那次故障里 mask 是 `0x0E`（三相机）→ `0x3E`（+双手套）→ `0x3F`（+Quest），**按各自 3 s 超时逐段涨**，不是一次全置。

⚠️ 一个容易踩的陷阱：**三路相机跑在同一个进程里**（`triple_camera_mjpeg_node`），
所以"相机"最少 1 位、常见是 **3 位一起**；但它仍然不是六位。

**那屏幕上的"六路全断"怎么来的？只有三条路**（至少满足一条）：

| # | 条件 | 判据 |
| --- | --- | --- |
| A | **六个发布端真的都停了**：hub/控制器被一路 USB 带下去（§3），或传输层坏了（`RemoveIPC`，见另一篇） | `journalctl -k` 有 `over-current`/`reset`/`error -71`；或 `/dev/shm` 里 fastrtps 对象消失 |
| B | **现场长按**把它放大了：ERROR 态长按 = 关机 → `_kill_scripts()` → `tmux.kill_all()` 杀光四个脚本（§2.5） | 触发节点日志里那次 `长按/long` + 脚本收尾记录 |
| C | **开机窗口里多路没报到**：STARTING 60 s 到点，`check_startup` 把 `not_ready` 一次性置位（§2.6） | `启动检查超时 \| timeout=60s \| 未就绪=…` |

除此之外还有一条**视觉陷阱**：位是**累积锁存**的（§2.1），长时间运行中相机偶发一次、手套偶发一次、
Quest 偶发一次……位会一位一位攒到 `mask=0x3F`，但每一路都是**各自**偶发的结果，不是被相机带坏的。

## 2. 程序里的放大点

板子上路径：`/opt/data_collection/current/lib/python3.12/site-packages/<包>/`。

### 2.1 0x80 错误快照是 OR 锁存，只有"显式关机"才复位 ⭐ 最关键

`lcd_voice_bridge/lcd_voice_bridge_node.py:488-506`：

```python
def _send_error_if_needed(self, periodic=False):
    """按 v3.0 发送 0x80：首次非零后锁存，外设恢复不清零，显式关机才复位。"""
    latched = bytes(old | current for old, current in zip(self._latched_error_payload, payload))
    self._latched_error_payload = latched
    # 正常恢复不会把 latched 变为零；只有显式关机进入 OVER 才会清空锁存。
```

- 复位点全项目只有两处（`grep _latched_error_payload`）：初始化 `:138`、**进入 OVER**（`_on_enter_over`，`:300`）；
  而 OVER 只由 `scripts_stop`（长按关机）触发（`phase_fsm.py:45/90`）。
- ⇒ **只要有一路曾经超过 `peripheral_timeout_sec`（板上 3.0 s）没数据、或发过一条 `ok=False`，
  它的位就永久留在屏幕/面板上**，设备随后恢复也不消 —— 这就是"线是好的也触发、而且不自愈"的直接原因。
- 而 `lcd_voice_bridge_node.py:368`：`if status in RUNNING_PHASES and any(self._latched_error_payload): 强制 ERROR_RUN`
  ⇒ 锁存里有任何一位，运行阶段就**一直显示故障运行**。
- 现场日志形态也对得上：`错误下发` 是**累加式**的（后一条包含前面所有位），12:58 那次就是
  `相机 3 路 + 手套 2 路` → 再加 `Quest` = `0x3F`。

### 2.2 错误位本身也只能靠"新鲜健康消息"清除

`error_monitor.py`：`apply()` 用 `report_glove/report_camera/report_quest` 逐位 `set_error(flag, not ok)`，
而 `evaluate()` 对"静默超时"的源返回 `True`（= 错误）。**没有"源消失/长期无数据就清位"的路径** ⇒
发布端一旦停了（脚本被杀、节点崩、线掉了），位就再也清不掉，只能靠 2.1 的显式关机。

### 2.3 触发节点：单路 3 s 超时 = 整机 ERROR，没有单路降级

`keyboard_trigger_node.py:1248-1310`（`_check_health`，周期 `health_check_period_sec=0.5`）：

```python
for topic, last_seen in self._topic_last_seen.items():
    if now - last_seen > self.params.health_topic_timeout_sec:      # 板上 3.0 s
        self._enter_error('TOPIC_TIMEOUT', f'Required topic {topic} timed out: ...')
        return
```

必需话题（`params.py:188-206`）= **3 路 `/sensors/cameras/*/image_compressed` + 2 路 `/sensors/gloves/*/raw`
+ Quest 的 `controller_poses`/`head_pose`/`status`** —— 任一路抖 3 s 就把整个流程判死，
**没有"只隔离这一路、其它照跑"的档位**。

### 2.4 进入 ERROR 会发 `command='error'`，而相机节点把它当"停止录制"

- 触发节点 `_enter_error()`（`:1312`）→ `self._publish('error', code, message)`。
- 相机节点 `triple_camera_mjpeg_publisher/triple_camera_mjpeg_node.py:33-34`：

```python
# Trigger commands (TriggerEvent.command) that should stop any active recording.
_STOP_COMMANDS = ('collect_stop', 'collect_preroll_failed', 'scripts_stop', 'stop', 'error')
```

⇒ **手套或 Quest 出一次错，三路相机的 MP4 录制会被一起停掉**（只停录制、不清发布，但已经跨设备耦合）。

### 2.5 ERROR 态下"长按"= 关机，会把四个外设脚本全部杀掉 ← 人机闭环把一路放大成六路

`keyboard_trigger_node.py:313-322`：

```python
elif kind == 'long':
    if state == STATE_IDLE:
        self._launch_scripts()          # 开机
    else:  # SCRIPTS_RUNNING / COLLECTING / ERROR
        self._kill_scripts()            # 关机：结束所有脚本 -> IDLE
```

`_kill_scripts()` 最后调 `self.tmux.kill_all()`（`:478`）—— 相机/Quest/左手套/右手套四个脚本是
**同一个 tmux session 的 4 个 pane**，一起被杀 ⇒ 六路真的全没了 ⇒ 屏幕上正好是"六路全断"。
（现场 SOP 恰恰是"看到报错就长按 2 秒重启 ROS 流程"。）

### 2.6 启动校验窗口会批量置位，而且窗口只有 60 s

`phase_fsm.py:188-221`（`check_startup`）：

```python
not_ready = [k for k in STARTUP_REQUIRED_SOURCES if results.get(k) is not False]
if not not_ready:            -> STARTUP_READY -> READY
elif now >= self._startup_deadline:
    bits = ... for k in not_ready: bits |= STARTUP_REQUIRED_SOURCES[k]
    self._raise_error_bits(bits)      # 一次把"未就绪"的那几路全置位
    -> STARTUP_TIMEOUT -> ERROR_RUN
```

- 进 STARTING 时 `error.reset_sources()` 把六路的 `seen` 全清空（`error_monitor.py:174-190`）。
- `results[k] is not False` 包含**两种**情况：`None`（还没上报）和 `True`（报错/静默超时）—— 截止时刻一起判死。
  开机瞬间三路相机本来就要 5~10 s 才出第一帧（板上实测 13:01:56 报过一次相机故障、13:01:57 自恢复），
  窗口里掉一下就直接进 ERROR_RUN。
- 窗口值 = `startup_grace_sec`（板上 60 s），与 `peripheral_startup_grace_sec` 同值；不要改成更小。

### 2.7 没有节点级看门狗

外设脚本的 pane 结尾是 `exec bash`（`tmux_manager.py:124`），脚本/节点死了 **pane 还在、也没人拉起它们**；
触发节点只在 `_launch_scripts()`（长按开机）里起脚本。⇒ 单路进程死掉 = 永久 ERROR，直到人按一次"关机再开机"。

## 3. 硬件侧：六路外设全是 USB，一路线缆能带掉一整组

| 外设 | 总线 | 节点 |
| --- | --- | --- |
| 胸前/左手/右手相机 | USB（V4L2） | 同一个进程 `triple_camera_mjpeg_node` ⇒ 一死就是 3 位 |
| 左手套/右手套 | USB CDC (`/dev/glove_*`) | 各一个 `ble_hand_node` |
| Quest | USB（adb） | `meta_quest_node` |
| LCD 语音桥 | USB CDC (`/dev/lcd_voice_bridge`) | `lcd_voice_bridge_node` |

一路线缆接触不良/短路可能触发**同一个 hub/控制器复位或过流**，把这一组设备一起带下去
（表现为"一路先坏、几秒内六路全没"）。**判据在内核日志**：

```bash
journalctl -k --since "-10min" | grep -Ei 'over-current|reset (high|full)-speed|device descriptor read/64|error -(71|110|32)'
```

有 `over-current change` / `reset ... USB device` / `error -71` ⇒ 是**电气/hub**问题（换线、换口、查 hub 供电），
不是程序。⚠️ 这条也可能和 §2 同时发生：hub 复位把六路都打掉 → 触发节点 ERROR → 面板 OR 锁存 → 看着像"全断"。

## 4. 现场怎么区分（口诀）

1. **只看第一条** `错误下发` 是哪一路，以及 `启动检查超时 | 未就绪=…` 那行 —— 那才是真正坏的一路；
   后面累加到 `mask=0x3F` 的是**锁存累积**，不代表六路都坏。
2. 触发节点日志里第一条 `TOPIC_TIMEOUT` 的 `Required topic ...` 就是先坏的那一路。
3. `journalctl -k` 有 hub 复位/过流 ⇒ 硬件；干净 ⇒ 程序或 hub 之外。
4. 六路**在几秒内一批**掉的、且期间没有 `长按` 记录 ⇒ 更像 §2.1+2.3（一路把流程判死、位累积）或 §3（hub 连锁）；
   紧跟在一次 `长按` 之后 ⇒ 是 §2.5（人机闭环）。
5. 想彻底复位屏幕上那些锁存位：**长按关机**（进 OVER 清 `_latched_error_payload`）再长按开机；
   只重启 `collect_system` 不够（桥没重启的话面板里的锁存还在）。

## 5. 可复现的验证（不用拔线、可逆；工程机上随时可做）

用 `SIGSTOP` 让**某一路发布节点**几秒不发布（进程还在、线也是好的），再 `SIGCONT` 恢复：

```bash
sudo systemctl start collect_system      # 需要先 6/6 就绪（长按 LCD 2 秒，或用合成按键）
P=$(pgrep -f 'lib/triple_camera_mjpeg_publisher/triple_camera_mjpeg_node')
sudo kill -STOP "$P"; sleep 6; sudo kill -CONT "$P"
L=$(ls -t ~/.lsp/log/lcd_voice/*.log | head -1); tail -6 "$L"
```

预期（用来说明"程序侧锁存"）：

- `新增故障=胸前相机、左手相机、右手相机`（三路在**同一个进程**里，STOP 一次置三位）；
- **`kill -CONT` 恢复之后屏幕仍然显示这三路故障**（`错误下发` 不再变化、`当前故障` 仍带这三位）——
  因为位是锁存的，**只有长按关机才清**。这就是"线是好的也触发、还不自愈"。
- 想验证 §2.5：在此状态下**长按 LCD 2 秒**（= 关机）→ 四路脚本全被杀 → 屏幕上就是"六路全断"。

## 6. 给厂商的最小改动建议（按性价比排序）

1. **错发的 0x80 不要 OR 锁存**（或至少"源恢复就清位"）：这是"一路偶发 → 屏幕全断"的根。
   如果保留下位机的锁存语义，也应该给上位机一条"清位"通道（现在只有整机 OVER 能做）。
2. **单路故障隔离**：`_check_health` 不要一路超时就整机 ERROR；相机/手套完全可以"这个源降级、其它照跑"，
   由上位机决定要不要中断采集。
3. **看门狗**：外设脚本 pane 退出自动重启（现在 `exec bash` 兜住，没人拉起）。
4. **ERROR 态长按的语义**：现在 = 关机（`_kill_scripts`）。故障态下现场最需要的是"重启脚本"，
   建议 ERROR 态长按 = `_kill_scripts()` + `_launch_scripts()`（先关后开），或至少提示不要关机。
5. **相机节点别把 `error` 当停止录制**：`_STOP_COMMANDS` 里去掉 `'error'`，或区分"本机录制停 / 整机故障"。
6. `check_startup` 的 `not_ready` 应区分"从未上报（仍在宽限期）"与"报错/静默超时"，
   并考虑把 `_startup_grace` 与各外设真实就绪时间对齐（相机实测 5~10 s，60 s 够，但期间不能抖）。
