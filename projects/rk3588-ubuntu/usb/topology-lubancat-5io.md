# lubancat-5io：USB 分配（DTS 层 + 连接器级）

> 关键结论：**5io 上不存在"拼接口"** —— 它不用 Controller2（`usbhost3_0` 在 dts 里出现 0 次、保持
> `disabled`），四个 USB 口全落在单控制器里；供电切分比 v2 细（**每路一条 5V 轨** + `*_en` pinctrl）。

## TL;DR

- 权威源码是 `kernel-6.1/…/rk3588-lubancat-5iob.dts`（1752 行，5io 真正的内容；
  `rk3588-lubancat-5io.dts` 只是 9 行 wrapper）。⛔ 别读 `build/linux-rockchip/`，那是旧副本。
- ⛔ DTS 只能到"哪个控制器配了哪颗 PHY、由哪条 5V 轨供电"这一层；**"控制器接板子上哪个 A 口"
  在原理图里**（v2 那篇的 J16A/J16B 结论就是读网名 + 板载实测得来的）。
- 供电每路独立 + 带 `*_en` pinctrl ⇒ 比 v2 细（v2 三个口共用一条 `vcc5v0_usb`）。
- ⚠️ 有几个口的 `EN` 直接接 `VCC5V0_SYS` ⇒ **VBUS 常开**；派生底板想让 hub 可软复位/上报过流，
  必须把这两个脚改接 GPIO。
- 认口一律以**原理图网名**为准，不要按 `A/B` 推上下（v2 就反着）。

> 依据：权威源码树 `kernel-6.1/…/rk3588-lubancat-5iob.dts`（1752 行，5io 的真正内容；
> `rk3588-lubancat-5io.dts` 只是 9 行 wrapper 包它）。**别读 `build/linux-rockchip/`，那是旧副本。**
>
> ⚠️ **本节只到"哪个控制器配了哪颗 PHY、由哪条 5V 轨供电"这一层** —— 这是 DTS 能给的极限。
> **"这颗控制器接到板上的哪个 A 口、板载 hub 扇出几个口"在原理图里，DTS 看不出来**
> （DTS 只写 `status = "okay"`，不写走线）。5io 的连接器命名需要看 5io 原理图才能补。
> 对照：[topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) 那些 J16A/J16B/J17A 的结论是**用户读 v2 原理图的网名** + 板载实测得来的。

| 控制器 | PHY | `dr_mode` | 5V 供电轨（GPIO） | 备注 |
| --- | --- | --- | --- | --- |
| `usbdrd3_0`（OTG0） | `u2phy0_otg` + `usbdp_phy0_u3` | `otg` | `vbus_typec` | Type-C（FUSB302、DP alt 由 `usbdp_phy0` 配） |
| `usbdrd3_1`（OTG1） | `u2phy1_otg` + `usbdp_phy1_u3` | `host` | `vcc5v0_usb30_otg`（`gpio4 PA4`） | **标准 USB3 口**：SS+HS 同一个 xHCI ⇒ 可挂 hub |
| `usb_host0_ehci/ohci`（HOST0） | `u2phy2_host` | host | `vcc5v0_usb20_host`（`gpio2 PC3`） | 一路 USB2 |
| `usb_host1_ehci/ohci`（HOST1） | `u2phy3_host` | host | `vcc5v0_usb20_hub`（`gpio4 PA5`） | **经板载 USB2 hub**（名字直接写着 hub） |
| **`usbhost3_0`（Controller2）** | ~~`combphy2_psu`~~ | — | — | ⛔ **`5iob.dts` 里出现 0 次 → 保持 dtsi 默认 `disabled`**；`combphy2_psu` 给了 `pcie2x1l1`（Mini-PCIe 的 PCIe） |

**关键结论**：**5io 上不存在"拼接口"** —— 它不用 Controller2，四个 USB 口全部落在单控制器里
（OTG0 双通路 / OTG1 双通路 / HOST0 纯 USB2 / HOST1 纯 USB2），所以**每个口只有一条 `ID_PATH`、
与设备速率无关、都可以挂 hub**。代价是比 v2 少一个 USB3 口（v2 的蓝色·上层就是 Controller2 那个）。

