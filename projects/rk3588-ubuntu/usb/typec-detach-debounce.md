# Type-C 口把 CC/VBUS 瞬态当成"拔线" —— Quest 每秒级断连、LCD 报手柄低电

> 现场"Quest 断连 / 左右手柄低电"不是电池、也不是线的问题：是板子 Type-C(OTG) 口上
> FUSB302 的**两个瞬时判据**（CC 比较器、VBUSOK）被直接当成"对端被拔掉"，一次瞬态就把
> 整条 DWC3/xHCI 主机控制器拆掉重建 —— 口上的 Quest 于每秒级断连重枚举，采集程序读电量
> 失败按 0% 上报，LCD 就报"手柄低电 / 头显未连接"。
>
> 本篇只讲**现象与根因**。怎么按需复现、怎么验证修法、怎么在不能刷镜像的现场落地，
> 见 [typec-fault-inject-and-verify.md](typec-fault-inject-and-verify.md)。

## TL;DR

- **症状在设备，根因在主机**：一条瞬态 → tcpm 拆 Type-C 口 → `usbdrd3_0` 上的伴生 xHCI
  （`xhci-hcd.N.auto`，usb9/usb10）被 unregister 再 probe → 口上设备全部断连重枚举。
- 判据：`journalctl -k | grep -c 'xhci-hcd xhci-hcd\..*\.auto: remove'`。现场实测一个 boot
  **706 次**拆除、**233 次** `usb 9-1: USB disconnect`；突发内每 ~3.5 s 一次，每 ~50 s 一轮。
- "手柄低电"是**假象**：掉连期间读不到电量 → 采集侧按 0% 上报；恢复后立刻回 100%。
  拔插换线无效（换过新线仍复现），因为问题在主机侧的协议处理，不在线束。
- 修法：仅对已建立的 source 连接过滤短时 CC/VBUS 丢失，持续丢失仍上报；
  `detach_debounce_ms=0` 用于反面对照。当前实现与插拔恢复测试见
  [fusb302-debounce.md](fusb302-debounce.md)。100 ms 是板级策略，不是 detach 的规范要求。
- ⚠️ 别把 `data-role="host"` 理解成"dwc3 锁死 host"：detach 时 tcpm 递下来的是
  **`USB_ROLE_NONE`**（不是 device），DWC3 把它映射成"默认角色"，而这块板 DT 里没有
  `role-switch-default-mode` ⇒ 默认是 peripheral ⇒ 控制器被拆。见下文那两节。
- ⚠️ 日志里的 `VBUS off` **有一半以上是 tcpm 自己关的**（detach 流程里的 `tcpm_init_vbus()`
  → `set_vbus(false)`），不是"板子供电故障"；`VBUS VSAFE0V` 也不是独立测量。
- 这个故障**会自己来自己走**（实测连续犯 57 分钟后自己停）⇒ 别等它复现来验证，用故障注入。

## 现象与判据（怎么确认是这个）

现场 59 号（`cat@<HOST_08>`，内核 6.1.118）LCD 弹窗"头显未连接 + 左右手柄低电"，
手柄电池刚换过、头显线也换过新的。

```bash
# 这才是判据（dmesg 在本板会被噪声冲掉，见 typec-fault-inject-and-verify.md）
journalctl -k -b --no-pager | grep -c "xhci-hcd xhci-hcd\..*\.auto: remove"   # 706
journalctl -k -b --no-pager | grep -c "usb 9-1: USB disconnect"               # 233
```

内核日志里不是设备报错，而是**控制器自己在拆**：

```
xhci-hcd xhci-hcd.15.auto: remove, state 4
usb usb10: USB disconnect, device number 1
xhci-hcd xhci-hcd.15.auto: USB bus 10 deregistered
xhci-hcd xhci-hcd.15.auto: remove, state 1
usb 9-1: USB disconnect, device number 2
xhci-hcd xhci-hcd.15.auto: USB bus 9 deregistered
xhci-hcd xhci-hcd.15.auto: new USB bus registered, assigned bus number 9   ← 又起来
```

`xhci-hcd.15.auto` 挂在 `usbdrd3_0/fc000000.usb`（Type-C OTG）上；Quest 挂在这条总线上
（`9-1`，`2833:5013` "Quest 3"）。**每次都是全新总线，所以设备号永远是 2。**

## 为什么"手柄低电"是假象

