# v2 的 USB 口细节：USB2/USB3 的稳定标识、白色口背后的 HUB、现场接线

> 本文是 [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) 的**细节篇**：
> 同一个物理口上"USB2 设备与 USB3 设备的内核编号为什么不同"、蓝色双层 A 口的稳定标识、
> 白色双层两个口背后那颗 CH334F hub、以及工程机现场接线的实测读数。
> 物理口 ↔ 内核控制器的**映射表和原理图交叉验证**见主篇。
> ⚠️ 正文**原样搬自主篇**（从「蓝色双层 A 口」那节起）。

## TL;DR

- 同一个物理口上，**USB2 设备与 USB3 设备会走不同的控制器**（`usbN` 编号不同）——
  按 `ID_PATH` 绑定时要成对看，别只认一个号。
- 白色双层那两个口背后是**板载 CH334F hub**（不是直连 SoC）；蓝色双层是直连的 A 口。
- 现场接线实测（`tools/lsp-usb-map.sh` 一条命令读出）在主篇也有对应表。


## 蓝色双层 A 口：USB2 / USB3 的**稳定标识**（2026-09-17）

> ⚠️ **本节不记 BUS 号**。BUS 号（`usbN`）是每次启动／按插拔顺序重新分配的，同一块板上都变过
> （带板载 hub 的那个 EHCI 在不同启动里就换过号），写下来只会误导。
> **认口只用这些稳定标识**：`ID_PATH`（写 udev 规则用）、sysfs platform 路径、控制器节点名。
> 下文本文档～本文档里出现的 `bus N` 都只是**当时那一次启动的观测值**，用来做证据，不是标识。

**① 蓝色·下层 = `J16A`（TYPEC1 / OTG1 / 手册 Controller1）** —— 一个 xHCI 实例，SS+HS 同控制器 ✅

| 项 | 值 |
| --- | --- |
| sysfs platform 路径 | `/sys/devices/platform/usbdrd3_1/fc400000.usb/xhci-hcd.4.auto/` |
| 控制器节点 / 驱动 | `usbdrd3_1`（子节点 `usbdrd_dwc3_1`）；总线驱动 `xhci-hcd`（USB3.0，同时提供 USB2 与 USB3 两条根总线） |
| `ID_PATH`（插设备时） | `platform-xhci-hcd.4.auto-usb-0:1`（设备）；接口为 `platform-xhci-hcd.4.auto-usb-0:1:1.0` |
| SoC 侧来源 | USB2 = `u2phy1_otg`；USB3 = `usbdp_phy1_u3` |
| 原理图网名 | `TYPEC1_OTG_DM/DP`（USB2）+ `TYPEC1_SSRX1N/P`、`TYPEC1_SSTX1N/P`（USB3） |
| 关键性质 | **SS 与 HS 属于同一个 xHCI 实例** ⇒ ✅ 可作 4 口 USB3 hub 的上游 |

**② 蓝色·上层 = `J16B`（USB2 HOST0 + USB30 HOST2 / 手册 Controller2）** —— **拼接口**（SS/HS 分属两个控制器）

| 角色 | sysfs platform 路径 | 驱动 | `ID_PATH` | SoC 侧来源 / 原理图网名 |
| --- | --- | --- | --- | --- |
| USB2 (HS) | `/sys/devices/platform/fc840000.usb/` | `ohci-platform`（同名 EHCI 是 `fc800000.usb`） | `platform-fc840000.usb-usb-0:1` | `u2phy2`（USB2 HOST0）/ `USB20_HOST0_DM/DP` |
| USB3 (SS) | `/sys/devices/platform/usbhost3_0/fcd00000.usb/xhci-hcd.3.auto/` | `xhci-hcd` | `platform-xhci-hcd.3.auto-usb-0:1` | `combphy2_psu`（Mux2：PCIE20/SATA30/USB30 复用）/ `USB30_SSRX/SSTX` |

⚠️ 后果：**同一个口按设备速率落到不同控制器**（USB3 → Controller2；USB2 → HOST0）⇒ 该口 `ID_PATH`
会随设备速率变化，**别在这个口上绑 udev 符号链接**；也**不能挂 hub**（会分裂成两棵 USB 树）。

**③（参考）白色双层 2 个口 = USB2 only，挂在板载 4 口 hub 后面**

