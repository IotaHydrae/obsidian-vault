# 把方法用起来：相机规则逐行解读、排查清单、`xhci-hcd.N` 的坑

> 本文是 [device-mapping-method.md](device-mapping-method.md) 的**实战篇**：拿现场那条相机规则
> **逐行解读**（每一行为什么这么写）、symlink 没出现时的**排查清单**、
> 以及"`xhci-hcd.N.auto` 是全局自增实例号、换内核就变，⛔ 不要写进 udev 规则"这条坑。
> 方法本身（四步法：认人 / 认口 / 取字段）见主篇。
> ⚠️ 正文**原样搬自主篇的 §④ 与后面两节**。

## TL;DR

- 规则**逐行读一遍**比背语法有用：`ENV{}` 用 `-q property` 的字段、`ATTRS{}` 用 `-a` 的父链字段（§④）。
- symlink 没出现时按**排查清单**走（内核认到没 → udev 字段 → 规则匹配 → 实时事件 → reload）。
- ⛔ `xhci-hcd.N.auto` 的 `N` 会变，**不要写进规则**（用 `*` 通配或按 `ID_PATH`）。


## ④ 你的相机规则逐行解读

```udev
SUBSYSTEM=="video4linux", ENV{ID_VENDOR_ID}=="0abd", ENV{ID_MODEL_ID}=="8050", \
  ENV{ID_PATH}=="platform-fc800000.usb-usb-0:1:1.0", ENV{ID_V4L_CAPABILITIES}=="*:capture:*", \
  SYMLINK+="camera_left", GROUP="video", MODE="0660"
```

| 部分 | 作用 |
| --- | --- |
| `SUBSYSTEM=="video4linux"` | 只对视频节点生效（`/dev/videoN`） |
| `ENV{ID_VENDOR_ID}` / `ID_MODEL_ID` | 认人（但**左右相机这两个值相同，起不到区分作用**，只是收紧范围） |
| `ENV{ID_PATH}=="platform-fc800000.usb-usb-0:1:1.0"` | **真正区分左右的依据** —— 插在 `fc800000.usb`（USB2.0 Host0）的端口 1 |
| `ENV{ID_V4L_CAPABILITIES}=="*:capture:*"` | 有些相机同时暴露**元数据节点**（`index=1`），这个条件保证只匹配**能采集图像的那个** |
| `SYMLINK+="camera_left"` | 生成稳定名字 `/dev/camera_left` |
| `GROUP="video" MODE="0660"` | 权限：同组可读写 |

右手同理，但 `ID_PATH` 是 `platform-xhci-hcd.11.auto-usb-0:1:1.0`
→ 对应 `usbdrd3_1`（**USB3.1 Host1**，总线 3/8）。

> 也就是说：**你的左右手相机接在完全不同的两个控制器上**（一个 USB2、一个 USB3），
> 所以哪怕 VID/PID/SN 全同也绝不会混淆。`99-chest-camera.rules` 则是同一个相机用
> `ATTR{index}=="0"/"1"` 把视频与元数据节点分开。

## 排查清单：symlink 没出现怎么办

```bash
# 1) 设备到底认到了没（内核视角）
lsusb ; lsusb -t ; dmesg | tail -20

# 2) udev 眼里它的字段是什么（把 ID_PATH 抄出来跟规则比对）
udevadm info -q property -n /dev/video0 | grep -E "ID_PATH|ID_VENDOR_ID|ID_MODEL_ID|ID_V4L_CAPABILITIES"

# 3) 规则为什么不匹配（-a 会把属性链全打出来，逐条对）
udevadm test /sys/class/video4linux/video0 2>&1 | grep -iE "camera_left|ID_PATH|rule"

# 4) 实时看事件（插拔时最直观）
udevadm monitor --udev --subsystem-match=video4linux

# 5) 规则改完让 udev 重载
sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=video4linux
```

**最常见的三种失败原因**：

1. **换了口**：`ID_PATH` 变了（左右相机只差控制器名，插错口就互换）；
   中途加了 hub，路径会多一段 `.N`（如 `0:1.4:1.0`）。
2. **`ID_PATH` 压根没生成**：对应的 `60-persistent-*.rules` 被精简掉了（见本文档）。
3. **`ATTRS{}` 跨了多层**：udev 只允许"设备自身 + **一个**父设备"的属性组合，
   写了三层就永远不匹配。

> 写规则时优先用 `ENV{ID_PATH}` 这类**udev 算好的稳定字段**；
> `ATTRS{}` 更适合"按芯片型号匹配"（例如 `ATTRS{idVendor}=="05a3"`），
> 它对插在哪个口不敏感。

---


## `xhci-hcd.N.auto` 是**全局自增实例号**，换内核就变 —— 不要写进 udev 规则