**与 v2 的横向对比（都是 DTS 可查的）**：

| | v2（`5.dts`，1636 行） | 5io（`5iob.dts`，1752 行） |
| --- | --- | --- |
| Controller2（`usbhost3_0`） | **使能** → 蓝色·上层（SS 走 Controller2、HS 借 `u2phy2` = 拼接口） | **不使能** |
| `combphy2_psu` 给谁 | `usbhost3_0`（USB3） | `pcie2x1l1`（Mini-PCIe 的 PCIe） |
| `combphy0_ps` 给谁 | `pcie2x1l2`（Mini-PCIe） | `sata0` |
| `u2phy2_host`（HOST0）供电 | `vcc5v0_usb`（与其它 USB 共用） | `vcc5v0_usb20_host`（单独一路，`gpio2 PC3`） |
| `u2phy3_host`（HOST1）供电 | `vcc5v0_usb`（共用） | `vcc5v0_usb20_hub`（单独一路，`gpio4 PA5`，名字点明接的是 hub） |
| `u2phy1_otg` 供电 | 无单独轨 | `vcc5v0_usb30_otg`（`gpio4 PA4`） |

⇒ **5io 的 USB 供电切分比 v2 细得多**（v2 三个口共用一条 `vcc5v0_usb`；5io 每路一条，且配了 `*_en` pinctrl）。
**但 5io 与 v2 都用 `u2phy3_host`（USB2 HOST1）接板载 USB2 hub** —— v2 那个 CH334F 的 upstream
就是 `u2phy3`（[topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) 实测），5io 的供电轨名字同样是 `usb20_hub`，两边一致。

---

## 原理图已入库 + `lubancat-5io` 的**连接器级** USB 分配（2026-09-17）

原理图 PDF 放在 `hardware-docs/`（镜像源码：`hardware-docs/README.md`）（索引在那边，含页码速查）。
三份都是 Foxit 打印版、**带真实文字层** ⇒ `pdftotext -layout` 直接抽网名，不用 OCR。
提取时**先把 `\f` 换成哨兵行再切页**（PDF 里 `\f` 常与内容同行，直接 `awk '/\f/{p++}'` 会 off-by-one）：
`pdftotext -layout x.pdf - | sed 's/\f/\n@@PAGE@@\n/g' > x.pages.txt`

## v2 原理图交叉核对：[topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) / [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) 的结论**逐条对上**

**依据**：`hardware-docs/lubancat-5-v2/lubancat_5_v2_EBF410149V2R2_SCH_20250908.pdf`
第 **32** 页（图纸名 `USB30/USB20/SATA30_PORT` V2R1；网名对应的 SoC 侧在第 9/15/19 页）：

| 网名 ↔ 连接器（图上原文） | 连接器符号名 | 与 [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) 记录的对照 |
| --- | --- | --- |
| `J16A`：`TYPEC1_OTG_DM/DP` + `TYPEC1_SSRX1N/P` + `TYPEC1_SSTX1N/P`（→ `[15]` SoC USB PHY 页） | `USB3D0-A-D` | ✅ 蓝下 = J16A = **OTG1，SS+HS 同一控制器** |
| `J16B`：`USB30_SSRXN/P` + `USB30_SSTX/SSTXN/P`（→ `[19]`）与 `HOST0_DM/DP` → `USB20_HOST0_DM/DP`（→ `[15]`） | `USB3D0-A-D` | ✅ 蓝上 = J16B = **拼接口**（SS 走 Controller2、HS 借 USB2 HOST0） |
| `J17A`：`D-`→`DM3`、`D+`→`DP3` | `USB-A-D` | ✅ 白上 = J17A = CH334F **端口 3** |
| `J17B`：`D-`→`DM4`、`D+`→`DP4` | `USB-A-D` | ✅ 白下 = J17B = CH334F **端口 4** |
| `U31 = CH334F`：上游 `DMU/DPU` ← `USB20_HOST1_DM/DP`（→ `[15]`）；下游 `DP1/DM1`→`HUB_DP1/DM1`（→ **p37 MINI PCIE**）、`DP2/DM2`→`HUB_DP2/DM2`（→ **p38 E-KEY**）、`DP3/DM3`/`DP4/DM4` → J17A/J17B | `CH334F` | ✅ 与 [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) 完全一致（下游 1=Mini-PCIe、2=E-KEY、3=J17A、4=J17B） |

