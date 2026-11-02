# RK806 的 PWRON：电源键的开机 / 运行 / 关机全链路

> 适用板卡：`lubancat-5-v2`、`lubancat-5io`（两者的 RK806 节点逐字相同，见 §9）。
> 内核：Rockchip 6.1 BSP（`rockchip_linux_defconfig`）。⚠️ **本文的寄存器值与事件时间戳实测于 `6.1.118`**
> （当时板子上那版）；现在仓库/板子是 **`6.1.172`**（同一 6.1.y BSP 小步，RK806 相关链路与节点未变，
> 见 [../build/kernel-upgrade-feasibility.md](../build/kernel-upgrade-feasibility.md)）。
> 本文所有寄存器值、事件时间戳均为**板子上实测**，不是从手册抄的。
> 相关需求背景：想把"按住 2 秒触发正常关机"做出来，必须先搞清楚这条链路上**谁说了算**。

## TL;DR

- 先分清**两个引脚**：`PWRON`（按键输入）与 PMIC 中断线（§1）。
- 上电阶段由 PMIC/BL31 说了算；系统起来后按一下键，用户态收到的是 **input 事件**（`rk8xx-pwrkey`）；
  长按约 8 s 触发 **PMIC 硬件断电**（§2/§3/§4）。
- ⚠️ **硬件长按的 8 s 改不了**（本工程最贵的一手实测数据，§4）⇒"按住 2 秒正常关机"只能靠用户态（§8）。
- 想改行为能动的地方：§7（内核驱动 / logind / 我们的 `lsp-powerkey`）。
- v2 与 5io 的 RK806 节点**逐字相同**（§9）；自查命令见 §10。

## 0. 一句话结论

**PWRON 引脚由 RK806 芯片内部状态机全权处理，SoC 完全看不见它。**
开机、运行中的按键上报、以及"按住 N 秒直接断电"都是 PMIC 的行为；
Linux 只能通过 PMIC 的中断"听说"按键，**没有**任何机制能在硬件长按断电前收到通知。

而 PMIC 那个长按定时器**关不掉**（§4.1：能改的只有 6/8/10/12 秒）。
本工程的做法：**放宽到 12 秒 + 用户态自己计时**，在第 10 秒主动写 `DEV_OFF`
强制断电（§8），全程**零内核改动**。

## 1. 两个别混的引脚

| 引脚 | 方向 | 作用 | 谁驱动 |
| --- | --- | --- | --- |
| **PWRON**（PWRON_KEY） | 按键 → PMIC | 开机 / 上报按键 / 硬件长按断电 | 只有按键，SoC 无任何连接 |
| **PWRCTRL1** | SoC → PMIC | Linux 关机时通知 PMIC 断电 | `gpio0 RK_PA2` alt1（`pmic_sleep1`） |

PWRON 没有进 RK3588 的 GPIO/pinctrl，设备树里也没有它的节点——
整个 DTS 关于按键只有一行 `pwrkey { status = "okay"; }`（`rk3588-lubancat-5.dts:704`）。
所以它的行为 100% 由 PMIC 内部决定，改内核代码只能改"报什么事件给用户态"，
改不了芯片自己的判定。

## 2. 上电阶段（系统还没起来）

1. VB 有效后，PWRON 内部上拉为高；按键短接到 GND 把它拉低。
2. 芯片内部去抖 + 持续判定（`PWRON_DB_SEL` / `PWRON_LP_TM_SEL`），
   低电平保持够长才启动上电时序。
3. 上电判定时长由 `PWRON_ON_TIME`（0x76 bit7）决定：`0` = 500 ms，`1` = 20 ms。
4. **本板实测是 500 ms**：DT 里没有写 `pwron-on-time-500ms`，驱动 `rk806-core.c:785-787`
   走了 else 分支写 `0x00`，把 OTP 原本的 20 ms **改成了 500 ms**。
   → 开机要按住约半秒，这是驱动改出来的结果，不是芯片默认。
5. 上电成功后 `PWRON_STS`（0x5D bit7）置位，`ON_SOURCE`（0x74）记录触发源
   （实测 `0x74 = 0x40`）。开机日志会打印：
   `rk806 spi2.0: ON: 0x40 OFF:0x0`。

## 3. 运行阶段：按下 PWRON，用户态收到什么

### 3.1 链路

```
按键（拉低 PWRON）
   └─ RK806 内部：去抖 → 置 INT_STS0 的 PWRON_FALL/RISE 位 → 拉低 INT 脚
        └─ gpio0 RK_PA7（低有效，rk806_irq_init() 写 INT_POL=LOW）
             └─ regmap_irq 分发（rk806-core.c:352-355，四个中断都定义了）
                  ├─ RK806_IRQ_PWRON_FALL ─→ rk805-pwrkey.c:19  pwrkey_fall_irq()
                  │                              └─ input_report_key(KEY_POWER, 1) + input_sync
                  └─ RK806_IRQ_PWRON_RISE ─→ rk805-pwrkey.c:29  pwrkey_rise_irq()
                                                 └─ input_report_key(KEY_POWER, 0) + input_sync
                       ↓
   /dev/input/eventN  ←── 所有用户态从这里读（evdev / libinput / logind / gnome-shell）
                       ↓
   systemd-logind 监听 power-switch，打日志 "Power key pressed short."
                       ↓
   GNOME 等桌面组件（本镜像里是 gsd-media-keys / gnome-shell）各自处理
```

