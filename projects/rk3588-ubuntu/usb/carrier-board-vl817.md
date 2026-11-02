# 自研底板：VL817 做 hub 的上游接法（评审 + 最终决策）

> 最终采用"**分家接法**"：VL817 的 USB2 上游接 `USB20_HOST0`（u2phy2/HOST0）、
> USB3 上游接 `USB30_SSRX/TX`（combphy2_psu = `usbhost3_0` / Controller2）。
> 代价是"同一个下游口有两条 USB 路径"（口身份随速率变），**用户已明确接受**。

## TL;DR

- 决策（2026-09-17 用户拍板）：分家接法；J16A 另做一个 Type-C 或全功能口。
- ⛔ **收发交叉别接错**：hub 的 `SSRX±`（输入）要接主机的 **TX**、`SSTX±`（输出）接主机的 **RX**。
- 代价与对策：下游第 n 口在 USB3 落 `platform-xhci-hcd.N.auto-usb-0:1.n`、USB2 落
  `platform-fc800000.usb-usb-0:1.n` ⇒ 按口绑定**一条带 `|` 的规则**就够（板载实测过）。
- ⚠️ 用了 Controller2 就必须让 `combphy2_psu` 归 `usbhost3_0`，且**不能开 `sata0` overlay**
  （那块 Mini-PCIe 槽的根因见 [../hardware/combphy-mux.md](../hardware/combphy-mux.md)）。
- 两个"干净方案"（4 口都是完整 USB3、口身份与速率无关）**未采纳**，保留作对照 ——
  前提是先定死"下游需不需要 SuperSpeed"。
- 现场普查（2026-09-17 工程机）：现有设备**全是 USB2**，SS 通路从未被用过。

> 需求（用户口述）：自研底板用 **VL817**（VIA Labs USB3.1 Gen1 4 口 hub）做主 hub，
> 打算 **VL817 的 USB2.0 UFP ← `USB20_HOST0_DP/DM`（u2phy2 / HOST0）**、
> **VL817 的 USB3 上游 ← `USB30_SSRXN/P` + `USB30_SSTXN/P`（combphy2_psu / `usbhost3_0` = Controller2）**。
> 手册已入库：`hardware-docs/modules/VL817-Q7.PDF`。

## ✅ 最终决策（2026-09-17，用户拍板）：**分家接法**，即本文档评审的那一版

> 用户原话：「我们已经决定了 VL817 接 `USB20_HOST0_DM` `USB30_SSRXN` 这两组控制器。
> J16A 可以单独做一个 type-c 口或者全功能来用。」
> ⇒ **采用最初的"分家"接法**（不是本文档的方案 A / B）；本文档列出的代价用户明确表示**可以接受**
> （「同一个设备如果要按 usb 路径绑定的话，要写两组 udev rule 对吧」→ 已实测一条带 `|` 的规则就够，本文档）。

| VL817 引脚 | 接到哪里 | 对应 DTS 节点 |
| --- | --- | --- |
| `HSD0±`（USB2 UFP） | **`USB20_HOST0_DP/DM`** | `usb_host0` / `u2phy2`（480M 根总线） |
| `SSRX0±`（hub 收） | **`USB30_SSTXN/P`** | `usbhost3_0` / `combphy2_psu`（Controller2，5000M 根总线） |
| `SSTX0±`（hub 发） | **`USB30_SSRXN/P`** | 同上 |
| `VBUSDET` / `EXTPWRON` | 上游 VBUS 检测 / 自供电标志 | 见本文档|

⛔ **收发交叉别接错**：hub 的 `SSRX0±`（输入）要接主机的 **TX**、hub 的 `SSTX0±`（输出）接主机的 **RX**。
AC 耦合电容按 VIA 参考设计放在**发送端**。

