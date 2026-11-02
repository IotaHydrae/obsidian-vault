# LSP EVB 构建入口与底板核对边界

> `rk3588-lsp-evb` 已接入板卡配置，使用独立 Linux DTB 与现有 5io 启动包。
> EVB 暂无实物，目前仅保留入口；DTS 中的走线 TODO 尚未完成原理图复核，未验证自研底板启动。

## TL;DR

- 派生基线为 LubanCat-5 核心板 + **5iob 底板**；构建默认板卡仍为 `lubancat-5-v2`。
- 新板卡选择 `rk3588-lsp-evb.dtb`，复用 `u-boot-lubancat-5io`，未新增或改名 U-Boot deb。
- DTS 继承值与实板接线必须逐项核对；菜单可选、DTB 能编译都不证明硬件可用。

## 1. 构建入口

| 环节 | 权威入口 | 当前行为 |
| --- | --- | --- |
| 板卡配置 | `config/boards/rk3588-lsp-evb.conf` | 继承 `lubancat-5io.conf` 的核心板启动配置与音频钩子，覆盖板名、厂商和 Linux DTB |
| U-Boot 源包目录 | `UBOOT_PACKAGE=u-boot-lubancat-rk3588` | 复用现有打包工程 |
| U-Boot 平台 | `UBOOT_PLATFORM=lubancat-5io-rk3588` | 复用现有 defconfig 与 U-Boot 控制设备树 |
| 安装包映射 | `UBOOT_DEB_PACKAGE=u-boot-lubancat-5io` | 板卡 ID 与 deb 包名分别选择，避免寻找不存在的 `u-boot-rk3588-lsp-evb` |
| Linux DTS | `kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588-lsp-evb.dts` | 已列入同目录 `Makefile` 的 DTB 构建目标 |
| 显示配置 | `config/uEnv/rk3588-lsp-evb.uEnv` | 仅开启 `rk3588-lubancat-5io-hdmi0-vp0-overlay` |
| 自定义叠加层 | `overlay/boot/firmware/dtbs/rockchip/overlay/rk3588-lsp-evb.overlays` | 只列 `usbcon-host`、`usbcon-fixedsrc`、`dwc3-rolehost`；不重复叠加电量计 |

板卡列表从 `config/boards/*.conf` 发现，无需维护另一份固定白名单。
U-Boot 平台与 deb 的对应关系来自
`packages/u-boot-lubancat-rk3588/debian/{targets.mk,control}`。
该复用选择基于相同核心板与 5io 派生关系；底板启动供电和 USB 差异仍须实板验证。

`5iob.dts` 中的显示 include 均被注释，HDMI0/VP0 overlay 负责开启控制器、路由、PHY 和音频。
它设置的 HDMI 使能 GPIO 也属于底板接线核对项；没有自动启用 DP0、HDMI1 或其它显示输出。
完整编译入口和阶段语义见 [构建指南](build-guide.md)。

## 2. 按底板硬件核对

核心板继承 RK3588、DDR、RK806、晶振、eMMC 与上电时序；底板控制接口供电、复位、PHY、
USB/PCIe 走线、显示、相机及低速收发器。继承配置只是起点，不能替代原理图。

| 项目 | 必核内容 | 相关依据 |
| --- | --- | --- |
| 电源 | regulator GPIO、默认电压、`vin-supply`、开关有效电平 | 目标 `rk3588-lubancat-5iob.dts` 与新底板原理图 |
| 以太网 | PHY 型号/地址、复位、时钟、RGMII 延迟及丝印映射 | [硬件映射](../hardware/lubancat-5-v2-hardware.md) §4 |
| 存储与扩展 | NVMe/PCIe reset、供电、lane、Mini-PCIe/E-Key 接法；禁止同时占用 combo PHY | [Combo PHY](../hardware/combphy-mux.md) |
| USB | FUSB302、角色切换、HUB 上游、复位、过流和各口 VBUS | [VL817](../usb/carrier-board-vl817.md) |
| 显示与相机 | HDMI/DP/DSI 与 VP 分配、使能脚、CSI/ISP 时钟和供电时序 | 目标板级 DTS 与所选 overlay |
| 音频 | ES8388 或替代 codec、供电/时钟、音频命名规则 | `config_image_hook__rk3588-lsp-evb` 当前委托 5io 钩子 |
| 低速接口 | RS485/CAN/UART/I2C/SPI/PWM/ADC pinmux、方向脚、按键、风扇与温控 | 新底板原理图与 pinmux 表 |

**以太网现状**：新 DTS 禁用 gmac1，仅保留 5iob 的 gmac0（原丝印 `eth1`）。
继承的 gmac0 复位为 `gpio3 PC0`、`tx_delay=0x26`、`rgmii-rxid`；这些值必须按新走线复核，
并用实板 `ethtool -S` 检查 CRC/错误。默认 `ethernet0` alias 指向 gmac0，仍需核对实际接口命名与 MAC。
禁用 gmac1 不代表其所有 GPIO 可自由重用；周围复用脚还可能被 PCIe reset/disable 占用。

