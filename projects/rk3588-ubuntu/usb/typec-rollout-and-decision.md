# Type-C 掉连：在 59 上怎么落地、以及"放弃 device 能力"的决定

> 前两篇讲的是**现象/根因**（[typec-detach-debounce.md](typec-detach-debounce.md)）与
> **复现/验证**（[typec-fault-inject-and-verify.md](typec-fault-inject-and-verify.md)）。
> 本篇讲**工程落地**：不能刷镜像的现场（59，内核 6.1.118）怎么改，以及
> 2026-10-02 定的产品决定 —— **Type-C 口锁成固定 source，放弃 device 能力**。

## TL;DR

- **决定（2026-10-02，用户）：放弃 device 能力，Type-C 口锁成固定 source。**
  默认清单里 `-usbcon-fixedsrc` + `-dwc3-rolehost` 一起启用
  （`lubancat-5-{v2,5io}.overlays`）。用户口径："不能保留 device 能力了 —— 如果板子切换成 sink
  不给 quest 充电，quest 的电很快就会耗光，设备就没法继续工作了，只能先给 quest 单独充电。"
- ✅ **解决**："detach 之后被对端抢成 sink ⇒ Quest 反供电 ⇒ 头显耗光 ⇒ 设备停摆"。
  **20 轮注入验证：0 次被抢、0 次拆控制器、`SNK_=0`**（对照：DRP 下 5 轮里 1 次被抢、每轮都拆）。
- ⚠️ **没解决**："每次瞬态的掉线重枚举" —— 同一段 20 轮里 `usb 9-1: USB disconnect = 20`。
  那条只能靠**去抖（51 换内核）或用户态 mask**，两篇前置文档里有做法。
- **接口边界**：这个口不再作为 USB device；device 命令与专用配置已移除，
  当前主机工具见 [usb-gadget.md](usb-gadget.md)。
- ⚠️ `-dwc3-rolehost` **只在固定 source 下有效**（DRP 下实测无效），别把它们拆开用。

## 在 59 上落地（它不能刷镜像）

59 跑 **6.1.118**，`CONFIG_TYPEC_FUSB302=y`（内建、没有 .ko、`modinfo` 找不到）、
`# CONFIG_KPROBES is not set`（没有 kprobe/eBPF 这种运行时后路）。

**路线 1（换内核，上真正的去抖）**：这棵树能精确复现 ——

> ✅ **这个补丁已经在 6.1.172 上实测通过**（工程机：`tests/usb-typec-detach-debounce.sh`
> 通过 9/失败 0；真实 ~50 ms 瞬态下 `=0`/`=10` 会掉设备、`=100` 不掉 ⇒ 参数真实生效）。
> 细节见 [typec-fault-inject-and-verify.md](typec-fault-inject-and-verify.md)。
> 所以搬到 6.1.118 只是"同一份补丁 + 另一棵树 + 别的东西都没动"的机械动作。

- 本仓 git 里 `e4e9c64cb^` 就是 6.1.118 全树（那笔 commit 是 6.1.118 → 6.1.172 的 bump）；
- 59 的 `/proc/version` = `aarch64-linux-gnu-gcc 13.3.0 … root@lubancat`（板卡原生编），
  deb = `linux-image-6.1.118 6.1.118-1`，`CONFIG_LOCALVERSION=""` ⇒ `uname -r` = `6.1.118`；
- `fusb302.c` 6.1.118 与 6.1.172 只差 **26 行**（都与本处无关），**补丁改的两段 IRQ 处理
  逐字相同**；6.1.118 的 tcpm 里 `SRC_UNATTACHED` / `tcpm_mux_set(…, USB_ROLE_NONE, …)` 也都在；
  59 运行时打出来的 fusb302 日志字符串，在本树里逐条命中。

做法：git 取 6.1.118 树 + 59 的 `/boot/config-6.1.118` + 补丁 → **开发板原生编**（gcc 13.3.0
与 59 同版本）→ **只替换 vmlinuz**，modules 一个不动（只改了内建的 fusb302、没动任何导出符号
⇒ `CONFIG_MODVERSIONS` 的 per-symbol CRC 不变，vermagic 由 config 决定也一致 ⇒ 现有 37 个模块
照旧加载）→ 重启 → 跑 `inject` 的 `100 / 0` A/B。
⚠️ 59 是**远程无串口**设备，换内核属于"起不来就得上现场"的操作，而回退链还没做 —— 先有回退
手段再换。

**路线 2（用户态 mask 中断）**：不改内核、不用重启，代价是 PD 层不再检测真拔线 + mask 会被
驱动重写（见 [typec-fault-inject-and-verify.md](typec-fault-inject-and-verify.md) 的"方案 A"）。