| 控制器（DT 节点） | 工程机 kernel **6.1.118** | 开发板 kernel **6.1.172** |
| --- | --- | --- |
| `usbdrd3_1/fc400000.usb`（OTG1 = **J16A**） | `xhci-hcd.`**11**`.auto` | `xhci-hcd.`**4**`.auto` |
| `usbhost3_0/fcd00000.usb`（Controller2 = J16B-SS） | `xhci-hcd.`**10**`.auto` | `xhci-hcd.`**3**`.auto` |
| `usbdrd3_0/fc000000.usb`（TypeC） | `xhci-hcd.`**15**`.auto` | 该次启动无 xhci（Type-C 不是 host 角色） |

**同一块板、同一个 DT 节点，实例号 11 → 4**。原因：`xhci-hcd.%d.auto` 的 `%d` 来自 platform 总线的
**全局 auto-id 分配器**（`PLATFORM_DEVID_AUTO`），取决于"在它之前有多少个自动 id 的 platform 设备
注册过"，所以随**内核版本 / 内核配置 / probe 顺序**变（同一内核下逐次启动是否稳定**未验证**）。

⇒ `overlay/etc/udev/rules.d/99-right-hand-camera.rules` 里写死的
`ENV{ID_PATH}=="platform-xhci-hcd.11.auto-usb-0:1:1.0"` **只对当前那台工程机的内核成立**：
换成 6.1.172 的镜像（或任何内核/配置变动）后 `/dev/camera_right` 会**静默消失**（无报错）。
对照：左手相机写的是 `platform-fc800000.usb-usb-0:1:1.0` —— `fc800000.usb` 是 **DT 节点名**，稳定，不受影响。

**稳定写法 + 已改**（2026-09-17，经用户同意后改了 `99-right-hand-camera.rules`）：

```ini
SUBSYSTEM=="video4linux", KERNELS=="fc400000.usb", ENV{ID_VENDOR_ID}=="0abd", ENV{ID_MODEL_ID}=="8050", ENV{ID_V4L_CAPABILITIES}=="*:capture:*", SYMLINK+="camera_right", GROUP="video", MODE="0660"
```

`KERNELS=="fc400000.usb"` 沿父链匹配 **DT 节点名**（dwc3 那个 platform 设备），不随内核变；
VID/PID 用 `ENV{...}` 而不是 `ATTRS{...}`（原因见下）。

> ⚠️ 工程机上装的**仍是旧规则**（本次只改了仓库里的文件，没有动工程机的 `/etc`）——
> 旧写法在那台的 6.1.118 上本来就能用，所以 `camera_right` 现在照样存在。

### ⛔ 顺手踩到的坑：一条规则里**只能有一种"父链匹配键"**

第一版写的是 `KERNELS=="fc400000.usb"` + `ATTRS{idVendor}` + `ATTRS{idProduct}`，
在工程机上**一条都不匹配** —— `udevadm test` **不报错、也不提示**，纯静默失败。
逐项拆开测（探针规则放 `/run/udev/rules.d/`，tmpfs，不碰 eMMC；`udevadm test --action=add`）：

| 规则（都带 `SUBSYSTEM=="video4linux"`） | video1（右手） | video3（左手） |
| --- | --- | --- |
| `KERNELS=="fc400000.usb"` 单独 | ✅ | ❌ |
| `KERNELS=="4-1"` 单独（USB 设备自身名） | ✅ | ❌ |
| `ATTRS{idVendor}=="0abd"` 单独 | ✅ | ✅（同型号，本来就区分不了） |
| **`KERNELS=="fc400000.usb", ATTRS{idVendor}=="0abd"`** | ❌ | ❌ |
| `ATTRS{idVendor}, ATTRS{idProduct}, KERNELS=="fc400000.usb"`（换顺序） | ❌ | ❌ |
| `ENV{DEVPATH}=="*/fc400000.usb/*"`（`ENV` 不参与父链） | ✅ | ❌ |

⇒ `KERNELS` / `ATTRS` / `SUBSYSTEMS` / `DRIVERS` 都是**父链匹配键**：**第一个命中的键会把"当前
父设备"钉住，后面同类的键只在那个设备上找**。本链上 `fc400000.usb` 是 `4-1` 的**祖父**
（`video1 ← 4-1:1.0 ← 4-1 ← usb4 ← xhci-hcd.N.auto ← fc400000.usb`），两个键要求的是不同设备
⇒ 永远不匹配。所以：**设备自身属性（VID/PID/序列号）一律用 `ENV{ID_VENDOR_ID}` /
`ENV{ID_MODEL_ID}` / `ENV{ID_SERIAL_SHORT}`**（属性查询，不参与父链）；非要用 `ATTRS` 就不能同时写
`KERNELS`。

**最终验证**（把改好的规则行原样放上去，只把 `SYMLINK` 改成 `camera_right_verify` 以便区分）：

```
video1 -> : camera_right,camera_right_verify    ← 右手采集节点：命中 ✓
video2 -> :                                      ← 右手的 metadata 节点（index=1，无 capture 能力）：不命中 ✓
video3 -> : camera_left                          ← 左手：不命中 ✓（关键判别）
video4 -> :                                      ← 左手的 metadata 节点：不命中 ✓
```

> 工具里也据此加了提示：`-v` 打出的 `ID_PATH` 只作"当前这次"的证据，**别当规则键**。
