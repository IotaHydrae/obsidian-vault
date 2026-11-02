# LubanCat-5 v2 与 5io 的 DTS 差异速查（逐节点 diff）

> 本文回答**"两块板的设备树到底差在哪"**（2026-09-16 逐节点 diff）：
> 只有一边覆盖的节点（功能差异）、同名节点但配置不同（板级差异）、以及派生自研底板时要记住什么。
> 板级资源清单（SoC/电源/存储/网络/USB/显示/音频/CSI）见
> [lubancat-5-v2-hardware.md](lubancat-5-v2-hardware.md)。
> ⚠️ 正文**原样搬自该篇的 §13**（节号保留）。

## TL;DR

- 读内核 DTS **一律用仓库里的 `kernel-6.1/`**（当前 6.1.172），别读 `build/linux-rockchip/` 那份旧副本
  —— 老版本 5io 是一个 1814 行的独立 dts，新版是 9 行 wrapper + `rk3588-lubancat-5iob.dts`（1752 行）。
- 派生自研底板时：**SoM/carrier 的边界要在 dts 里表达清楚**，别把"核心板固有"和"底板差异"混在一层
  （见 [../build/derived-board-bringup.md](../build/derived-board-bringup.md) §3）。


## 13. 与 `lubancat-5io` 的 DTS 差异速查（2026-09-16 逐节点 diff）

**文件结构**（2026-09-17 在权威源码树 `kernel-6.1/`（= 6.1.172）复核）：
v2 = `rk3588-lubancat-5-v2.dts`（**65 行薄 wrapper**）→ include `rk3588-lubancat-5.dts`（1636 行）；
5io = `rk3588-lubancat-5io.dts`（**9 行 wrapper**）→ include `rk3588-lubancat-5iob.dts`
（**1752 行，独立维护**，不是从 `5.dts` 派生的）。同族还有 `5iobi.dts`(17 行) / `5iof.dts`(1702 行) /
`5ioi.dts`(10 行)，都是"薄 wrapper 覆盖 model/compatible"，都不在构建列表里。
两份基板：**99 个节点两边都覆盖**，`diff` 共 34 处 hunk / 919 行差异。

> ⛔ **读 DTS / 内核源码一律用仓库里的 `kernel-6.1/`**（git 跟踪、当前 6.1.172）。
> `build/linux-rockchip/` 只是 `build-kernel.sh` 用 `cp -r ../kernel-6.1` 生成的**副本**，
> 保留的是**上一次在这台机器上编内核时的版本**（实测那份还是 6.1.118）——
> 两个版本的 EBF dts 文件名都不一样（6.1.118 里 5io 是一个 1814 行的独立文件），照它读会得出错结论。

### 13.1 只有一边覆盖的节点（功能差异）

| 只有 v2（`5.dts`） | 只有 5io（`5iob.dts`） |
| --- | --- |
| `&pcie2x1l2` = **Mini-PCIe**（走 `combphy0_ps`） | `&sata0` = **SATA**（走 `combphy0_ps`） |
| `&usbhost3_0` + `&usbhost_dwc3_0` = **USB3.1 HOST2**（走 `combphy2_psu`） | `&pcie2x1l1` = **Mini-PCIe**（走 `combphy2_psu`） |
| `&i2c6`、`&pwm3` | `&pcie3x2` = **PCIe-64P**；`&i2c2`；`&pwm2 / pwm6 / pwm7 / pwm11` |

**两边一致、所以不算差异的**：显示（HDMI0/1、DP0/1、DSI、eDP、HDMI-IN 都由 `overlays=` 决定，
基础 dts 里同名节点的 `status` 两边相同）、CSI（两个 `-csi.dtsi` 几乎相同：2268 vs 2269 行，
都只使能 `csi2_dphy0_hw` / `csi2_dphy1_hw`，cam0-5 都是注释状态）、以及 UART/SPI/CAN/PWM
等大部分接口的 `status`。

### 13.2 同名节点但配置不同（板级差异）