| 项 | 值 |
| --- | --- |
| 上游 platform 路径 | `/sys/devices/platform/fc880000.usb/`（`ehci-platform` = `usb_host1`；其 OHCI companion 是 `fc8c0000.usb`） |
| 下游 | 板载 4 口 USB2 hub（在 `usb_host1`/`u2phy3` 下面） |
| 两个口的 `ID_PATH` | `platform-fc880000.usb-usb-0:1.3`（hub 端口 3 = **上层** = J17A）、`platform-fc880000.usb-usb-0:1.4`（hub 端口 4 = **下层** = J17B） |
| SoC 侧来源 | `u2phy3`（USB2 HOST1） |

**④ 现场怎么重新确认**（不依赖 BUS 号）：

```bash
# 最权威：某个设备到底挂在哪 —— ID_PATH / DEVPATH 里就带着控制器名
udevadm info -q property -p /sys/bus/usb/devices/<N>-<M> | grep -E 'ID_PATH|DEVPATH'
# 或者列出每个根 hub 的 platform 路径（第一列的 usbN 只是"当前这次"，路径里的控制器名才是标识）
for b in /sys/bus/usb/devices/usb*; do
  printf '%-5s %-7s %s\n' "$(basename "$b")" "$(cat "$b/speed")M" "$(readlink -f "$b" | sed 's|/sys/devices/||')"
done
```

记忆锚点（路径里的控制器名才是稳定的）：`platform/usbdrd3_1/…` = **OTG1 = J16A（蓝色下层）**；
`platform/fc840000.usb` = **HOST0 = J16B 的 USB2**；`platform/usbhost3_0/…` = **Controller2 = J16B 的 USB3**；
`platform/fc880000.usb` = **USB2 HOST1（白色双层背后的 hub）**。


## 白色双层那两个口背后是 HUB：CH334F（2026-09-17 用户读图）

白色双层两个 A 口（**J17A = `DP3/DM3`、J17B = `DP4/DM4`**）接在 **CH334F**（4 口 USB2 hub）上：

| CH334F 端口 | 原理图网名 | 去向 |
| --- | --- | --- |
| 上游 | `USB20_HOST1_DP/DM` | RK3588 **USB2.0 HOST1**（`u2phy3` / `usb_host1`；⚠️ 不是 `u2phy2`/HOST0 —— 那个被 J16B 的 USB2 占了）＝ 实测"挂着板载 4 口 hub 的那个控制器" ✓ |
| 下游 1 | `HUB_DP1/DM1` | **MINI-PCIE 槽的 `USB-D+/D-`** |
| 下游 2 | `HUB_DP2/DM2` | **PCIe 2.0 x1 E-KEY 槽的 `USB-D+/D-`** |
| 下游 3 | `DP3/DM3` | 白色双层 **J17A** |
| 下游 4 | `DP4/DM4` | 白色双层 **J17B** |

**与实测完全吻合**：该 hub 的 4 个下游口里，实测 **端口 1 = `btusb`（蓝牙）、端口 3/4 = 两个白色 A 口** ✓
⇒ **端口 1 那个蓝牙，就是 Mini-PCIe 槽里那块 WiFi/BT 卡的 USB2 功能**（该槽的 PCIe 部分是 WiFi 本体）。

**三条推论**：

1. ⭐ **WiFi 掉 ≠ 蓝牙掉**：Mini-PCIe 槽的 PCIe（WiFi）与 USB2（蓝牙）走**两条完全不同的路**
   —— PCIe 走 `combphy0_ps` → `pcie2x1l2`；USB2 走 CH334F hub → `u2phy3`/`usb_host1`。
   这正好解释了 §4.1 的 sata0 实验现象：切了 sata0 后 `lspci` 空、WiFi 消失，但 **`btusb` 还在**（蓝牙照常）。
2. **两个槽位都带 USB2**（Mini-PCIe 与 E-KEY 都从 CH334F 引了 D+/D-）⇒ 插带蓝牙/串口的模块卡都能用。
3. **白色双层 2 口 + 两个槽的 USB2 + 蓝牙，共用同一条 480 Mbps 上游**（`u2phy3`）：
   4 个下游口共享 ≈40 MB/s。要在白色口上挂 USB2 相机/跑大量数据时，注意与蓝牙抢带宽。

