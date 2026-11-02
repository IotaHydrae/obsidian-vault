# 音量问题：拉满还是小声（UCM 写回 27）、以及超过 100% 会削顶

> 本文回答**"为什么音量拉满还是小声"**：硬件链路（原理图 page 40）、根因
> （UCM 每次使能都把模拟音量写回 27、而且没把音量元素交给 ACP）、我们的修法（改自己那份 UCM）、
> 排查中踩的坑、"aplay 没声音"的真凶（默认输出跑到 HDMI），以及"音量超过 100%"为什么是数字增益。
> 音频栈全貌与"判没声音的三步走"见 [audio-pipewire.md](audio-pipewire.md)。
> ⚠️ 正文**原样搬自该篇的「音量拉满还是小声」整节**（2026-09-20 实测）。

## TL;DR

- 根因**不是喇叭上限**，是 **UCM 每次使能都把模拟音量写回 27**，且没把音量元素交给 ACP；修法 = 改我们那份 UCM（已实施）。
- ⛔ **默认输出会跑到 HDMI** —— 这才是"aplay 没声音"的真凶。
- ⛔ 音量 **>100% 是数字增益且会削顶**（100% 已经是硬件电气上限）。


## 音量拉满还是小声：**是增益问题，不是喇叭上限**（2026-09-20 实测）

### 硬件链路（原理图 page 40，Rev V2R1）

| 器件 | 说明 |
| --- | --- |
| **U42 = ES8388** | 编解码器（i2c7 0x11，注册在 i2s0_8ch 上） |
| **U43 = `8002A`** | **独立 class-D 功放（3W）**：输入取 ES8388 的 **LOUT2/ROUT2**（经 `R376/R377 20K`），输出到 **J24（PH-2A 喇叭座）** |
| **J23 = PJ-393** | 3.5mm 耳机座，由 **LOUT1/ROUT1** 驱动（经 47µF） |
| 检测/切换 | `HP_PLUG_IN_DET` = GPIO1_D3（DTS `hp-det-gpio`）；`EARPHONE_CTL` = GPIO1_B5（DTS **`hp-con-gpio`**），经 Q32/Q33 切换 |

⇒ 喇叭接 J24 时走的是功放，**功放本身不是瓶颈**；接 3.5mm 耳机座（J23）才是走耳机放大器（推不动 4Ω/8Ω 的 2W 喇叭）。

### 根因：UCM 每次使能都把模拟音量写回 27，而且没把音量元素交给 ACP

> ⚠️ **更正**：本节早先写过"27 是 ES8388 的 POR 默认值" —— **错的**。真正写它的是
> **我们自己 overlay 里的 UCM**（`overlay/usr/share/alsa/ucm2/rockchip-es8388/HiFi.conf`），
> 芯片只是碰巧也是 27。查证方法：`dpkg -S` 找不到这个 UCM 目录（不是包安装的），
> 而仓库里就有那份文件。

```
$ cat /usr/share/alsa/ucm2/rockchip-es8388/HiFi.conf     # 改动前
SectionVerb {
	EnableSequence [
		cset "name='PCM Volume' 192"
		cset "name='Output 1 Playback Volume' 27"     ← 就是这两行
		cset "name='Output 2 Playback Volume' 27"
		...
```
- 这段 `EnableSequence` **每次使能设备都会跑** ⇒ 不光开机，连端口切换/设备重配置都会把音量打回 27。
  所以"`alsactl store` 固化"的朴素做法**会被它盖掉** —— 必须先改这里。
- 而 `SectionDevice."Speaker"` / `"Headphones"` 的 `Value{}` 里**只有 `PlaybackPCM`，没有
  `PlaybackMixerElem`** ⇒ ACP 找不到"这一路的硬件音量元素" ⇒ 整个 sink 退回**软件衰减**。
  实测证据：把桌面音量从 100% 调到 50%，`amixer contents` diff **完全为空**（没有任何 ALSA 控件跟着动）。