| 位置 | v2 | 5io |
| --- | --- | --- |
| `pcie30phy` 模式 | `PHY_MODE_PCIE_AGGREGATION`（4 lane 全给 `pcie3x4`） | `PHY_MODE_PCIE_NANBNB`（2+2） |
| `pcie3x4`（M.2 M-Key） | 4 lane、reset `gpio1 PB2`、`vcc3v3_sys` | **`num-lanes = <2>`**、reset `gpio4 PB6`、`vcc3v3_m2_pcie` |
| `pcie2x1l0`（M.2 E-Key） | reset `gpio4 PA5`、**无** `vpcie3v3-supply`；wrapper 把 disable 改成 **PCA9535 扩展器** `&gpio6 0` | reset `gpio0 PC5`、有 `vpcie3v3-supply = <&vcc3v3_ekey_pcie>` |
| PMIC 输入 / 系统轨 | `vcc5v0_sys`（**5.0 V**） | **`vcc4v0_sys`（4.0 V）** |
| 可控电源轨 | 少：`vcc5v0_usb`（USB 共用）、`vcc3v3_sys`、`vdd_0v75_s0`、`vbus_typec` | **多**：每个槽位单独 3.3V（`vcc3v3_m2_pcie` / `ekey_pcie` / `mini_pcie` / `pciex4`）、`sata_power`、**`combophy_avdd0v85`**、三条 5V（`vcc5v0_usb30_otg` / `usb20_host` / `usb20_hub`），且大多配了 `*_en` pinctrl 开关 |
| USB2 PHY 供电 | `u2phy2_host` / `u2phy3_host` = `vcc5v0_usb` | = `vcc5v0_usb20_host` / `vcc5v0_usb20_hub`；`u2phy1_otg` 另有 `vcc5v0_usb30_otg` |
| Type-C / fusb302 | 中断 `gpio0 PB2`（wrapper 改的，配 `fusb302_int`）；SBU `gpio1 PA0/PA1`；vbus `vbus_typec` | 中断 `gpio0 PD3`；SBU `gpio1 PB0/PB1`；vbus `vbus5v0_typec`；`usbdp_phy1` 还配了 `rockchip,dp-lane-mux = <2 3>` |
| 以太网（**都是 RTL8211F**） | reset `gpio4 PC6`(gmac0) / `gpio3 PB7`(gmac1)，tx_delay **0x20 / 0x20** | reset `gpio3 PC0`(gmac0) / `gpio3 PC1`(gmac1)，tx_delay **0x26 / 0x30**（PCB 走线不同） |
| I2C5 复用 / GPIO 扩展器 | `i2c5m2`，PCA9535（`gpio6`，地址 0x21）挂 **i2c5**（中断脚注释掉了） | `i2c5m3`，PCA9535 挂 **i2c0** |
| PWM 复用 | `pwm0/pwm1` = `m0`；wrapper 还**关掉** `pwm0/1/2/5` 与 DSI 背光（`backlight_dsi0/dsi1`） | `pwm0/pwm1` = `m2`；`pwm2` 在 5io 侧单独覆盖 |
| 其它零散 | `vop-supply = <&vdd_log_s0>`；`mdio1` 有 `realtek,clkout-disable` | 无 `vop-supply`；多了 `mipi_dsi0/1_power_en`、`sata_power_en` 等 pin group |

### 13.3 派生自研底板时要记住的

1. **5io 的电源树细得多**（几乎所有外设槽位都能单独切电，整机输入是 4.0 V）—— 照抄 v2 的简单
   电源树会出现"缺 rail"；反过来 v2 的简单是有意的。
2. **PCIe/USB 的 PHY 归属不同**（同样叫 Mini-PCIe，v2 在 `combphy0_ps`、5io 在 `combphy2_psu`）——
   详见 [combphy-mux.md](combphy-mux.md) §0。
3. **复位脚、供电脚、PHY 供电、RGMII delay 全是板级选择**：E-Key/M-Key/Mini-PCIe 的 reset、
   网口 reset 与 tx_delay、USB2 PHY 的 `phy-supply`……派生时逐个对原理图，别照抄另一块板。
4. `rk3588-lubancat-5iob.dts` 是**独立文件**，所以"新板卡写一个 include 它的薄 wrapper 再覆盖差异"是改动最小的路线
   （见 [../build/derived-board-bringup.md](../build/derived-board-bringup.md) §3、[combphy-mux-switching.md](combphy-mux-switching.md) §3.1）。
