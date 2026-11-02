# lubancat-5-v2：USB 物理口 ↔ 控制器映射（含工程机接线实测）

> v2 的四个 USB-A 口到内核控制器的对应关系（含蓝色口的 USB2/USB3 归属、两个白口背后是 **CH334F hub**），
> 以及一个必须记住的坑：**同一个物理口在 USB2/USB3 下走的是两棵不同的树**（"拼接口"），
> 口身份随速率变 —— 根因在 SoC，不在板厂。

## TL;DR

- 工程机（LSP 镜像从 SD 卡启动）实测：**7 个符号链接全对**，采集功能（`v4l2-ctl`/`ffmpeg`）正常。
- 蓝色双层口 = **J16A（下层）/ J16B（上层）**（实测确定；⛔ 别按 `A/B` 推上下，v2 的 J17 是反的）。
- 白色双层那两个口**背后是板载 hub CH334F**；蓝色口是 SoC 直出的两个控制器。
- ⛔ **"拼接口"**：蓝口插 USB2 设备落 `fc800000.usb-…`、插 USB3 设备落 `xhci-hcd.N.auto-…`
  ⇒ 按口绑定时要意识到"口身份依赖速率"。
- ⛔ `xhci-hcd.N.auto` 的 `N` 会随内核变（见 [device-mapping-method.md](device-mapping-method.md)），规则用通配。
- 工程机上还有**两路蓝牙**：`hci0` 板载、`hci1` 在绿联 hub 上（排查"蓝牙串口"时别搞混）。
- 工具：`tools/lsp-usb-map.sh` 一条命令就能按"控制器 + 物理口"列出全部设备。

## 实测：工程机上 7 个符号链接全对（2026-09-12，LSP 镜像从 SD 卡启动）

工程机（`<HOST_09>`，全部外设接好、跑的是这张启动 SD 卡上的 LSP 系统，eMMC 未挂载）上
把五个用户规则的产物逐个核对，**7/7 都生成了，且指向的节点与规则设计一致**：

| 符号链接 | → 节点 | VID:PID | 区分依据 | 实际值 |
| --- | --- | --- | --- | --- |
| `/dev/chest_cam_video` | `/dev/video5` | `05a3:9230` | `ATTR{index}=="0"` + `:capture:` | `ID_PATH=platform-fc880000.usb-usb-0:1.3:1.0` |
| `/dev/chest_cam_metadata` | `/dev/video6` | `05a3:9230` | `ATTR{index}=="1"` | 同上（`ID_V4L_CAPABILITIES` 为空 ⇒ 非采集节点） |
| `/dev/camera_left` | `/dev/video3` | `0abd:8050` | `ID_PATH` = USB2 控制器 | `platform-fc800000.usb-usb-0:1:1.0` ✓ 规则里写的就是这条 |
| `/dev/camera_right` | `/dev/video1` | `0abd:8050` | `ID_PATH` = USB3 控制器 | `platform-xhci-hcd.11.auto-usb-0:1:1.0` ✓ |
| `/dev/lcd_voice_bridge` | `/dev/ttyACM0` | `303a:1001` | `ID_SERIAL_SHORT`（MAC） | `02:00:00:00:00:03` |
| `/dev/glove_left` | `/dev/ttyACM1` | `303a:1001` | 同上 | `02:00:00:00:00:01` |
| `/dev/glove_right` | `/dev/ttyACM2` | `303a:1001` | 同上 | `02:00:00:00:00:02` |

三块 ESP32-S3 的 `ID_PATH` 分别是 `…-0:1.4.1:1.0` / `…-0:1.4.4:1.0` / `…-0:1.4.3:1.0`
—— **同一个 hub（`6-1.4`）的三个口**，所以只能靠 MAC 区分，规则写的就是 MAC。

⚠️ **上表 `camera_right` 那行的 `xhci-hcd.11.auto` 只对当天那套内核成立**（工程机 kernel 6.1.118）。
同一个控制器在 6.1.172 上叫 `xhci-hcd.4.auto` —— 见 [device-mapping-method.md](device-mapping-method.md)，这条直接影响用户手写的右手相机规则。

