# RK3588 三路 Combo PHY 复用（PCIe / SATA / USB3）与板卡接线

> 覆盖：RK3588 的三个 Combo PHY 各能接什么、**lubancat-5-v2 与 5io 实际接了什么**（两者不一样）、
> 怎么切换（overlay 的写法与原理）、切换时的坑、以及自研底板要先想清楚的事。
> 相关：板级资源清单见 [lubancat-5-v2-hardware.md](lubancat-5-v2-hardware.md)；
> 自研底板派生清单见 [../build/derived-board-bringup.md](../build/derived-board-bringup.md)；
> 启动链路见 [rk3588-boot-flow.md](rk3588-boot-flow.md)。

---

## TL;DR

- RK3588 有**三个 Combo PHY**，每个能在 PCIe / SATA / USB3 里选一种；**v2 与 5io 的接法不一样**（§0/§2）。
- 复用是**内核 DTS 层面**的事，"切换"= 换 overlay（§3 有两个现成范例，含写法与原理）。
- ⛔ **v2 切 `sata0` 会掉 WiFi**（那路 PHY 上还挂着 Mini-PCIe）—— 别照抄示例 overlay。
- 想确认"某路 PHY 现在归谁"用 §5 的排查清单。
- ⚠️ **自研底板动手前先定 PHY 分配**（§6），否则后面很难改。

## 0. 速查表（`rk3588s.dtsi` / `rk3588.dtsi` 源码核对 + 开发板实测）

| Combo PHY | 基址 | 驱动支持的模式 | 能服务的控制器 | **v2 实际接的** | **5io 实际接的** |
| --- | --- | --- | --- | --- | --- |
| `combphy0_ps` | `fee00000` | PCIe / SATA | `pcie2x1l2 @fe190000`、`sata0 @fe210000` | **`pcie2x1l2` = Mini-PCIe** | **`sata0` = SATA** |
| `combphy1_ps` | `fee10000` | PCIe / SATA | `pcie2x1l0 @fe170000`、`sata1 @fe220000` | **`pcie2x1l0` = M.2 E-Key** | 同 v2：**M.2 E-Key** |
| `combphy2_psu` | `fee20000` | PCIe / SATA / USB3 | `pcie2x1l1 @fe180000`、`sata2 @fe230000`、`usbhost3_0` | **`usbhost3_0` = USB3.1 HOST2** | **`pcie2x1l1` = Mini-PCIe** |

**同一个 PHY 同时只能服务一个控制器**（一颗 PHY 只有一种模式），这决定了"板子上某个槽位能不能用"。

不在这三路之内的 PCIe/USB PHY：

| PHY | 基址 | 说明 |
| --- | --- | --- |
| `pcie30phy` | `fee80000` | **专用** PCIe3 PHY（不是 combo），模式由 `rockchip,pcie30-phymode` 决定：`PHY_MODE_PCIE_AGGREGATION`(4) = 4 lane 全给 `pcie3x4`；`PHY_MODE_PCIE_NANBNB`(0) = 2+2，`pcie3x4` 与 `pcie3x2` 各 2 lane（另有 NANBBI/NABINB/NABIBI 三种分法，见 `include/dt-bindings/phy/phy-snps-pcie3.h`）。v2 用 AGGREGATION，5io 用 NANBNB |
| `usbdp_phy0/1` | — | USB3.1 与 DP 的 combo PHY（Type-C / DP 用），与本文三路无关 |
| `hdptxphy0/1`、`mipidcphy0/1` | — | HDMI TX、MIPI DCPHY，无关 |

### 0.1 这些名字怎么读（`pcie2x1l2` 到底是什么）

`pcie2x1l2` = **PCIe 2.0（Gen2）× 1 lane，实例 2**：

