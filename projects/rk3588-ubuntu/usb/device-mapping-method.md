# 怎么确认某个 USB 口接的是哪个设备（方法）

> 四步法：`lsusb` **认人**（VID:PID）→ `lsusb -t` / sysfs **认口** → `udevadm info` **取字段** →
> 写 udev 规则。需要按口区分设备时（例如左右手相机 VID/PID/SN 全同）用 `ENV{ID_PATH}`。

## TL;DR

- **不想手敲上面那四步，就跑 `lsp-usb-map`**：`apps/lsp-usb-map`（Rust + ratatui，进镜像是
  `/usr/local/bin/lsp-usb-map`；`--once` 纯文本、`--json` 给脚本、不带参数是 TUI）或兜底的
  `tools/lsp-usb-map.sh`（bash，没编 Rust 时用）。**Rust 主实现在任何 Linux 上都通用**：
  * 通用层：控制器标识（PCI 地址 `0000:00:14.0` / platform 节点 `usbdrd3_1/fc400000.usb`）、
    总线号、端口链、速率代次、接口驱动、**`/dev` 节点（完整路径）**、`ID_PATH`；
  * 板级层（可选）：device tree 认出是 LubanCat 时，才把「控制器 + 端口」翻译成丝印口名
    （`J16A(蓝色下层)`、`Hub.P3(白色上层J17A)`、`TypeC(板边Type-C)`）；认不出就退回
    `Port <端口链>` 且**不报错**（本机、自研板都这样）。`--board=auto|none|<id>` 可显式控制。
  * DEV 列按树缩进（`└ 6-1.4.1` = 挂在外接 hub 6-1.4 的第 1 口）；**"这口上的相机是 /dev/video 几"
    直接看最后一列**（`/dev/video4`、`/dev/input/event12`、`/dev/ttyACM0`）。
  * 自测：`tests/lsp-usb-map.sh`（PC 与板子通用，查字段/端口链/节点存在性/过滤子集/缩进这些不变量）。
  * 要加一块新板：改 `apps/lsp-usb-map/src/collect.rs` 的 `BOARDS`（shell 版对应
    `controller_info()`），只加一段映射表，别的代码不用动。
- bash 兜底版默认只列设备表，`-v` 增加控制器一览与 `ID_PATH`，过滤时只列匹配设备。
  口代码与拓扑解释在本篇和板级拓扑文档中，不追加到运行结果。
  它目前仅内置 v2 映射，未映射控制器返回 2，不支持 Rust 版的 `--board` 选项。
- 认口最有用的是 **sysfs 名字**：`/sys/bus/usb/devices/platform-…-usb-0:1.3` 里的 `1.3` 就是端口路径。
- xHCI **会注册两条总线**（USB2 480M + USB3 5000M），同名设备在两条上都可能出现 —— 正常，别当成两个设备。
- ⛔ `xhci-hcd.N.auto` 里的 `N` 是**全局自增实例号**，换内核就变 ⇒ **别写死**，规则里用 `*` 通配。
- 取字段分两种：`udevadm info -q property` 给的是 udev 数据库字段（写 `ENV{}` 用这个）；
  `udevadm info -a` 是沿父设备链的属性（写 `ATTRS{}` 用这个）。同一规则可以使用多个父链键，
  但它们必须匹配**同一个祖先设备**（`udev(7)`）。按 DWC3 控制器 `KERNELS` 定位时，
  VID/PID 用 `ENV`，不能同时要求这个控制器具有 USB 设备的 `ATTRS{idVendor}`。
- `ID_PATH` 由 `60-persistent-*.rules` 生成 —— 精简掉那套规则时 `ID_PATH` 会**压根不存在**。
- 规则可以只用 `KERNELS` + `ENV{ID_V4L_CAPABILITIES}`，**VID/PID 可以省**（老款相机就靠这个，
  见 [lsp-set-rule.md](../system/lsp-set-rule.md)）。
- ⚠️ 软件只能定位到"**控制器 + 端口**"；"这颗控制器对应板上哪个丝印口"要对照原理图/实物 ⇒
  v2 的对应表见 [topology-lubancat-5-v2.md](topology-lubancat-5-v2.md)，
  5io 的见 [topology-lubancat-5io.md](topology-lubancat-5io.md)。

## 起因：为什么要有这套方法

> **需求来源**：这套设备的左右手相机 **VID / PID / 序列号完全相同**，
> 所以 `overlay/etc/udev/rules.d/99-{left,right}-hand-camera.rules` 只能靠
> **插在哪个口**（`ID_PATH`）来区分。这篇把"从 `lsusb` 到写出 udev 规则"的方法固定下来，
> 换硬件 / 换机器时照着走一遍即可。
>
> 数据都是在 **lubancat-5-v2 实测**的（2026-09-12）。硬件层的口表见
> [lubancat-5-v2-hardware.md](../hardware/lubancat-5-v2-hardware.md) §5。