### 3.2 用户态能拿到的只有两个事件

* 按下一次：`EV_KEY` / `KEY_POWER`(116) / `value=1`
* 松开一次：`EV_KEY` / `KEY_POWER`(116) / `value=0`

**没有** `value=2`（自动重复）。实测按 4 秒、6 秒都不会收到重复事件；
`rk805-pwrkey.c` 也从不调 `input_event(value=2)`。
→ 用户态想知道"按了多久"，**只能自己记 fall 和 rise 之间的时间差**。

设备节点实测（`/proc/bus/input/devices`）：

```
N: Name="rk805 pwrkey"
P: Phys=rk805_pwrkey/input0
S: Sysfs=/devices/platform/feb20000.spi/spi_master/spi2/spi2.0/rk805-pwrkey.8.auto/input/input1
H: Handlers=kbd event1 dmcfreq
capabilities/key: 10000000000000   → BIT(116) = KEY_POWER
```

⚠️ **eventN 的编号会漂移**：同一天里实测过 `event1` → `event8/event9`。
用户态必须**按名字找**（`/sys/class/input/eventN/device/name` == `rk805 pwrkey`），
不能写死节点号。

⚠️ **权限**：`/dev/input/eventN` 是 `root:input 0660`，而 `board` 用户**不在 `input` 组**
（`id` 只有 `sudo,audio,video`）。以 `board` 身份读会直接 `Permission denied`；
读它的进程必须 root（`systemd-logind` 就是 root），或者把用户加进 `input` 组。

### 3.3 实测原始事件（准到毫秒）

用 `read()` 直接读 `input_event`（不要用 `dd`+`od` 拼 24 字节，
实测那样会在事件竞争时把结构读散），两次独立录制：

```
[1789361015.108] KEY_POWER down
[1789361015.244] KEY_POWER up   held 136 ms      ← 短按
[1789361017.831] KEY_POWER down
[1789361021.964] KEY_POWER up   held 4133 ms     ← 长按 4.1 s

[1789361041.866] KEY_POWER down
[1789361043.600] KEY_POWER up   held 1734 ms
[1789361044.980] KEY_POWER down
[1789361045.075] KEY_POWER up   held 95 ms
[1789361045.716] KEY_POWER down
（没有 up —— 板子在 6 s 后被 PMIC 直接断电，录制进程一起消失）
```

### 3.4 logind 看到什么（logind 开 debug 实测）

```
systemd-logind: Power key pressed short.
systemd-logind: Handling of handle-power-key (edge) is disabled, taking no action.
```

要点：

* `"Power key pressed short."` 是**按下瞬间**打的，**不代表它测出了按键时长**。
  上游 `logind-button.c` 里这条日志在"短按分支"，
  而该分支只在 `HandlePowerKeyLongPress` 为 `ignore` 或与 `HandlePowerKey` 相等时走；
  否则 logind 会起一个 1.5 s 定时器去打 `"Power key pressed long."`。
  **Ubuntu 24.04 的 systemd 255.4 实际选哪个分支，与配置值的组合有关**——
  实测 `HandlePowerKeyLongPress=ignore` 时仍然打 "short"，
  所以别把这条日志当成"logind 觉得这是短按"。
* logind 的长按阈值 **1.5 s 是编译进二进制的常量**，
  `logind.conf` 里没有毫秒旋钮，只有 `HandlePowerKey` / `HandlePowerKeyLongPress`
  两个动作开关（可取 `ignore/poweroff/reboot/...`）。

## 4. 硬件长按断电：实测行为 + 怎么放宽 → 见 [rk806-pmic-registers.md](rk806-pmic-registers.md)

`PWRON_LP_OFF_TIME`（`0x76` bit5:4）、`0x76` 写入跨软关机保留、⛔ bit7 不能碰、volatile 读取与缓存边界、
`DEV_OFF`（`0x72` bit0）这个用户态断电入口、以及"硬断电 vs 软关机"的判据 —— 都在
**[rk806-pmic-registers.md](rk806-pmic-registers.md)**。


## 5. 关机（软件路径）

`rk806-regulator.c:1274` 注册了 `.shutdown = rk806_regulator_shutdown`，`systemctl poweroff`
走到内核关机阶段时执行（`rk806-regulator.c:1264-1266`）：

