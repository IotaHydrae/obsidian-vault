# 精简桌面的蓝牙用户态依赖

> HCI 与固件可用后仍需 BlueZ；PipeWire 蓝牙音频另需 SPA 插件。

## TL;DR

- 两套桌面显式安装 `bluez` 与 `libspa-0.2-bluetooth`。
- 补装后重新登录桌面并重启用户 WirePlumber，核对 agent 与音频插件注册。
- 当前只确认控制器开关往返；无线发现、配对和蓝牙出声尚未验证。

## 如何区分故障层

| 层次 | 可检查的证据 | 不能据此推断 |
| --- | --- | --- |
| 板级与内核 | DT 蓝牙节点、HCI 驱动、固件加载、rfkill | 桌面能够配对 |
| BlueZ | `org.bluez`、`Adapter1`、实际 `Powered` 属性 | 蓝牙音频能出声 |
| 桌面控制 | Noctalia 开关命令后，BlueZ 与外壳状态一致 | 无线发现、配对成功 |
| 音频集成 | SPA 插件加载、BlueZ 注册 Audio Sink/Source 等 UUID | 耳机连接与实际音频传输成功 |

BlueZ 是通用用户态依赖；CM4 的 UART、BCM4345C0 固件和设备树是当前板级实现。
CM5 或未来 Rockchip 核心板可复用用户态组合，但必须独立验证自己的控制器总线、
固件、供电与 rfkill，不能直接套用 CM4 的底层判断。

## 运行中补装的注意点

Noctalia 5.3.0 的 `BluetoothService` 启动时调用一次 `GetManagedObjects`，
`BluetoothAgent` 也只在构造时注册。BlueZ 尚未安装时启动的外壳可能一直报告
`bluetooth state unavailable`；后补装服务不等于外壳已经重新初始化。
重新登录桌面可恢复这两条路径。已运行的 WirePlumber 也应重启以加载新安装的 SPA 插件。

这与新镜像预装依赖的路径不同，不应为修复一次补装而增加 root 桌面、
广泛 rfkill 写权限或硬件专用启动脚本。当前 BlueZ 默认 D-Bus 策略允许普通用户调用，
无需给 pi 额外添加 bluetooth 组；其余发行版须核对各自策略。

## CM4 实测证据（2026-10-24）

Ubuntu Base 24.04.5、`6.18.57-v8-uconsole`，启动标识
`00000000-0000-4000-8000-000000000035`：

- 补装前，`hci_uart`/`btbcm` 已加载，`hci0` 存在；BCM4345C0 固件按 CM4 文件名匹配并加载成功，
  rfkill 软硬阻断均为 0。没有 `bluetooth.service` 和 `org.bluez`，两个用户态包均未安装。
- 安装 `bluez 5.72-0ubuntu5.8`、`libspa-0.2-bluetooth 1.0.5-1ubuntu3.3` 后，
  BlueZ 服务 active/enabled。仅新增 9 个包，没有升级或移除原有包。
- 重新登录桌面、重启用户 WirePlumber 后，控制器注册 Audio Sink、Audio Source、
  Handsfree 等 UUID；Noctalia 能读取开关状态。
- 普通用户通过 Noctalia 依次关闭、打开蓝牙。两次命令均返回 `ok`，随后实际 BlueZ
  `Powered` 属性与外壳 `off`/`on` 一致，最终恢复原来的开启状态。
  测试没有扫描、配对或修改 Wi-Fi；控制器最后保持不可发现、未扫描。

开关判据是“请求状态、BlueZ 实际属性、外壳状态一致”，不是命令退出码。
**发现、配对、重连及蓝牙音频尚未验证**；已注册音频能力不等于已听到蓝牙耳机出声。
本轮没有重启设备，因此也不代表新镜像首次启动验证。

## 依据与复核

- 工程依赖：桌面 recipe（工程源码：`image/desktops/recipe.json`）、安装说明（工程源码：`image/desktops/README.md`）。
- 已安装的 `/usr/lib/systemd/system/bluetooth.service`、
  `/usr/share/dbus-1/system.d/bluetooth.conf` 和 BlueZ `Adapter1` 属性。
- 固定 Noctalia 5.3.0 源码：`src/dbus/bluetooth/bluetooth_service.cpp`、
  `bluetooth_agent.cpp` 与 `src/app/application_services.cpp`。
- 本机非发布证据：`~/.cache/uconsole-desktop-20261024/functional/` 下的
  `bluetooth-inventory.txt`、`bluetooth-details.txt`、`bluetooth-package-plan.txt`、
  `bluetooth-install.txt`、`bluetooth-roundtrip.json`。
