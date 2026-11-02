# Combo PHY 复用怎么切（overlay 写法、选脚、热插拔）

> 本文回答**"怎么动手切"**：现成的两个范例 overlay、以 5io 为基线改回 v2 归属的完整实例、
> `reset-gpios`/`disable-gpios` 怎么选、PCIe 热插拔能不能做、以及"别拿另一块板的 DTB 来模拟自研底板"。
> 原理（谁决定模式、冲突为什么静默）与两块板的实际接线见 [combphy-mux.md](combphy-mux.md) §1/§2；
> `usbhost3_0` 上挂 4 口 hub 为什么不成立见 [combphy-mux-usbhost3-hub.md](combphy-mux-usbhost3-hub.md)。
> ⚠️ 下面正文是**从 `combphy-mux.md` §3 原样搬过来的**（只重排了 §3.5 的位置）。

## TL;DR

- **"切换"= 换 overlay**：`overlays=` 里启用厂商现成的 `-sata0`/`-pcie*` overlay，或按 §3 的范例自己写。
- 三条硬注意：① **重启才生效**；② 必须保证**同一颗 PHY 只有一个消费者**（另一个 `status` 要 disabled，
  否则"后者静默覆盖"）；③ ⛔ **v2 上切 `sata0` 会掉 WiFi** —— `combphy0_ps` 上挂的就是 WiFi 卡的控制器
  （2026-09-17 板上实测，见 [combphy-mux.md](combphy-mux.md) §4.1）。
- `reset-gpios` **不能随便挑一根空闲 GPIO**（要按原理图/实测，见 §3.2）；`disable-gpios` 在本内核里
  **没有驱动消费**，关控制器只能靠 `status`。
- ⛔ 别拿"另一块板的 DTB"来模拟自研底板（§3.5）—— 厂商 5iob/5iof/5ioi 那种差几个 GPIO 的才适合照抄。


## 3. 怎么切换（现成的两个范例 overlay）

`kernel-6.1/arch/arm64/boot/dts/rockchip/overlay/` 里已经有两个"把 PCIe 槽切成 SATA"的例子：

```dts
/* rk3588-lubancat-5-sata0-overlay.dts —— v2：Mini-PCIe → SATA0 */
fragment@0 { target = <&pcie2x1l2>; __overlay__ { status = "disabled"; }; };
fragment@1 { target = <&sata0>;     __overlay__ { status = "okay";    }; };

/* rk3588-lubancat-5io-sata2-overlay.dts —— 5io：Mini-PCIe → mSATA（SATA2） */
fragment@0 { target = <&pcie2x1l1>; __overlay__ { status = "disabled"; }; };
fragment@1 { target = <&sata2>;     __overlay__ { status = "okay";
              vpcie3v3-supply = <&vcc3v3_mini_pcie>; }; };
```

**模板就是"禁掉原控制器 + 打开另一个 + 把槽位的 3.3 V 供电补给新控制器"**。

用法与注意：

1. 加进 `config/uEnv/<板>.uEnv` 的 `overlays=`（板子上是 `/boot/firmware/ubuntuEnv.txt`），**重启生效**
   —— U-Boot 在启动时套 overlay，**运行期不能切**。
2. 必须保证**同 PHY 的另一个消费者是 disabled**，否则按 [combphy-mux.md](combphy-mux.md) §1 的规则"后者赢"、静默覆盖模式。
3. `disable-gpios` 不生效（见 [combphy-mux.md](combphy-mux.md) §1），所以"关掉控制器"只能靠 `status`。
4. ⛔ **v2 上不要随手开 `rk3588-lubancat-5-sata0-overlay`**：那块 PHY（combphy0_ps）上的
   `pcie2x1l2` 正是 **WiFi 卡所在的控制器**（见 [combphy-mux.md](combphy-mux.md) §4）⇒ 切 SATA 会**掉 WiFi**
   （**2026-09-17 已在 v2 板上实测证实**，见 [combphy-mux.md](combphy-mux.md) §4.1）。
   📌 **自研底板 combphy 照 v2 做（用户 2026-09-17 确认）⇒ 这条同样适用**：只要 Mini-PCIe 槽里插的是
   WiFi 模块，就别开 `sata0` overlay（想在这个槽上插 mSATA 才开，那时本来也不要 WiFi）。