- 这张卡走的是 **UCM** 而不是 profile-set/path：`wpctl inspect` 里
  `device.profile.name = "HiFi: hw:rockchipes8388: sink"`（UCM 的 `<verb>: <device>` 命名）。
  ⇒ `/usr/share/pulseaudio/alsa-mixer/**` 与 `/usr/share/alsa-card-profile/mixer/**` 里的
  path/profile 对这张卡都不参与（ACP 的编译期目录是 `/usr/share/alsa-card-profile/mixer`，
  可用 `strings libspa-alsa.so | grep alsa-card-profile` 核实）。
- 元素名对照（**cset 用完整控制名，音量元素用 simple 名**）：

  | 用途 | 完整控制名（cset） | simple 名（`PlaybackMixerElem`） | 能力 |
  | --- | --- | --- | --- |
  | 喇叭模拟输出 | `Output 2 Playback Volume` | `Output 2` | pvolume 0–33 = -45…+4.5 dB |
  | 耳机模拟输出 | `Output 1 Playback Volume` | `Output 1` | pvolume 0–33 |
  | DAC 数字 | `PCM Volume` | `PCM` | volume 0–192 = -96…0 dB |
  | 输出级开关 | `OUT1 Switch` / `OUT2 Switch` | `OUT1` / `OUT2` | pswitch |
  | 端口开关 | `Speaker Switch` / `Headphone Switch` | `Speaker` / `Headphone` | pswitch |

  刻度来自厂商驱动 `es8323.c:175` 的 `DECLARE_TLV_DB_SCALE(out_tlv, -4500, 150, 0)`（每档 1.5 dB）。

### 修法：改我们自己那份 UCM（已实施，2026-09-20）

改 `overlay/usr/share/alsa/ucm2/rockchip-es8388/HiFi.conf`（构建脚本 `:2852` 会 `cp -r overlay/usr/share/alsa/*` 装进镜像）：

```diff
 SectionVerb {
 	EnableSequence [
 		cset "name='PCM Volume' 192"
-		cset "name='Output 1 Playback Volume' 27"
-		cset "name='Output 2 Playback Volume' 27"
 		cset "name='Capture Digital Volume' 192"
 ...
 SectionDevice."Speaker" {
 	Value {
 		PlaybackPriority 100
 		PlaybackPCM "hw:${CardId}"
+		PlaybackMixerElem "Output 2"      ← 把模拟输出宣布为这一路的硬件音量
 	}
 SectionDevice."Headphones" {
 	Value {
 		PlaybackPriority 200
 		PlaybackPCM "hw:${CardId}"
 		JackControl "Headphone Jack"
 		JackHWMute "Speaker"
+		PlaybackMixerElem "Output 1"
 	}
```

- `PlaybackMixerElem` 是 ACP（`libspa-alsa.so` 里能 `strings` 到这个字面量，
  以及它导出的 `alsa.ucm.playback.mixer.element` 属性）用来决定硬件音量元素的 UCM 指令；
  值用 **simple 名**（`Output 2`，不是 `Output 2 Playback Volume`）。
- **板子上实测（2026-09-20，`systemctl --user restart wireplumber` 后）**：

  | GNOME 推子 | `Output 2` | dB |
  | --- | --- | --- |
  | 1.00 | 33 | **+4.5 dB（硬件最大）** |
  | 0.50 | 21 | -13.5 dB |
  | 0.20 | 6 | -36 dB |
  | 0.00 | 0 | **-45 dB（硬件最小）** |

  `PCM` 全程 192 = 0 dB 不动；把音量设到 1.53（>100%）硬件仍是 33（被钳在最大）。
  ⇒ **0%↔硬件最小、100%↔硬件最大**，桌面推子不再做软件衰减。
- 备份/回退：板上原文件备份在 `/root/HiFi.conf.bak`；仓库侧 `git checkout -- overlay/usr/share/alsa/ucm2/` 即可。
- ⚠️ 之前提过的"① `amixer` 设 33 + ② `alsactl store`"只是**临时听感验证**用的（当时还不知道 UCM 在写 27）；
  现在不需要它了，而且单靠它会**被 UCM 的 EnableSequence 打回 27**。