正常上报 `Quest左=100% 右=100% 头显=48~49%`；故障瞬间三条一起变 **0%**，恢复后立刻回 100%。
`adb shell dumpsys battery` 一直是 `AC powered: true`、电量单调上涨。所以"低电"只是
**USB 链路断掉期间读不到电量**，采集侧把失败当 0% 上报。设备侧同一时刻也在掉：
Quest 的 `UsbPortManager` 日志同样记录了 DISCONNECT/CONNECT，且 `lastConnectDurationMillis`
只有 3.1 s / 97 s 这种短命值。

## 根因（代码级）

FUSB302 有两个"对端没了"的单比特、非锁存判据，`fusb302_irq_work()` 里原来都是**立刻**上报：

```c
if (interrupt & FUSB_REG_INTERRUPT_VBUSOK) {            /* VBUS 掉到阈值以下 */
        vbus_present = !!(status0 & FUSB_REG_STATUS0_VBUSOK);
        if (vbus_present != chip->vbus_present) { chip->vbus_present = ...; tcpm_vbus_change(); }
}
if ((interrupt & FUSB_REG_INTERRUPT_COMP_CHNG) && intr_comp_chng) {
        comp_result = !!(status0 & FUSB_REG_STATUS0_COMP);
        if (comp_result) {                              /* "cc level > Rd_threshold, detach" */
                chip->cc1 = TYPEC_CC_OPEN; chip->cc2 = TYPEC_CC_OPEN;
                tcpm_cc_change(chip->tcpm_port);
        }
}
```

`tcpm` 在 `SRC_READY` 收到这两种事件都**没有去抖**，直接 `SRC_UNATTACHED` → 关 VBUS、
把 data role 切成 device → `dwc3_host_exit()` → 整条 xHCI 没了。顺序（混合了 tcpm log 与
xhci tracepoint 的时间戳）：

```
2598.851  VBUS off ; state change SRC_READY -> SRC_UNATTACHED     ← tcpm 先决定
2598.857  dwc3_readl/writel (GCTL/GUSB2PHYCFG)                   ← 切 data role
2598.864  xhci_dbg_init: // Halt the HC                           ← xHCI 被拆
2598.880  CC1: 2 -> 2, CC2: 1 -> 1 [TOGGLING, connected]          ← 29 ms 后"又插上"
2599.191  xhci_init / xhci_run                                    ← 控制器重建
2599.826  port-0: Enabled Link:U0 PortSpeed:3                     ← 设备重新枚举
```

阈值本身就偏紧：板子 `source-pdos = PDO_FIXED(5000, 3000)`（对外宣告 5V/3A），而 3A 档用的
比较阈值是 `rd_mda_value[SRC_CURRENT_HIGH] = 61`（**2604 mV**，实测 `MEASURE(0x04)=0x3d`），
3A 档稳态 CC 只有 ~1.69 V —— 对端 CC 稍微抬高一点就越过阈值。

**两类触发各占一半**（fusb302 log）：

| 型 | 日志特征 | 含义 |
| --- | --- | --- |
| A（VBUS） | `IRQ: 0x80` + `status0: 0x13`（VBUSOK=0, COMP=0）→ 紧跟 `VBUS VSAFE0V` | VBUS 真被测到掉到阈值以下，前后无 CC 变化 |
| B（CC） | `IRQ: 0x20` + `status0: 0xb3`（VBUSOK=1, COMP=1）→ 驱动报 `cc1=Open, cc2=Open` | 比较器瞬态被当成"整根线被拔了" |

## 为什么 `data-role="host"` 挡不住 dwc3 切 device

老镜像的 dtbo 把 Type-C 口写成了 `data-role="host"` + `power-role="source"`，很容易以为
"dwc3 被锁死在 host 了"。**没有。** `data-role` 只是**能力描述**：`typec_get_fw_cap()` 把它
落到 `cap->data`，tcpm 里唯一的作用是这一段 —— 挡住"主动要 device 数据角色"：

```c
	} else if (port->typec_caps.data == TYPEC_PORT_DFP) {
		if (data == TYPEC_HOST) { ... } else { return -ENOTSUPP; }
	}
```

而 detach 时 tcpm 要的**不是 device**，而是"没有连接"：