1. 所有 BUCK/LDO 的 `*_VSEL_CTR_SEL` 写 `CTR_BY_NO_EFFECT`；
2. `SLAVE_RESTART_FUN=1`、`ENB2_2M=1`（`rk806-core.c:869-871`）；
3. `RST_FUN`(0x72 bit7:6) 写 1 —— 实测 `0x72 = 0x42`，与 DT `pmic-reset-func = <1>` 一致；
4. `pinctrl_select_state(p, "pmic-power-off")`，DT 里是
   `pinctrl-1 = <&rk806_dvs1_pwrdn>` → `gpio_pwrctrl1` 切到 `pin_fun2`
   （源码 `PWRCTRL_POWOFF_FUN = 2`，`include/linux/mfd/rk806.h:355-362`）
   → PMIC 收到掉电请求，按 `OFF_SOURCE`(0x75) 断电。

运行中的系统还多一层：`KEY_POWER` → logind → `systemctl poweroff`，
这一段是纯用户态行为，和 PMIC 寄存器无关。

## 6. 实测寄存器快照 / ## 7. 想改行为时能动的地方 → 见 [rk806-pmic-registers.md](rk806-pmic-registers.md)

板上寄存器快照与"能动的地方"清单都搬到同一篇：**[rk806-pmic-registers.md](rk806-pmic-registers.md)**。


## 8. 「按住 2 秒正常关机 / 10 秒强制断电」的落地实现 → 见 [lsp-powerkey-implementation.md](lsp-powerkey-implementation.md)

状态机、`-static -s` 的取舍、板上实测记录与 2026-10-02 那次"按住期间忙等"的修复 —— 都在
**[lsp-powerkey-implementation.md](lsp-powerkey-implementation.md)**（含 §8.1）。


## 9. 两块板的差异

RK806 的 PWRON / 掉电相关字段**逐字相同**（各自独立声明，不是 include）：

| | `rk3588-lubancat-5.dts`（v2 由它 include） | `rk3588-lubancat-5io.dts` |
| --- | --- | --- |
| `interrupts`（PMIC INT） | `RK_PA7 IRQ_TYPE_LEVEL_LOW`（:665） | 同（:730） |
| `pinctrl-names` | `"default", "pmic-power-off"`（:667） | 同（:732） |
| `pinctrl-1` | `<&rk806_dvs1_pwrdn>`（:669） | 同（:734） |
| `pwrkey` 节点 | `:704` | `:769` |
| `rk806_dvs1_pwrdn` | `:722` | `:787` |
| 电源脚 | `vcc5v0_sys` | `vcc4v0_sys`（与 PWRON 行为无关） |

## 10. 自查命令（都能直接抄）

```bash
# 输入设备与能力
grep -A5 "rk805 pwrkey" /proc/bus/input/devices

# 按名字找节点（不要写死编号）
for e in /dev/input/event*; do
  n=$(cat /sys/class/input/$(basename $e)/device/name 2>/dev/null | tr -d '\0\n')
  [ "$n" = "rk805 pwrkey" ] && echo "$e"
done

# 读 PMIC 寄存器（write-only 节点，结果在 dmesg）
for a in 5d 72 74 75 76; do echo "r $a" | sudo tee /sys/rk806single/debug >/dev/null; done
sudo dmesg | tail -20

# 判别"上次是硬断电还是软关机"
sudo journalctl -b -1 --no-pager | tail -8
#   有 "systemd-shutdown[1]: Syncing filesystems and block devices." + "Journal stopped" → 软关机
#   日志突然截断在普通用户态记录 → 硬断电

# PMIC 启动日志（chip id / 开机源）
sudo journalctl -k -b --no-pager | grep -E "rk806.*(chip id|ON: )"

# --- 本工程的电源键守护进程 ---
systemctl status lsp-powerkey --no-pager     # 服务状态
grep -v '^#' /etc/default/lsp-powerkey       # 当前阈值/开关
cat /var/log/lsp-powerkey.log                # 日志（[绝对时间 +开机秒数] 前缀）
tail -f /var/log/lsp-powerkey.log            # 实时跟（这块没有 journal 的缓冲问题）
journalctl -u lsp-powerkey -b --no-pager     # 同一份日志，从 journal 看
: | sudo tee /var/log/lsp-powerkey.log > /dev/null   # 测之前清空，判断更干净
busctl get-property org.freedesktop.login1 /org/freedesktop/login1 \
    org.freedesktop.login1.Manager HandlePowerKey    # 应为 "ignore"

# 单独验"10 秒强制断电"（2 秒那级换成 /bin/true，避免它先把系统关掉）
sudo sh -c 'cat > /etc/default/lsp-powerkey <<EOF
LSP_POWERKEY_HOLD_GRACEFUL=2000
LSP_POWERKEY_HOLD_RETRY=0
LSP_POWERKEY_HOLD_FORCE=10000
LSP_POWERKEY_GRACEFUL_CMD=/bin/true
EOF
systemctl daemon-reload && systemctl restart lsp-powerkey'

# 单独验"2 秒软关机"（不写 GRACEFUL_CMD 即走默认的 systemctl poweroff）
sudo sh -c 'cat > /etc/default/lsp-powerkey <<EOF
LSP_POWERKEY_HOLD_GRACEFUL=2000
LSP_POWERKEY_HOLD_RETRY=0
LSP_POWERKEY_HOLD_FORCE=10000
EOF
systemctl daemon-reload && systemctl restart lsp-powerkey'
```