## 采集功能实测（`v4l2-ctl` / `ffmpeg`）

三路相机都能出图（各抓 3 帧原始 MJPEG 均非空，再各转 1 张 PNG 用 `cv2` 统计）：

| 设备 | 分辨率 | 亮度 B/G/R | 最大像素 | 结论 |
| --- | --- | --- | --- | --- |
| `/dev/chest_cam_video` | 1920×1080 | 39.9 / 47.0 / **88.3** | 255 | 正常（暖色实景） |
| `/dev/camera_right` | 1920×1080 | 47.8 / **156.2** / 132.9 | 255 | 正常（绿色实景） |
| `/dev/camera_left` | 1920×1080 | 131 左右 | 255 | 正常（复测见下） |

**`camera_left` 的"全黑"排查记录**（第一次测试抓到全黑，复测正常，**当时的"镜头没光"结论是错的**）：

| 时刻 | 命令 | 结果 |
| --- | --- | --- |
| 17:19 首测 | `v4l2-ctl --stream-count=3 --stream-to` | 3 帧只有 139248 字节 = **46 KB/帧**；正常 YUYV 1920×1080 应是 4147200 字节/帧 |
| 17:19 | `ffmpeg -f v4l2 -i /dev/camera_left`（默认 YUYV 1080p） | 均值 0.33 / 最大像素 1（全黑），连抓 30 帧仍全黑 |
| 17:19 | 同上 + `--set-ctrl=brightness=64,auto_exposure=1,exposure_time_absolute=10000` | 均值立刻升到 **173** ⇒ 传感器与增益通路都好使（测完已还原） |
| 17:22 | 同一分钟内 `-input_format mjpeg` 1920×1080 | 均值 **231（正常画面）** ⇒ 排除"场景没光" |
| 17:26 起复测 | 最初那条命令**原样**跑 5 次 | 5/5 正常，均值 130.7~131.3 |
| 17:26 起复测 | `v4l2-ctl --stream-count=3` | 12441600 字节 = 3 × 4147200（真 YUYV 数据） |
| 17:26 起复测 | 8 轮 ×（YUYV 30 帧 + MJPG 30 帧），YUYV 首帧略亮(≈178)后稳定 | 全部非黑，最暗 129.6 |

⇒ 那批黑帧是**瞬时的流状态异常**（数据本身就不完整：46 KB/帧），**不是光照、不是规则、不是相机坏**，
之后完全复现不出来。判据：**同一时刻 MJPG 出图正常而 YUYV 全黑** → 与"镜头被挡"矛盾。
若再遇到：换 `-input_format mjpeg` 或重开一次流（`ffmpeg` 重跑 / 重新枚举）。

⚠️ 这条相机挂在 **USB2 控制器**（`fc800000.usb`，EHCI）上，1920×1080 的**未压缩 YUYV**
（4.15 MB/帧、5 fps）本来就贴着 USB2 等时带宽上限；**应用里固定用 MJPG** 最稳。
用户自己的 `triple_camera_mjpeg_publisher` 请求的就是 `image/jpeg` 1280×720@30，不受影响。

> 另外：相机（uvcvideo）默认 2 秒空闲就被 **USB 自动挂起**，重新出流会有几秒异常 ——
> 现在整机已关闭自动挂起，见 [usb-autosuspend-off.md](../system/tuning/usb-autosuspend-off.md)。


## 顺带核对的系统侧项（同一台机器）

- 7 个符号链接全部生成（上表）；`lsusb -t` 拓扑与 [device-mapping-method.md](device-mapping-method.md) 里的控制器↔总线号表一致；
- `adb devices -l` → `EXAMPLE_ID_01 device … Quest_3`；
- Type-C：`data_role=[host]`、`power_role=[source]`、`gpio vbus-typec=out hi`、
  `vbus_typec num_users=1`，Quest 侧 `AC powered: true` / `status: 2`（`lsp-usb-powerfix` 生效）；
- 首启项：`/swapfile` 2 G（prio 0）+ `vm.swappiness=10`、根分区 939 G（`lsp-growroot` 已跑）、
  `fstrim.timer` enabled/active；唯一 failed 单元是 `NetworkManager-wait-online.service`（开机无网，正常）。