⚠️ **上/下 与 J17A/J17B 的对应 = 已确认**（2026-09-17，四个 A 口全插满时用
`tools/lsp-usb-map.sh` 一次读出，并由用户现场核对物理位置）：

| 物理位置 | CH334F 下游 | 原理图网名 | 实测（当时那次启动） |
| --- | --- | --- | --- |
| 白色·**上层** | 端口 3 | `DP3/DM3` = **J17A** | 戴尔 KB216 键盘 `5-1.3`（1.5M 低速） |
| 白色·**下层** | 端口 4 | `DP4/DM4` = **J17B** | 罗技 G102 鼠标 `5-1.4`（12M 全速） |

⇒ 头两次的结论（先白下后白上的插拔顺序 → 端口 4 → 3）**成立**，不再是推测。
同一次全插满的测试还顺带钉死了蓝色双层（用户确认）：
**上层 = UMaster（USB3 盘 `1f75:0903`，5000M，落 `USB30HOST2` = J16B-SS）**、
**下层 = Pico 屏（`2e8a:0001`，12M，落 `USB3OTG1` = J16A）** —— 与本文档/本文档完全一致。

## 为什么同一个口上，USB2 设备和 USB3 设备的 `usbN` 不一样（2026-09-17 追问）

**一个 xHCI 控制器 = 两个 root hub = 两条总线**：USB3 规范要求 xHCI 同时提供 USB2 与 USB3
两套根端口，Linux 给 USB2 那半**单独注册一个 HCD**（`xhci->shared_hcd`），于是同一个 platform
设备下挂着两个 root hub。实测（开发板，同一时刻）：

```
$ ls -l /sys/devices/platform/usbdrd3_1/fc400000.usb/xhci-hcd.4.auto/
drwxr-xr-x 6 root root 0 Sep 17 10:28 usb6          ← speed=480（USB2 那半）
drwxr-xr-x 5 root root 0 Sep 17 10:28 usb8          ← speed=5000（USB3 那半）

usb6  speed=480   平台路径=platform/usbdrd3_1/fc400000.usb/xhci-hcd.4.auto/usb6
usb8  speed=5000  平台路径=platform/usbdrd3_1/fc400000.usb/xhci-hcd.4.auto/usb8
```

⇒ 所以：**USB3 设备（5000M）落 `usb8`、USB2 设备（12M/480M）落 `usb6`，但两个 `ID_PATH`
完全相同**（都是 `platform-xhci-hcd.4.auto-usb-0:1`）—— 走的是同一个物理口的同一组引脚，
只是 USB3 用 `TYPEC1_SSRX/SSTX` 那对差分线、USB2 用 `TYPEC1_OTG_DM/DP`，由 xHCI 按协商速率
决定挂哪个 root hub（同一颗设备换根只支持 HS 的线，也会从 5000M 那条掉到 480M 那条）。

> ⚠️ 别把 `8-1` / `6-1` 读成"两个口"：**横杠前是总线号、横杠后才是端口号** —— 两条总线的
> 端口号都是 `1`（各自的 root hub 的 1 号口），就是同一个物理口。
> udev 的 `ID_PATH` 里没有总线号正是这个原因：**一个口一条 ID_PATH，与速率无关**，
> 所以写规则要认它（或 platform 路径），不要认 `usbN`。

**源码依据**：`kernel-6.1/drivers/usb/host/xhci-plat.c:352-369`（上游文件；旧副本里是 351-367，差 1 行）

```c
	if (!xhci_has_one_roothub(xhci)) {
		xhci->shared_hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
						    dev_name(&pdev->dev), hcd);
		...
	}
	...
	if (xhci->shared_hcd) {
		ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
```

`xhci_has_one_roothub()` 为真（只有一种速率的控制器）时才不建第二条总线；RK3588 的 USB3 口
都是双速率 ⇒ 每个 USB3 控制器必然两条总线。

**与 J16B（蓝色·上层）的区别**（这条差别就是"能不能挂 hub"的判据）：

