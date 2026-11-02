# 设备策略：接电源、用电池都不自动休眠

> 需求原话："我们设备需要做成接入电源或者使用电池都永不休眠"。要堵的是**四层**，
> 缺一层就漏：GNOME 空闲挂起、dconf 锁、logind 的按键/空闲动作、电池耗尽时 UPower 的动作。

## TL;DR

- ① GNOME `gsd-power`：`sleep-inactive-{ac,battery}-timeout=0` **且** `-type='nothing'`
  （两个是**独立**闸门，只改一个以后可能被重新激活）；落在 gschema override + 系统级 dconf。
- ② dconf **lock** 让用户改不动；③ logind：`IdleAction=ignore` 且 `HandlePowerKey*`/`Handle{Lid,Suspend,Hibernate}*` 全 `ignore`；
  ④ UPower：`CriticalPowerAction=PowerOff`（**电池耗尽干净关机**，不是休眠）。
- ⛔ logind drop-in 缺 `[Login]` 段头 = **整个文件是死的**（只在启动时打一行 "outside of section"），
  生效值退回默认 `HandlePowerKey=poweroff` ⇒ 短按 ON 键直接关机。判据只能是 `busctl` 读回的实际属性。
- ⛔ 改"设备策略"**必须动 gschema override**：greeter 会话用 `DCONF_PROFILE=gdm`，
  系统级 dconf + lock 对它不生效。
- 判据：`tools/verify-image.sh` 对这四层都有断言；区分"谁让它睡的"看有没有 `systemd-logind: The system will suspend now!`（软件）还是只有内核 `PM: suspend entry`（有人直接写 `/sys/power/state`）。

## 四层，缺一层就漏

> 用户原话："我们设备需要做成接入电源或者使用电池都永不休眠，至少目前是这样的。"
> 这一节把"所有能把设备弄睡/弄关"的路都列出来，并说明每一层是谁在管、怎么验。
> ⚠️ 与电量计的关系（为什么突然要管这件事）见
> [../hardware/battery-gauge-bq40z50.md §8](../../hardware/battery-gauge-bq40z50.md)。


| 层 | 谁在管 | 我们设成什么 | 落在哪 |
| --- | --- | --- | --- |
| ① 空闲挂起（AC / 电池） | GNOME `gsd-power` | `sleep-inactive-{ac,battery}-timeout=0` **且** `-type='nothing'` | `overlay/usr/share/glib-2.0/schemas/99-lsp-desktop.gschema.override`（编译期默认值）+ `overlay/etc/dconf/db/local.d/01-desktop-behavior`（系统级 dconf） |
| ② 用户改不动 | dconf **lock** | 上面四个键路径 | `overlay/etc/dconf/db/local.d/locks/01-desktop-behavior` |
| ③ logind 的空闲/按键动作 | `systemd-logind` | `IdleAction=ignore`（默认就是）；`HandlePowerKey*`、`Handle{Suspend,Hibernate}Key*`、`HandleLidSwitch*` 全 `ignore` | `apps/lsp-powerkey/files/etc/systemd/logind.conf.d/99-lsp-powerkey.conf` |
| ④ 电池**耗尽**时的动作 | UPower | `CriticalPowerAction=PowerOff`（干净关机，**不是**休眠） | 构建脚本 `scripts/build-lsp-image.sh` 在桌面段 sed（`/etc/UPower/UPower.conf`，UPower 没有 conf.d） |

**为什么 ① 要写两条**（`timeout=0` 与 `type='nothing'`）：gnome-46
`plugins/power/gsd-power-manager.c` 的 `idle_set_mode()` 里

```c
if (timeout_sleep != 0) {                       /* 0 ⇒ 这个 idle sleep watch 根本不注册 */
        if (action_type != GSD_POWER_ACTION_NOTHING) { ... gnome_idle_monitor_add_idle_watch(...); }
```

两条是**各自独立**的闸门，任一成立就不会睡；写两条是防以后有人只把 timeout 调大又把它激活。

**为什么 ③ 的 `[Login]` 段是硬要求**：logind 的 drop-in 是 ini，**键写在 section 之外会被静默丢掉**
（2026-09-21 踩过，详见 §13.3）。

## 怎么验（都能直接抄）

```bash
# ① 编译期默认值（绕开 dconf；期望 0 / 'nothing'）
for k in sleep-inactive-ac-timeout sleep-inactive-battery-timeout sleep-inactive-ac-type sleep-inactive-battery-type; do
  GSETTINGS_BACKEND=memory gsettings get org.gnome.settings-daemon.plugins.power $k
done
# ② 实际生效值 + 锁是否生效（期望 0/'nothing' 与 false）
gsettings get      org.gnome.settings-daemon.plugins.power sleep-inactive-battery-timeout
gsettings writable org.gnome.settings-daemon.plugins.power sleep-inactive-battery-timeout
# ③ logind 实际生效值（期望全 s "ignore"）+ 有没有解析抱怨（期望无输出）
busctl get-property org.freedesktop.login1 /org/freedesktop/login1 org.freedesktop.login1.Manager HandlePowerKey
journalctl -u systemd-logind -b | grep 'outside of section'
# ④ 电池耗尽动作
grep '^CriticalPowerAction' /etc/UPower/UPower.conf      # 期望 =PowerOff
```