---

## 实测：lubancat-5-v2 板上 4 个 USB-A 口 ↔ 内核控制器映射（2026-09-17）

> ⚠️ **本文档～本文档里写的 `bus N` 都只是"当时那一次启动的观测值"**，用来做证据；
> **BUS 号不稳定、不作为标识**。稳定标识（`ID_PATH` / platform 路径 / 控制器名）见 **本文档**。

**一键工具**：本节这套判断已固化成 **`tools/lsp-usb-map.sh`**，无需 root，默认只输出设备表：

```bash
tools/lsp-usb-map.sh              # 所有 USB 设备 → 控制器 + 物理口
tools/lsp-usb-map.sh -v           # 增加控制器一览与每台设备的 ID_PATH
tools/lsp-usb-map.sh --json       # 给脚本用
tools/lsp-usb-map.sh Hub.P1       # 按物理口 / VID:PID / sysfs 名过滤
```

映射表（`J16A` / `J16B-SS` / `J16B-HS` / `Hub.P1~P4` / `TypeC`）**就内置在脚本里**，
和本文档完全一致；**退出码 2 = 有设备落在未知控制器上**，说明该更新映射表了。
下面几节是这张表的推导过程（证据），脚本是它的执行形态。

**方法**（可复用）：在板子上挂一个后台记录器，一边跑
`udevadm monitor --udev --subsystem-match=usb --property`（拿 `ID_PATH`），一边每 2 s 快照
`lsusb -t`（拓扑变化才记），然后**依次**把同一个 USB2 设备插进每一个口（每次插上停 3~5 s 再拔）。
`ID_PATH` 把"物理口"和"控制器"直接串起来了（形如 `platform-<控制器>-usb-0:<端口链>`）。

**测试设备**：`2e8a:0001 embeddedboys Pico USB Display`（**全速 12 Mbps**、vendor-specific class）。
4 次插拔产生 **4 组 add/remove**，顺序与人工顺序一一对应：

| 人工顺序 / 物理口 | 事件顺序 | `ID_PATH` | 总线 / 驱动 | 归属的内核控制器 |
| --- | --- | --- | --- | --- |
| ① 蓝色·**下层** | add #1 | `platform-xhci-hcd.4.auto-usb-0:1` | bus 6 / `xhci-hcd` 480M | **`usbdrd3_1`（USB3OTG1）的 USB2 侧**（`u2phy1_otg`） |
| ② 蓝色·**上层** | add #2 | `platform-fc840000.usb-usb-0:1` | bus 4 / `ohci-platform` 12M | **`usb_host0`（USB2 HOST0，`u2phy2`）** |
| ③ 白色·**下层** | add #3 | `platform-fc880000.usb-usb-0:1.4` | bus 5 / `ehci-platform` + **板载 hub 的端口 4** | **`usb_host1`（USB2 HOST1，`u2phy3`）→ 板载 4 口 hub** |
| ④ 白色·**上层** | add #4 | `platform-fc880000.usb-usb-0:1.3` | 同上，**hub 的端口 3** | 同上 |

**结论**：

1. **4 个 A 口全都能认 USB2 设备**（4/4 都有 add 事件），说明这 4 个口的 USB2 通路都挂在
   **有 USB2 PHY 的控制器**上（OTG1 / HOST0 / HOST1）。
2. ⭐ **没有任何一个 A 口映射到 `usbhost3_0`（USB3OTG2）**。如果某个口的 HS 走 OTG2，
   插 USB2 设备应当**毫无反应**；4 个口都有反应 ⇒ 与"**OTG2 是 USB3-only**"的判断一致，
   而且它至少不负责这 4 个口的 USB2。
3. **蓝色双层并不是"两颗 USB3"**：下层是真正的 USB3 控制器 OTG1（SS + HS 都在），
   上层的 USB2 侧却在 **USB2 HOST0** 上（所以插 USB2 设备会以全速枚举在 `ohci-platform` 上）。