| 口 | USB2 侧 | USB3 侧 | 同一个 xHCI？ | 两条总线的 `ID_PATH` |
| --- | --- | --- | --- | --- |
| **J16A**（蓝下） | `u2phy1_otg` | `usbdp_phy1_u3` | ✅ 是（同一个 `xhci-hcd.N.auto`） | **相同**（`platform-xhci-hcd.N.auto-usb-0:1`）→ 可挂 USB3 hub |
| **J16B**（蓝上） | `usb_host0`（`fc800000`/`fc840000`） | `usbhost3_0`（`combphy2_psu`） | ❌ 否，**两个控制器** | **不同**（`platform-fc840000.usb-usb-0:1` vs `platform-xhci-hcd.N.auto-usb-0:1`）→ 不能挂 hub |

（`tools/lsp-usb-map.sh` 的根 hub 表里同一个控制器出现两行 —— `usb6 480M` 与 `usb8 5000M` ——
不是重复，就是它的 USB2 半与 USB3 半。）

#### 为什么 J16B 会长成"拼接口"：根因在 SoC，不在板厂

RK3588 的第三个 USB3 控制器（`usbhost_dwc3_0: usb@fcd00000`，即 Controller2）在芯片里
**只连了 USB3 PHY** —— 权威树 `kernel-6.1/…/rk3588s.dtsi:3510-3518`：

```dts
		usbhost_dwc3_0: usb@fcd00000 {
			compatible = "snps,dwc3";
			phys = <&combphy2_psu PHY_TYPE_USB3>;
			phy-names = "usb3-phy";          /* ← 没有 usb2-phy 这一项 */
```

对照 `usbdrd_dwc3_0`（`rk3588s.dtsi:3401-3410`）与 `usbdrd_dwc3_1`（`rk3588.dtsi:38-47`）都是
`phys = <&u2phyX_otg>, <&usbdp_phyX_u3>` / `phy-names = "usb2-phy", "usb3-phy"` 双通路。
而 SoC 上的 USB2 PHY **一共只有 4 个**（`u2phy0`/`u2phy2`/`u2phy3` 在 `rk3588s.dtsi:3676/3703/3729`，
`u2phy1` 在 `rk3588.dtsi:83`、只有全功能 RK3588 才有），各自已被 OTG0(Type-C) / OTG1 /
`usb_host0` / `usb_host1` 占满 ⇒ **这颗控制器物理上做不了 USB2**。

USB 3.x 规范里 Standard-A 插座本身就带 USB2 的 D+/D- 触点，要求这个口向下兼容 2.0 设备
（否则插 U 盘/键盘**完全不枚举**，用户会以为口坏了）⇒ 板厂把 `u2phy2`（USB2 HOST0）那对
D+/D- **借**到同一个连接器上，代价就是本节表格里那三条后果（口身份随速率变、udev 绑不住、
**不能挂 hub**）。

全树情况（权威树 `kernel-6.1/arch/arm64/boot/dts/rockchip/`，2026-09-17 统计）：
**31 个 dts** 提到 `usbhost3_0`，其中 **29 个**把 `&usbhost3_0` 设成 `okay`
（Rock 5B、Rock 5A、Orange Pi 5 Ultra/Max、Khadas Edge2、LubanCat-4 …），
**没有任何一个能给它加 `usb2-phy`**（grep `u2phy` 命中 0 —— 硬件上就没这根线）；
至于各自连接器有没有"借"一对 D+/D-，那是原理图的事，DTS 看不出来（⛔ 别据此断言"只有野火这么做"）。
厂商主 EVB（`rk3588-evb1-lp4.dtsi:817-819`）则明确 `status = "disabled"`。

---

## 工程机现场接线实测（2026-09-17，用 `tools/lsp-usb-map.sh` 一条命令读出）

工程机 `<HOST_09>`（kernel **6.1.118**）现场接法 ↔ 工具判定，**逐条对上**：

