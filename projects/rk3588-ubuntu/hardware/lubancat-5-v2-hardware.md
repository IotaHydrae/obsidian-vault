# LubanCat-5 V2 硬件资源清单（基于内核 DTS）

来源：`kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588-lubancat-5-v2.dts` →
`rk3588-lubancat-5.dts` → `rk3588-lubancat-5-csi.dtsi` / `rk3588-lubancat-cam.dtsi` /
`rk3588-linux.dtsi` → `rk3588.dtsi` → `rk3588s.dtsi`。

按元件位号查找板上物理位置，可对照 V2R2 位号图（镜像源码：`hardware-docs/lubancat-5-v2/lubancat-5-v2-placement.pdf`）（2025-09-08，2 页：`SILKTOP` / `SILKBOT`）。

> 只列 **DTS 里明确启用（status="okay"）** 的外设。SoC 本身有、但板级没打开的
> （如 HDMI0/1 输出、MIPI DSI、摄像头、SATA、SPI NOR）单独标注「默认关闭」，
> 它们是**硬件上有、需要改 DTS 才能用**的资源。

---

## TL;DR

- 本文是 **v2 的硬件资源清单**，来源是**内核 DTS**（不是原理图）：SoC / 电源 / 存储与启动介质 /
  网络 / USB / 显示 / 音频 / CSI / 板载杂项。
- 显示：出厂默认**三路同时输出**（DP0 + HDMI0 + HDMI1）；**CSI 默认全部关闭**。
- USB 那节的"物理口 ↔ 控制器"详细映射在
  [../usb/topology-lubancat-5-v2.md](../usb/topology-lubancat-5-v2.md)。
- ⚠️ **5io 的清单不在这里**（两块板不同）⇒ 见
  [../usb/topology-lubancat-5io.md](../usb/topology-lubancat-5io.md) 与 5io 原理图。
- 板卡 V1 → V2 的差异速查见 §10。

## 1. SoC：RK3588（8 核）

| 模块 | 规格 |
| --- | --- |
| CPU | 4× Cortex-A76（大核 @400-700）+ 4× Cortex-A55（小核 @0-300），8 核 |
| GPU | Mali-G610（`arm,mali-valhall`），经 `gpu_panthor` 启用 |
| NPU | RKNPU（`rockchip,rk3588-rknpu`），3 核，供电 `vdd_npu_s0`（RK8602） |
| 视频编解码 | **解码**：rkvdec0/rkvdec1（H.265/VP9/AVS2 等）、av1d（AV1）、jpegd、vdpu |
| | **编码**：rkvenc0/rkvenc1（H.264/H.265）、vepu、jpege0-3 |
| 图像 | rga2 + rga3_core0/rga3_core1（2D 加速）、iep、mpp_srv |
| 其他 | crypto（硬件加解密）、rng（随机数）、hwlock、mailbox0-2、dmac0-2、GIC600 |
| 内存/电源管理 | DMC/DFI（DDR 调频）、rockchip_suspend（深度睡眠 ARMOFF+DDRPD，GPIO/CPU0 唤醒）|
| 温度 | tsadc + thermal_zones + rockchip_system_monitor，联动 pwm-fan 散热 |

---

## 2. 电源

- 输入 **12V DC**（`vcc12v_dcin`）→ `vcc5v0_sys` / `vcc3v3_sys` / `vcc_5v0` / `vcc5v0_usb`
- **PMIC：RK806**（挂 SPI2，`rk806single@0`），内部完整电源树：vdd_gpu / vdd_cpu_lit /
  vdd_log / vdd_vdenc / vdd_ddr / vdd2_ddr / vcc_3v3_s3 / vcc_1v8 / 多路 LDO/NLDO 等
- CPU/NPU 独立调压：i2c0 上的 **RK8602/8603**（vdd_cpu_big0/big1）、i2c1 上的 **RK8602**（vdd_npu）

---

## 3. 存储 / 启动介质

| 介质 | 接口 | 规格 |
| --- | --- | --- |
| eMMC | `sdhci`（mmc0） | 8-bit，HS400 + enhanced strobe，1.8V，≤200MHz，`non-removable` |
| SD 卡 | `sdmmc`（mmc1） | 4-bit，UHS-SDR104，1.8/3.3V（`vccio_sd_s0`），`disable-wp` |
| NVMe SSD | **PCIe3x4**（`pcie30phy` 聚合 x4，M.2 **M-Key**）| 复位 GPIO1_B2，`vpcie3v3` |
| M.2 E-Key | **PCIe2x1l0** | 复位 GPIO4_A5；**V2 的 disable-gpio 改接到 PCA9535 扩展 GPIO6_0** |
| Mini-PCIe | **PCIe2x1l2** | 复位 GPIO3_D1，disable GPIO3_C1 |

