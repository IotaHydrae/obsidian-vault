# Type-C 掉连故障：怎么按需复现、怎么验证修法、怎么落地

> [typec-detach-debounce.md](typec-detach-debounce.md) 讲的是**现象与根因**（Type-C 口的
> CC/VBUS 瞬态被当成"拔线" → tcpm 拆口 → 伴生 xHCI 被拆重建 → Quest 每秒级断连）。
> 本篇讲**怎么让它按需发生、怎么证修法有效、以及在不能刷镜像的现场怎么落地**。

## TL;DR

- **不用改内核就能注入**：把 FUSB302 的 `MEASURE(0x04)`（COMP 比较器阈值）压到 CC 电平以下，
  芯片**真**报 `COMP_CHNG` ⇒ 整条链跑一遍。驱动在重连时会自己把阈值写回 ⇒ 自愈、可重复。
- **判据别用 `dmesg`**：本板 RTW + `[ROW]/[AVG]` 噪声把 ring 冲得极快（实测一个 boot 706 次
  拆除，`dmesg` 里一条不剩）。盯 `/sys/bus/usb/devices/9-1` 的存在性，或用监视器。
- `tools/typec-watch.sh`：掉连时把两侧现场一起存盘（含**持续滴灌**的 tcpm/fusb302
  debugfs log —— 那两个是"读一次就清空"的环形缓冲，只在快照时读会丢触发过程）。
- 两个 dtbo：`-dwc3-rolehost`（让控制器不被拆）、`-usbcon-fixedsrc`（把口锁成老镜像那样的
  固定 source，做实验时变量才干净）。**前者实测不够**（控制器保住了，设备照样掉）。
- **方案 A（不改内核、不用重启）**：`i2cset 0x22 0x0a` 把 COMP_CHNG / VBUSOK 的**中断 mask 掉**
  ⇒ tcpm 收不到 ⇒ 不 detach ⇒ 设备不掉。**实测通过**（A/B 见下）。
- 59 不能刷镜像、`CONFIG_TYPEC_FUSB302=y`（内建）、`# CONFIG_KPROBES is not set`
  ⇒ 要上去抖内核只能换 vmlinuz；不想换内核就用方案 A。

## 现场怎么做（不重启也能抓证据）

`tools/typec-watch.sh install` —— systemd transient unit（不进 /etc、重启即消失），
每次掉连把两侧现场存盘到 `/var/log/lsp-typec-watch/`：typec 角色 / gpio / regulator、
FUSB302 寄存器、tcpm + fusb302 的 debugfs log、dmesg 尾部、Quest 侧 battery/usb/logcat。

三条硬约束（都踩过）：

1. ⛔ **绝不能读 FUSB302 的 `0x3e`/`0x3f`/`0x42`（INTERRUPT[A/B]）—— 读一下就清中断锁存**，
   会瞒掉驱动。其余寄存器（含驱动自己在读的 `0x40` STATUS0）随便读。
2. ⛔ **判据不能用 `dmesg | grep -c 'xhci …: remove'`** —— 见 TL;DR。盯 sysfs
   `/sys/bus/usb/devices/9-1`（掉线持续 ~1-2 s，200 ms 轮询稳拿）。
3. ⚠️ `/run` 是 **noexec**，unit 必须写 `ExecStart=/bin/bash <脚本> run`。
4. ⚠️ **那两个 debugfs log 必须"持续滴灌"**（`live/` 里滚动文件）。2026-10-02 工程机实测：
   只在快照时读会 `overflow`，触发过程整段丢掉。

## 怎么按需复现（故障注入）—— 不用改内核

原理就是驱动自己做的事：源模式下 COMP 的阈值写在 `MEASURE(0x04)`
（`rd_mda_value[SRC_CURRENT_HIGH] = 61 → 2604mV`），而 3A 档的稳态 CC 只有 ~1.69V
（实测 `STATUS0(0x40)=0x93` ⇒ `BC_LVL=3`）。**把阈值压到 CC 电平以下，比较器必然置位**：

