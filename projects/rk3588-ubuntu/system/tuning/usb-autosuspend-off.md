# USB 自动挂起：已整机关闭

> 相机（UVC）和蓝牙驱动会在 probe 里**主动打开**自动挂起，设备空闲 2 秒就被挂起 ——
> 相机再出流时头几帧异常（实测抓到过整段全黑）。机器人外设"随时要用"，所以整机关掉。

## TL;DR

- 三层缺一不可：**内核启动参数** `usbcore.autosuspend=-1`（PM core 级：负延迟让设备 `usage_count+1`，
  驱动即使把 `power/control` 设成 `auto` 也不会真挂起）+ **udev 规则**（设备/接口 `add` 时写
  `power/control=on`）+ **开机 oneshot 服务**（补上 udev 读到规则之前就枚举完的根 hub / 板载 hub / 相机）。
- ⚠️ **接口事件也要处理**：驱动是在**接口** probe 里开挂起的，接口 `add` 排在设备 `add` 之后，
  脚本对接口事件会连父设备一起关。
- ⚠️ 启动参数那层**不重刷不生效**（或改启动分区的 `ubuntuEnv.txt` 后重启）；udev + service 两层当前就生效。
- 判据：`for d in /sys/bus/usb/devices/*/; do cat $d/power/control; done` 全为 `on`，
  且 `grep -o 'usbcore.autosuspend=-1' /proc/cmdline` 有输出；`tools/verify-image.sh` 有对应断言。

## 为什么会被挂起（内核依据）

**为什么要关**：相机（UVC）和蓝牙的驱动会在 probe 里**主动打开**自动挂起，
设备空闲 2 秒就被挂起；相机再出流时头几帧是异常的（实测一度抓到整段全黑画面），
串口板也要重新协商。机器人上的外设是"随时要用"，不该被挂起。

内核源码依据：本仓库 `kernel-6.1/`，Rockchip 6.1 BSP。

| 位置 | 行为 |
| --- | --- |
| `drivers/usb/core/hub.c:2628` | 新设备**默认禁止**自动挂起（`usb_disable_autosuspend`），只有 hub 在绑定时才放开 |
| `drivers/media/usb/uvc/uvc_driver.c:2305` | 相机 probe 结尾 `usb_enable_autosuspend(udev)`（除非设备/父设备带 `USB_QUIRK_AUTO_SUSPEND`） |
| `drivers/bluetooth/btusb.c:2324` / `rtk_bt.c:1496` | 蓝牙同样会开 |
| `/lib/udev/rules.d/60-autosuspend.rules` | 发行版还会按 hwdb 把设备设成 `auto` |

实测（工程机，`/sys/bus/usb/devices/*/power/control`）：两个 Xitech 相机 `auto`、
蓝牙 `auto`，且 `runtime_suspended_time` 已累计 350 秒左右 —— 确实被挂起过。

**三处一起做**（少一处都留口子）：

| 层 | 位置 | 作用 |
| --- | --- | --- |
| 内核启动参数 | `ubuntuEnv.txt` 的 `bootargs=` 里加 `usbcore.autosuspend=-1`（构建脚本写死） | **PM core 级**：负延迟会让设备 `usage_count+1`，**即使驱动调了 `pm_runtime_allow`（`power/control` 显示 `auto`）也不会真的挂起**（`drivers/base/power/runtime.c:1680` 的 `update_autosuspend()`） |
| udev 规则 | `overlay/etc/udev/rules.d/99-lsp-usb-no-autosuspend.rules` | 每个 USB 设备/接口 `add` 时 `RUN+=` 调脚本把 `power/control` 写成 `on`。**接口事件也要处理**：驱动是在接口 probe 里开挂起的，接口 add 事件排在设备 add 之后，脚本对接口事件会连父设备一起关 |
| 开机 service | `lsp-usb-no-autosuspend.service`（oneshot，`After=multi-user.target`） | 开机后整机扫一遍，补上"设备在 udev 读到我们的规则之前就枚举完"的那批（相机、根 hub、板载 hub 都是内核早期枚举的） |

脚本：`/usr/local/sbin/lsp-usb-no-autosuspend.sh`（不带参数 = 整机扫，带 `%p` = 只处理一个）。
只接受一个 `/devices/...` 路径，拒绝路径穿越或多参数；节点消失/无法写入属于尽力操作，正常退出。
`python3 tests/usb-no-autosuspend.py` 在模拟 sysfs 中验证扫描、设备/接口事件、父设备、
无副作用的 help/参数拒绝与节点消失，不证明真实硬件的 runtime PM 状态。

**验证**（2026-09-12 工程机实测）：

```bash
# 1) 装上规则后强制重新枚举一个相机（authorized 0→1），看是否仍是 on
echo 0 | sudo tee /sys/bus/usb/devices/4-1/authorized; sleep 1
echo 1 | sudo tee /sys/bus/usb/devices/4-1/authorized; sleep 3
cat /sys/bus/usb/devices/4-1/power/control        # on  ← uvcvideo 没能把它改回 auto
ls -l /dev/camera_left                            # 符号链接照常重建 → /dev/video3

# 2) 整机状态（service 跑完 / 手工 `sudo lsp-usb-no-autosuspend.sh` 之后）
for d in /sys/bus/usb/devices/*/; do printf '%-8s %s\n' "$(basename $d)" "$(cat $d/power/control)"; done
#   实测 21 个节点全部 on（含根 hub、板载 hub、两个相机、蓝牙、三块 ESP32、Quest、键盘）

# 3) 启动参数（重启后才有）
grep -o 'usbcore.autosuspend=-1' /proc/cmdline
```

`tools/verify-image.sh` 会检查关闭 USB 自动挂起的脚本、规则、服务是否就位、
是否 enable；live 模式下还会遍历 `power/control` 是否全为 `on`、启动参数是否含
`usbcore.autosuspend=-1`，并核对启动分区 `ubuntuEnv.txt` 里的 `bootargs=`。

⚠️ **这次改动不会让已经刷好的机器自动生效**：`usbcore.autosuspend=-1` 是启动参数，
必须重刷镜像（或改启动分区的 `ubuntuEnv.txt`）后重启；udev 规则 + service 那两层当前就能生效。


---