| 片段 | 含义 | 证据 |
| --- | --- | --- |
| `pcie` | PCIe 控制器 | — |
| `2` | **代次 = PCIe 2.0（Gen2，5 GT/s）** —— 不是"2 条 lane" | TRM 写法 `PCIE20X1_2`（板卡 dts 注释里就是 `PCIe20x1_2`）；同族命名 `SATA30_HOST0`=SATA 3.0 host0、`USB30_HOST2`=USB 3.0 host2、`pcie30phy`=PCIe 3.0 PHY |
| `x1` | **lane 数 = 1 条** | 时钟名就是 `ACLK_PCIE_1L0/1L1/1L2_*`（`1L` = 1 Lane） |
| `l0/l1/l2` | **同规格控制器的实例号**（芯片内部的 `1L0/1L1/1L2`） | `fe170000`→`1L0`、`fe180000`→`1L1`、`fe190000`→`1L2`；对照 `fe150000`(pcie3x4)→`4L`、`fe160000`(pcie3x2)→`2L` |

- 为什么需要 `lN` 后缀：**RK3588 有三个同规格的 Gen2 x1 控制器**才要区分；RK3568 只有一个，
  标签就直接叫 `pcie2x1`（没有后缀）。
- 这 5 个控制器是 **5 个独立 root complex**：`bus-range` 各不相同
  （`00-0f` / `10-1f` / `20-2f` / `30-3f` / `40-4f`）。⭐ 所以 `lspci` 里那块 WiFi 显示为
  **`0004:41:00.0`** —— 总线 `0x40–0x4f` 正是 `pcie2x1l2` 那段，Linux 给它分了 PCI domain 4。
- 带宽量级：Gen2 x1 ≈ **500 MB/s** 理论 / ~400 MB/s 实用（8b/10b）；Gen3 x1 ≈ 985 MB/s；
  Gen3 x4 ≈ 3.9 GB/s。⚠️ 链路速率由**两端共同决定**：本板 WiFi 那条实测 `LnkSta: 2.5GT/s, Width x1`
  （RTL8822CE 本身只到 Gen1），虽然控制器是 Gen2。
- 与 `pcie3x4`/`pcie3x2` 的区别：那两个是**一个 PHY 的 lane 分配**（AGGREGATION 全给 x4 / NANBNB 2+2），
  而 `pcie2x1l0/l1/l2` 是**三条各自独占一个 combphy 的独立控制器**。

---

## 1. 复用是怎么生效的（源码级）

- **模式由"消费者"给**：控制器节点里写 `phys = <&combphy0_ps PHY_TYPE_PCIE>`（或 `PHY_TYPE_SATA` /
  `PHY_TYPE_USB3`），PHY 驱动在 `xlate` 里把模式**锁存**：
  - `drivers/phy/rockchip/phy-rockchip-naneng-combphy.c:333-343`：第一个消费者设 `priv->mode`；
    **第二个消费者要求不同模式时只 `dev_warn("phy type select %d overwriting type %d")` 然后直接覆盖**
    —— 也就是**后者赢**，不会报错。
  - 同文件 `:306-318` `validate`：`phy_init` 时若请求的模式与锁存的不一致，才
    `dev_err("expected mode is %s, but current mode is %s")` + `-EINVAL`。
- ⇒ **必须靠 DT 的 `status` 保证"每个 PHY 只有一个活着的消费者"**。切复用 = 禁掉一个控制器、打开另一个。
- 内核侧开关都已开（`arch/arm64/configs/rockchip_linux_defconfig`）：
  `CONFIG_PHY_ROCKCHIP_NANENG_COMBO_PHY=y`、`CONFIG_PHY_ROCKCHIP_SNPS_PCIE3=y`、
  `CONFIG_PCIE_DW_ROCKCHIP=y`、`CONFIG_AHCI_DWC=y`（SATA 控制器 `compatible = "rockchip,rk-ahci","snps,dwc-ahci"`，
  实际匹配的是第二个；`rockchip,rk-ahci` 在树里没有驱动认）。
- ⚠️ **`disable-gpios` 目前是装饰**：v2/5io 的 PCIe 节点里写了 `disable-gpios = <...>`，但
  `drivers/pci/controller/dwc/pcie-dw-rockchip.c` 只解析 `reset`（`:695`）和 `prsnt`（`:714`），
  全树也搜不到消费 `disable-gpios` 的代码 ⇒ 它**不影响功能**，也别指望它能"关掉某个控制器"。