---

## 四步法（本文的骨架）

| 步骤 | 命令 | 回答什么问题 |
| --- | --- | --- |
| ① 认人 | `lsusb` | 这是什么设备（VID:PID、厂商、型号） |
| ② 认口 | `lsusb -t` / `ls /sys/bus/usb/devices/` | 它挂在哪个**控制器**、哪个**端口**（`9-1`、`6-1.4`） |
| ③ 取字段 | `udevadm info -q property -p <sysfs路径>` | 写出**稳定**匹配要用的字段（`ID_PATH` 等） |
| ④ 写规则 | `udevadm info -a -n <设备节点>` + 写 `.rules` | 规则里该怎么写（`ENV{}` 还是 `ATTRS{}`） |

**核心观念**：`lsusb` 的 Bus/Device 号（`Bus 009 Device 002`）**每次插拔/重启都会变，不能用来匹配**；
真正稳定的是**拓扑路径**（插在哪个控制器的哪个端口）。

---

## ① 认人：`lsusb`

```
$ lsusb
Bus 006 Device 002: ID 1a86:8091 QinHeng Electronics USB HUB
Bus 006 Device 003: ID 0bda:c822 Realtek Semiconductor Corp. Bluetooth Radio
Bus 006 Device 004: ID 046d:c092 Logitech, Inc. G102/G203 LIGHTSYNC Gaming Mouse
Bus 006 Device 005: ID 413c:2113 Dell Computer Corp. KB216 Wired Keyboard
Bus 009 Device 002: ID 2833:5013 Oculus VR, Inc. Quest 3
```

`1a86:8091` 里 `1a86` 是 VID、`8091` 是 PID。**同型号的两个设备这里会一模一样**（你的相机就是），
所以只看 `lsusb` 分不出左右 —— 必须进第 ② 步。

## ② 认口：三种粒度

### `lsusb -t`：看层次与端口号

```
$ lsusb -t
/:  Bus 006.Port 001: Dev 001, Class=root_hub, Driver=ehci-platform/1p, 480M
    |__ Port 001: Dev 002, If 0, Class=Hub, Driver=hub/4p, 480M
        |__ Port 003: Dev 004, If 0, Class=Human Interface Device, Driver=usbhid, 12M
        |__ Port 004: Dev 005, If 0, Class=Human Interface Device, Driver=usbhid, 1.5M
/:  Bus 009.Port 001: Dev 001, Class=root_hub, Driver=xhci-hcd/1p, 480M
    |__ Port 001: Dev 002, If 0, Class=Vendor Specific Class, Driver=[none], 480M
```

缩进就是拓扑：**键盘挂在 Bus 006 根 hub 的 Port 001 那个 Hub 的 Port 004**。
`Driver=ehci-platform` / `xhci-hcd` 告诉你它走的是哪类控制器。

### sysfs 的名字**就是**路径（最有用）

```bash
$ ls /sys/bus/usb/devices/
6-1  6-1.3  6-1.4  9-1  usb1 usb2 ... usb10
```

命名规则：`<总线号>-<端口>[.<下级 hub 的端口>]…`

- `9-1` = 总线 9 的 **端口 1**
- `6-1.4` = 总线 6 的端口 1 上那个 hub 的**端口 4**

一眼就能看出"谁是直插在根口、谁在 hub 后面"。

### 控制器 ↔ 总线号：`readlink` 才是权威

`lsusb` 只给总线号，**总线号本身也可能变**，所以要知道它对应板上哪个控制器：

```bash
$ for u in /sys/bus/usb/devices/usb*; do
    printf '%-6s → %s\n' "$(basename "$u")" "$(readlink -f "$u" | sed 's|/sys/devices/platform/||')"
  done
```

**lubancat-5-v2 实测结果**（DTS 名 ↔ 运行时名 ↔ 总线 ↔ 角色）：

| 总线 | 运行时 platform 名 | DTS / 硬件名 | 角色（见硬件笔记 §5） |
| --- | --- | --- | --- |
| `usb9` / `usb10` | `usbdrd3_0/fc000000.usb`（`xhci-hcd.15.auto`） | `usbdrd_dwc3_0` | **Type-C 口**（OTG 双角色 + DP alt mode） |
| `usb3` / `usb8` | `usbdrd3_1/fc400000.usb`（`xhci-hcd.11.auto`） | `usbdrd_dwc3_1` | USB3.1 Host1 |
| `usb1` / `usb2` | `usbhost3_0/fcd00000.usb`（`xhci-hcd.10.auto`） | `usbhost_dwc3_0` | USB3.1 Host2 |
| `usb4` / `usb5` | `fc800000.usb` / `fc840000.usb` | `usb_host0_ehci/ohci` | USB2.0 Host0 |
| `usb6` / `usb7` | `fc880000.usb` / `fc8c0000.usb` | `usb_host1_ehci/ohci` | USB2.0 Host1 |