**J16A（OTG1 / `TYPEC1_*`）= 独立保留，类型未定**（2026-09-17 用户：「J16A 还没决定做成什么类型」）。
因为 VL817 不再占用 OTG1，J16A 拿到了完整的双通路控制器（`usbdrd3_1` = `u2phy1_otg` + `usbdp_phy1_u3`），
选项：纯 host 的 USB3-A 口（同 v2/5io 现状，最省事）／全功能 Type-C（需 TCPC + role-switch + 可选 DP alt，
参考 v2 的 OTG0/`fusb302`）／USB2 口／不用。**别按某一种推进，先等用户定。**

**连带影响（要一起定的）**：
1. ⭐ **hub 的 SS/HS 两半会分别枚举在两个控制器上** ⇒ 4 个下游物理口，每个都有**两条 `ID_PATH`**：
   USB3 设备落 `platform-xhci-hcd.N.auto-usb-0:1.<口>`、USB2 设备落 `platform-fc800000.usb-usb-0:1.<口>`
   （`N` 会变，别写死；`fc800000.usb` 是 `usb_host0`）。**按口绑 udev 写一条、两路径用 `|` 隔开**
   （本文档已用 `udevadm test` 实测可行）。`tools/lsp-usb-map.sh` 这类按口归类的工具会把一个物理口报成两个。
2. **`USB20_HOST0`（u2phy2）被 hub 的 HS 半占用** ⇒ 底板上不再有"HOST0 的普通 USB2-A 口"。
3. **`combphy2_psu` 归了 `usbhost3_0`（USB3）** ⇒ ✅ **自研底板的 combphy 三路照 v2 做**
   （用户 2026-09-17 确认）：`combphy0`→`pcie2x1l2`(Mini-PCIe)、`combphy1`→`pcie2x1l0`(E-Key)、
   `combphy2`→`usbhost3_0`（本 hub 的 SS 半），`pcie30phy` 照 v2 的 mode 4 ⇒ **v2 那套 combphy/PCIe
   DTS 可整段照抄**；⛔ 因此**不能开 `sata0` overlay**（`combphy0` 上的 `pcie2x1l2` 就是 Mini-PCIe
   槽里 WiFi 卡的控制器，[combphy-mux-usbhost3-hub.md](../hardware/combphy-mux-usbhost3-hub.md)）。
   ⚠️ 未定项：两个槽的 USB D+/D- 接哪（`USB20_HOST0` 已被 HS 半占用，只剩 `USB20_HOST1`）。
4. **OTG1（`TYPEC1_*`）空出来**给 J16A；**`USB20_HOST1`（`u2phy3`）本来就空着**，可做一路普通 USB2-A
   或第二颗 USB2 hub（v2/5io 用它挂板载 CH334F/FE1.1s）。
5. **DTS 侧起点就是 v2 的 USB 配置**：v2 的 J16B 本来就是"SS 走 `usbhost3_0` + HS 借 `USB20_HOST0`"这个接法，
   自研底板只是把 J16B 那个连接器换成 VL817 ⇒ `usbhost3_0` / `usb_host0` 使能方式可直接照抄 v2，
   **不需要重新分配 PHY**；`usbdrd3_1` 按 J16A 最终类型再定（host / otg+role-switch）。
6. **按口上下电不受分家影响**：VL817 是一颗芯片，4 个下游口共享 `USBPE1~4`/`USBOC1~4`（本文档），
   仍是**按物理口**的开关与过流；VL817 还比 v2/5io 的板载 hub 多一个 `RESET#`（pin 24）可以接 GPIO
   （v2/5io 的 hub 都不可软复位、过流也不进 SoC）。
7. **不会重复枚举**：SS 链路谈成的设备不会再拉 D+，只出现在 SS 那棵树；USB3 设备若 SS 谈不成
   （线材/连接器差）会按规范**回落到 HS 那棵树** ⇒ 同一设备同一根线接触不良就"换树"，是偶发故障的来源。
8. **FS/LS 设备（ESP32 12M、键鼠）留在 EHCI 那棵**（hub 做 TT），不会跑到 OHCI。