---

## 2. 两块板的实际接线（源码行号）

### lubancat-5-v2（`rk3588-lubancat-5.dts`，v2 就是 include 它）

| 槽位 / 外设 | 控制器 | PHY | DT |
| --- | --- | --- | --- |
| M.2 M-Key | `pcie3x4 @fe150000`（4 lane） | `pcie30phy` AGGREGATION | `:1347`（phy）/ `:1353`（控制器，reset `gpio1 PB2`，`vcc3v3_sys`） |
| **Mini-PCIe** | `pcie2x1l2 @fe190000` | **`combphy0_ps`** | `:1360` / `:1365`（reset `gpio3 PD1`、disable `gpio3 PC1`、`pcie_disable_pin`） |
| M.2 E-Key | `pcie2x1l0 @fe170000` | **`combphy1_ps`** | `:1374` / `:1379`（reset `gpio4 PA5`、disable `gpio0 PC6`） |
| USB3.1 HOST2 | `usbhost3_0` | **`combphy2_psu`** | `:1473` / `:1478`（+ `usbhost_dwc3_0` `dr_mode="host"` `:1482`） |
| USB3.1/DP | `usbdp_phy1` | — | `:1386` |

### lubancat-5io（`rk3588-lubancat-5iob.dts`）

| 槽位 / 外设 | 控制器 | PHY | DT |
| --- | --- | --- | --- |
| M.2 M-Key（2 lane） | `pcie3x4 @fe150000` `num-lanes=<2>` | `pcie30phy` **NANBNB** | `:1427` / `:1433`（reset `gpio4 PB6`，`vcc3v3_m2_pcie`） |
| PCIe-64P（2 lane） | `pcie3x2 @fe160000` | 同上（NANBNB 的另一半） | `:1441`（reset `gpio4 PB0`，`vcc3v3_pciex4`） |
| **SATA** | `sata0 @fe210000` | **`combphy0_ps`** | `:1448` / `:1453` |
| M.2 E-Key | `pcie2x1l0 @fe170000` | **`combphy1_ps`** | `:1458` / `:1463`（reset `gpio0 PC5`、`vcc3v3_ekey_pcie`） |
| **Mini-PCIe** | `pcie2x1l1 @fe180000` | **`combphy2_psu`** | `:1471` / `:1476`（reset `gpio4 PA2`、disable `gpio3 PA6`、`vcc3v3_mini_pcie`） |

⇒ **同一个"Mini-PCIe"槽，两块板挂的是不同的 PHY 与控制器**（v2 走 combphy0_ps/pcie2x1l2，
5io 走 combphy2_psu/pcie2x1l1）。派生自研底板时**不能照抄 v2 的这套覆盖**。

---

## 3. 怎么切换

**"切换"= 换 overlay**：`overlays=` 里启用厂商现成的 `-sata0`/`-pcie*` overlay，或自己写。
⛔ 三条硬注意：重启才生效；**同一颗 PHY 只能有一个消费者**（另一个 `status` 要 disabled）；
**v2 上切 `sata0` 会掉 WiFi**（`combphy0_ps` 上就是 WiFi 卡的控制器，§4.1 有实测）。

- **§3.1 以 5io 为基线改回 v2 归属的完整实例**、**§3.2 `reset-gpios`/`disable-gpios` 怎么选**、
  **§3.3 PCIe 热插拔**、**§3.5 别拿别的板的 DTB 模拟自研底板** → [combphy-mux-switching.md](combphy-mux-switching.md)
- **§3.4 在 `usbhost3_0` 上挂 4 口 hub（结论：不成立）** → [combphy-mux-usbhost3-hub.md](combphy-mux-usbhost3-hub.md)

## 4. 开发板实测现状（v2，内核 6.1.172，2026-09-16）

**PCIe 控制器最终状态**（`readlink /sys/bus/platform/devices/*.pcie/driver` + `lspci -D`）：