```bash
journalctl -k -b --no-pager | grep -c 'xhci-hcd xhci-hcd\..*\.auto: remove'   # 基线
i2cget -y -f 1 0x22 0x04          # 0x3d = 61（先备份）
i2cset -y -f 1 0x22 0x04 0x20     # ≈1.4V < 1.69V ⇒ COMP 置位 ⇒ 整条链跑一遍
i2cset -y -f 1 0x22 0x04 0x3d     # 还原
```

- **自愈 + 可重复**：驱动在切 CC 状态 / 重连时会自己把 MEASURE 写回正确值，所以
  "一次注入 ≈ 刚好一个故障周期"，想跑几次跑几次。
- **两种用法**：

| 目的 | 操作 | 预期 |
| --- | --- | --- |
| 复现故障 | `0x04 = 0x20` 并**保持** | 复检也看到 COMP 成立 ⇒ 必然拆控制器 |
| 验证去抖 | `0x04=0x20; sleep 0.02; 0x04=0x3d`（**~20-30ms 真瞬态**） | 100ms 去抖吃掉；`=0` 时仍拆 |

- **反向对照**：`MASK(0x0a) |= 0x20`（屏蔽 COMP_CHNG）/ `|= 0x80`（屏蔽 VBUSOK）⇒ 应当彻底
  不再拆口；还原写回 `0x5d`（驱动也会在下一次状态变化时自己重写）。
- ⛔ 这些都是**写寄存器**，`MEASURE`/`MASK` 都不属于"读了会清中断"的那三个。

⚠️ **两条实测踩到的坑（2026-10-02 工程机 LubanCat-5 V2 / 6.1.172）**：

1. **只在"板子是 source"时有效**。第一次注入成功拆口；之后口被对端抢成 sink，驱动随即把
   `MASK(0x0a)` 变成 `0x7c`（**COMP_CHNG 被 mask**、改用 BC_LVL），于是同样的注入**完全
   没反应**（`STATUS0` 里 `COMP=1` 但中断被屏蔽，驱动根本不知道）。
   注入前先确认 `power_role` 是 `[source]`、且 `MASK(0x0a)` 的 bit5 = 0。
2. **DRP 口在 detach 之后会被对端抢走角色**：工程机注入一次之后 `power_role` 从 `[source]`
   变成 `[sink]`、`gpio-77 (vbus-typec)` 变 `out lo`，**Quest 变成供电方且不再充电**
   （`dumpsys battery` ⇒ `AC powered: false`，电量掉到 15%）。用
   `echo source > /sys/class/typec/port0/power_role`（一次 PR_SWAP）能抢回来。
   ⇒ **做实验前先把测试机的口做成和老镜像一样的固定 source**（`-usbcon-fixedsrc`）。
   59 用的就是固定 source，所以它没有这条；但它照样犯原故障。

**注入不了的那一支（型 A / VBUSOK）**：VBUSOK 是硬比较器，寄存器伪造不了；`vbus_typec` 的
GPIO 被 regulator 占着、`/sys/class/regulator/N/state` 只读；PHY 的 `otg_mode` 也切不了
VBUS（`rockchip_set_vbus_power()` 第一行就是 `if (!rport->vbus) return 0;`，而本板 otg-port
没配 `vbus-supply`）。当前驱动已删除合成 `inject`，型 A 的逻辑在确定性 C 回归中测试；
这里的 MEASURE 板测只证明 CC 路径，不能作为 VBUS 瞬态的硬件证据。

## 量那个阈值：`tools/typec-comp-trace.sh`

那个"拔线判据"的参考电平写在 `MEASURE(0x04)`，而 `STATUS0(0x40)` bit5（COMP）= "CC 是否高于
该电平"。**把 COMP_CHNG mask 掉之后**（否则一观测就把连接打断），就能拿芯片当比较器用。

```bash
bash typec-comp-trace.sh sweep                  # 量 CC 电压
bash typec-comp-trace.sh trace --seconds=1800   # 盯 30 分钟，找瞬态持续多久
```

**`sweep` 实测（2026-10-02 工程机，3A 档）**：从 `0x3d`(2604mV) 往下扫，
翻转点在 **`0x2a`(1806 mV, COMP=0) 与 `0x29`(1764 mV, COMP=1) 之间**：