## 评审意见：⛔ 这个接法**不建议**（等于把 [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md) 的「拼接口」放大成 4 个口）
> 📌 下面是**评审时的分析**，结论已被本文档的用户决策覆盖（用户明确接受这些代价）——
> 留在这里是为了"分家之后会看到什么"这份对照表，不是"还没有决定"。

**VL817 的 UFP 是一个口，两半不能分家**。手册引脚表（p7/p12）写得很清楚：

| UFP 引脚 | 方向 | 说明 |
| --- | --- | --- |
| `SSTX0±` | O | USB 3.1 UFP Differential TX |
| `SSRX0±` | I | USB 3.1 UFP Differential RX |
| `HSD0±` | I/O | **USB 2.0** UFP Differential D+/D- |
| `VBUSDET` | I | UFP VBUS Detection（≤3.3V） |

p4 系统概览也点明它是「**integrated USB 2.0 hub**」+ USB3 hub 两半（"any combination of SuperSpeed /
High Speed / Full Speed / Low Speed devices"、"the integrated USB 2.0 hub's MTT"）。
按你的接法，**同一个上游口的两半落在两个不同的 xHCI 上**：

- SS 半 → `usbhost3_0`（Controller2，USB3-only 控制器，自己的 xHCI/5000M 总线）
- HS 半 → `usb_host0`（`u2phy2`，EHCI/OHCI，480M/12M 总线）

**功能上大概率能跑**（两半各自是个正常的 hub，各自枚举；USB3 设备走 SS 那棵、USB2 设备走 HS 那棵；
USB3 设备 SS 链路失败会按规范回落到 HS）。我查过 `kernel-6.1/drivers/usb/core/hub.c`，
**内核并没有"USB3 hub ↔ 它的 USB2 半"的配对逻辑**（`grep companion` 只命中"伴生控制器"=EHCI/OHCI 的注释），
所以不会因为分家而枚举失败。**但代价是拓扑被拆成两棵互不相关的树**：

| 后果 | 说明 |
| --- | --- |
| 同一个物理口有**两条 USB 路径** | 下游第 n 口：USB3 设备落 `platform-xhci-hcd.N.auto-usb-0:1.n`，USB2 设备落 `platform-fc800000.usb-usb-0:1.n` ⇒ **口身份随速率变**（与 v2 蓝色上层 J16B 完全同病，[topology-lubancat-5-v2.md](topology-lubancat-5-v2.md)） |
| `lsusb` 里是**两个 hub 设备** | 正常 USB3 hub 本来也会显示两半（USB3 hub + USB2 hub），但正常设计里两半在**同一个控制器的两条根总线上**，是可关联的一对；分家后「同口」这件事在软件里再也表达不出来 |
| 按口绑 udev 规则 / 每口上下电 / 拓扑诊断 | 全部要**成对处理**；`tools/lsp-usb-map.sh` 这种按口归类的工具会把一个物理口报成两个 |
| 排查成本 | 现场一根线/一个口出问题时，要看两棵树的日志 |

**分家之后长成什么样**（`lsusb -t`；"SS 半""HS 半"是 VL817 内部的两个 hub 设备）：

```
/:  Bus 00X.Port 001: Dev 001, Class=root_hub, Driver=xhci-hcd/1p, 5000M      ← Controller2（usbhost3_0）
    |__ Port 001: Dev 002, Class=Hub,   Driver=hub/4p, 5000M                  ← VL817 的 USB3 半
        |__ Port 001..004: <插 USB3 设备时落这里>
/:  Bus 00Y.Port 001: Dev 001, Class=root_hub, Driver=ehci-platform/1p, 480M   ← HOST0（u2phy2）
    |__ Port 001: Dev 002, Class=Hub,   Driver=hub/4p, 480M                   ← VL817 的 USB2 半
        |__ Port 001..004: <插 USB2 设备时落这里>
```