`tools/verify-image.sh` 已把这四条都做成断言（离线查文件、上板跑查实际生效值）；
判据是 `通过/失败` 计数，任何一条松动都会红。

**⛔ 区分"谁让它睡的"——不要只看 `PM: suspend entry`**（2026-09-21 差点误判成"策略失效"）：

```bash
journalctl -b -o short-iso | grep -E 'systemd-logind.*suspend|sleep.target|systemd-suspend'
```

| 日志长相 | 谁干的 |
| --- | --- |
| `systemd-logind: The system will suspend now!` + `Reached target sleep.target` + `Starting systemd-suspend.service` | **软件/自动触发**（`gsd-power` 空闲挂起、`systemctl suspend`、logind 按键动作…） |
| 只有内核 `PM: suspend entry (deep)`，**没有**上面那几行 | **有人直接写 `/sys/power/state`**（绕开 logind）。`journalctl -b \| grep 'tee /sys/power/state'` 能抓到是哪个 sudo 会话干的 |

实测对照（5io 板，2026-09-21）：`18:22:58` 那次带全套 logind 日志 = 修好前的**自动**休眠；
`18:46:37` 那次只有内核行 + journal 里有 `cat : TTY=ttyFIQ0 … COMMAND=/usr/bin/tee /sys/power/state`
= 串口上**手工**挂起（9 s 后按 ON 唤醒），与策略无关。

## 三个坑（都踩过）

1. ⛔ **logind drop-in 缺 `[Login]` 段 = 整个文件是死的**。我们那份
   `99-lsp-powerkey.conf` 一直没写段头，logind 只在启动时打两行
   `99-lsp-powerkey.conf:17: Assignment outside of section. Ignoring.`，
   生效值退回默认 `HandlePowerKey=poweroff` ⇒ **短按 ON 键直接关机**（而 `lsp-powerkey`
   不 `EVIOCGRAB`，logind 同样看得见这个键）。⚠️ 旧镜像（5io）上看着"生效"，是因为那份
   drop-in 是厂商的、**带 `[Login]`** —— 别拿别的板子/别的文件当参照，**判据只能是
   `busctl` 读回来的实际属性**。
2. ⛔ **UPower 没有"什么都不做"这一档**：`CriticalPowerAction` 只有
   `HybridSleep` / `Hibernate` / `PowerOff`。Ubuntu 默认写的是 `HybridSleep`，而本机没有
   hibernate ⇒ logind 拒绝后 UPower **退化成 PowerOff**。所以"电池到 2%（`PercentageAction=2`）
   会干净关机"是既定行为，我们只是把它**显式写出来**，免得以后 upower 升级静默变成 Suspend。
   真要让设备跑到没电只能动 `PercentageAction`（有抽干 3S 锂电的风险）。
3. ⛔ **dconf lock 连 `reset` 都挡**：上锁前要把用户级里已有的值清掉（`gsettings reset …`），
   否则"用户值 3600 + 锁"会把 3600 永久冻在那儿（虽然 `type='nothing'` 时不影响是否休眠，
   但体检会一直红）。要放开锁只能改镜像/删 `/etc/dconf/db/local.d/locks/` + `dconf update`。

## 板上落地记录（2026-09-21）

* **开发板 `<HOST_07>`**（我们的镜像）：四层全部就位。`HandlePowerKey` 从 `poweroff` → `ignore`
  （重启 logind，会话没丢），编译期默认与生效值都是 `0`/`'nothing'`，`gsettings writable` = `false`，
  `CriticalPowerAction=PowerOff`；live 体检 `通过 230 / 失败 2`（剩余两项是板子自己缺
  `wireless-regdb` / `bash-completion`，与本节无关）。
* **5io 开发板 `<HOST_10>`**（2026-09-16 前的旧镜像，跑电量计）：①②③④ 也都装上了
  （override 用的是**只含电源切片**的版本 —— 该镜像没有 kitty，整份照拷会把默认终端指到不存在的
  kitty；另外它的 `/etc/dconf/profile/user` 原本**不存在**，是这次补建的）。
  实测 `gsettings set … 900` → `键不可写`，值保持 `0`。
  修好后实测 0 次自动休眠（唯一那次 18:22:58 是修之前；18:46:37 那次是串口手工写
  `/sys/power/state`，见 §13.2 的判据表）。

**⚠️ 谁在读哪一层：greeter 会话用的是 `DCONF_PROFILE=gdm`**（板上实测
`grep -z . /proc/$(pgrep -f gsd-power)/environ`），而 `/etc/dconf/profile/gdm` 这个文件
**镜像里和旧板子上都没有** ⇒ dconf 对 greeter 只剩它自己的 user db，**系统级 dconf + lock 那一层
对它不生效**，greeter 的值得**全靠 gschema override 的编译期默认值**（这正是它当初会睡的原因——
旧镜像连 override 都没有）。登录用户（autologin 的 `board`）走 profile `user`
（`user-db:user` + `system-db:local`）⇒ 三层都在。
结论：**改"设备策略"必须动 gschema override，只改 `/etc/dconf/db/local.d` 对 greeter 无效。**