> `sdio`（mmc2，WiFi/SDIO）、`sfc`（SPI NOR flash）节点在 SoC dtsi 里有，但板级**未启用**。
> combphy0/1/2（SATA/USB3/PCIe 组合 PHY）已打开，但 SATA 控制器节点默认关闭（要 SATA 得改 DTS）。

⛔ **v2 不只是"sfc 节点没打开"—— 板上根本没有 SPI flash 焊盘**（2026-09-30 按原理图
`hardware-docs/lubancat-5-v2/lubancat_5_v2_EBF410149V2R2_SCH_20250908.pdf` 核对）：
全图没有 SPI flash 器件、也没有 WSON8/uDFN8 焊盘，`FSPI_` 只出现在 SoC 引脚复用表里，
对应的网名是 `EMMC_*`（`FSPI_D0_M0 / EMMC_D0 / GPIO2_D0_u` … → 第 24 页 eMMC 区）
⇒ **那组引脚被板载 eMMC 占用**。

⇒ 因此 v2 的 BootROM 启动介质**只有 eMMC 和 SD**，"不依赖 eMMC/SD（例如只用 SPI flash + NVMe）启动"
在 v2 上做不到。SPI flash 启动为什么必须用 SoC 的 **SFC** 那组固定引脚、5io 核心板那张
`U12` 焊盘（默认 `NC`、与 eMMC 互斥）的情况，见
[rk3588-boot-flow.md](rk3588-boot-flow.md) §3.1.1。

---

## 4. 网络

- **千兆以太网 ×2**：RGMII，PHY 为 RTL8211F（DTS 里同时留了 JLSemi `jl2xxx` 的
  LED/tx-delay 参数），`rgmii_phy0/1` @ `mdio0/mdio1` `phy@0`；复位脚
  **`gmac0` = `gpio4 PC6`、`gmac1` = `gpio3 PB7`**，两块都是 `rgmii-rxid` + `tx_delay 0x20`。