5. 5io 的 mSATA 用法（厂商 `lubancat-5io.uEnv` 注释原话："开启 sata2, mini pcie接口可接 msata 硬盘"）
   **我们没在 5io 板子上实测过**（手里只有 v2 板）。

---

### 3.1 实例：以 5io 为基线，把 `combphy0_ps` 改回 Mini-PCIe、`combphy2_psu` 改回 USB3.1 HOST2

> ✅ **状态（2026-09-17）：用户已确认"底板 combphy 口确实是照 v2 做的" ⇒ 本节的目标分配就是最终方案**，
> 本节那 5 个节点的改法可以直接用（若底板从 v2 的 DTS 起步，连改都不用改）。
> ⚠️ 若核心板/底板组合仍从 **5io** 起步，才需要按下面这套"5io → v2 归属"的改法（注意 §3.1 里
> PERST#/电源脚是按 5io 槽位写的，换成自研底板要**按自研板原理图核**，别照抄）。

自研底板（5io 核心板 + 自研底板）要的分配 = **v2 的 PHY 归属**：

| PHY | 5io 原状 | **目标** |
| --- | --- | --- |
| `combphy0_ps` | `sata0`（SATA） | **`pcie2x1l2` = Mini-PCIe** |
| `combphy1_ps` | `pcie2x1l0` = M.2 E-Key | 不变 |
| `combphy2_psu` | `pcie2x1l1`（Mini-PCIe） | **`usbhost3_0` = USB3.1 HOST2** |

相对 `rk3588-lubancat-5iob.dts` 一共只动 **5 个节点**（`combphy*` 三个本来都是 `okay`，不用动）：

```dts
/* ① 让出 combphy0_ps（5io 原本拿它做 SATA0） */
&sata0 { status = "disabled"; };

/* ② 把 combphy0_ps 给 Mini-PCIe（pcie2x1l2 在 5iob.dts 里从未出现，默认 disabled） */
&pcie2x1l2 {
	pinctrl-names = "default";
	pinctrl-0 = <&pcie_disable_gpio>;               /* ⚠️ 见下面的"槽位信号"说明 */
	reset-gpios = <&gpio4 RK_PA2 GPIO_ACTIVE_HIGH>; /* ⚠️ PERST#：沿用原槽位的脚 */
	disable-gpios = <&gpio3 RK_PA6 GPIO_ACTIVE_HIGH>;
	vpcie3v3-supply = <&vcc3v3_mini_pcie>;          /* 同一个槽的 3.3V，可直接沿用 */
	status = "okay";
};

/* ③ 让出 combphy2_psu（5io 原本拿它做 Mini-PCIe） */
&pcie2x1l1 { status = "disabled"; };

/* ④⑤ combphy2_psu 给 USB3.1 HOST2（USB3OTG2，v2 上就是这么开的） */
&usbhost3_0 { status = "okay"; };
&usbhost_dwc3_0 { dr_mode = "host"; status = "okay"; };
```

**三条要点**：

1. ⭐ **`reset-gpios` 是槽位信号（PERST#），不是控制器信号** —— 同一个物理槽、同一根走线，
   不管是哪个控制器在服务它，PERST# 还是那根脚。所以这里应当**沿用 5io 原本写在 `&pcie2x1l1`
   上的 `reset-gpios`/`disable-gpios`/`pinctrl-0`**，而不是照抄 v2 的 `gpio3 PD1`/`gpio3 PC1`
   （那是 v2 的 PCB 上的脚）。`vpcie3v3-supply` 同理：同一个槽，沿用 `vcc3v3_mini_pcie`
   （v2 上没有这个开关所以没写；5io 这个槽是可切 3.3V 的，要写）。
   ⚠️ 自研底板若**重新设计了槽位**（换了 PERST# 脚 / 去掉 3.3V 开关），就按原理图改。