4. **白色双层是挂在板载 4 口 USB2 hub 后面**的（hub 又挂在 `usb_host1`/`u2phy3` 上，端口 3/4 就是这两个白色口）
   —— 这就是"控制器 → hub → 多口"结构的**现成先例**（与自研底板想做的 hub 方案同构）。
5. 想确认"蓝色口的 **SS** 到底归 OTG1 还是 OTG2"（即 OTG2 的物理出口在哪），需要插一颗
   **USB3 设备**：USB3 设备会枚举在 5000M 的 xHCI 总线上（bus 2 = OTG2，bus 8 = OTG1），一次即可定位。

**复现命令**（板子上）：

```bash
# 记录器：事件 + 拓扑变化（30 分钟窗口）
sudo udevadm monitor --udev --subsystem-match=usb --property > /root/usb-events.log &
while :; do lsusb -t | md5sum; sleep 2; done   # 变化才记，见 /root/usb-topology.log
# 插拔后看结果
grep -E '^(ACTION|DEVTYPE|ID_PATH|DEVNAME)=' /root/usb-events.log | less
```

## 补测：蓝色两个口的 **SS（USB3）** 归属（同一台设备换成 USB3 读卡器）

再跑一遍本文档的记录器，把一颗 **USB3 设备**（`05e3:0749` Genesys SD 读卡器，SuperSpeed）
**依次**插两个蓝口，结果（`lsusb -t` 里 5000M 的那一级就是 SS 链路）：

| 顺序 / 物理口 | SS 侧（USB3） | HS 侧（USB2，来自本文档的 Pico 测试） |
| --- | --- | --- |
| 蓝色·**下层** | **`usbdrd3_1`（OTG1）**：bus 8 `5000M` + `usb-storage, 5000M` | **OTG1**（bus 6 / 480M，`u2phy1_otg`） |
| 蓝色·**上层** | **`usbhost3_0`（OTG2）**：bus 2 `5000M` + `usb-storage, 5000M` | **`usb_host0`（USB2 HOST0）**（bus 4 / OHCI 12M） |

⭐ **结论：v2 的"蓝色·上层"是一个 SS/HS 分属两个控制器的拼接口** ——
**SS 来自 `combphy2_psu`→`usbhost3_0`（OTG2），HS 来自 `u2phy2`→`usb_host0`（HOST0）**。
这解释了整条线索：

- 为什么 v2/5io 使能了 `usbhost3_0`（它服务的正是这个口的 SS），而厂商主 EVB 是禁用的；
- 为什么"OTG2 是 USB3-only"却**不影响**这个口的 USB2 兼容性（USB2 走 HOST0）；
- 为什么 USB2 设备插这个口会以全速枚举在 `ohci-platform`（bus 4）上。

⚠️ **代价（写软件要注意）**：同一个物理口，设备**按速率落到不同控制器/总线**
（USB3 → bus 2/OTG2；USB2 → bus 4/HOST0）⇒ 该口的 `ID_PATH`/总线号**会随设备速率变化**，
写 udev 规则绑符号链接时不能假设它稳定。

⛔ **对自研底板的 hub 方案很关键**：**USB3 hub 不能这么接**。hub 的上游 SS 与 HS 必须属于
**同一个 xHCI 实例**，否则它会在两个控制器上分别枚举（SS 侧挂 OTG2、USB2 侧挂 HOST0），
下游设备散在两棵 USB 树里 —— 既不符合规范也无法预期。
⇒ 想在一个 hub 上同时提供 USB3+USB2，上游要用 **`usbdrd3_1`（OTG1，SS+HS 天然同控制器）**；
或者接受"只有 USB3"（挂 OTG2）/ 只做 USB2 hub（挂 HOST1）。详见
[../hardware/combphy-mux-usbhost3-hub.md](../hardware/combphy-mux-usbhost3-hub.md)。

## 原理图网名交叉验证（2026-09-17 用户读图）

用户在 v2 原理图上读到"蓝色双层 A 口"其中**一层**的信号网名：