- **同一个物理口 → 两条可能的 `ID_PATH`**：USB3 设备 `platform-xhci-hcd.N.auto-usb-0:1.<口>`；
  USB2 设备 `platform-fc800000.usb-usb-0:1.<口>`（`N` 是**会变**的 xHCI 实例号，别写死，见 [device-mapping-method.md](device-mapping-method.md)）。
- **不会重复枚举**：SS 链路建立成功的设备**不会**再拉 D+，所以只出现在 SS 那棵树里；反之 USB2 设备
  也只在 HS 树里。**但如果 SS 链路没谈成（线材/连接器差），USB3 设备会按规范回落到 HS** ⇒
  同一个设备、同一根线稍微接触不良就**换一棵树**，这是"偶发"类故障的来源。
- **HS 树只有一棵**：下游的 FS/LS 设备（ESP32 12M、键鼠）也挂在 **EHCI** 那棵（hub 做 TT），
  不会跑到 OHCI 去 —— v2 上实测的 `USB2HOST1-EHCI` + `6-1.4.x` 就是这个形态。
- **每棵树内部仍能区分是哪个口**（端口号 1~4），丢的是"**一个物理口只有一个标识**"这件事。
- **VBUS/过流仍是按物理口**：VL817 是一颗芯片，两个 hub 半共享同一套 `USBPE1~4`/`USBOC1~4`
  （见本文档），所以"按口上下电"的能力不受分家影响。

## 两个干净的方案（先定一件事：**下游需不需要 SuperSpeed**）

> ⛔ **未被采纳**（2026-09-17 用户选了本文档的分家接法）。以下保留作对照与"万一要回头"的备选：
> 若将来发现两棵树的代价不可接受，方案 B 是唯一能"4 个口都是完整 USB3 且口身份与速率无关"的接法。

**先看事实**：现场的 USB 设备——3 路 UVC 相机（480M HS）、3 块 ESP32（12M FS）、CSR 蓝牙从机（12M）、
键鼠（1.5M/12M）—— **全是 USB2**；唯一见过的高速设备是拿来测的 USB3 硬盘盒。

**方案 A：不需要下游 SS（推荐给现在的现场硬件）** —— 只接 HS，SS 两对**悬空不接**
- VL817：`HSD0±` ← `USB20_HOST0_DP/DM`；`SSTX0±`/`SSRX0±` 不接（或按参考设计端接）。
- 得到：**一个干净的 4 口 USB2 hub**（单控制器、单棵树、每口一条 `ID_PATH`、可挂任意 USB2 设备）；
  插 USB3 设备会按规范回落到 HS（480M）。
- 代价：下游没有 SS（5 Gbps）；`USB20_HOST0` 被占用。
- 🔬 **现在就能验证**：拿一颗市售 VL817 hub（多数 USB3 hub 都是 VL817），**把它的上游插到一个 USB2 口上**
  （上游插头只跑 D+/D-）⇒ 应当枚举成 4 口 USB2 hub、且插 USB3 设备回落 HS。
  这条实验不需要等自研板，能直接证明"只用 HSD0"这条路可用。

**方案 B：需要下游 SS** —— VL817 的 **UFP 整个（SS+HS）挂 `usbdrd3_1`（OTG1 / `TYPEC1`）**
- 即把 5io 上 `J9A` 那个位置让给 hub（或 v2 上 `J16A`）：`USB30_DM/DP`(=OTG1 的 USB2) + `USB30_SSRX/SSTX`
  本来就同属一个 xHCI ⇒ hub 上下游都在一个控制器里，**4 个下游口每个都是完整 USB3 口**，
  口身份与速率无关，也才能按口做 udev/上下电。
- Controller2（`usbhost3_0`）这时**空出来**：要么不用，要么当**纯 SS 口**给固定设备（例如内部 USB3 SSD），
  ⛔ 但**不能再用它当任何 hub 的上游**（同样的分家问题）。