> XHCI 控制器会注册**两条总线**：一条 USB2（480M）、一条 USB3（5000M），
> 同名设备在两条上都可能出现 —— 这是正常的，别当成两个设备。

**实测印证这颗表**：Quest 3 在 `9-1`，而 `usb9` = `usbdrd3_0` = Type-C 口 ——
与我们从 `typec/port0`、PD 协商一路查出来的结论一致（见 [quest3-usb-power.md](quest3-usb-power.md)）。

> ⚠️ 软件只能定位到**控制器 + 端口**这一级。"这个控制器对应板子上哪个丝印口"
> 要对照原理图/实物；本板的对应关系见 [lubancat-5-v2-hardware.md](../hardware/lubancat-5-v2-hardware.md) §5。

## ③ 取字段：`udevadm info`

### `-q property`：udev 数据库给出的字段（写 `ENV{}` 用这个）

```bash
$ udevadm info -q property -p /sys/bus/usb/devices/9-1
DEVPATH=/devices/platform/usbdrd3_0/fc000000.usb/xhci-hcd.15.auto/usb9/9-1
DEVNAME=/dev/bus/usb/009/002
PRODUCT=2833/5013/510          # VID/PID/版本
ID_VENDOR=Oculus
ID_MODEL=Quest_3
ID_SERIAL_SHORT=EXAMPLE_ID_01 # 序列号（很多设备会缺或重复！）
ID_PATH=...                    # ← 路径，写规则的关键
```

### `-a`：沿**父设备链**的属性（写 `ATTRS{}` 用这个）

```bash
$ udevadm info -a -n /dev/bus/usb/009/002
Udevadm info starts with the device specified by the devpath and then
walks up the chain of parent devices. It prints for every device
found, all possible attributes in the udev rules key format.
A rule to match, can be composed by the attributes of the device
and the attributes from one single parent device.

  looking at device '/devices/platform/usbdrd3_0/fc000000.usb/xhci-hcd.15.auto/usb9/9-1':
    KERNEL=="9-1"
    SUBSYSTEM=="usb"
    ATTR{idProduct}=="5013"
    ATTR{idVendor}=="2833"
  looking at parent device '/devices/.../usb9':
    KERNEL=="usb9"
    ...
```

**这句话最关键**：*"A rule to match can be composed by the attributes of **the device and one single parent device**"*
—— 即 `ATTRS{}` 可以匹配父设备的属性，但**只能跨一层**（不能"设备的属性 + 爷爷的属性"混着写）。

### `ID_PATH` 是谁给的

不是内核给的，是 **udev 的 `path_id` builtin** 算出来的，各子系统由不同规则文件调用：

| 设备类型 | 调用 `path_id` 的规则文件 |
| --- | --- |
| 视频（V4L2） | `/usr/lib/udev/rules.d/60-persistent-v4l.rules` |
| 输入设备 | `60-persistent-input.rules` |
| 串口 | `60-serial.rules` |
| 块设备 | `60-persistent-storage.rules` |
| 网卡 | `80-net-setup-link.rules` |

> 这就是为什么你的**相机规则能用 `ENV{ID_PATH}`**：板上 `60-persistent-v4l.rules` 存在，
> 且对带 `index` 属性的 `video*` 节点执行了 `IMPORT{builtin}="path_id"`（已实测确认）。
> 如果哪天精简镜像把它删了，**相机规则会静默失效**（`ENV{ID_PATH}` 为空 → 条件不成立 → 没有 symlink 且不报错）。

**格式示例**（本板实测，HID 设备）：

```
鼠标: ID_PATH=platform-fc880000.usb-usb-0:1.3:1.1
键盘: ID_PATH=platform-fc880000.usb-usb-0:1.4:1.0
```

拆开读：`platform-<控制器>-usb-0:<端口>[:<配置>.<接口>]`
→ 键盘 = `fc880000.usb` 控制器 → USB 端口 1 → 该口上的 hub 的**端口 4** → 接口 0。

与你相机规则里的 `platform-fc800000.usb-usb-0:1:1.0` 完全同构。

## ④ 你的相机规则逐行解读 / 排查清单 / `xhci-hcd.N` 的坑 → 见 [device-mapping-in-practice.md](device-mapping-in-practice.md)

现场相机规则的逐行解读、symlink 没出现时的排查清单、以及 `xhci-hcd.N.auto` 那条坑
**已搬成单独一篇** → **[device-mapping-in-practice.md](device-mapping-in-practice.md)**。