- ⚠️ **"丝印 ETH0/ETH1" 与 "SoC 的 gmac0/gmac1" 是交叉的，而 Linux 的 ethN 又是另一回事**
  （2026-09-20 按原理图网表 + 板上实测核过，原来这里写的 "`gmac0`（eth0）+ `gmac1`（eth1）" 是错的）：

  | 层 | "0" 是谁 | "1" 是谁 | 由谁决定 |
  | --- | --- | --- | --- |
  | 板子丝印 / 原理图 | **ETH0 = GMAC1**（`PHY1`，复位 `GPIO3_B7`） | **ETH1 = GMAC0**（`PHY0`，复位 `GPIO4_C6`） | 厂商 PCB 编号：原理图 p25 页标题 `ETH_1` 上是 16 条 `GMAC0_*` 网，p26 页标题 `ETH_0` 上是 16 条 `GMAC1_*` 网 |
  | SoC 控制器 | `gmac0` @ `fe1b0000` | `gmac1` @ `fe1c0000` | Rockchip IP 编号（`rk3588.dtsi:628` / `rk3588s.dtsi:6810`） |
  | DT alias | `ethernet0 = &gmac0` | `ethernet1 = &gmac1` | Rockchip dtsi 默认（`rk3588.dtsi:16`、`rk3588s.dtsi:36`），**板级 dts 没有覆盖** |
  | U-Boot 的 ethN + MAC 分配 | `eth0` = gmac0（取 `ethaddr`） | `eth1` = gmac1（取 `eth1addr`） | `fdt_fixup_ethernet()`（`u-boot/common/fdt_support.c:636`）**按 `/aliases/ethernetN`** 写 `local-mac-address` |
  | udev 的可预测名 | `end0` = gmac0 | `end1` = gmac1 | udev `net_id` 按 alias（`ID_NET_NAME_ONBOARD`） |
  | **Linux `ip link`** | **`eth0` = gmac1** | **`eth1` = gmac0** | **内核注册顺序（probe 顺序），与 alias 无关** |

  - **内核名字为什么不看 alias**：`stmmac` 确实读了 `of_alias_get_id(np,"ethernet")`，但只存进
    `plat->bus_id`，而 `bus_id` 只用于 MDIO 总线的名字/日志（`stmmac_mdio.c:512`、
    `stmmac_main.c:7571`）⇒ 网卡名是 `alloc_etherdev()` 按注册顺序发的 `eth%d`。
  - **谁先注册**：`rk3588.dtsi` 第一行就 `#include "rk3588s.dtsi"`，而 `gmac1` 定义在
    `rk3588s.dtsi`、`gmac0` 在 `rk3588.dtsi` ⇒ DTB 里先出现 `ethernet@fe1c0000`(gmac1)、
    后出现 `ethernet@fe1b0000`(gmac0)（`dtc -I fs` 实测 3586 行 vs 12523 行）⇒ **gmac1 先 probe**。
  - 于是：`ip link` 的 eth0/eth1 **碰巧与丝印一致**，而 **alias 那一层（U-Boot 的 ethN、
    `ethaddr/eth1addr` 的落点、udev 的 `endN`）才是与丝印交叉的**。
  - 镜像里 `/usr/lib/systemd/network/99-default.link` 是
    `NamePolicy=keep kernel database onboard slot path` —— **头一条是 `keep`**，内核给了
    `eth0/eth1` 之后 udev 就不再改成 `end0/end1`，所以看不到可预测名。
  - ⛔ **别把 eth0/eth1 当成固定硬件**：它只是 probe 顺序的结果（换内核版本、probe 顺序一变就互换）。
    要钉死就按**硬件路径**匹配（`/etc/systemd/network/10-eth0.link`）：
    `[Match] Path=platform-fe1c0000.ethernet`（= gmac1 = 丝印 **ETH0**）与
    `Path=platform-fe1b0000.ethernet`（= gmac0 = 丝印 **ETH1**）。**别用 MAC 匹配**：两个 MAC 只差
    第一个字节的 bit2（`eth1addr` 由 U-Boot 从环境/OTP 派生），换启动链就会变。
  - 想让**所有层**（含 U-Boot 的 ethN 与 MAC 落点）都跟丝印一致，得在板级 dts 里加
    `/ { aliases { ethernet0 = &gmac1; ethernet1 = &gmac0; }; };` —— 但那会把 `ethaddr`/`eth1addr`
    对应的物理口换过来（现场按 MAC 做的 DHCP 绑定会跟着变），要动就一起看。
  - 现状：**仓库/镜像里没有任何配置按 `eth0`/`eth1` 绑网口**（`overlay/ scripts/ tools/ config/`
    全 grep 无命中），所以目前只影响自己写的脚本。
- **无线（WiFi+BT）**：本板实际为 **RTL8822CE**（PCIe，通常装在 **M.2 E-Key** 槽
  `pcie2x1l0`；用户实测确认）。**实证**（先在 `linux-image-6.1.118` 那版 deb 里查的，
  2026-10-03 在板上 `6.1.172` 复核：路径/别名/vermagic 都照旧）：
  - 厂商驱动 **`8822ce.ko`**（`kernel/drivers/net/wireless/rtl8822ce/`），绑定 **PCI
    `10EC:C822`**（即 RTL8822CE），**固件编在模块内**，无需外部 fw 文件；
    `modinfo 8822ce` 在 6.1.172 上给出 `vermagic: 6.1.172 SMP mod_unload modversions aarch64`；
    同 deb 还有 `8852be.ko`（RTL8852BE）。
  - 主线 rtw88（8822b/8822be/8723d）、rtw89（8852a/8852ae/8852c/8852ce）、rtlwifi 等也在。
  - BT：8822C 系共用 `rtl_bt/rtl8822cu_fw.bin`+config（`prebuilt/firmware.tgz`，由 btusb 加载）。
  - 注意 `config/boards/*.conf` 里的 `rtl8852be-reload.service` 是上游按 **RTL8852BE**
    写的通用补丁（`modprobe 8852be`），对本板 8822CE 是**无害空操作**。
- 驱动**不在内核源码树**：`.ko` 由 **linux-image deb 编译产物**带进 rootfs
  （`rockchip_linux_defconfig` 里 `CONFIG_RTL8822CE=m`/`CONFIG_RTL8852BE=m`，见
  README「内核/U-Boot deb」说明）。板级 DTS 未启用 SDIO（`sdio`/mmc2 未开），无线走 PCIe 模块。