- ⚠️ PHY 冲突：用 Controller2 就得让 `combphy2_psu` 归 `usbhost3_0`（USB3），
  那就**不能同时**给 Mini-PCIe 做 PCIe —— 5io 现在正是把 combphy2 给了 `pcie2x1l1`（Mini-PCIe）。
  这块要在自研底板上一开始就定死（见 [combphy-mux.md](../hardware/combphy-mux.md) §0 与 [combphy-mux-usbhost3-hub.md](../hardware/combphy-mux-usbhost3-hub.md)）。

## 分家之后按口绑 udev：**可以只写一条**（2026-09-17 板载实测）

在工程机上用 `/run/udev/rules.d/` 探针 + `udevadm test` 实测（左手相机 `video3` 的真实
`ID_PATH` = `platform-fc800000.usb-usb-0:1:1.0`）：

| 探针写法 | video3 是否命中 | 结论 |
| --- | --- | --- |
| `ENV{ID_PATH}=="<真实路径>\|<假路径>"` | ✅ | **`\|` 二选一支持**（备选放前面也行） |
| `ENV{ID_PATH}=="<假路径>\|<真实路径>"` | ✅ | 备选放后面同样成立 ⇒ **一条规则能写两组路径** |
| `ENV{ID_PATH}=="platform-fc800000.usb-usb-0:1:*"` | ✅ | `ENV{}` 里的 `*` 通配有效（可用来兜掉会变的 xHCI 实例号） |
| `KERNELS=="*-1:1.0"` | ✅（但**右手的 video1 也命中**） | 按端口名匹配与速率/控制器无关，但**粒度太粗会误伤**（见下） |

⇒ 实操建议：

1. **同一个口要"不管插什么速率都绑同一个名字"** → 写**一条**、两条路径用 `|` 隔开：
   ```ini
   # 例：hub 的 3 号口，SS 走 Controller2、HS 走 HOST0
   SUBSYSTEM=="video4linux", ENV{ID_PATH}=="platform-xhci-hcd.*.auto-usb-0:1.3:1.0|platform-fc800000.usb-usb-0:1.3:1.0", \
     ENV{ID_V4L_CAPABILITIES}=="*:capture:*", SYMLINK+="camera_x", GROUP="video", MODE="0660"
   ```
   ⚠️ SS 那半的 `xhci-hcd.`**`N`**`.auto` 里的 `N` 会随内核变（[device-mapping-method.md](device-mapping-method.md)）⇒ 用 `*` 通配。
   同一个设备**只会落在其中一棵树**（不会同时匹配两条），所以 `|` 不会产生双符号链接。
2. **设备固定只跑一种速率**（现场那批相机/ESP32/蓝牙从来是 USB2）→ **一条路径就够**，
   连 `|` 都不用写；只有"USB3 设备可能因线材回落"或"口上插什么都要同一个名字"才需要两组。
3. **按端口名匹配**（`KERNELS=="*-1.3:1.0"`）也能横跨速率与控制器，但有坑：
   `KERNELS` 是**沿父链**匹配 ⇒ 该口后面再挂一级 hub 时，**孙设备也会命中**
   （`5-1.3.2` 的父是 `5-1.3`）。上面实测里 `*-1:1.0` 把两个控制器上的相机**都**命中了，
   就是这个"粒度"问题的体现。要用就把端口写全（`*-1.3:1.0`），并且心里清楚孙设备的误匹配。

## 📌 现场 USB 版本普查：**全是 USB2**，SS 通路从未被用过（2026-09-17 工程机实测）

**判据**：`/sys/bus/usb/devices/<dev>/version`（= 设备描述符里的 `bcdUSB`，即**设备声称支持的 USB 版本**）
与 `lsusb -v` 的 `bcdUSB` 交叉核对；再用"设备是否出现在 5000M 根总线下"验证。

