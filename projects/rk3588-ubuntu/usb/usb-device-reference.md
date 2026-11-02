# Type-C USB device 的配置方法与 BSP 经验

> 当前产品固定 source + host，device 功能已退役。本文用于未来单独恢复 device 能力时的设计参考，
> 不是当前镜像的启用教程；本轮只核对源码与旧实现，没有重新进行 device 板测。

- 先恢复控制器与 Type-C 的 device 角色通路，再配置 gadget；单独解除服务 mask 不够。
- 一个 UDC 同时只绑定一个 gadget；function 配置完成后才写 `UDC`，拆除时先解除绑定。
- 先验证单功能，组合需重新验证；旧 BSP 有 `android_work` 崩溃记录。
- 当前主机命令与 Quest ADB 见 [usb-gadget.md](usb-gadget.md)、
  [usb-gadget-power-and-adb.md](usb-gadget-power-and-adb.md)。

## 角色与恢复范围

| 层 | 需要核对的内容 |
| --- | --- |
| Type-C / TCPM | connector 的 `data-role`、`power-role`、PDO 和 role swap 策略；数据角色与电源角色分别管理 |
| DWC3 | `dr_mode`、`usb-role-switch`、与 connector 的连接关系、`role-switch-default-mode`；按本 BSP 核对 |
| 内核 | 最终 `.config` 的 DWC3 gadget/dual-role、`USB_GADGET`、`USB_CONFIGFS` 与所需 function |
| rootfs | gadget 管理入口、依赖、服务、udev 与体检；只恢复所需功能，不恢复整套旧默认值 |

当前两板使用 `-usbcon-fixedsrc` 与 `-dwc3-rolehost`：前者设 connector 为 host/source，
后者把 `USB_ROLE_NONE` 回退到 host。恢复 device 是独立的板级方案，需同时复核这两项及
运行中的合并设备树，不是删除一个 overlay 或写 `power_role` 就完成。
DRP 曾出现 Quest 反向供电，取舍依据见 [typec-rollout-and-decision.md](typec-rollout-and-decision.md)。

`lsp-usb` 已删除全部 device 命令，`usbdevice.service` 被 mask；构建 `rootfs` 阶段还会清理旧配置。
未来恢复必须同步构建、工具、体检（镜像源码：`tools/verify-image.sh`） 与
[系统集成账](../system/system-integration.md)，否则临时安装会在重建时被撤销。
还需复核 `lsp-usb-powerfix` 与新角色策略的关系，避免后台纠正服务干预测试。

只读确认入口：

```sh
cat /sys/class/typec/port0/data_role
cat /sys/class/typec/port0/power_role
ls /sys/class/udc
findmnt /sys/kernel/config
```

`/sys/class/udc` 目录存在不代表有可用控制器，必须确认其中的实际条目和对应物理口。
没有绑定 gadget 也不能据此判定当前是 host。绑定成功不等于 PC 枚举成功，
UDC `state=configured` 只证明枚举/配置层，业务功能需另外验证。

## ConfigFS 单功能流程

以下 ACM 示例只用于已经恢复 device 角色的专用测试镜像，在 root shell 中执行。
先确认 configfs 挂在 `/sys/kernel/config`、目标 UDC 空闲，并设置实际的 `UDC`、
合法的 `VID` / `PID`（如 `0x....`）与不含现场敏感信息的 `SERIAL`。不要用 `ls` 输出自动选 UDC。

```sh
set -eu
: "${UDC:?Set the tested UDC}" "${VID:?Set VID}" "${PID:?Set PID}" "${SERIAL:?Set serial}"
test -e "/sys/class/udc/${UDC}"
G=/sys/kernel/config/usb_gadget/lsp-reference
mkdir "${G}"                         # 已存在就失败，不覆盖其它 gadget
printf '%s\n' "${VID}" > "${G}/idVendor"
printf '%s\n' "${PID}" > "${G}/idProduct"
mkdir -p "${G}/strings/0x409" "${G}/configs/c.1/strings/0x409"
printf '%s\n' "${SERIAL}" > "${G}/strings/0x409/serialnumber"
printf '%s\n' 'LSP' > "${G}/strings/0x409/manufacturer"
printf '%s\n' 'ACM reference' > "${G}/strings/0x409/product"
printf '%s\n' 'ACM' > "${G}/configs/c.1/strings/0x409/configuration"
mkdir "${G}/functions/acm.usb0"
ln -s "${G}/functions/acm.usb0" "${G}/configs/c.1/acm.usb0"
printf '%s\n' "${UDC}" > "${G}/UDC"  # 描述符、function、configuration 完成后才绑定
```

解除绑定后按依赖逆序清理，不对 configfs 使用 `rm -rf`：

```sh
G=/sys/kernel/config/usb_gadget/lsp-reference
printf '\n' > "${G}/UDC"
rm "${G}/configs/c.1/acm.usb0"
rmdir "${G}/configs/c.1/strings/0x409" "${G}/configs/c.1"
rmdir "${G}/functions/acm.usb0" "${G}/strings/0x409" "${G}"
```