| 量 | 值 |
| --- | --- |
| 实测 CC 电压 | **≈1.76 ~ 1.81 V**（理论 5.0×5.1/(5.1+10)=1.69 V，实测略高 ⇒ 板子 VBUS 略高于 5.0V 或 Rp/Rd 有容差） |
| 驱动设的判据阈值 | **2604 mV**（3A 档 `rd_mda_value[61]`） |
| 余量 | **≈0.8 V**；比规范里 vRd-3.0 上限（约 2.04 V）也还高 0.56 V |

⇒ **"合规 sink 正常连接"不该越阈值**。越过去只能是对端瞬时松了 Rd（CC 往板子自己的 Rp 电平抬），
或芯片在 PD 收发/切测量通道时读岔了 —— 而现有代码**一次采样就判死、没有去抖**。

**`trace` 已验证**：人为制造一次 ~60 ms 的 COMP 置位，工具量到
`61 ms`（`comp1_episodes=1`），且**全程没有触发 detach**（mask 生效）。
⚠️ 采样间隔 **≈4.8 ms（~209 Hz）**（每采样 fork 一次 `i2cget` 是瓶颈）⇒ 只能分辨 **≥5-10 ms**
的瞬态；要更高分辨率得换成 python+ctypes 在进程内做 SMBus ioctl。
⚠️ **真实瞬态的时长还没量到**（要在一台会犯这个故障的机器上长期跑），而它正是
`detach_debounce_ms` 该设多少的唯一依据。

## 用 dtbo 做"判决实验"：光保住控制器够不够？

`rk3588-lubancat-5-v2-dwc3-rolehost.dtbo`（`role-switch-default-mode="host"`）让 detach 时
递下来的 `USB_ROLE_NONE` 映射成 HOST ⇒ 控制器不被拆，而且它**完全不动 VBUS/VCONN/CC**。
配上 MDA 注入就能回答"VBUS/CC 被撤掉、设备物理上还插着时，它会不会掉"。

### 实测结论（2026-10-02，工程机；同时装了 `-usbcon-fixedsrc` + `-dwc3-rolehost`）

```
xhci ...: remove             = 0     ← 控制器没被拆
xhci new USB bus registered  = 0     ← 总线没重新注册
usb 9-1: USB disconnect      = 1     ← 但设备掉了
[56.578] usb 9-1: USB disconnect, device number 2
[58.617] usb 9-1: new high-speed USB device number 3      ← 设备号 2→3（同一条总线）
```

快照（`9-1=DOWN` 那一刻）：`data_role [host]` / `power_role [source]`（固定 source，没被抢）、
**`gpio-77 (vbus-typec) = out lo`、`regulator vbus_typec = 0`** —— VBUS 被切。滴灌缓冲完整留到：

```
fusb302: [50.838222] IRQ: 0x20 … status0: 0xb3   ← COMP_CHNG, COMP=1
         [50.838226] IRQ: COMP_CHNG, comp=true
         [50.838236] cc1=Open, cc2=Open            ← 驱动立刻判"拔线"
         [50.845485] IRQ: VBUS_OK, vbus=Off        ← 紧接着 tcpm 把 VBUS 关掉
tcpm:    [50.838242] state change SRC_READY -> SRC_UNATTACHED
         [51.071614] vbus:=1 charge=0              ← 0.23s 后重新上电
```

⇒ **"VBUS 关掉只是不充电、USB 通信还正常"不成立**：控制器都保住了（设备号 2→3 证明总线没
重置），设备**照样掉线**并在 2 s 后重新枚举。
⇒ **`role-switch-default-mode="host"` 不足以修这台设备** —— 它只免掉"控制器拆建 + 设备号重置 +
udev 风暴"。**真正必须做的是不让瞬态触发 detach。**

## 方案 A：用户态 mask 中断（不改内核、不用重启）—— 实测通过

`tests/usb-typec-mask-ab.sh`：同一颗内核里做 A/B，各注入一次 CC 瞬态（`0x04=0x20` 保持 40 s）。