```c
static void tcpm_reset_port(struct tcpm_port *port)          /* ← tcpm_src_detach() 调它 */
{
	...
	tcpm_mux_set(port, TYPEC_STATE_SAFE, USB_ROLE_NONE, TYPEC_ORIENTATION_NONE);
	...
}
static int tcpm_mux_set(struct tcpm_port *port, int state, enum usb_role usb_role, ...)
{
	tcpm_log(port, "Requesting mux state %d, usb-role %d, orientation %d", state, usb_role, orientation);
	...
	if (port->role_sw)
		ret = usb_role_switch_set_role(port->role_sw, usb_role);   /* ← 推给 dwc3 */
	...
}
```

实测日志正好对上（`TYPEC_STATE_SAFE=0` / `USB_ROLE_NONE=0` / `TYPEC_ORIENTATION_NONE=0`）：

```
2598.856  Requesting mux state 0, usb-role 0, orientation 0     ← detach：NONE
2599.080  Requesting mux state 1, usb-role 1, orientation 1     ← attach：HOST
```

而 DWC3 把 `USB_ROLE_NONE` 落进 default 分支，去读"默认角色"：

```c
static int dwc3_usb_role_switch_set(struct usb_role_switch *sw, enum usb_role role)
{
	switch (role) {
	case USB_ROLE_HOST:   mode = DWC3_GCTL_PRTCAP_HOST;   break;
	case USB_ROLE_DEVICE: mode = DWC3_GCTL_PRTCAP_DEVICE; break;
	default:                                            /* ← USB_ROLE_NONE 走这里 */
		if (dwc->role_switch_default_mode == USB_DR_MODE_HOST)
			mode = DWC3_GCTL_PRTCAP_HOST;
		else
			mode = DWC3_GCTL_PRTCAP_DEVICE;
	}
	dwc3_set_mode(dwc, mode);
}
```

默认角色来自 DT 的 `role-switch-default-mode`；这块板（仓库 DTS 与 59 实测 `/proc/device-tree/`
都）**没有这个属性** ⇒ `usb_get_role_switch_default_mode()` 返回 `USB_DR_MODE_UNKNOWN`
⇒ `dwc3_setup_role_switch()` 把它强制成 `USB_DR_MODE_PERIPHERAL`：

```c
	dwc->role_switch_default_mode = usb_get_role_switch_default_mode(dwc->dev);
	if (dwc->role_switch_default_mode == USB_DR_MODE_HOST) { ... }
	else { dwc->role_switch_default_mode = USB_DR_MODE_PERIPHERAL;   /* ← 走这条 */
	       mode = DWC3_GCTL_PRTCAP_DEVICE; }
```

⇒ `dwc3_set_mode(DEVICE)` ⇒ `dwc3_host_exit()` ⇒ 伴生 xHCI 被拆。
**这就是 `VBUS=OFF`（或 CC 瞬态）能一路传到"整条控制器消失"的最后一环。**

注：这块板的 DWC3 **没有 `extcon` 属性**（仓库 DTS 与 59 实测都没有）⇒ `dwc->edev` 是 NULL，
`dwc3_drd_notifier` 那条路根本不注册，能把 dwc3 推来推去的**只有 role switch 这一条**。

## `VBUS off` 为什么会 off —— 是 tcpm 自己关的

日志里 `VBUS off` 有**两层**，必须分开看：

| 日志 | 含义 |
| --- | --- |
| `IRQ: VBUS_OK, vbus=Off` | FUSB302 的比较器**测到** VBUS 低于阈值（测量层） |
| `vbus := Off` | tcpm **命令** fusb302 关掉 `vbus_typec` 这个 GPIO 开关（命令层） |

**detach 流程本身就会把 VBUS 关掉**：

```c
/* tcpm_src_detach -> tcpm_detach -> tcpm_reset_port 里： */
	tcpm_init_vbus(port);	/* also disables charging */
/* 而 tcpm_init_vbus() 就是： */
	ret = port->tcpc->set_vbus(port->tcpc, false, false);   /* fusb302: regulator_disable(vbus) → GPIO 低 */
```

同一个 `tcpm_reset_port()` 里还会 `tcpm_init_vconn()`（**VCONN off** —— Quest 用的 e-marked 线，
线里的 e-marker 芯片靠 VCONN 供电，一断线缆自身状态就变），随后 `SRC_UNATTACHED` 处理里
`tcpm_start_toggling()` 进 `TOGGLING`（**CC 不再稳定拉 Rp**）。