| 原理图网名 | RK3588 侧标注 | 对应内核 |
| --- | --- | --- |
| `TYPEC1_OTG_DM/DP` | "USB2.0 of TYPEC1(OTG/HOST/DEVICE) HS/FS/LS" | **`u2phy1_otg`** → `usbdrd3_1` 的 USB2 侧（bus 6 / 480M） |
| `TYPEC1_SSRX1N/P`、`TYPEC1_SSTX1N/P` | "USB3.0 OTG/DP1.4 Alt of TYPEC1 USB:U3/Gen1 —— Controller1" | **`usbdp_phy1_u3`** → `usbdrd3_1` 的 SS 侧（bus 8 / 5000M） |

⇒ 这一层就是本文档里**实测为 `usbdrd3_1`（OTG1）**的那一层（按用户插拔顺序 = 第一个插的"蓝下"）：
**原理图（TYPEC1 / Controller1）与内核实测（bus 6 + bus 8）完全吻合** ✓，
而且它 SS 与 HS 同属一个控制器 ⇒ **是自研底板挂 4 口 USB3 hub 的合适上游**（§3.4 方案 ①）。

⚠️ **还要核一层**：蓝色双层的**另一层**在实测里是 **SS = `usbhost3_0`/OTG2（bus 2）+ HS = `usb_host0`/HOST0（bus 4）**，
它的原理图网名**不该**是 `TYPEC1_*`（应为 `USB30_HOST2_*` / `PCIE20X1_1_*` + `USB2_HOST0_*` 之类的组合）。
读图时顺手确认一下，就能把四个口的"原理图网名 ↔ 内核控制器"闭环。

⚠️ **上/下要对齐**：本文档的表是按**用户插拔顺序**（先蓝下、后蓝上）记录的，归属是实测的、不会错；
但"上/下"是人的叫法，若与 PCB 丝印方向相反，只需把"第一层/第二层"与原理图对应即可
（第一个插的口 = TYPEC1/OTG1）。

## J16B 网名确认 ⇒ v2 四个 A 口全部闭环（2026-09-17）

用户读图的另一半（**J16B**）：

| 原理图网名 | RK3588 侧标注 | 内核实测（本文档） |
| --- | --- | --- |
| `USB20_HOST0_DM/DP` | USB2.0 HOST0 HS/FS/LS | **`u2phy2` → `usb_host0`**（bus 4 / OHCI 12M） |
| `USB30_SSRXN/P`、`USB30_SSTXN/P` | PCIE20/SATA30/USB30 HOST **Mux2** = USB30 HOST **Controller2**（USB 3.0 Gen1） | **`combphy2_psu` → `usbhost3_0`**（bus 2 / 5000M） |

⇒ **原理图（J16A/J16B 网名）↔ 芯片手册（Controller0/1/2、Mux2）↔ 内核（`usbdrd3_*`/`usbhost3_0`）↔ 实测（总线号/速率）四方一致**，
没有猜的成分了。注意命名换算：手册的 **Controller0/1/2** = 内核的 `usbdrd3_0`(OTG0) / **`usbdrd3_1`(OTG1)** / **`usbhost3_0`(USB30 HOST2)**。

**v2 完整 USB 端口地图（实测 + 原理图）**：

| 物理口 / 位号 | USB3（SS） | USB2（HS） | 内核控制器 |
| --- | --- | --- | --- |
| Type-C（带 DP alt + FUSB302） | `usbdp_phy0_u3` | `u2phy0_otg` | `usbdrd3_0`（OTG0，Controller0） |
| 蓝色双层 **J16A** | `TYPEC1_SSRX1/SSTX1` → `usbdp_phy1_u3` | `TYPEC1_OTG_DM/DP` → `u2phy1_otg` | **`usbdrd3_1`（OTG1，Controller1）** —— SS/HS 同控制器 ✓ |
| 蓝色双层 **J16B** | `USB30_SSRX/SSTX` → `combphy2_psu`（Mux2） | `USB20_HOST0_DM/DP` → `u2phy2` | SS：`usbhost3_0`（Controller2）；HS：`usb_host0` —— **拼接口** |
| 白色双层（2 口） | — | 板载 4 口 USB2 hub（挂 `usb_host1`/`u2phy3`，端口 3/4） | `usb_host1`（USB2 HOST1） |