| 用例 | `MASK(0x0a)` | 注入后 40 s 内 | `usb 9-1: USB disconnect` |
| --- | --- | --- | --- |
| A | `0xfd`（bit5 COMP + bit7 VBUSOK 都 mask） | `9-1` 一直 up、`gpio-77` 一直 `out hi`、`MEASURE` 停在 `0x20`（驱动没被惊动） | **+0** |
| B | `0x5d`（还原） | 驱动照常 detach + 重连（`MEASURE` 被写回 `0x3d`） | **+1** |

A 里 `STATUS0` 的 COMP 位确实是 **1** —— 注入真生效了，只是驱动收不到中断。监视器也印证：
mask 那一轮 `events.log` 没有事件，还原那一轮多了一条 `teardown`。

⇒ **mask 能把这条故障链整条切断。** 59 上的操作就是
`i2cset -y -f 1 0x22 0x0a 0xfd`（还原 `0x5d`）。

> 这个测试按 `skills/developer-testing/SKILL.md` 声明了 oracle（`RELATIONSHIP` + `INVARIANT`），
> 运行时打印 `[ORACLE]/[SOURCE]/[EXPECTED]/[RESULT]`，退出时自动还原 MASK/MEASURE
> （连 INT/TERM 一起 trap）。**阴性对照跑不出来时整轮报 `INCONCLUSIVE`（退出码 5）**，不当 PASS。

⚠️ 三个未覆盖点：

1. **40 s 窗口内没有其它中断**。若 FUSB302 在 INTERRUPT 寄存器里仍然锁存被 mask 的事件，
   那么下一次"因别的原因读中断寄存器"时可能补上一次 detach —— 需要更长的 soak 才能排除。
2. **mask 会被驱动重写**：`fusb302_set_toggling()` / `fusb302_set_cc_pull()` 都会重写 MASK。
   一旦真发生一次状态切换，mask 就没了，需要重新施加 —— 现场长期用要配个"定期/事件后重新
   施加"的小服务。
3. 代价：PD/typec 层不再自己检测真拔线（USB 层照样掉、应用照样能看到"设备未连接"）。

## 在 59 上落地

→ 见 [typec-rollout-and-decision.md](typec-rollout-and-decision.md)（6.1.118 换内核 /
用户态 mask 两条路线、风险，以及"放弃 device 能力"的决定与 20 轮验证）。

## 当前去抖内核与测试

当前实现、计时策略、Ra/Ra 恢复和验证边界统一见
[fusb302-debounce.md](fusb302-debounce.md)。参数默认为 100 ms，0 可禁用；
只读 debugfs `stats` 为 version 2，不再提供合成 `inject`。

```bash
python3 tests/fusb302-debounce.py                         # 确定性 C 回归，不需要板子
sudo bash tests/usb-typec-detach-debounce.sh --cycles 20 --keep
```

板测直接写 MEASURE，要求固定 source 且接着正常 USB sink；自动恢复保存的寄存器和参数。
判据是实际 USB 设备不掉/重新枚举，并核对 TCPM 的 source、VBUS 与 accessory 状态，
不能用 xHCI 是否拆除代替。100 ms 是现行板级过滤策略，真实故障的脉宽仍需现场测量。

## "放弃 device 能力"这个决定

→ 见 [typec-rollout-and-decision.md](typec-rollout-and-decision.md)：依据（DRP 下 5 轮注入
1 次被抢成 sink、Quest 反供电、头显耗光）、代价（这个口不能当 USB device 用）、以及新默认配置
（`-usbcon-fixedsrc` + `-dwc3-rolehost`）的 **20 轮验证：0 次被抢 / 0 次拆控制器 / `SNK_=0`**。

## References

- [typec-detach-debounce.md](typec-detach-debounce.md) —— 现象与根因（本篇的前置）
- `tools/typec-watch.sh`、`tests/usb-typec-mask-ab.sh`、`tests/usb-typec-detach-debounce.sh`
- `overlay/boot/firmware/dtbs/rockchip/overlay/{rk3588-lubancat-5-v2-dwc3-rolehost,rk3588-lubancat-5-v2-usbcon-fixedsrc}.dts`
- 下游两个 dtbo 的用法与警告见 `overlay/boot/firmware/dtbs/rockchip/overlay/README.md`