| 控制器 | 对应槽位 | 状态 | 证据 |
| --- | --- | --- | --- |
| `fe190000` (pcie2x1l2) | Mini-PCIe | **链路 up，驱动 `rk-pcie`** | `lspci -D`：`0004:40:00.0` PCI bridge + **`0004:41:00.0` RTL8822CE WiFi**；`readlink /sys/bus/pci/devices/0004:41:00.0` → `platform/fe190000.pcie` |
| `fe170000` (pcie2x1l0) | M.2 E-Key | 探测失败（**空槽**） | `rk-pcie fe170000.pcie: PCIe Link Fail, LTSSM is 0x3` → `failed to initialize host` |
| `fe150000` (pcie3x4) | M.2 M-Key | 探测失败（**空槽**） | `rk-pcie fe150000.pcie: PCIe Link Fail, LTSSM is 0x0` → `failed to initialize host` |
| `fe180000` (pcie2x1l1) | 本板未使能 | — | v2 的 DT 没打开它（5io 才用它做 Mini-PCIe） |

⭐ **重要事实：这块板的 WiFi 卡插在 Mini-PCIe 槽里**（挂在 `fe190000.pcie` 之下），E-Key 是空的。
所以"把 Mini-PCIe 切成 SATA"在 v2 上等于**放弃 WiFi**；而 5io 上 Mini-PCIe 走的是另一路 PHY
（`combphy2_psu`），WiFi 插哪由现场决定 —— **换板/换底板时这条必须先确认**。

**三路 PHY 的 DT 状态都是 `okay`**（`/proc/device-tree/phy@fee00000/status` 等）——
"PHY 使能"不等于"有消费者"，真正决定绑谁的是控制器节点的 `status`。

**两条容易误判的日志**：

```
[5.389716] dw-pcie fe150000.pcie: Failed to initialize host          # ← 通用 dw 驱动先探到 Rockchip 节点上失败
[5.389736] dw-pcie: probe of fe150000.pcie failed with error -22
[5.390451] rk-pcie fe190000.pcie: no vpcie3v3 regulator found         # ← v2 的 Mini-PCIe 没写 vpcie3v3-supply，正常
[7.700795] rk-pcie fe150000.pcie: PCIe Link Fail, LTSSM is 0x0, hw_retries=0
[7.700828] rk-pcie fe150000.pcie: failed to initialize host           # ← 空槽的正常表现
```

- `dw-pcie ... invalid resource / Failed to initialize host / probe failed with error -22`：**通用**
  `pcie-designware-plat` 驱动先匹配上（`snps,dw-pcie`）并失败，随后 **`rk-pcie`**（`pcie-dw-rockchip.c`）
  接手成为真正的驱动。为什么会有这次"先失败"（探测顺序/回退机制）**没查透**，见 §7。
- `PCIe Link Fail, LTSSM is 0x0/0x3`：**插槽空着**，不是复用配错。
- ⚠️ **更正一条旧记录**：此前笔记把 `invalid resource` 记在 `fe190000`（Mini-PCIe）上，**记错了** ——
  实际是 `fe170000`（E-Key）。Mini-PCIe 是好的（WiFi 就在它上面）。

---

### 4.1 实验（2026-09-17，v2 开发板）：把 Mini-PCIe 那路切成 SATA0 → WiFi 确实消失

**做法**：把 `rk3588-lubancat-5-sata0-overlay`（`disable &pcie2x1l2` + `enable &sata0`）加进板上
`/boot/firmware/ubuntuEnv.txt` 的 `overlays=`，重启。

**板上采集到的证据**：

```
--- dmesg ---
[5.49] ahci-dwc fe210000.sata: Looking up ahci-supply from device tree   ← SATA0 控制器起来了（ahci-dwc）
[5.80] ata1: SATA link down (SStatus 0 SControl 300)                     ← 控制器 up，但没有盘
--- lspci -D ---                                                          ← 空！PCI 设备一个都没有
--- PCIe 控制器绑定 --- 只剩 fe150000 / fe170000；fe190000 不见了         ← Mini-PCIe 控制器被关掉
--- 网络 --- 只有 lo（p2p0/wlan0 消失）；/sys/class/ieee80211/ 空
--- USB 树 --- 那 4 口 USB2 hub 上的 btusb 还在（蓝牙走 USB，不受影响）
```

