# 音频栈：判"没声音"的三步走 + 音量 >100% 的真相

> 排查顺序固定三步：① `wireplumber` / `pipewire` 用户服务在不在 ② 声卡认没认到 ③ 默认 sink 对不对。
> **音量 >100% 确实更响，但那是数字增益、会削顶**；≤100% 动的只有硬件音量。

## TL;DR

- **三步走**（§"音频栈"）：① user 服务（`wireplumber` 挂了就是"没声音"）② `pactl list cards short`
  看 4 张卡（`es8388`/`hdmi0`/`hdmi1`/`dp0`）③ 默认 sink 对不对。
- ⚠️ **HDMI 与内置声卡抢默认 sink 是竞速**（两边 `dev == 0 → 1000` 打平）⇒ 没接显示器时"完全没声音"
  可能只是声音被送到了 HDMI。
- `pulseaudio.socket` / `.service` 显示 inactive 是**设计使然**（被 `pipewire-pulse.*` 的 `Conflicts=`
  确定性删掉 start），不是故障。
- 音量实测：**≤100% 只有硬件在动**（0.8→1.0 = −4.96 dB ≈ 硬件 +4.5 dB）；
  **>100% 是数字增益**（153% = +11.1 dB）：RMS +12.3 dB 确实更响，但峰值只 +4.7 dB、
  波峰因数 9.7→4.0 ⇒ **数字域削顶**。
- **100% 已是硬件的电气上限**（ES8388）⇒ 再往右只有数字增益。
- ⚠️ wireplumber 升级会覆盖 `alsa.lua`，那个补丁要重打。

## 音频栈：判"没声音"的三步走

### 我们跑的是 **PipeWire**，不是 PulseAudio（2026-09-20 核实）

```
$ pactl info | grep "Server Name"
Server Name: PulseAudio (on PipeWire 1.0.5)      ← "PulseAudio" 只是**协议名**，括号里才是真身
$ systemctl --user is-active pipewire wireplumber pipewire-pulse pulseaudio
active / active / active / inactive              ← pulseaudio 守护进程从没跑过（见本文"pulseaudio…"那节的 Conflicts 结论）
$ ss -xlp | grep pulse/native
/run/user/1000/pulse/native  users:(("pipewire-pulse",pid=1072,fd=3))
```

| 入口 | 实际路径 |
| --- | --- |
| `pw-play` | 直接 PipeWire native |
| `pactl` / `pavucontrol` / 讲 pulse 协议的应用 | pulse 协议 → **`pipewire-pulse`** → PipeWire |
| **`aplay` / `sox`（默认设备）** | libasound `default` = **`type pulse`** → ALSA pulse 插件（`libasound2-plugins`）→ `pipewire-pulse` → PipeWire |
| GNOME 音量/设置 | PipeWire（等价的 `wpctl` API） |
| 最终落点（都一样） | PipeWire → WirePlumber 策略 → ALSA `hw:rockchipes8388`（ES8388） |

⚠️ **`pulseaudio` 这个"包"是必需的，它的守护进程则完全不需要**：ALSA 的 `default` 由
**`/usr/share/alsa/pulse-alsa.conf`**（`pcm.!default { type pulse }`，`dpkg -S` → **pulseaudio** 包）
定义，而它**是在"pulse 服务在跑"时才被加载的**：`/usr/share/alsa/alsa.conf.d/pulse.conf`（同一个包，
`/etc/alsa/conf.d/99-pulse.conf` 是同一段）注册了一个 hook
`conf_pulse_hook_load_if_running`（来自 `libasound_module_conf_pulse.so`，属 `libasound2-plugins`），
只有它认为 pulse 在跑时才去 `files [ "/usr/share/alsa/pulse-alsa.conf" ]`，且 `errors false`
（加载失败静默）。而 `pipewire-alsa`（让 ALSA 直连 PipeWire 的另一条路）**没有装**。

✅ **已实测（2026-09-20）**：临时把 `/usr/share/alsa/pulse-alsa.conf` 挪走 ⇒

```
$ aplay -L | head          # 挪走前第 3 行是 "default / …PulseAudio sound server"，挪走后 default 整个消失
$ aplay /tmp/tone2.wav
aplay: main:834: audio open error: Device or resource busy    ← rc=1，完全放不出来
```

原因是 PipeWire 一直占着 `hw:0,0`（`cat /proc/asound/card0/pcm0p/sub0/status` → `state: RUNNING
owner_pid: 1069` = pipewire）。**这就是"aplay/sox 没声音"的真因**（手动做镜像时漏了 pulseaudio 包
或这份 ALSA 桥接配置），比先前猜的"缺那 6 个 mixer 文件"贴切得多。
⇒ ⛔ **别为了"反正不跑 pulseaudio"把它从 `BASE_PACKAGES` 里删掉。**

### 判"没声音"的三步走

```bash
export XDG_RUNTIME_DIR=/run/user/1000
systemctl --user status wireplumber        # ① 会话管理器活着吗
pactl list cards short                     # ② 声卡有没有被 PipeWire 接管
wpctl status | sed -n '/Sinks:/,/Sources:/p'   # ③ 有没有真 sink（只剩 Dummy Output 就是没接上）
```

| 现象 | 判据 | 含义 |
| --- | --- | --- |
| `wireplumber` `code=exited, status=70` + `Failed to connect to session bus` | `systemctl --user status wireplumber` | 会话总线问题（§1），不是音频本身的问题 |
| `pactl list cards short` 为空、`wpctl` 只有 `Dummy Output` | 同上 | 没有会话管理器 = 没有 ALSA 节点 |
| `RTKit error: ServiceUnknown` | `journalctl --user -b \| grep RTKit` | `rtkit` 包缺失或会话总线不通（两者都要修） |