2. **USB3 HOST2 的 USB2 伴生口不在这个控制器上**：`usbhost3_0` 只用 `combphy2_psu` 的 USB3 通道
   （`usbhost_dwc3_0` 里 `phys = <&combphy2_psu PHY_TYPE_USB3>`），**USB2 那半是单独的
   `usb_host1_ehci/ohci` + `u2phy3_host`**，5io/v2 都已经是 `okay`。所以只需确认
   `&u2phy3_host` 的 `phy-supply` 与自研底板上那个口的 VBUS 电源一致
   （5io 是 `vcc5v0_usb20_hub`、v2 是 `vcc5v0_usb`）——这是原理图问题，不是 PHY 复用问题。
3. ⛔ **改完就不要再叠加 `rk3588-lubancat-5io-sata2-overlay`**：它会 `enable &sata2`，
   而 `sata2` 正是 `combphy2_psu` 的另一个消费者 ⇒ 与 `usbhost3_0` 抢同一颗 PHY（按 [combphy-mux.md](combphy-mux.md) §1 的
   "后者赢"规则会静默覆盖）。反过来，**`rk3588-lubancat-5-sata0-overlay`（disable `pcie2x1l2` +
   enable `sata0`）在新分配下正好是"Mini-PCIe 槽改插 mSATA"的正确开关** —— 原来给 v2 写的那个
   overlay，在自研底板上原样可用。

**落地形式**（按 `../build/derived-board-bringup.md` §3 的取舍）：

- 推荐 **A′ 薄 wrapper**：新板卡 dts 只有十几行，`#include "rk3588-lubancat-5iob.dts"` +
  上面 5 处 override（只改 `status`/属性、不删节点，这种改法 wrapper 完全够用），与 EBF 上游保持可 diff；
- 或者 **A 整份复制** `5iob.dts` 再改（1700+ 行，想删节点时用）；
- ⛔ 不要用 `.dtbo` overlay 表达"底板固有差异"（§3 方案 C 的理由）。

**改完自检**（在目标板上）：

```bash
for c in fe190000 fe180000 fe150000 fe170000; do
  printf '%-10s driver=%s\n' $c "$(readlink -f /sys/bus/platform/devices/$c.pcie/driver 2>/dev/null | sed 's|.*/||')"; done
sudo dmesg | grep -iE 'rk-pcie|combphy|naneng|usb3|dwc3' | head -20
lspci -D -nn                     # Mini-PCIe 上的卡应出现在 40-4f 段（domain 4）
lsusb -t                         # USB3 HOST2 应出现 5000M 的根 hub
```
若看到 `expected mode is … but current mode is …` 或 `phy type select … overwriting type …`，
说明还有第二个消费者没关掉。

#### 3.1.1 自研底板的引脚方案（2026-09-17 用户确认）

用户交底：**mini-PCIe 槽的两根控制脚直接从 5io 原来那套平移到新的 `pcie2x1l2` 上**，
`combphy2_psu` 改成 USB3 之后**不再需要这两个脚**：

```dts
&pcie2x1l2 {
	pinctrl-names = "default";
	pinctrl-0 = <&pcie_disable_gpio>;                  /* 配的是 3 RK_PA6（disable 脚） */
	reset-gpios   = <&gpio4 RK_PA2 GPIO_ACTIVE_HIGH>;  /* 原 5io mini-PCIe 的 PERST#，同脚平移 */
	disable-gpios = <&gpio3 RK_PA6 GPIO_ACTIVE_HIGH>;  /* 同上；本内核不消费，属文档性质 */
	vpcie3v3-supply = <&vcc3v3_mini_pcie>;             /* 同一个槽的 3.3V */
	status = "okay";
};
```