**图上还看到三条 [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) 没记的**：

1. **蓝色双层是两个 USB3-A（符号 `USB3D0-A-D`）、白色双层是两个 USB2-A（符号 `USB-A-D`）** ——
   即"蓝上蓝下"在一个双联连接器里，"白上白下"在另一个里（与用户说的双层结构一致）。
2. **4 个 A 口各有一个"2A 限流"的 VBUS 开关**（`U27`/`U28`/`U29`/`U30`，网名 `USB_VBUS0..3`），
   但 ⛔ **`EN` 直接接 `VCC5V0_SYS`**（2026-09-17 用户读图确认）⇒ **这 4 个口的 VBUS 常开、
   没有软件断口能力**；其输入 `vcc5v0_usb` 在 DTS 里也是 `regulator-fixed` + `regulator-always-on`
   （`rk3588-lubancat-5.dts` 里**没有 `gpio =`**）。
   ⇒ **v2 全板唯一软件可控的 USB 供电是 Type-C 的 `vbus_typec`**（`gpio2 PB5`，挂在 FUSB302 上，
   由 PD 角色控制）。要"给某个 A 口断电复位外设"在 v2 上做不到，得动硬件。
   对照：**5io 是按组可控**（`usb30_otg` / `usb20_host` / `usb20_hub` 三条独立 GPIO 轨，见本文档）。
3. **hub 常使能、不可软复位、过流不进 SoC**（2026-09-17 用户逐脚读图确认）：
   `CH334F` 的 **`RESET#/CDP` 串一只 100K 电阻上拉到 `HUB_3V3`**、
   **`OVCUR#` 直接连 `HUB_3V3`** ⇒ 复位脚被固定为"不复位"、过流脚被固定为"无过流"电平，
   **SoC 侧既没法复位 hub、也永远读不到过流**（5io 的 `FE1.1s` 同样：`XRSTJ` 100K 上拉到
   `USB_HUB_3V3`、`OVCJ` 直连 `USB_HUB_3V3`）。
   ⚠️ **派生底板要点**：想让底板上的 hub 可软复位 / 能报过流，必须把这两个脚改接 GPIO
   （v2/5io 的原设计都做不到；hub 卡死只能整机断电）。

## 5io 的连接器级 USB 分配（回答"5io 的 USB 口现在怎么分配"）

**依据**：`hardware-docs/lubancat-5io/LubanCat5IOB_EBF410153V1R0_SCH_20240820.pdf`
第 **8** 页（图纸名 `USB3.0/USB2.0/TYPEC接口/IR` V1.0）+ 第 14/15 页（Mini-PCIe / E-KEY）
+ 核心板 `LubanCat5CB_...pdf` 第 22 页（`BTB连接器`，两张板对接处，两边网名相同）。