这只是配置顺序示例；失败时保留错误，按实际创建的对象清理。
生产管理脚本还需覆盖失败、中断和并发切换；不要沿用旧脚本忽略清理错误的做法。

## 各功能需要补齐什么

| 功能 | 配置要点与验收 |
| --- | --- |
| ACM | `acm.*` 创建板侧 `ttyGS*`；登录还需 getty，必要时 `agetty -L`。节点寿命跟 function，拔线不必然删除节点；不能仅靠节点存在判定连接 |
| RNDIS / NCM | 配置对应 function 与主机驱动；接口名以实际 sysfs 为准。NM shared 需要 `dnsmasq-base`，keyfile 0600 root:root，检查 managed、carrier 与地址。接口 DOWN 时需核对激活顺序；不默认添加全局 `usb*` 例外 |
| UMS | `mass_storage.*` 的 LUN 后端是普通文件或块设备，不能导出目录。用专用、未在板上挂载的介质或离线镜像；只读导出活动文件系统也不保证一致快照 |
| MTP | 目录共享由 `umtp-responder` + FunctionFS 实现；先配置、挂载 FunctionFS，让 responder 通过 ep0 写 descriptors 和 strings，确认就绪后绑定 UDC；退出先解绑 UDC，再停 responder、拆 function 并卸载 |
| HID | `hid.*` 的 protocol、subclass、report_length 与 report_desc 必须一致；验证 `/dev/hidg*` 的实际报告内容，枚举成功不能代替按键/鼠标验收 |
| ADB device | 需要 `adbd` 与 FunctionFS，就绪后绑定；认证策略按 adbd 版本核对。两账户预置的 `.android/adbkey` 是 Quest 主机客户端身份，不是 device 端授权清单 |
| UAC | 按所需 UAC 版本配置通道、采样率与格式，验证真实音频链路；工程机发声测试仍需单独授权 |

UDC 独占的是 gadget，多个 function 可以放在同一个 gadget；旧实现的 NCM/MTP/HID 各自
建了独立 gadget，因而不能直接与厂商 gadget 同时绑定。不要把旧工具的限制写成协议限制。
RNDIS/NCM 的免驱范围、吞吐与 CPU 差异需按 PC 系统版本和同板测量，不作通用保证。

## 厂商脚本与已知限制

`fire_config_special_1.2.6_arm64.deb` 提供 `/usr/bin/usbdevice`，配置入口是
`/etc/init.d/.usb_config`，`usb_rndis_en`、`usb_acm_en` 等多行会累积启用功能。
切功能应完整替换配置、先停止旧 gadget；不能追加一行就当作模式切换。
包安装脚本会尝试 enable/restart `usbdevice.service`，所以当前用 `/etc` 下的 mask 保持禁用。
UMS 后端参数是 `ums_block`、`ums_block_size`、`ums_block_type`、`ums_block_ro` 与
`ums_block_auto_mount`，含义需结合具体包版本核对。包内 unit 的 `ExecStop` 被注释，
不能把 `systemctl stop` 当作已经完成 ConfigFS / FunctionFS 清理。

未来复用厂商脚本时先核对 `usb_prepare()`、实际 function 和用户态程序路径：
Android 状态节点可能缺失，但不能沿用“脚本在创建 gadget 前读状态”的旧解释；
包 1.2.6 是先创建 gadget 并初始化，再读取状态。`#!/bin/sh -e` 下需要明确处理缺节点，
而 ROS profile 的 Bash 守卫仍是独立要求，见 [profile-d-ros-guard.md](../system/tuning/profile-d-ros-guard.md)。

旧记录报告 `acm+adb` 触发 `android_work → kobject_uevent_env` oops，旧工具只放行
`rndis+acm`。这些是旧版本经验，不是当前内核组合验收；具体失效指针根因未在本轮证明。
当前 BSP 仍启用 `CONFIG_USB_CONFIGFS_UEVENT`，`android_work()` 会访问 `gi->dev`，
以后恢复组合时需要单独审查生命周期并做反面对照，不能恢复旧“unsafe”绕过开关。

## 来源与验证范围

- 目标内核：ConfigFS 使用说明（镜像源码：`kernel-6.1/Documentation/usb/gadget_configfs.rst`）、
  `drivers/usb/gadget/configfs.c` 的 `android_work()`、
  `drivers/usb/gadget/function/storage_common.c` 的 `fsg_lun_open()`；
  FunctionFS 就绪见 `f_fs.c` 的 `ffs_ep0_write()` / `ffs_ready()`，串口节点寿命见
  `u_serial.c` 的 `gserial_alloc_line()` / `gserial_free_line()`。
- 旧实现与记录：提交 `90425ba413` 中的 `overlay/usr/local/bin/lsp-usb`、
  `notes/usb/usb-gadget.md`；厂商实现核对包 `fire_config_special_1.2.6_arm64.deb`。
- 本文核对的是 Rockchip BSP 6.1.172 与上述包/旧脚本；ACM 示例仅作 shell 语法检查。
  当前主机回归不证明 device 功能；未来验收应分别验证真实 PC 枚举、业务通信、拔插和恢复。