- ⏳ 没做的：0% 只是 -45 dB（不是真静音）。要真静音可以再给 UCM 加
  `PlaybackSwitch "OUT2 Switch"` / `"OUT1 Switch"` 让 ACP 用开关做 mute。

⚠️ **副作用**：模拟输出现在可以到 +4.5 dB 而数字固定 0 dB，满幅素材**可能在模拟级削顶**。破音就把
`PCM` 压到 ~186（-3 dB）或把桌面推子留一点余量。

### 排查中踩的坑

- ⚠️ **读 ES8388 寄存器必须加 `-f`**：`i2cget -y -f 7 0x11 0x30`（不加报 `Device or resource busy`）。
  绑定它的驱动是厂商 `es8323.c`（`/sys/bus/i2c/devices/7-0011/driver` → `ES8323`），
  i2c 设备名仍叫 `es8388`。
- ⚠️ **别用 `amixer scontrols` 的名字去做 cset**：`scontrols` 给的是 simple 名（`Output 2`），
  而 UCM/cset 要的是完整控制名（`Output 2 Playback Volume`）；`PlaybackMixerElem` 反过来要 simple 名。
- ⚠️ **`amixer contents` 的 diff 是判断"音量有没有真的落到硬件"的最快手段**：diff 为空 = 纯软件音量。
- ✅ **那 6 个 rockchip pulseaudio 文件的对照实验已做（2026-09-20）**：把
  `overlay/usr/share/pulseaudio/alsa-mixer/{paths,profile-sets}/*rockchip*` 临时挪走 + 重启 wireplumber 后，
  **sink 仍在、`aplay -D default` 与 `pw-play` 都 rc=0、播放后 `OUT2 Switch`/`Output 2` 与挪走前逐字相同**
  ⇒ 在**当前**（pipewire + ACP + UCM）栈里它们没有消费者。文件已按仓库原件还原（sha256 逐一核对一致）。
  - **它们是干什么的**：这是 **PulseAudio 的"声卡配置"**（profile-set + path），不是 ALSA 的：
    `pulse-rockchip.conf` 定义 analog-stereo 映射走 `analog-output-headphones-rockchip` +
    `analog-input-headset-mic-rockchip`（`priority = 5`，刻意压低），`pulse-rockchip-hdmi.conf` 定义
    HDMI stereo 走 `hdmi-output-rockchip-1/2`（path 里用 `[Jack rockchip,hdmi0]` 做插孔检测）。
    上游当年的镜像跑 **pulseaudio**、且（当时）没有 UCM ⇒ 没有它们，pulseaudio 不知道这张卡支持什么
    profile、有哪些端口、切端口该动哪些 mixer 元素 —— 那才是"aplay 没声音"的场景。
  - **为什么现在没人读**：① ACP 的编译期目录是 `/usr/share/alsa-card-profile/mixer`
    （`strings libspa-alsa.so | grep alsa-card-profile` 实证），不读 `/usr/share/pulseaudio/alsa-mixer`；
    ② 这张卡走 **UCM**；③ udev 的 `PULSE_PROFILE_SET` 规则里**没有我们这张卡**（`90-pulseaudio.rules`
    全是笔记本机型），pulseaudio 也没在跑（与 pipewire-pulse 冲突）。
  - **对照：真正撑住这张卡的是 UCM**（决定性的反向实验）：把
    `/usr/share/alsa/ucm2/rockchip-es8388/` 临时挪走 + 重启 wireplumber，sink 名字从
    **`ES8388 Headphones + Speaker`（UCM 设备名）** 退化成 **`ES8388 Stereo`（ACP 通用 profile）**
    —— 端口（Headphones/Speaker/Headset/Mic）与 `PlaybackMixerElem` 音量映射都来自 UCM。
    ⏳ 没测那种状态下到底有没有声音（要有声音就说明通用 profile 也能出声）。
  - ⚠️ 构建脚本 `:2853` 仍会安装这 6 个文件，**保留不动**（无害，且将来若有人在镜像里跑 pulseaudio
    或退回 pulseaudio 栈，它们是必需的）。
