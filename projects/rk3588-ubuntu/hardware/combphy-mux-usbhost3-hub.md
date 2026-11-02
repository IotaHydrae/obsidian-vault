# 在 `usbhost3_0`（`combphy2_psu`）上挂 4 口 USB hub —— 不成立

> 这是自研底板早期的一个想法（把 `usbhost3_0` 那路 USB3 分给 4 个 Type-A 口），评估结论是**不能这么做**。
> 结论与最终采用的"分家"方案见 [../usb/carrier-board-vl817.md](../usb/carrier-board-vl817.md)；
> PHY 复用的原理与接线见 [combphy-mux.md](combphy-mux.md)。
> ⚠️ 下面正文**原样搬自 `combphy-mux.md` §3.4**（只改了标题层级）。

## TL;DR

- ⛔ **`usbhost3_0` 只有 SS（USB3）通路，没有 USB2 通路** ⇒ 挂 4 口 hub 时下游那 4 个口的 USB2 半
  无处可接；而现场外设几乎全是 USB2（2026-09-17 工程机普查）⇒ 这条路等于白给 4 个跑不起来的口。
- **最终做法**：把 USB2 与 SS 两条路**"分家"** —— HS 半走 `usb_host0`，SS 半走 `usbhost3_0`，
  由板载 VL817 汇合（用户 2026-09-17 拍板，见 [../usb/carrier-board-vl817.md](../usb/carrier-board-vl817.md)）。


## 1. 在 `usbhost3_0`（`combphy2_psu`）上挂 4 口 USB hub 分 4 个 Type-A

> 📌 **状态（2026-09-17）：架构已决策 —— 见本节表格后的决策框与 `notes/usb/carrier-board-vl817.md`。**
> 用户拍板采用 **VL817 分家接法**（HS←`USB20_HOST0`、SS←`combphy2_psu`/`usbhost3_0`），
> 接受"hub 分属两个控制器"。**芯片选型与每口供电/限流方案仍待定。**
> ⚠️ **关键结论（2026-09-17 源码核实）：`usbhost3_0` 是 USB3-only，没有 USB2 通路** ——
> 4 个 Type-A 口**只有 SS 半**挂在这个控制器上，USB2 半靠 `USB20_HOST0`，见下面"①"。

#### ⛔ ① `usbhost3_0` 只有 USB3：`phys` 里没有 usb2-phy

| 控制器 | `phys` | `phy-names` | 含义 |
| --- | --- | --- | --- |
| **`usbhost_dwc3_0`（USB3OTG2 / `usbhost3_0`）** | `<&combphy2_psu PHY_TYPE_USB3>` | **只有 `"usb3-phy"`** | **USB3-only** |
| `usbdrd_dwc3_1`（USB3OTG1，对照） | `<&u2phy1_otg>, <&usbdp_phy1_u3>` | `"usb2-phy"`, `"usb3-phy"` | **USB2 + USB3 双通路** |

- 全树（权威树 `kernel-6.1/`，2026-09-17 统计）**31 个 dts 提到 `usbhost3_0`、其中 29 个设成 `okay`**，
  节点里**都没有任何 u2phy 引用**；厂商主 EVB（`rk3588-evb1-lp4.dtsi:817`）明确 `status = "disabled"`。
- xHCI 一定会建一条 USB2 总线，所以我们看到 `usb1`(480M) 存在 —— 但**那是个空壳**（没有 USB2 PHY
  接上去），现场表现是：**USB2 设备插上毫无反应**（不枚举、dmesg 里连"新设备"都没有）。
- 反证：`usbdrd3_1` 的 `usb7`(480M) 上实测挂着一个 12 Mbps 设备 ⇒ OTG1 的 USB2 通路确实能用。
- ⇒ 对机器人外设影响很大：**ESP32/串口板、键鼠、很多 U 盘/相机都是 USB2**，只把 hub 挂在这一路
  （USB2 半不接）等于把它们全废掉 —— 这正是分家接法要接 `USB20_HOST0` 的原因。

**评审时列出的三条可选架构**（推荐度从高到低；**均未被采纳**，见下面决策框）：