- 本板声卡（`cat /proc/asound/cards`）：`rockchipes8388` / `rockchiphdmi1` / `rockchipdp0` / `rockchiphdmi0`。
- `wireplumber` 是**系统级依赖链上无关、但音频链上必需**的组件：它死了不报 failed 服务之外的任何错，
  表现就是"界面里没有声音设备"。
- `pipewire-pulse` 才是 `/run/user/1000/pulse/native` 的持有者（`ss -xlp | grep pulse/native` 可核对），
  不是 `pulseaudio`。

---


## `pulseaudio.socket` / `.service` 为什么"条件判否且从未评估"：**已查清（2026-09-20）**

**结论：`ConditionResult=no` 不是"条件判否"，而是"条件从未被评估"的布尔默认值**；这两个单元的
start 任务在开机事务里被 `pipewire-pulse.socket` / `pipewire-pulse.service` **自带的
`Conflicts=pulseaudio.*`** 删掉了。这是 Ubuntu 用来消除"pulseaudio / pipewire 双音频栈竞争"的
**确定性设计** —— 不是本板特有问题，也不是 systemd bug。**"pulseaudio 不跑、pulse socket 归
pipewire-pulse"可以放心依赖，不要去"修"它。**

证据：

- `ConditionResult` / `AssertResult` 在 systemd v255 里是**布尔**属性
  （`src/core/dbus-unit.c:923-924`、`src/core/unit.h:472-473`），从未求值 ⇒ `false` ⇒ 显示 `no`；
  `ConditionTimestamp` 为空同理。反证：一个**没有任何 `Condition*=` 且从未启动**的单元同样是
  `no / no / 空`（`systemctl --user show systemd-ask-password-wall.service -p …`）。
- `grep -c '^Assert'` 在 `pulseaudio.{socket,service}` 里都是 **0** —— 没有任何断言可失败。
- 冲突是**单向声明**：`pipewire-pulse.socket` 里有 `Conflicts=pulseaudio.socket`
  （`systemctl --user show pipewire-pulse.socket -p Conflicts` → `pulseaudio.socket shutdown.target`），
  反向 `pulseaudio.socket` 只声明 `Conflicts=shutdown.target` ⇒ **没有竞速，"谁赢"是确定的**。
- 两个单元**确实进了同一事务**（`systemctl --user show default.target -p Wants` 里有 `pulseaudio.service`），
  systemd 的事务合并规则是"被 Conflicts 拉来的 stop 与 start 冲突时**宁可删掉 start**"
  （`src/core/transaction.c` 的 `delete_one_unmergeable_job()`）。
- 开机 journal 有 12 条 `Listening on …`，**唯独没有 `pulseaudio.socket`**。
- 条件本身没问题：`condition_test_user()` 对 `root` 用 `getuid()==0 || geteuid()==0`，
  user manager 是 uid 1000。⚠️ `systemd-analyze --user condition "ConditionUser=!root"` 是在
  **调用进程**里求值（当然 succeeded），**不能拿它跟 manager 的状态对账**。
- 7 个带 `ConditionUser=` 的用户单元实测：`pipewire{,.socket}` / `pipewire-pulse{,.socket}` 四个
  `yes`（时间戳相同），pulseaudio 三个 `no`（都属"未评估"）；其中 `pulseaudio-x11.service` 是
  static + `PartOf=gnome-session-x11.target`，本会话是 Wayland ⇒ 从不被拉入。

唯一没能直接看到的只有 journal 里那行 `Fixing conflicting jobs … by deleting job pulseaudio.socket/start`
（info 级日志不打印，要 `systemd.log_level=debug` 重启才看得到），但源码规则 + 单向 `Conflicts=`
+ 现象三者一致，置信度高。

### 原始现象（留档：当初为什么会以为"矛盾"）

`is-enabled` 都是 `enabled`，但从未启动：

```
$ systemctl --user show pulseaudio.socket -p ConditionResult -p ActiveState -p AssertResult -p ConditionTimestamp
ConditionResult=no
ActiveState=inactive
AssertResult=no
ConditionTimestamp=            ← 空 = 条件压根没被评估过

$ systemctl --user show pipewire-pulse.socket -p ConditionResult -p ConditionTimestamp
ConditionResult=yes
ConditionTimestamp=Sun 2026-09-20 19:07:53 CST
```

当初的三个"矛盾点"现在都能解释：`AssertResult=no` 在无 `Assert*=` 的单元上只是同一个布尔默认值；
`ConditionTimestamp` 为空正是"从未评估"；`systemd-analyze --user` 的 succeeded 是**另一个进程**在求值。
⚠️ 另外当初误记了 symlink 位置：它们在 **`/etc/systemd/user/{default,sockets}.target.wants/`**，
`/usr/lib/systemd/user/default.target.wants/` 根本不存在。

---

## 音量拉满还是小声 / 音量超过 100% → 见 [audio-volume-ucm.md](audio-volume-ucm.md)

硬件链路、根因（UCM 写回 27）、我们的修法、排查踩的坑、"aplay 没声音的真凶"（默认输出跑到 HDMI）
与"超过 100% 是数字增益"**已搬成单独一篇** → **[audio-volume-ucm.md](audio-volume-ucm.md)**。