- 装机后核对：`lsmod | grep -iE '8822|8852|rtw'`、`dmesg | grep -i rtw`、`ip a`

---

## 5. USB

| 端口 | 控制器 | PHY | 模式 |
| --- | --- | --- | --- |
| USB Type-C | `usbdrd_dwc3_0` | `usbdp_phy0`（USB3.1 + DP combo）| **OTG 双角色** + **DP alt mode** |
| USB3.1 Host1 | `usbdrd_dwc3_1` | `usbdp_phy1` | host |
| USB3.1 Host2 | `usbhost_dwc3_0` | `combphy2_psu` | host |
| USB2.0 Host0/1 | `usb_host0/1_ehci+ohci` | `u2phy2/u2phy3` | host（带 `vcc5v0_usb` 供电）|
| USB2.0（C 口）| `u2phy0_otg` | `u2phy0` | otg + typec-vbus-det |

> 上表是 **DTS 层**的名字。要在**运行时**把"某个设备接在哪个口"对出来（`lsusb` 只能给出
> 总线号，而总线号会变），用 `readlink -f /sys/bus/usb/devices/usbN` 拿到 platform 名，
> 再对照上表的控制器 —— **控制器 ↔ 总线号的实测映射表**在
> [device-mapping-method.md](../usb/device-mapping-method.md)（含完整四步定位法）。

- **PD 控制器：FUSB302**（i2c1 0x22），双角色（sink/source，5V/1A sink · 5V/3A source），
  `vbus_typec` 使能脚 GPIO2_B5。
- **V2 差异**：FUSB302 中断脚由 V1 的 GPIO0_C4 改为 **GPIO0_B2**（`fusb302_int` pinctrl 一并重定义）。

---

## 6. 显示（出厂默认三路同时输出：DP0 + HDMI0 + HDMI1）

> ⚠ 只看基础 DTS 会得出"只有 DP0"的错误结论——**出厂默认还通过 U-Boot 的
> dtb overlay 开启了 HDMI0 和 HDMI1**（见第 12 节启动机制）。三层关系：

| 输出 | 使能来源 | 挂载 VP |
| --- | --- | --- |
| **DP0**（USB-C）| 基础 DTS 直接启用 | **VP1**（`route_dp0` → `vp1_out_dp0`）|
| **HDMI0** | uEnv 默认 overlay `rk3588-lubancat-5-hdmi0-overlay` | **VP0**（`route_hdmi0` → `vp0_out_hdmi0`）|
| **HDMI1** | uEnv 默认 overlay `rk3588-lubancat-5-hdmi1-vp2-overlay` | **VP2**（`route_hdmi1` → `vp2_out_hdmi1`）|
| **HDMI IN（采集）**| 基础 DTS 启用 | `hdmirx_ctrler`（8K 采集）+ `hdmiin_sound`，HPD 检测 GPIO1_D5，预留 **256MB CMA** |
| MIPI DSI0/DSI1 | 默认关闭（V2 连 `backlight_dsi0/1`、`pwm0/1` 一并禁用）| 需 overlay：`rk3588-lubancat-5-v2-dsi0-vp2-*` / `dsi1-vp3-*` |

VOP2 四路 VP 平面掩码已配（vp0-vp3，各含 cluster + esmart 层）。

---

## 7. 音频

- **编解码器 ES8388**（i2c7 0x11，绑定的驱动是厂商 `es8323.c`）+ `i2s0_8ch`：耳机（LOUT/ROUT）+ 主 Mic + 耳机 Mic；
  耳机检测 GPIO1_D3；**按键（播放/上一首/下一首）经 SARADC ch4** 检测
- **喇叭有独立功放**（原理图 page 40，Rev V2R1）：**U43 = `8002A`（3W class-D）**，
  输入取 ES8388 的 **LOUT2/ROUT2**（经 R376/R377 20K），输出到 **J24（PH-2A 2-pin 喇叭座）**；
  耳机座 **J23（PJ-393 3.5mm）** 由 LOUT1/ROUT1 驱动。`EARPHONE_CTL` = GPIO1_B5 = DTS 的 `hp-con-gpio`
  （DTS 里**没有** `spk-con-gpio`，所以 `spk switch` 那个控件是空转）