| 方案 | 做法 | 4 个 Type-A 的能力 | 代价 |
| --- | --- | --- | --- |
| **1（推荐）** | 4 口 USB3 hub 挂 **`usbdrd3_1`（USB3OTG1）** —— 唯一双通路 host 口 | **USB3 + USB2 都能用** | 占用原来那个 USB3.1 Host1 口；⚠️ 要确认 `usbdp_phy1` 的 DP altmode 是否还要用（5io 上配了 `rockchip,dp-lane-mux = <2 3>`） |
| 2 | hub 仍挂 `usbhost3_0` | **只有 USB3**（USB2 设备全废） | 适合"4 口全是 USB3 相机/硬盘"；USB2 设备另找口 |
| 3 | 4 口 **USB2** hub 挂 `u2phy2`/`u2phy3` 那两个 USB2 host 口（照抄 v2 板上已有的 4 口 USB2 hub） | 只有 USB2（40 MB/s 共享） | USB3 留给 `usbhost3_0` 单口；适合"多数外设是 USB2，只有相机要 USB3" |

> 📌 **已决策（2026-09-17，用户拍板）：以上三条都不是最终选择** —— 自研底板采用 **VL817 的"分家"接法**：
> `HSD0±` ← `USB20_HOST0`（u2phy2）、SS 上游 ← `USB30_SSRX/SSTX`（`combphy2_psu`/`usbhost3_0`），
> 也就是**故意让 hub 的两半分属两个控制器**（= 把 v2 蓝色上层那个"拼接口"放大成 4 个口），
> 代价（两棵 USB 树、每口两条 `ID_PATH`）用户明确接受。J16A（OTG1）另作它用、类型未定。
> ⇒ 本节的"⛔ 不能这么接"是**评审意见**（结论已被决策覆盖），完整记录见
> [../usb/carrier-board-vl817.md](../usb/carrier-board-vl817.md)；**连带约束：`combphy2_psu` 已归
> `usbhost3_0`，而用户 2026-09-17 确认"底板 combphy 口照 v2 做" ⇒ `combphy0`→`pcie2x1l2`(Mini-PCIe)、
> `combphy1`→`pcie2x1l0`(E-Key)，**三路全定死、v2 那套 DTS 可整段照抄**；因此 [combphy-mux.md](combphy-mux.md) §2 那条
> "v2 上别开 `sata0`" 同样适用于自研底板。**

**✅ 已实测（2026-09-17）**：把一颗 USB2 设备（`2e8a:0001` Pico USB Display，全速 12 Mbps）
**依次插进 v2 板上 4 个 USB-A 口**，4 次全部枚举成功，映射如下（详见
[../usb/topology-lubancat-5-v2.md](../usb/topology-lubancat-5-v2.md)）：

| 物理口 | 归属控制器 |
| --- | --- |
| 蓝色·下层 | `usbdrd3_1`（USB3OTG1）的 USB2 侧 |
| 蓝色·上层 | `usb_host0`（USB2 HOST0 / u2phy2） |
| 白色·下层 / 上层 | `usb_host1`（USB2 HOST1 / u2phy3）→ **板载 4 口 hub 的端口 4 / 3** |

⇒ **没有任何一个 A 口的 USB2 走 `usbhost3_0`（OTG2）** —— 若某个口走它，插 USB2 设备应当毫无反应。
这与"**OTG2 是 USB3-only**"一致。

**✅ 再补一颗 USB3 设备（`05e3:0749` Genesys SD 读卡器）测 SS 归属**（同样两个蓝口）：

| 物理口 | **SS（USB3）** | **HS（USB2）** |
| --- | --- | --- |
| 蓝色·下层 | **`usbdrd3_1`（OTG1）**，bus 8 `5000M` | OTG1（bus 6 / 480M） |
| 蓝色·上层 | **`usbhost3_0`（OTG2）**，bus 2 `5000M` | **`usb_host0`（USB2 HOST0）**，bus 4 / OHCI |