- ✅ **USB3 HOST2 侧确实一个 GPIO 都不需要**（源码核对：`usbhost3_0` / `usbhost_dwc3_0` 只有
  `resets = <&cru SRST_A_USB3OTG2>`（CRU 复位，不是 GPIO）＋ `dr_mode` ＋ `phys`，
  没有任何 `-gpios` / `-supply` 属性）。
- ✅ **`gpio4 PA2` 用于槽位 PERST# 不冲突**：5io 上 `gpio4 PA2~PA5` 分别是
  **PA2 = mini-PCIe 的 PERST#**、PA3 = `vbus5v0_typec` 使能、PA4 = `vcc5v0_usb30_otg` 使能、
  PA5 = `vcc5v0_usb20_hub` 使能 —— 新底板若保留这几路电源，PA3~PA5 仍被占用，PA2 给槽位正好；
  ⚠️ 但若还要给 **USB3 HOST2 加 VBUS 开关**，得另找空闲脚，别撞这几个（也别撞 bank3 的
  `PA6`=槽位 disable、`PC0`/`PC1`=两个网口 PHY reset）。
- ⚠️ **mSATA 备选控制器跟着换了**：新槽位挂在 `combphy0_ps` 上，它的 SATA 备选是 **`sata0`**
  （不再是 5io 的 `sata2`）⇒"Mini-PCIe 槽插 mSATA"的开关变成 `disable pcie2x1l2 + enable sata0`，
  也就是**现成的 `rk3588-lubancat-5-sata0-overlay`**（原本给 v2 写的那个，可直接复用）；
  同时**绝不能再用 `rk3588-lubancat-5io-sata2-overlay`**（它会去抢 `combphy2_psu`，与 USB3 HOST2 冲突）。
- ⚠️ 极性仍按"新底板怎么驱动 PERST#"定：直连（像 5io/v2 那样，串个 100 Ω）就保持
  `GPIO_ACTIVE_HIGH`；若加了反相器/缓冲要改 `GPIO_ACTIVE_LOW`（见 §3.2）。
- ⚠️ **"两根脚平移"本身没问题，但前提是 `&pcie2x1l1` 必须 disabled**（上面第 ③ 条）：
  Linux 的 `gpiod_get` 是**独占**的，两个控制器同时声明同一对 GPIO 时，后 probe 的那个会因
  `-EBUSY` 失败（除了 PHY 被抢，这是第二个坏法）。`pcie2x1l2` 上这两根脚对**控制器**是中性的
  （普通 GPIO，SoC 侧不与任何 combphy/控制器绑定），冲突只可能来自"另一个消费者没关掉"。

---

### 3.2 `reset-gpios` / `disable-gpios` 能不能"随便挑一根空闲 GPIO"？

**结论：软件上可以随便写（SoC 不规定用哪根），但硬件上必须与原理图一致；写错 = 槽位起不来。**
`disable-gpios` 在本内核里**根本没有驱动消费**（见 §1），所以今天填什么都不影响功能 —— 但也**别乱填**（见下）。

**① 同一个用途、两块板用不同脚 ⇒ 证明这是 PCB 走线决定的选择**：

| 板 | reset | disable | pinctrl |
| --- | --- | --- | --- |
| v2 | `gpio3 PD1` | `gpio3 PC1` | `pcie_disable_pin`（配的是 **disable** 脚 `3 RK_PC1`，`RK_FUNC_GPIO &pcfg_pull_up`） |
| 5io | `gpio4 PA2` | `gpio3 PA6` | `pcie_disable_gpio`（配的是 **disable** 脚 `3 RK_PA6`） |