- ⚠️ **"音量拉满还是小声"是增益/配置问题不是喇叭上限**：ES8388 的模拟输出音量
  `Output 1/2 Playback Volume` 被我们自己的 UCM（`overlay/usr/share/alsa/ucm2/rockchip-es8388/HiFi.conf`）
  每次使能设备时写成 **27/33 = -4.5 dB**，而且 UCM 没声明 `PlaybackMixerElem` ⇒ ACP 找不到硬件音量、
  整个 sink 退回软件衰减。**2026-09-20 已修**（删掉那两行 cset + 补 `PlaybackMixerElem`）：
  现在桌面推子 0%↔硬件最小(-45 dB)、100%↔硬件最大(+4.5 dB)。
  证据与验证见 [../system/audio-pipewire.md](../system/audio-pipewire.md)
- ✅ **耳机口（J23）与扬声器（J24）是互斥的**（2026-09-20 查清，三层保证）：
  1. **软件端口策略（当前栈里起决定作用）**：UCM 里 `Speaker` 与 `Headphones` 互为 `ConflictingDevice`，
     Headphones 还带 `JackHWMute "Speaker"` ⇒ 插上耳机后 ACP 把活动设备从 Speaker 切到 Headphones
     （`Speaker Switch off` + `Headphone Switch on`），同一时刻只有一个出声。
  2. **驱动 GPIO**：`SND_SOC_DAPM_HP("Headphone", mc_hp_event)`（`rockchip_multicodecs.c:353`）
     ⇒ 耳机 DAPM 路径一上电就把 **`hp-con`（GPIO1_B5 = 原理图 `EARPHONE_CTL`）拉高**去驱动
     J23 旁的 Q32/Q33，下电时拉低（`:303-326`）。实测当前（活动端口=Speaker）
     `cat /sys/kernel/debug/gpio` → `gpio-45 (|hp-con) out lo`。
  3. **硬件插入检测**：3.5mm 座的 `HP_PLUG_IN_DET` = GPIO1_D3（实测 `gpio-59 (|hp-det) in lo IRQ`）
     → `rk_multicodecs_jack_check()` → extcon + `snd_soc_jack_report`（kcontrol `Headphone Jack`，
     `numid=37`）→ ACP/UCM 据此切端口。
  - ⚠️ **电气上两路其实是独立的**（LOUT1/ROUT1 → J23；LOUT2/ROUT2 → 8002A → J24），
    而 UCM 的 verb `EnableSequence` 里 `OUT1 Switch` 与 `OUT2 Switch` **都打开** ⇒
    "互斥"主要由**软件端口切换**实现；强行让两个设备同时活跃（手动开两个 switch）是能两边都响的，
    但**不是默认栈的行为**。
  - 💡 因此想做"不出声的电气回环"测量，**插上耳机口正好会把喇叭静音**（一插就切到 Headphones），
    比对着喇叭录音更适合在工区做。
  - ⏳ **尚未用真插头验证**（2026-09-20 手上没有 3.5mm 插头）：上面三条是"源码 + UCM + 原理图 + 未插状态
    的 GPIO 实测"推出来的。补验只需插/拔一次（**不用放任何音频**）：
    `wpctl status`（Active Port ↔）、`cat /sys/kernel/debug/gpio | grep hp-`（`gpio-45 hp-con` lo↔hi、
    `gpio-59 hp-det` lo↔hi）、`amixer -c rockchipes8388 sget 'Headphone Switch'`。
- `dp0_sound`（SPDIF TX2）、`hdmiin_sound`（I2S7）— 随对应显示/采集节点启用
- 额外启用 `i2s5_8ch`、`i2s6_8ch`

---

## 8. 摄像头 / CSI（默认全部关闭）

板级 CSI 树齐全但**默认禁用**，需按所接 sensor 改 DTS 启用：

- 2× **MIPI CSI-2 DC-PHY**（`dcphy0`/`dcphy1`，4-lane，可接 imx415 8MP 等）
- 4× **MIPI CSI-2 D-PHY**（`dphy1`/`dphy2`/`dphy4`/`dphy5`）
- 数据通路：`rkcif` + `rkisp0/1`（含 unite、fec）、`rkispp0/1`、`rkcif_mipi_lvds*`
- 已带驱动节点：ov5647 / ov5648 / ov8858 / ov13850 / imx415 / gc08a8 / gc2053 / gc4653，
  及 dw9714 对焦马达；供电 `cam_dovdd 1.8V / cam_avdd 2.8V / cam_dvdd 1.2V`，
  外部时钟 24/25/27/37MHz