⭐ ⇒ **v2 的"蓝色·上层"是 SS/HS 分属两个控制器的拼接口**：SS 来自 `combphy2_psu`→`usbhost3_0`，
HS 来自 `u2phy2`→`usb_host0`。所以 **OTG2 确实引到了连接器上**（就是它），只是没有自己的 USB2。
（软件侧注意：同一个口按设备速率落到不同控制器 ⇒ 该口 `ID_PATH` 不稳定，别在上面绑 udev 符号链接。
详见 [../usb/topology-lubancat-5-v2.md](../usb/topology-lubancat-5-v2.md) 的"现场接线"节。）

⚠️ **对 hub 方案的关键约束**（评审意见）：**USB3 hub 的上游 SS 与 HS 正常应属于同一个 xHCI 实例**，
否则它会在两个控制器上分别枚举（SS 挂 OTG2、USB2 挂 HOST0），下游设备散在两棵 USB 树里。
⇒ "一个 hub 同时提供 USB3+USB2"且口身份稳定的做法是上游用 **`usbdrd3_1`（OTG1）**（方案 1）。
📌 **自研底板没有采用这条**：用户 2026-09-17 决定接受分家（见上面那张表后的决策框 + `notes/usb/carrier-board-vl817.md`），
所以本节这段只作为"代价已知"的记录，**不要再拿它去纠正接法**。

| 控制器 | 对应的 USB 总线 | 现状 |
| --- | --- | --- |
| **`usbhost3_0`（USB3OTG2 / combphy2_psu）** | **`usb1`（480M）+ `usb2`（5000M）**，各 1 口 | **两条总线都在、都没插东西** ← 你要挂 hub 的就是它 |
| `usb_host0`（u2phy2，EHCI/OHCI） | `usb3`(480M) / `usb5`(12M) | `usb3` 上已挂 4 口 USB2 hub（BT + HID） |
| `usb_host1`（u2phy3） | `usb4`(480M) / `usb6`(12M) | 空 |
| `usbdrd3_1`（USB3OTG1） | `usb7`(480M) / `usb8`(5000M) | `usb7` 上挂了一个 12M 的 vendor-specific 设备 |

要核的 4 件事：

1. ~~**上游两条通路都要接齐**~~ → **已查清：`usbhost3_0` 没有 USB2 通路**（见上面 ①），
   所以挂在这里的 hub 下游只能认 USB3 设备。要"USB3+USB2 都能用"，上游得换成**双通路**的
   `usbdrd3_1`（USB3OTG1），或改用 USB2 hub 挂 USB2 host 口（方案 1/3）。
2. **带宽是上游共享的**：Gen1 5 Gbps ≈ **450 MB/s**（4 口共享）、USB2 HS 480 Mbps ≈ **40 MB/s**（4 口共享）。
   4 个口不能同时满速。📌 对本工程的实际含义：三路相机同时录制合计 ≈ **5.4 MB/s**
   （见 AGENTS §6 的容量实测）⇒ 带宽远远够，真正的约束是**供电**与**枚举/浪涌**。
3. **供电与 VBUS 开关**：每口 USB2 500 mA / USB3 900 mA ⇒ 4 口最坏 **2–3.6 A@5V**，必须自供电 +
   每口 VBUS 开关/限流（或用带电源控制的 hub 芯片）；别指望板子 5V 轨富余（Type-C PD 那路上限才 5V/3A）。
4. **选型与布线**：USB3 hub 芯片（VL822 / RTS5411 / GL3510 / TUSB8041 之类）+ 25 MHz 晶振 +
   自供电/端口供电 strap + 可选 EEPROM；SS/HS 差分 90 Ω、等长、无 stub、每口 ESD，走线长要 redriver。

**软件侧不用改**（`dr_mode = "host"` + 内核自带 hub 支持即可）；我们的镜像已经全局关掉 USB 自动挂起
（AGENTS §6「USB 自动挂起」），对 hub 与外设都友好。只有要做"每口软件控电/过流上报"时才需要
DT 里再加 regulator/GPIO（可参考 5io 的 `vcc5v0_usb20_hub` + `vcc5v0_usb20_hub_en` 那条写法）。

⚠️ 资源账提醒：`usbhost3_0` 与 Mini-PCIe/mSATA 抢 **同一颗 `combphy2_psu`**（见 [combphy-mux-switching.md](combphy-mux-switching.md) §3.1）——
决定在这里做 4 口 hub，就等于放弃这路的 SATA2/PCIe 用途。

---