- 若改完 UCM 后还是明显不够，下一个怀疑对象是**功放的 SDN/使能**（DTS 里只有 `hp-con-gpio`，
  没有 `spk-con-gpio`，所以 `spk switch` 控件是空转；`hp switch` 才对应 GPIO1_B5），
  以及 `R376/R377 20K` 那个输入衰减网络（这两条都还没量）。

### ⛔ 默认输出会跑到 HDMI（"aplay 没声音"的真凶）

**现象**：重启 wireplumber 后，默认 sink 有时会变成 **HDMI0**；`aplay`/`sox` 走 ALSA `default`
→ PipeWire 的默认 sink ⇒ 声音被送到 HDMI，没接显示器就是**完全没声音**。

```
$ wpctl inspect <es8388 sink> | grep priority      →  priority.session = "1000"
$ wpctl inspect <hdmi0  sink> | grep priority      →  priority.session = "1000"    ← 打平
```

`priority.session` 由 `/usr/share/wireplumber/scripts/monitors/alsa.lua` 的 `createNode()` 算：
`dev == "0" → 1000`。**两个声卡都是 `hw:0`（各自的 card 0）⇒ 必然打平**，谁当默认就成了竞速。

⛔ **不能在 `/etc/wireplumber/main.lua.d/*.lua` 的 `alsa_monitor.rules` 里修**：那里的
`apply_properties` 只作用于 **device** 对象，而 `default-nodes` 模块（`strings
libwireplumber-module-default-nodes.so` 能查到它读 `priority.session`）看的是 **node**。
实测在规则里写 `["priority.session"] = 2000` **完全无效**（重启后仍是 1000）。

✅ **修法（已实施）**：构建期给 `alsa.lua` 打补丁，在 `createNode()` 算完 priority 之后按卡名 +1000
（`scripts/build-lsp-image.sh`，`/tmp/lsp-wp-priority.sed`，幂等 + grep 守卫）：

```lua
    -- LSP: prefer onboard ES8388 (otherwise it ties with the HDMI cards at 1000, …)
    if dev_props["device.name"] == "alsa_card.platform-es8388-sound" then
      priority = priority + 1000
    end
```

板上实测：板载 **2000** vs HDMI **1000**，`wpctl clear-default` 清掉"记住的默认"后仍然选板载。
⚠️ wireplumber 升级会覆盖 `alsa.lua` ⇒ **将来升级 wireplumber 要重打这个补丁**。

### 音量超过 100%（overamplification）

GNOME 设置里的"允许音量超过 100%"对应 dconf 键 `org.gnome.desktop.sound allow-volume-above-100-percent`。
**2026-09-20 已写成镜像默认值**（`overlay/usr/share/glib-2.0/schemas/99-lsp-desktop.gschema.override`）：
100% 已经是硬件的电气上限，再往右靠数字增益，用户在"还嫌不够"时可以自己推（代价是满幅素材可能削顶）。

⚠️ **实测口径（2026-09-20 已定论）**：把 sink 音量从 1.0 拉到 1.53，**ALSA 混音器 diff 完全为空、
硬件 `Output 2` 都是 33（钳在最大）**，但 node 的 **`softVolumes`** 变了 —— 多出来的增益全在
**PipeWire 的软件音量**里：

| slider | 硬件 `Output 2` | `channelVolumes`（= slider³） | **`softVolumes`（真正加的软件增益）** |
| --- | --- | --- | --- |
| 0.5 | 21 | 0.1250 | **1.0000（0 dB，纯硬件）** |
| 0.8 | 30 | 0.5120 | **0.8596（-1.3 dB）** |
| 1.0 | 33（硬件最大） | 1.0000 | **1.0000（0 dB，纯硬件）** |
| 1.2 | 33 | 1.7280 | **1.7279（+4.8 dB，数字）** |
| 1.53 | 33 | 3.5816 | **3.5814（+11.1 dB，数字）** |

⇒ **≤100% 是纯硬件模拟音量；>100% 硬件已到顶，多出来的是数字增益**（153% = +11.1 dB）。