**对自研底板的结论**：要"一个 hub 同时提供 USB3+USB2"，上游就取 **J16A 那一组**
（`TYPEC1_SSRX1/SSTX1` + `TYPEC1_OTG_DM/DP`，天然同属 OTG1）；
**J16B 可保留为单个 USB3-A 口**（对单台设备两种速率都能用，但**不能挂 hub**）；
白色双层那两个口本来就挂在板载 hub 后面，要更多 USB2 口可以在此基础上扩。

## ✅ J16A/J16B ↔ 上下层：实测确定（2026-09-17 补测）

只把 USB2 设备（Pico 显示）插进**上面那一层**，实测：

```
add   platform-fc840000.usb-usb-0:1   /dev/bus/usb/004/003    ← bus 4 = OHCI/USB2 HOST0（u2phy2）
remove ...
```

⇒ **bus 4 = HOST0 = J16B 那一组** ⇒ **J16B = 蓝色·上层，J16A = 蓝色·下层**。
（与最初"J16A 可能是上层"的猜测**相反**；两次测试互相印证：第一次插拔的"蓝下"= bus 8/bus 6 = OTG1 = J16A ✓。）

**最终对应（原理图位号 ↔ 物理 ↔ 内核）**：

| 位号 / 物理位置 | SS（USB3） | HS（USB2） | 内核控制器 |
| --- | --- | --- | --- |
| **J16A** = 蓝色·**下层** | `TYPEC1_SSRX1/SSTX1` | `TYPEC1_OTG_DM/DP` | **`usbdrd3_1`（OTG1/Controller1）** —— SS/HS 同控制器（适合挂 hub） |
| **J16B** = 蓝色·**上层** | `USB30_SSRX/SSTX`（Mux2） | `USB20_HOST0_DM/DP` | SS：`usbhost3_0`（Controller2）；HS：`usb_host0` —— 拼接口，**不能挂 hub** |

## 原理图旁边那句 `U: USB 3.0 OTG / D: USB 3.0 HOST` 怎么读（2026-09-17）

⛔ **不要按 Up/Down 读**（用户最初的理解）。依据：

-本文档实测：**上层 = J16B = HOST 那一组**（`USB20_HOST0_*` + `USB30_SSRX/SSTX`）；
  而 legend 把 **OTG** 标成 `U` ⇒ 若 `U` = Up，上层就该是 OTG，**自相矛盾**。
- **第三次独立验证**（只插**下面那一层**，Pico USB2 设备）：
  `add platform-xhci-hcd.4.auto-usb-0:1 → /dev/bus/usb/006/004`
  = **bus 6**（`xhci-hcd` 480M）= **OTG1 的 USB2 侧** ⇒ **下层 = TYPEC1/OTG = J16A** 确认。
  三次测试（插拔顺序 / 只插上层 / 只插下层）完全互相印证。

✅ **正确读法**：`U`/`D` 是**功能代号**，不是物理位置 ——
`U` = **有 OTG 能力的口**（控制器 1 / OTG1 / `usbdrd3_1`）、`D` = **只能当 host 的口**
（控制器 2 / USB30 HOST / `usbhost3_0`）；也可以理解成 **U**pstream-capable vs **D**ownstream-only。

**要点**：双联连接器的原理图符号常把两排画得与实物顺序不同，**认口一律以网名为准**
（`TYPEC1_*` = OTG 那层 = 下层 = J16A；`USB20_HOST0_*` + `USB30_SSRX/SSTX` = HOST 那层 = 上层 = J16B）。

## 口细节：USB2/USB3 稳定标识、白色口背后的 HUB、现场接线 → 见 [topology-lubancat-5-v2-ports-and-hub.md](topology-lubancat-5-v2-ports-and-hub.md)

"同一个口 USB2/USB3 为什么编号不同"、蓝色双层 A 口的稳定标识、白色双层背后的 CH334F hub、
以及工程机现场接线实测读数**已搬成单独一篇** →
**[topology-lubancat-5-v2-ports-and-hub.md](topology-lubancat-5-v2-ports-and-hub.md)**。

