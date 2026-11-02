# USB 主机状态与外设重新枚举（`lsp-usb`）

> Type-C 固定 source + host，`lsp-usb` 只查询角色和重新枚举外设；它不提供板子接 PC 的 USB device 模式。

## TL;DR

- `lsp-usb status` 查询 Type-C 数据角色、电源角色与端口策略。
- `lsp-usb power` 查询供电角色；`power source` 只确认当前固定 source 状态，不发 PD role swap。
- `reenumerate` 需 root；不带设备只列出候选，指定设备会中断该设备的 I/O。
- Quest 的 `adb` 是主机客户端，继续保留；它与让板子作为 USB device 的 `adbd` 不同。

## 当前角色边界

v2、5io 默认清单一起启用 `-usbcon-fixedsrc` 与 `-dwc3-rolehost`。
Type-C 只作为主机接相机、串口板、U 盘或 Quest，不切换为 USB 网卡、串口、存储、MTP、HID 或音频设备。
`sink`、`auto` 及 device 模式命令拒绝执行；没有“切回 host”的操作，host 是设备树确定的固定角色。

固定 source 防止 Quest 反向给板子供电；固定 host 不等于过滤 CC/VBUS 瞬态。
角色依据与去抖边界分别见 [Type-C 策略](typec-rollout-and-decision.md)、
[FUSB302 去抖](fusb302-debounce.md)。实际充电判据见 [Quest 供电](quest3-usb-power.md)。

## 查询与重新枚举

```bash
lsp-usb status
lsp-usb power
lsp-usb power source
sudo lsp-usb reenumerate
sudo lsp-usb reenumerate /dev/camera_left
```

`reenum`、`usbreset` 是 `reenumerate` 的别名。
重新枚举接受 USB sysfs 名（如 `6-1.4.1`）、`/dev` 节点或设备链接、`VID:PID`。
同一 VID:PID 匹配多台设备时拒绝执行，改用具体节点定位。
它通过目标 USB 设备的 `authorized` 从 `0` 写回 `1` 重新绑定设备，
不是重启 xHCI 控制器，也不是向 Type-C 控制器注入 CC/VBUS 故障。

操作会中断相机流、串口和 ADB；应用需重新打开设备，`videoN` 等动态编号可能变化。
现场绑定的 `/dev/camera_left` 等链接由 udev 重新建立。
相机故障的适用边界见 [camera-uvc-stall.md](camera-uvc-stall.md)。

## 构建与验证

镜像不安装本工程的 device 服务、ACM getty 规则、HID 描述符或 USB shared 网络配置。
厂商包保留，但 `usbdevice.service` 被 mask，不能通过它重新启用 device 模式；
重新配置旧 rootfs 时清理旧 device 配置与专用依赖。
ROS 的 Bash profile 守卫独立保留，见 [profile-d-ros-guard.md](../system/tuning/profile-d-ros-guard.md)。

实现以 `overlay/usr/local/bin/lsp-usb`、`scripts/build-lsp-image.sh` 为准；
`tools/verify-image.sh` 核对主机工具、服务屏蔽与旧配置清理。
USB 设备归属用 [USB 映射工具](device-mapping-method.md)，
Quest 客户端授权见 [ADB 固定密钥](usb-gadget-power-and-adb.md)。

未来恢复 device 的配置方法与 BSP 经验见 [usb-device-reference.md](usb-device-reference.md)，
该参考不改变当前固定主机策略。