---

## 9. 板载杂项

| 设备 | 位置/说明 |
| --- | --- |
| **RTC：HYM8563**（丝印 U44 = `BM8563EMA`）| i2c0 @0x51，32.768kHz，中断 GPIO0_B0，`wakeup-source`（休眠可唤醒）|
| **RTC 备电** | **J25**（`BAT_RTC`）与 `VCC_3V3_S3` 经 **D61 = BAT54C** 共阴极二极管或接到 U44 VDD；断电测试须断开主供电及反供连接。供电图、实测与寿命边界见 [RTC 备电](rtc-hym8563-battery.md)，不以芯片耗电规格保证整板电池寿命。 |
| 系统 LED | `sys_led`：GPIO0_D3，心跳模式（heartbeat）|
| **ADC 按键** | SARADC ch1：方向键（up）|
| **IR 红外遥控** | `pwm3` remotectl，key_table `ir_key_lubancat`（电源/菜单/返回/Home/音量/数字键…，usercode 0xff00）|
| **PWM 风扇** | `pwm4` + pwm-fan，5 级（0/100/150/200/255），35/40/45/50°C 阶梯 |
| **GPIO 扩展 ×2** | i2c5@0x21 → `gpio6`、i2c6@0x21 → `gpio7`（各 16 路，PCA9535）|
| 看门狗 | `wdt`（`/dev/watchdog`）|
| 调试串口 | **UART2**（`ttyFIQ0`，fiq-debugger，**1500000** 波特，earlycon 0xfeb50000）|
| 已启用的 I2C | i2c0（RTC+CPU调压）、i2c1（FUSB302+NPU调压）、i2c5/i2c6（GPIO 扩展）、i2c7（ES8388）|
| 已启用的 PWM | pwm3（IR）、pwm4（风扇）；pwm0/1 在 V2 禁用 |

---

## 10. V1 → V2 差异速查

| 项 | V1（rk3588-lubancat-5.dts）| V2（rk3588-lubancat-5-v2.dts）|
| --- | --- | --- |
| model/compatible | `Embedfire LubanCat-5` | `Embedfire LubanCat-5 V2` |
| FUSB302 中断 | GPIO0_C4 | **GPIO0_B2** |
| M.2 E-Key disable | GPIO0_C6 | **PCA9535 扩展 GPIO6_0** |
| MIPI DSI 背光 + pwm0/1 | 启用 | **全部禁用** |

---

## 11. 常用对照命令（在设备上）

```bash
lscpu                              # 8 核 / 架构
cat /proc/cpuinfo                  # A76+A55
cat /sys/class/rtc/rtc0/device/name   # hym8563
ls /sys/class/net/                 # eth0/eth1（+wlan 视模块）
lsusb                              # USB 拓扑
ls /sys/class/gpio/                # gpiochip 0-6（0-4 SoC + 5/6 扩展）
v4l2-ctl --list-devices            # 摄像头（启用后）
cat /sys/class/thermal/thermal_zone*/temp

# 启动分区 / 显示 overlay 实况
cat /boot/firmware/ubuntuEnv.txt   # 当前 bootargs / fdtfile / overlays=
ls /boot/firmware/dtbs/rockchip/overlay/   # 可用 overlay 列表
cat /sys/kernel/debug/dri/0/state  # 各 VP 实际挂的输出（需 root）
```

---

## 12. U-Boot 启动机制（`boot.scr` + `ubuntuEnv.txt` + dtb overlay）→ 见单独一篇

`boot.scr`/`boot.cmd`、`ubuntuEnv.txt`（`bootargs`/`overlays=`/`fdtfile`）与 dtb overlay 的合并时机
**已搬成单独一篇** → **[u-boot-boot-scr-uenv.md](u-boot-boot-scr-uenv.md)**。

## 13. 与 `lubancat-5io` 的 DTS 差异速查 → 见单独一篇

逐节点 diff（只有一边覆盖的节点 / 同名不同配置 / 派生自研底板要注意什么）**已搬成单独一篇** →
**[lubancat-5-v2-vs-5io-dts.md](lubancat-5-v2-vs-5io-dts.md)**。