## 决定的依据："detach 之后被抢成 sink"实测

配置 = 新镜像默认的 DRP（`data-role=dual` + `try-power-role=source`）+ 保留 `-dwc3-rolehost`，
连续注入 5 次 detach：

| 次数 | 结果 |
| --- | --- |
| **5 次里 1 次** | `power_role` 变 `source [sink]`、`gpio-77 (vbus-typec) 变 out lo`、**Quest `AC powered: false` / `Max charging current: 0`** ⇒ 板子不给电了、**由 Quest 反过来给板子供电**、头显停充 |
| 其余 4 次 | 板子自己用一次 `POWER_ROLE_SWAP` 把 source 抢回来（`SNK_READY -> AMS_START [POWER_ROLE_SWAP]` → `SRC_TRANSITION_SUPPLY -> SRC_READY`） |

**机制（tcpm 日志，一次典型循环）—— 关键是"板子会先以 sink 接上"**：

```
VBUS off
Requesting mux state 0, usb-role 0, orientation 0    ← NONE（detach）
Requesting mux state 1, usb-role 2, orientation 1    ← DEVICE！板子先当 sink 接上
SNK_DEBOUNCED -> SNK_ATTACHED -> SNK_STARTUP -> SNK_READY
Requesting mux state 1, usb-role 1, orientation 1    ← HOST（DATA_ROLE_SWAP 拿回数据 host）
SNK_READY -> AMS_START [POWER_ROLE_SWAP]
Requested 5000 mV, 3000 mA   /   SRC_TRANSITION_SUPPLY -> SRC_READY   ← PR_SWAP 拿回 source
```

一次循环里 `usb-role` 出现 `0`×2 / `1`×2 / **`2`(DEVICE)×4**。

两个结论：

1. **`-dwc3-rolehost` 在 DRP 配置下没用**：板子先以 sink 接上，typec 层递下来的是
   `usb-role 2`（**DEVICE**，不是 `NONE`）⇒ `dwc3_set_mode(DEVICE)` ⇒ 控制器照样被拆 ——
   实测每次注入 `xhci ...: remove` 都 **+2 ~ +10**。它**只在固定 source 下有效**
   （进不了 SNK，递下来的才是 `NONE`→HOST，实测 **0 次拆除**）。
2. **被抢是会发生的（≈1/5），而且被抢走时头显停充、还反过来给板子供电**；即使没被抢走，
   板子也**每轮都要走一遍 sink → DR_SWAP → PR_SWAP** ⇒ 控制器每轮都被拆（设备重枚举）。

## 新默认配置的验证（连续 20 轮注入）

配置 = `-usbcon-fixedsrc` + `-dwc3-rolehost`（固定 source）：

```
端口  : [host] / [source]                     ← data_role 只有 host、power_role 只有 source
#1..#20: 全部 pr=[source]  gpio=hi  xhci+0  SNK_=0
被抢成 sink : 0 / 20      （对照：DRP 下 1 / 5）
xhci 被拆   : 0 / 20      （对照：DRP 下每轮都拆）
Quest      : AC powered: true  Max charging current: 3000000  level 31→33（一路在充）
```

`SNK_=0` = 这个口**一次都没进过 sink 态** ⇒ 机制上不可能被抢。
⚠️ 但同一段里 `usb 9-1: USB disconnect = 20`、设备号 18→22（同一条总线）——
**掉线重枚举仍在**，那条要靠去抖 / mask。

## 结论 / 剩余工作

- 现场镜像使用**固定 source**（`-usbcon-fixedsrc`）+ `-dwc3-rolehost`，两者一起启用。
  当前运行工具不提供 PC-device 模式；维护通过 SSH 或板载 UART。
- ❌ **去抖 / mask 仍然必须做** —— 新默认配置只去掉"角色被抢 + 控制器拆建"，
  那 ~0.23 s 的 VBUS 断电照样让设备掉线重枚举。
- 顺带查清的一件事：工程机上那台 Quest "电量卡在 13% 不动"**不是**板子/DRP 的问题，
  同次观测醒着净电池电流接近零，睡下转为约 +1.94 A；该值不是 USB 输入电流——
  见 [quest3-usb-power.md](quest3-usb-power.md) 的「电池电流的观测方法」。

## References

- [typec-detach-debounce.md](typec-detach-debounce.md) —— 现象与根因
- [typec-fault-inject-and-verify.md](typec-fault-inject-and-verify.md) —— 复现、验证脚本与工具
- `overlay/boot/firmware/dtbs/rockchip/overlay/README.md` —— 三个 dtbo 的用法与警告
- [usb-gadget.md](usb-gadget.md) —— 当前主机查询、外设重新枚举与 device 功能退役