| 设备 | 速率 | `version`(bcdUSB) | 说明 |
| --- | --- | --- | --- |
| 左/右手相机 `0abd:8050` Xitech | 480M | **2.00** | USB2 设备（不是"USB3 相机被回落"） |
| 胸部相机 `05a3:9230` LRCP | 480M | **2.00** | 同上，且挂在 USB2 hub 后面 |
| 板载 hub `1a86:8091` CH334F | 480M | **2.00** | USB2 hub |
| 绿联 hub `05e3:0610` | 480M | **2.10** | USB2.1 hub |
| 板载蓝牙 `0bda:c822` RTL8822CE | 12M | **1.00** | USB1.x 全速 |
| 三块 ESP32 `303a:1001` | 12M | **2.00** | 全速 |
| CSR 蓝牙从机 `0a12:0001` | 12M | **2.00** | 全速 |

**没有任何一个设备的 `version` 是 3.00** ⇒ 现场不存在 USB3 设备，也就不存在"USB3 被降到 480M"。
反向验证：三条 5000M 根总线（`usb2`=Controller2 / `usb8`=OTG1 / `usb10`=TypeC）**只有根 hub 自己，
一个子设备都没有** ⇒ **SS 通路在工程机上从未被使用过**。
（对照：开发板上插过一颗 USB3 硬盘盒 `1f75:0903`，实测 5000M、落在 `USB30HOST2`/J16B-SS ⇒
硬件通路是好的，只是**现场没有 USB3 设备**。）

**对自研底板的意义**：
- **如果只服务现场设备，"只接 `HSD0±`、SS 悬空"就完全够用**，还能省掉 4 对 SS 差分走线 + AC 耦合电容，
  **`combphy2_psu` 也不必给 `usbhost3_0`** ⇒ 可以像 5io 那样留给 Mini-PCIe 的 PCIe（或别的用途）。
  ⚠️ **但用户 2026-09-17 决定"将来可能接 USB3 设备" ⇒ SS 照接**（本文档分家接法），所以这条没被采纳。
- **带宽账（只走 HS 时）**：4 个下游口共享 480 Mbps（≈40 MB/s 有效），现场三路 720p30 MJPEG ≈ **5.4 MB/s** +
  ESP32/蓝牙那点流量 ⇒ 余量很大；SS 的唯一价值是"给未来留口子"。
- ⛔ **要用 USB3 设备就必须把 SS 也接上** —— 自研底板已按此决定（本文档分家接法；
  另一条干净路线是把 VL817 的整个 UFP 挂 OTG1，见本文档方案 B，未被采纳）。

## 手册里对自研底板有用的几条（VL817）

- **`RESET#`（pin 24/...) 存在** ⇒ **可以接 GPIO 做软件复位**（v2 的 CH334F、5io 的 FE1.1s 都做不到，
  是已知痛点）。建议留一个 GPIO 并留测试点。
- **每口电源/过流**：`USBPE1~USBPE4`（power enable 输出）+ `USBOC1~USBOC4`（over-current 输入），
  且特性页明确 **"Supports both Individual and Ganged Mode Operation for All Ports"**
  ⇒ 能做到**每口独立限流/上下电**（要外接 VBUS 开关 + 每路 OC 采样），或整组 gang。
  想要"给某个口断电复位外设"就配成 individual。
- **`VBUSDET`（≤3.3V）**：上游 VBUS 检测；自供电时还要处理 **`EXTPWRON`（High = 外部电源接入）**。
  注意上游 VBUS 由谁控制：v2 是常开、5io 是按组可控（[topology-lubancat-5io.md](topology-lubancat-5io.md)）。
- **电源**：内部有 5V→1.1V DC-DC 与 5V→3.3V LDO（可只用 5V 输入）；**需要 25MHz 晶振**
  （别和 CH334F 的 12MHz 搞混）。
- **可选 SPI flash**：`USBLED1~4` 与 `SPISI/SPICLK/SPISO/SPICS` 复用 —— 要 BC1.2 充电、
  自定义 GPIO 行为、HID/Billboard 虚拟设备就得挂 flash（否则走默认引脚配置）。

---