所以"设备一直对外供电、线也正常、Quest 一直在充电"和"日志里 VBUS off"**不矛盾**：
是判定 detach 之后板子**主动把电掐掉约 0.23 s**（`vbus := Off` → … → `vbus := On`），
Quest 因此掉电掉线，然后 tcpm 重新协商、再合上 —— 宏观上看着"一直在充电"。

⛔ 另外纠正我自己之前的一处说法：`VBUS VSAFE0V` **不是**独立测量。fusb302 没实现
`is_vbus_vsafe0v`，tcpm 的代码是"TCPC 测不了 vSafe0V 时，就把 VBUS 不在直接当成 vSafe0V"：

```c
		} else {
			_tcpm_pd_vbus_off(port);
			if (!port->tcpc->is_vbus_vsafe0v ||
			    port->tcpc->is_vbus_vsafe0v(port->tcpc))
				_tcpm_pd_vbus_vsafe0v(port);
		}
```

即那条 `VBUS VSAFE0V` 只说明"tcpm 认为 VBUS 不在了"，不带额外信息。

两类触发分开理解：

| 型 | 谁先动 | 说明 |
| --- | --- | --- |
| B（CC） | `COMP_CHNG, comp=true` → tcpm 判定 detach → **tcpm 命令关 VBUS** | 此刻 VBUS 本来是好的，"off" **纯粹是 tcpm 关的** |
| A（VBUS） | `IRQ: VBUS_OK, vbus=Off` 比 `vbus := Off` **早约 5 ms** | FUSB302 那一刻确实测到 VBUS 低；是"真跌落"还是"比较器误读"未定（需示波器），但后续长时间的 off 仍是 tcpm 关电的结果 |

## 当前修复

FUSB302 对已建立 source 连接的短时 CC/VBUS 丢失做板级过滤，保留真实持续丢失与重连。
参数、计时/错误恢复、Ra/Ra 重新检测，以及本地和板上测试见
[fusb302-debounce.md](fusb302-debounce.md)。驱动只保留只读 stats，寄存器测试走真实 IRQ。

## 还没搞清楚的

- **触发这个循环的"故障条件"是什么、为什么自己会停**。实测从 boot 起连续犯 57 分钟，
  14:27:10 之后**自己停了**（Quest 侧 `UsbPortManager` 记这次连接从 14:27:11 起一直没断）。
  板子侧前后状态**逐项相同**（`MASK=0x5d`/`MEASURE=0x3d`/`SWITCHES0=0x64`/`STATUS0=0x93`、
  host/source、VBUS 仍 on），且那几分钟里**只有 Type-C 这一条口在抖**，hub 上的相机/手套/LCD
  都没动 ⇒ 变化只在 Quest 侧或电气层面。
- Quest 的平均取电前后没有量级差别（`Charge counter` 差分：故障期间 ~1.36 A，停掉后 ~1.0-1.4 A），
  所以"纯过载欠压自激"证据不足；目前更倾向**两侧状态机卡在一个半协商的稳态**里，
  被一次扰动（最后那次断连前 2.5 s Quest 正在拉起透视相机通路）踢出来。

## References

- `kernel-6.1/drivers/usb/typec/tcpm/fusb302.c`（`fusb302_irq_work` / `fusb302_handle_comp` / `fusb302_handle_vbus`）
- `kernel-6.1/drivers/usb/typec/tcpm/tcpm.c`（`_tcpm_pd_vbus_off` / `_tcpm_cc_change` /
  `tcpm_reset_port` / `tcpm_mux_set` / `tcpm_set_roles` / `tcpm_init_vbus`）
- `kernel-6.1/drivers/usb/dwc3/drd.c` `dwc3_usb_role_switch_set()` / `dwc3_setup_role_switch()`，
  `kernel-6.1/drivers/usb/common/common.c` `usb_get_role_switch_default_mode()`
- `kernel-6.1/drivers/usb/typec/class.c` `typec_get_fw_cap()` —— `power-role` 决定 port type，
  `try-power-role` **只在 `power-role="dual"` 时才读**（所以老镜像 DT 里的 `try-power-role="sink"`
  是死值；那个老 dtbo 写的是 `data-role="host"` + `power-role="source"` = 固定 source）
- [usb/quest3-usb-power.md](quest3-usb-power.md) —— Quest 3 供电/角色争抢与 `lsp-usb-powerfix`
- [usb/usb-gadget.md](usb-gadget.md) —— OTG/Type-C 角色总览
- [usb/typec-fault-inject-and-verify.md](typec-fault-inject-and-verify.md) —— 复现·验证·落地