**实测举例**（用户测试用的语音文件 `~/Music/344250__honest_cactus__normal-voice-and-intro.wav`，
24-bit/44.1 kHz，`sox … -n stat`：峰值 `0.290571` = **-10.73 dBFS**、RMS `0.029195` = -30.7 dBFS）：
153%（+11.1 dB）刚好把峰值顶到 **+0.4 dBFS** —— 只有最响的那一两个采样点擦到 0，所以当时听不出破音；
换成峰值接近 0 dBFS 的流行乐，同样推子就是 11 dB 的明显削顶。

**声学回环实测（2026-09-20，用板载麦录扬声器；⛔ 见 AGENTS.md「不要在工程机上放声音」——这套测试会出声，
再做必须先问用户）**

方法：`pw-record`（48 kHz/2ch）录 4 秒，同时 `pw-play` 放 3 秒 440 Hz 正弦，`sox <rec> -n stat` 取 RMS/峰值。
采集侧 `ALC Capture Function = Off`（无自动增益），噪声底 RMS `0.001304`（100% 档信噪比 **+14.9 dB**，有效）。

| sink 音量 | RMS（相对 1.0） | 峰值（相对 1.0） | peak/RMS |
| --- | --- | --- | --- |
| 0.8 | **-4.96 dB** | -6.29 dB | 8.34 |
| 1.0 | 0 | 0 | 9.72 |
| 1.2 | **+6.40 dB** | +2.82 dB | 6.44 |
| 1.53 | **+12.34 dB** | +4.72 dB | 4.04 |

- **1.0→1.53 的 RMS +12.3 dB ≈ `softVolumes` 的 +11.1 dB** ⇒ **>100% 确实更响**（用户耳朵没错，
  先前只看 `volume`/ALSA 混音器得出的"应该没变化"是错的）。
- 但**峰值只涨了 4.7 dB**、peak/RMS 从 9.7 掉到 4.0 ⇒ 那次测试音（`vol 0.3` = -10.5 dBFS）在 153% 时
  **数字域已经削顶**（-10.5 + 11.1 > 0 dBFS）⇒ 多出来的 RMS 里含失真。**这就是">100% 会削顶"的直接证据。**
- **0.8→1.0 = -4.96 dB ≈ 硬件元素的 +4.5 dB** ⇒ **≤100% 只有硬件在动**（不存在"硬件 + 软件"双重衰减）。
- ⚠️ 0.5 及以下的录音已落到噪声底（0.5 仅 +2 dB），那几档的声学数字**不可用**。

**同一套方法、改用用户的人声文件**（`~/Music/344250__…wav` 的 `trim 6 8`，8 秒；100% vs 153%）：

| sink 音量 | RMS | 峰值 | 波峰因数 |
| --- | --- | --- | --- |
| 噪声底（不放音） | 0.001301 | 0.009705 | — |
| 1.0 | 0.004551（比底噪 +10.9 dB） | 0.100250 | 22.0 |
| 1.53 | 0.014412 | 0.236816 | 16.4 |

- **RMS +10.0 dB**（`softVolumes` 预期 +11.1 dB）⇒ 人声内容上同样成立：**>100% 确实更响**。
- 但**峰值只 +7.5 dB、波峰因数掉 2.55 dB** ⇒ 有轻微压缩/限幅。其中只有 0.4 dB 能由"该文件峰值
  +0.4 dBFS"的理论数字削顶解释，剩下的更可能来自**功放/喇叭在大电平下的压缩**（录音峰值仅 0.24，
  离 ADC 满量程很远，不是录音端削顶）。⇒ 想再干净地分离这三者，得**耳机口电气直连录音**或等工区安静时重测。
- ⚠️ 工区环境噪声让底噪偏高（人声 100% 档信噪比只有 +10.9 dB），所以 ±1~2 dB 的差异都在误差内。
⚠️ **判据别只看 `volume` 和 ALSA 混音器**：`volume` 恒为 1.0、`amixer contents` 的 diff 为空，
**要看 `Props` 里的 `channelVolumes` / `softVolumes`**（`pw-dump <sink-id> | awk '/"Props"/{f=1} f'`）。
这一段会削顶（+11 dB 数字增益叠在已到顶的模拟级上），素材有富余才听不出破音。

---