| 现场接法（用户口述） | sysfs 设备 | VID:PID | 工具判定 | `ID_PATH`（本次） | 规则产物 |
| --- | --- | --- | --- | --- | --- |
| 左手相机 → 蓝色·**上层** | `3-1` | `0abd:8050` | **J16B-HS**（`fc800000.usb` / USB2 HOST0 EHCI，480M） | `platform-fc800000.usb-usb-0:1` | `/dev/camera_left` ✓ |
| 右手相机 → 蓝色·**下层** | `4-1` | `0abd:8050` | **J16A**（`usbdrd3_1` / OTG1，480M 侧） | `platform-xhci-hcd.11.auto-usb-0:1` | `/dev/camera_right` ✓ |
| 胸部相机 → 白色·**上层** | `6-1.3` | `05a3:9230` | **Hub.P3(J17A)** | `platform-fc880000.usb-usb-0:1.3` | `/dev/chest_cam_{video,metadata}` ✓ |
| 绿联 4 口 hub → 白色·**下层** | `6-1.4` | `05e3:0610` | **Hub.P4(J17B)**（`hub/4p`） | `…usb-0:1.4` | — |
| └ hub 口1：`lcd_voice_bridge` | `6-1.4.1` | `303a:1001` | **ExtHub.P1@6-1.4**（12M） | `…usb-0:1.4.1` | `/dev/lcd_voice_bridge` → `ttyACM0` ✓（MAC `02:00:00:00:00:03`） |
| └ hub 口2：USB 蓝牙从机 | `6-1.4.2` | `0a12:0001` | **ExtHub.P2@6-1.4**（12M） | `…usb-0:1.4.2` | 无规则；`hci1` |
| └ hub 口3：右手套 `glove_right` | `6-1.4.3` | `303a:1001` | **ExtHub.P3@6-1.4**（12M） | `…usb-0:1.4.3` | `/dev/glove_right` → `ttyACM2` ✓（MAC `02:00:00:00:00:02`） |
| └ hub 口4：左手套 `glove_left` | `6-1.4.4` | `303a:1001` | **ExtHub.P4@6-1.4**（12M） | `…usb-0:1.4.4` | `/dev/glove_left` → `ttyACM1` ✓（MAC `02:00:00:00:00:01`） |

> 第一次跑时 hub 口 3/4 **空着**（两只手套没上电）；上电后补齐，10 个 USB 设备全在，
> 用户手写的 **5 条符号链接全部生成**（`camera_left`/`camera_right`/`chest_cam_*`/`glove_left`/
> `glove_right`/`lcd_voice_bridge`）。MAC ↔ hub 口的对应与本文档（2026-09-12）**逐条一致**
> （口 1 = lcd_voice_bridge、口 3 = glove_right、口 4 = glove_left；`ttyACM0/1/2` 同序）⇒ 现场线没动过。

**两条推论**：

1. ⭐ **白口上/下 ↔ J17A/J17B 在工程机上被独立复验**：胸相机（白上）= CH334F 端口 3 = J17A，
   绿联 hub（白下）= 端口 4 = J17B —— 与本文档用键盘/鼠标得到的结论一致。
2. **工具第一版在这里有个真 bug**（已修）：它原先对**任何**上级 hub 都套板载 CH334F 的端口名表，
   于是绿联 hub 后面的设备会被误标成 `Hub.P1(Mini-PCIe-USB)`。现在按"是否板载"分流成
   `ExtHub.P<n>@<hub 设备名>`；判据是 `hub_is_onboard()`：**直接挂在根 hub 上（名字无 `.`）
   且 platform 路径含 `fc880000.usb`**。外接 WCH hub 的 VID:PID 与板载**相同**（都是 `1a86:8091`），
   所以只看 VID:PID 不行。


## 工程机上有**两路蓝牙**：`hci0` = 板载，`hci1` = 绿联 hub 上的 CSR 从机

| 适配器 | 来源 | 地址 / 名字 | sysfs |
| --- | --- | --- | --- |
| `hci0`（**default**） | 板载 RTL8822CE 的 USB2 功能（CH334F 端口 1 = Mini-PCIe 槽） | `02:00:00:00:00:06` `example-project` | `…/usb6/6-1/6-1.1/6-1.1:1.0` |
| `hci1` | 绿联 hub 口 2 的 **CSR8510 A10** 从机（12M） | `02:00:00:00:00:05` `LDP-DC-000103` | `…/usb6/6-1/6-1.4/6-1.4.2/6-1.4.2:1.0` |

⚠️ `bluetoothctl list` 的 **default 是 `hci0`（板载）** ⇒ 程序若直接用 `bluetoothctl` / `hciconfig`
而不指定设备，用的是**板载**蓝牙，**不是**现场那颗 USB 蓝牙从机。要用从机得显式指定
`hci1`（或按地址 `02:00:00:00:00:05`）。

---