⚠️ 注意两块板**都没给 reset 脚写 pinctrl**（靠它默认功能就是 GPIO）；自研底板若选了默认功能不是 GPIO
的脚，**必须自己加一条 pinctrl 把它 mux 成 `RK_FUNC_GPIO`** 并给合适的 pull，否则驱动写寄存器也不管用。

**② 驱动怎么用它**（`drivers/pci/controller/dwc/pcie-dw-rockchip.c`）：

- `:695` `devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW)` —— probe 时先置**逻辑 0**（= 保持复位；
  注释解释：槽位 3.3V 给 100 MHz OSC 供电，refclk 可能比"一切就绪"更早，所以要先压在复位里）。
- `:375` 拉**逻辑 0 = 复位有效** → `:415` 等 **200 ms**（`rockchip,perst-inactive-ms`，默认 200；
  注释引用 PCIe 规范 T_PVPERL ≥ **100 ms**）→ `:415` 拉**逻辑 1 = 释放**。
- ⇒ **极性规则**：直连 PERST#（PERST# 低有效）时写 **`GPIO_ACTIVE_HIGH`**（逻辑 0 → 物理低 = 复位有效，
  正好是驱动要的语义）—— 两块板都是这么写的；若底板上加了反相器/缓冲（GPIO 高 → PERST# 低），
  才需要 `GPIO_ACTIVE_LOW`。**写反 = 器件一直被复位或一直出不来复位。**
- `:714` `prsnt-gpios`（GPIOD_IN）是**可选**的存在检测信号，不写就是那句
  `invalid prsnt-gpios property in node`（info 级，不是错误）；要热插拔检测就按原理图接。

**③ 自研底板选脚的检查清单**：

1. 必须是 SoM 连接器引出、**默认功能可作 GPIO** 的脚（否则补 pinctrl，见上）；
2. 不能与底板上其它外设或 SoC 固定功能冲突（启动采样脚、调试串口、SD/eMMC、PMIC I2C、JTAG、
   已被相机/电机/风扇占用的脚）；
3. 供电域要在槽位工作期间一直有电；电平与 PERST# 驱动电路匹配（3.3V）；
4. **上电默认态要安全**：从断电到 Linux 探测之间（BootROM/U-Boot 阶段）PERST# 应保持"复位有效"，
   所以选脚时看它复位后的默认电平/内外上下拉，必要时硬件加下拉/缓冲；
5. `disable-gpios`：**有真信号才填，没有就删掉**（本内核不消费；将来某版驱动若实现了
   `devm_gpiod_get_optional(dev, "disable")`，填一根"随手捡的空闲脚"就会在 probe 时去驱动一根
   跟这个槽毫无关系的线，可能撞上别的外设）。

#### 3.2.1 v2 原理图核对（2026-09-16 用户核对）：直连 + 100 Ω 串联，`GPIO_ACTIVE_HIGH` 是对的

- **reset / disable 各串一个 100 Ω 电阻，直接连到 RK3588 的 `GPIO3_C1`（disable）/ `GPIO3_D1`（reset）**
  ⇒ 没有反相器/缓冲 ⇒ **直连**。100 Ω 只是串联阻尼/保护，不改变极性。
- 于是极性自洽：PERST# 低有效，驱动用"**逻辑 0 = 复位有效、逻辑 1 = 释放**"，`GPIO_ACTIVE_HIGH`
  把逻辑 0 映射成**物理低** = PERST# 有效 ✓。⚠️ 别把 `GPIO_ACTIVE_HIGH` 理解成"PERST# 高有效"——
  这个 flag 描述的是**逻辑值到物理电平的映射**，与 PERST# 自己的有效电平无关。
  将来若在底板上加反相器/缓冲（GPIO 高 → PERST# 低），才需要改成 `GPIO_ACTIVE_LOW`。