| 板上连接器（物理位置） | 走哪个控制器 / PHY | 原理图依据 | 供电 |
| --- | --- | --- | --- |
| **J8 = Type-C** | `TYPEC0` = **OTG0**（`u2phy0_otg` + `usbdp_phy0_u3`） | `TYPEC0_SSRX1/SSTX1`、`TYPEC0_USB20_OTG_DP/DM`、`TYPEC0_USB20_VBUSDET` → FUSB302 | `VBUS5V0_TYPEC` + `TYPEC_VBUS_EN` |
| **J9A = USB3.0 A**（`J9` 双层·**下层**） | `TYPEC1` = **OTG1**（`u2phy1_otg` + `usbdp_phy1_u3`）—— `USB30_DM/DP`→`TYPEC1_USB20_OTG_*`、`USB30_RX/TX`→`TYPEC1_SSRX1/SSTX1` | 符号名 `USB3D0_USB2D0-A-D` | `VCC5V0_USB30_OTG` + `USB30_OTG_PWR_EN`（DTS：`gpio4 PA4`） |
| **J9B = USB2.0 A**（`J9` 双层·**上层**） | **USB2 HOST0**（`u2phy2_host`）：`USB20_DM1/DP1` → `USB20_HOST0_DM/DP` | 同上 | `VCC5V0_USB20_HOST` + `USB20_HOST_PWR_EN`（DTS：`gpio2 PC3`） |
| **J10A**（`J10` 双层·`A`） | 板载 USB2 hub **`FE1.1s`（Terminus 4 口）下游口 1**：`USB_HUB_DP1/DM1` | hub 上游 `DMU/DPU` ← `USB20_HOST1_DM/DP` | `VCC5V0_USB20_HUB`（DTS：`gpio4 PA5`） |
| **J10B**（`J10` 双层·`B`） | 同一 hub **下游口 2**：`USB_HUB_DP2/DM2` | 同上 | 同上 |
| **Mini-PCIe 槽** 的 `USB-D+/D-` | 同一 hub **下游口 3**：`USB_HUB_DP3/DM3` → **p14（MINIPCIE）** | `[9] USB_HUB_DP3` | 同上 |
| **M.2 E-KEY 槽** 的 `USB-D+/D-` | 同一 hub **下游口 4**：`USB_HUB_DP4/DM4` → **p15（EKEY）** | `[9] USB_HUB_DP4` | 同上 |

**物理位置**（2026-09-17 用户读图确认）：**`J9` 与 `J10` 都是双层 USB-A**，
**`J9A` = 下层（USB3.0）、`J9B` = 上层（USB2.0）**；`J10A`/`J10B` 是另一个双层
（`A` 对应 hub 口 1、`B` 对应 hub 口 2；两者上/下未单独确认，若与 `J9` 同惯例则 `A`=下、`B`=上）。

> ⛔ **`A`=下层**不是全板通则（v2 就反着）：v2 的 **J16A=蓝下、J16B=蓝上**（实测），
> 但 **J17A=白上、J17B=白下**（键盘/鼠标现场核对，见 [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md)）⇒ **认口一律以网名/实测为准，
> 不要按 `A/B` 推上下**。

⇒ **5io 一共给了 6 个 USB 出口**：Type-C×1（OTG0）+ USB3-A×1（OTG1）+ USB2-A×1（HOST0）
+ USB2-A×2（hub 口 1/2）+ 两个 M.2/Mini 槽的 USB2 各一（hub 口 3/4）。

**架构上与 v2 的异同**（v2 见 [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md)）：

| | v2 | 5io |
| --- | --- | --- |
| 板载 USB2 hub | **CH334F**（WCH），上游 `USB20_HOST1` | **FE1.1s**（Terminus），上游同样是 `USB20_HOST1`；下游 4 口去向也一致（2 个 A 口 + Mini-PCIe + E-KEY） |
| 第二个 USB3 口 | 用 **Controller2**（`usbhost3_0`）做 SS、借 `u2phy2` 做 HS ⇒ **拼接口** | **不用 Controller2**（`usbhost3_0` disabled，combphy2 给了 Mini-PCIe 的 PCIe）；第二个口是 **J9B 纯 USB2（HOST0）** |
| USB3 口数量 | 2 个（J16A=OTG1、J16B=Controller2+借用） | **1 个**（J9A=OTG1），另加一个纯 USB2 |
| 每口供电 | 4× 2A 限流开关挂在共用 `vcc5v0_usb` 后面 | **J9A/J9B/hub 各一条独立 GPIO 轨** + 各自的 `*_PWR_EN` |

⇒ 结论：**5io 上没有"拼接口"**（本文档从 DTS 得出的判断，原理图逐条证实）；代价是比 v2 少一个 USB3 口。

---