U-Boot 与 Linux 使用独立 DTB。现有 5io U-Boot 板级 DTS 未启用 GMAC，
但启动 Linux 时仍可按 alias 写入 MAC；Linux DTS 的更改不会同时改变 U-Boot 设备树。

### 2.1 自研底板 USB 设计

已确定采用 VL817，USB2 与 USB3 上游分别连接：

| 项目 | 已定连接 / 待定内容 |
| --- | --- |
| VL817 `HSD0±` | `USB20_HOST0_DP/DM`（`usb_host0` / `u2phy2`，USB2） |
| VL817 SS 上游 | `USB30_SSRXN/P` + `USB30_SSTXN/P`（`usbhost3_0` / `combphy2_psu`，USB3；收发交叉） |
| OTG1 / J16A | 独立保留；最终连接器与 host/otg 功能尚未确定 |
| `USB20_HOST1` / `u2phy3` | 可接单个下游或 USB2 hub；Mini-PCIe 与 E-Key 的 USB 分配尚需定稿 |

combo PHY 的既定分配为 `combphy0 → pcie2x1l2`、`combphy1 → pcie2x1l0`、
`combphy2 → usbhost3_0`；PCIe3 PHY 使用 AGGREGATION，四条 lane 交给 `pcie3x4`。
不得启用与 Wi-Fi 所占 `combphy0` 冲突的 `sata0`。

Mini-PCIe Wi-Fi 卡的蓝牙需要 USB。若两个插槽都要 USB，剩余一路 HOST1 需要额外 USB2 hub；
候选与接线依据见 [USB2 hub 选型](derived-board-usb-hub.md)。该链路没有 SS 通路。
Type-C 默认固定 source + host，`usbcon-fixedsrc` 与 `dwc3-rolehost` 必须配套，见
[Type-C 决策](../usb/typec-rollout-and-decision.md)。

VL817 每个插座同时对应 HS 与 SS 两条设备路径，现场绑定须覆盖两条路径。
新底板上电后需采集实际拓扑并补映射；现有五条现场 udev 绑定不得自动覆盖，详见
[VL817 按口绑定](../usb/carrier-board-vl817.md)。

### 2.2 已有 Linux DTS 骨架

`rk3588-lsp-evb.dts` 直接 include `rk3588-lubancat-5iob.dts`，当前差异如下：

| 节点 | 新板配置 | 目的 |
| --- | --- | --- |
| 根节点 | `model="LSP EVB …"`、`compatible="board,rk3588-lsp-evb", "rockchip,rk3588"` | 独立板卡标识 |
| gmac1 | disabled | 只保留 gmac0 |
| sata0 / pcie2x1l1 / pcie3x2 | disabled | 释放 combo PHY，避免与已选控制器冲突 |
| pcie2x1l2 | okay，使用 combphy0 | Mini-PCIe PCIe 链路 |
| usbhost3_0 | okay，使用 combphy2 | VL817 SS 上游 |
| pcie30phy / pcie3x4 | AGGREGATION(4) / `num-lanes=4` | PCIe3x4 |
| i2c7 / `battery@b` | `sbs,sbs-battery`，地址 `0x0b` | bq40z50 电量计 |

电量计保留 `sbs,disable-charger-broadcasts`，避免它在与 ES8388 共用的总线上向不存在的 SBS 充电器广播。
该接法来自 5io 开发板实验，不能当作新底板的总线确认，见 [电量计](../hardware/battery-gauge-bq40z50.md)。

2026-10-23 的静态验证使用仓库当前 DTS，经 `cpp` + `dtc -@` 编译，
再由 `fdtoverlay` 合入 HDMI0 与三项自定义 overlay；合并成功，`fdtget` 确认
固定 source/host、HDMI0 控制器及路由启用、gmac1 禁用。
这只验证语法、符号引用与配置结果；未执行整镜像构建、烧录或自研板启动。

## 3. DTS 组织与启动固件

当前采用厂商同族板卡的继承方式：Linux 新 DTS include 5iob，仅覆盖固定硬件差异。
这保留上游共同内容，避免复制整个底板文件；将来差异扩大时再评估拆分 SoM/carrier dtsi。
底板固定差异留在基础 DTS，现场可选装配用 overlay。已有 DTBO 文件名保持不变。

目前复用 5io 的 U-Boot 平台，没有新增 U-Boot DTS、defconfig、打包目标或 DDR/BL31 组合。
若底板改变了 U-Boot 会操作的供电、USB、存储或复位路径，需派生并验证 U-Boot 控制设备树；
不能由 Linux DTB 已独立就推断 U-Boot 配置适用。

## 4. 实板前仍需完成

- 逐项关闭源码 `TODO(自研底板)`：gmac0 reset/delay、Mini-PCIe reset/disable/supply、
  PCIe3x4 reset/supply、HDMI 使能脚、电量计总线与地址。
- 定稿 OTG1 功能、Mini-PCIe/E-Key USB 分配与额外 hub 的 reset/过流接线。
- 在实板验证启动介质、供电、串口、网络、USB 各速率、显示、音频和电量计；
  外设权限与现场 udev 绑定按真实拓扑复核。
- 构建入口和静态合并通过后，仍须分别完成镜像构建与实板验收。