- **内部上拉**：`pcfg_pull_up` 就是 SoC 的内部上拉（`rk3588s-pinconf.dtsi:8` = `bias-pull-up;`），
  但注意两点：① 它只在**应用该 pinctrl 状态时**生效（被某设备引为 `pinctrl-0` 并在 probe 时应用）；
  ② 它**只作用于该 entry 里写的那根脚** —— v2 这条配的是 `3 RK_PC1`（disable），
  **reset 脚 `3 RK_PD1` 没有 pinctrl entry**，所以 reset 网络上没有内核配置的内部上拉
  （按 SoC 复位默认/引导程序状态走）。内部上拉是弱上拉（几十 kΩ），只用于定电平、不能当驱动能力。
- 想让"上电 → Linux 探测"之间的 reset 状态确定，两个办法：给该脚补一条 pinctrl
  （`RK_FUNC_GPIO` + `bias-pull-down`，保持复位有效），或在底板上加下拉。**不过这不是致命问题**：
  驱动 probe 时会先置"复位"（`GPIOD_OUT_LOW`）、并在训练前完整重做一遍"拉低 → 等 200 ms → 释放"，
  所以只要有正确引脚、极性对，boot 阶段的浮动不影响最终训练。

### 3.3 PCIe 能不能热插拔？——**默认不要，我们这套 BSP 里也没有标准 pciehp**

- ⛔ **标准 PCIe 热插拔（`pciehp`）在本内核配置里没有**：`rockchip_linux_defconfig` 里
  `HOTPLUG_PCI` / `PCIEHP` 相关项**都没有开**（grep 无命中）。RK3588 的 root port 也没有把
  PCIe CEM 的标准热插拔信号（PRSNT1#/PRSNT2#、attention、power controller、slot power）完整引出来。
- 厂商另有一套**私有 GPIO 热插拔**（默认关闭，**树里没有任何板卡 DTS 用它**）：
  - DT 里写 `hotplug-gpios = <&gpioX ...>`（也可以用旧名 `hotplug-gpio`）⇒ 驱动里
    `slot_pluggable = true`（`pcie-dw-rockchip.c:733`）并 `register_gpio_hotplug_slot()`
    （`:1732`，实现在 `drivers/pci/hotplug/gpio_php.c`）；
  - 那个 GPIO 取成 **`GPIOD_IN`，并且要能产生中断**（`gpio_php.c:135`）：IRQ 里读到高 =
    "Hot-Plug Event" → `rk_pcie_slot_enable()`（上电 + PERST# 序列 + 使能 LTSSM）；
    读到低 = "Hot-UnPlug Event" → `rk_pcie_slot_disable()`（关 LTSSM + 拉 PERST# + 断电）；
  - 同时注册成标准 hotplug slot ⇒ `/sys/bus/pci/slots/` 下能手动 enable/disable。
- **结论**：现阶段我们的槽位是"**断电插好再上电**"的用法。带电插拔的风险是实打实的：
  没有电源开关时序 → 浪涌/3.3V 跌落；PERST# 状态未定义 → 链路不训练、总线可能挂死；
  即使 `echo 1 > /sys/bus/pci/rescan` 也可能因为 link 没重训而失败。**本板的 WiFi 卡就插在
  Mini-PCIe 上**，正是这个场景。
- 要真做现场热插拔，硬件必须提供三件事（规范要求，不是"槽能插就行"）：
  ① 可控的槽位电源（load switch / 热插拔控制器）；② 存在检测信号（PRSNT#/GPIO，且能中断）；
  ③ PERST#（和 CLKREQ#）控制。软件侧才谈得上打开厂商的 `hotplug-gpios` 机制
  （注意这是 vendor 私有实现，不是标准 pciehp）。⚠️ **mSATA 走 SATA 控制器，更不支持热插拔。**


### 3.5 ⛔ 不要拿"另一块板的 DTB"来模拟自研底板（2026-09-16 讨论结论）

> （本节编号保持 §3.5 不变 —— §3.4 已单独成篇：[combphy-mux-usbhost3-hub.md](combphy-mux-usbhost3-hub.md)。）