**结论**：`sata0` 与 `pcie2x1l2` 抢同一颗 `combphy0_ps` ⇒ 开 sata0 = 关掉 Mini-PCIe 控制器 ⇒
**插在该槽里的 WiFi 卡（PCIe）直接消失、板子失去网络**（本板管理地址走 WiFi ⇒ SSH 断开）。
以前这只是从 DTS/源码推断，现在**实测证实**。
另外 `ata1: SATA link down` 说明：即便切成 SATA，槽里插的是 PCIe 卡不是 mSATA 盘，所以**看不到盘**
（要复现 mSATA 得把盘插进该槽或换到 5io 板测 sata2）。

⚠️ 这块板**只有 WiFi 一条路**（`eth0`/`eth1` 都无 carrier），所以做这类启动配置实验**必须先装自动回滚**。

#### 可复用的"自动回滚"安全网（本次实测有效，适合所有"只有 WiFi 的板子 + 改启动配置"的实验）

1. 备份：`cp /boot/firmware/ubuntuEnv.txt /boot/firmware/ubuntuEnv.txt.bak-<实验名>`；
2. 写一个 oneshot 单元 + 脚本：开机后 ①等 45 s ②采集证据（`dmesg` 里 pcie/sata/phy、`lspci -D`、
   PCIe 控制器绑定、`lsblk`、`ip -br addr`、`nmcli`、`/sys/class/ieee80211/`、`lsusb -t`）写进
   `/root/<实验名>.log` ③再等 240 s ④`cp` 回备份、`systemctl disable` 自己、`reboot`：

   ```ini
   [Service]
   Type=oneshot
   ExecStart=/usr/local/sbin/<实验名>.sh
   TimeoutStartSec=infinity     # ⚠️ 必须！默认 90s 会把还在 sleep 的 oneshot 杀掉，安全网就废了
   [Install]
   WantedBy=multi-user.target
   ```
   脚本开头加 `grep -q '<实验用的 overlay 名>' /boot/firmware/ubuntuEnv.txt || exit 0`（幂等：
   回滚后的后续启动直接退出，不会反复重启）。
3. 改配置 → 重启 → 等。**本次时间线**：重启后 **329 s** 板子自己回来
   （≈ 启动 40 s + 等 45 s + 等 240 s + 再启动 40 s），`overlays=` 已恢复原样、WiFi 正常、
   `lspci` 里 WiFi 卡回来了。收尾记得 `systemctl disable` 并删掉单元与脚本。

#### 万一自动回滚也失效：手工恢复

- 把启动介质挂到另一台机器（或从 SD 卡启动），编辑 `ubuntuEnv.txt` 去掉那个 overlay 名，
  或直接 `cp ubuntuEnv.txt.bak-<实验名> ubuntuEnv.txt`；
- 串口：bootargs 里有 `console=ttyS2`（日志能看到），但**没有 getty**
  （`serial-getty@ttyS2` 默认 inactive）⇒ 想用串口交互得先起 getty 或先有网络；
- U-Boot 阶段：打断 autoboot，`setenv overlays ...; saveenv` 后 `boot`。
## 5. 排查清单（怎么确认某路 PHY 现在归谁）

```bash
# 1. 三路 PHY 谁绑了驱动、控制器是否起来
for d in /sys/bus/platform/devices/*.pcie; do
  printf '%-22s %s\n' "$(basename $d)" "$(readlink -f $d/driver 2>/dev/null | sed 's|.*/||' || echo 未绑定)"
done
ls -l /sys/bus/platform/drivers/rk-pcie/ /sys/bus/platform/drivers/rockchip-snps-pcie3-phy/

# 2. 槽位上到底有没有设备（域名/总线能反推控制器）
lspci -D -nn ; readlink -f /sys/bus/pci/devices/<BDF>      # 指到 platform/feXXXXXX.pcie

# 3. 内核怎么说（复用配错的典型报错）
sudo dmesg | grep -iE 'rk-pcie|dw-pcie|combphy|naneng|ahci|sata'
#   模式冲突：  "expected mode is SATA, but current mode is PCIe"（validate 失败）
#   被覆盖：    "phy type select %d overwriting type %d"（后者赢，需查 DT）

# 4. DT 里到底开了什么（运行中的真值）
for c in fe150000 fe170000 fe180000 fe190000; do
  printf '%-10s %s\n' $c "$(tr -d '\0' </proc/device-tree/pcie@$c/status 2>/dev/null)"
done
for p in fee00000 fee10000 fee20000; do printf '%-10s %s\n' $p "$(tr -d '\0' </proc/device-tree/phy@$p/status)"; done
tr -d '\0' </proc/device-tree/phy@fee80000/rockchip,pcie30-phymode | od -An -tu4   # 4=AGGREGATION, 0=NANBNB
```

---

## 6. 对自研底板的意义（⚠️ 动手前先定这个）

1. **一颗 PHY 一个用途**：三个 combo PHY + 一个专用 PCIe3 PHY，能接的东西总数是固定的。
   先列清单（要几个 PCIe 槽 / 要不要 SATA / 要不要 USB3 HOST2），再决定怎么分配 ——
   这块"资源账"要在画板之前算清，改板之后只能靠 overlay 妥协。
2. **别照抄 v2 的 Mini-PCIe 归属**：v2 与 5io 已经证明同一个物理槽可以挂不同 PHY/控制器
   （§2），自研底板要明确写自己那份，不要复制粘贴。
3. **SATA 能不能用不受内核配置限制**（`CONFIG_AHCI_DWC=y` 已开），但要给对 3.3 V 供电
   （overlay 里那行 `vpcie3v3-supply`）—— 槽位供电是底板设计的一部分。
4. **WiFi/模组插在哪要写进板卡文档**：本板 WiFi 在 Mini-PCIe 上，一旦那路切走就没有无线，
   现场会表现为"设备失联"（我们的设备管理地址就走 WiFi，见 AGENTS §6 的 `p2p0`）。
5. 与 `derived-board-bringup.md` 的对应关系：那份清单里的"存储/扩展：NVMe、SATA2、Mini-PCIe、
   PCIe-64P"这几项，实际都由本文这三路 PHY 决定，两边要一起看。

---

## 7. 未决 / 待办

1. **`dw-pcie` 与 `rk-pcie` 双驱动探测的机制没查透**（为什么通用 DW 驱动会先匹配并失败、随后 Rockchip 驱动接手；
   `pcie-dw-rockchip.c` 的 of_match 里 `rockchip,rk3588-pcie` 是 `.data = NULL`，值得顺着看 probe 路径）。
2. **6.1.118 → 6.1.172 这段有没有行为变化**：没有回看 6.1.118 的 PCIe 日志（升级前没留）。
3. **插卡实测缺**：E-Key / M-Key 现在只有"空槽 Link Fail"，没有插卡成功/失败的实测记录
   （需要硬件配合，且动板子要用户同意）。
4. **5io 上的 mSATA（`sata2-overlay`）没实测过**：手里只有 v2 板；文档里 5io 部分全部是源码核对，不是实测。
5. 三路 PHY 驱动还支持 **SGMII/QSGMII**（`phy-rockchip-naneng-combphy.c`），本套 RK3588 DTS 里没有消费者 ——
   自研底板若考虑用这些 PHY 走以太网要另查（`combphy2_psu` 的 "u" 就是 USB，别想当然）。

---

## 8. 相关文档

- 板级资源清单（v2）：[lubancat-5-v2-hardware.md](lubancat-5-v2-hardware.md)
- 自研底板派生清单（要动哪些文件 / dts 怎么拆）：[../build/derived-board-bringup.md](../build/derived-board-bringup.md)
- 启动链路与 FIT/ATF：[rk3588-boot-flow.md](rk3588-boot-flow.md)、[boot-serial-log.md](boot-serial-log.md)
- 各硬件模块占用率怎么看：[rk3588-hw-monitor.md](rk3588-hw-monitor.md)