有个自然的想法："自研底板 = 5io 的 dts + 改成 v2 的 PHY 归属，那先在 **v2 板**上刷这份改后的
5io DTB 试试"。**结论：不要**，两个独立理由。

**① 测不到目标。** 改后的归属（`combphy0_ps`→`pcie2x1l2`=Mini-PCIe、`combphy2_psu`→`usbhost3_0`=USB3 HOST2）
**恰好就是 v2 硬件的原生接线** ⇒ 等价于"用 v2 原厂 DTS 跑 v2 板"，而这件事已经有实测证据：
Mini-PCIe（`fe190000`）链路 up、WiFi 卡就在上面；`usbhost3_0` 的 `usb1`(480M)+`usb2`(5000M) 两个
root hub 都在。**PHY 归属由 PCB 走线决定**，换 DTB 搬不动走线 —— 任何现有板子都替代不了自研底板。

**② 5io 的 DTB 在 v2 板上会去驱动"别人的脚"**（逐脚核对，v2 侧行号来自 `rk3588-lubancat-5.dts`）：

| 引脚 | 5io DTB 里的用途 | **v2 板上实际是** |
| --- | --- | --- |
| `gpio4 PA5` | `vcc5v0_usb20_hub_en`（hub 5V 开关） | **M.2 E-Key 的 PERST#**（`pcie2x1l0` `reset-gpios`，`:1380`） |
| `gpio4 PC6` | `vcc3v3_pcie_en` | **gmac0 网口 PHY 复位**（`snps,reset-gpio`，`:1257`，低有效） |
| `gpio1 PD3` | `sata_power_en` | **耳机插入检测**（`hp-det-gpio`，`:228`） |
| `gpio2 PC5` | `vcc3v3_m2_pcie_en` | **PCA9535 GPIO 扩展器**的脚（`pinctrl_pca9535_2`） |

叠加其它板级差异（PMIC 输入 **4.0 V vs 5.0 V**、`i2c5m3` vs `i2c5m2`、PCA9535 挂 i2c0 vs i2c5、
网口 RGMII tx_delay 0x26/0x30 vs 0x20/0x20、E-Key reset 脚不同）⇒ 结果是"一堆外设失灵 + 内核按
错误语义去拨别人的复位/检测脚"，**排障会被彻底带偏**（分不清是 DTS 的锅还是 PHY 的锅）。
可回滚（换回 DTB / 从 SD 启动都能救），但没有意义。

**按目的选替代方案**：

- **A. 只想验证 DTS 写得对不对（推荐，零风险，不用刷板）**：
  ① 静态检查"每颗 combphy 只有一个 `okay` 消费者"（见 §1 的规则；本文件就是这种检查的产物）；
  ② 编 `dtb` 后 `dtc -I dtb -O dts` 反编译核对各处 `status`/`phys`（编译需要内核构建树）。
- **B. 想在真硬件上验证"复用开关"**：现有硬件上唯一有意义且可回滚的实验是在 **v2** 上加载
  `rk3588-lubancat-5-sata0-overlay`（禁 `pcie2x1l2` + 开 `sata0`）重启，用来验证
  "**v2 切 sata0 会掉 WiFi**"这条结论（顺带看 SATA 能否认盘）。换 DTB/overlay 只动启动分区
  （`/boot/firmware/dtbs/rockchip/` + `ubuntuEnv.txt` 的 `fdtfile=`/`overlays=`），不必重刷镜像。
- **C. 验证自研底板**：只能等板子 —— PHY 归属、PERST# 走线、hub 的 SS/HS 配对都是硬件问题。

（在 **5io 板**上刷改后的 5io DTS 是"安全但证明不了什么"的第三条路：走线仍在 `combphy2_psu`，
Mini-PCIe 槽不会因为 DTS 改了就跑到 `combphy0_ps` 上。）
