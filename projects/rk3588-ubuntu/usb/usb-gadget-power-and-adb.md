# 板子作为 Quest ADB 主机的固定密钥

> Type-C 固定 source + host；板子用 `adb` 连接 Quest，board 与 kiosk 账户使用同一对预置密钥。

## TL;DR

- 保留 `adb` 客户端与两账户的 `.android/adbkey{,.pub}`，避免每个账户自行生成不同密钥。
- `.android` 目录 0700、私钥 0600、公钥 0644，属主与各自账户一致。
- 新密钥仍需 Quest 侧授权；换密钥后重启对应账户的 adb server。
- 供电角色由设备树固定，不再需要手动切换 sink/source；充电诊断见 [quest3-usb-power.md](quest3-usb-power.md)。

## 镜像中的密钥

源文件为 `overlay/home/board/.android/adbkey` 与 `adbkey.pub`；构建安装到
`/home/board/.android/`，并将同一对密钥安装到 `/home/kiosk/.android/`。
上位机以 kiosk 运行，采集与维护主要使用 board；两个账户连接同一 Quest 时不需授权两把不同公钥。

| 文件 | 权限 | 用途 |
| --- | --- | --- |
| `.android/` | 0700 | 账户的 ADB 状态与密钥目录 |
| `adbkey` | 0600 | 主机客户端身份认证私钥 |
| `adbkey.pub` | 0644 | 对应主机公钥，供 Quest 侧 adbd 授权 |

Git 只保存可执行位，不能保证克隆后的 0600；构建必须显式设置权限与属主。
密钥不应打印到日志或复制进文档。目录及私钥检查见 `tools/verify-image.sh` 的 ADB 密钥组，
上位机账户约定见 [oem-host-integration.md](../system/oem-host-integration.md)。

## 连接与授权

USB 节点权限由 `99-meta-quest.rules` 为 VID `2833` 配置，见
[app-and-device-config.md](../system/app-and-device-config.md)。以实际使用客户端的账户执行：

```bash
adb devices -l
adb shell dumpsys battery
```

`adb devices` 的状态为 `device` 表示该 transport 已连接；`unauthorized` 需要在 Quest 侧允许 USB 调试。
`offline` 需独立排查 transport，不能一概归因于授权。
更换该账户的密钥后执行 `adb kill-server`，再运行 `adb devices -l`；
正在运行的 server 不会自动重新读取已替换的密钥，操作会打断它现有的 ADB 会话。

这里的 `adb` 是主机客户端；镜像不提供让板子成为 USB device 的 ADB 功能。
Type-C 供电策略见 [typec-rollout-and-decision.md](typec-rollout-and-decision.md)，
主机查询与外设重新枚举见 [usb-gadget.md](usb-gadget.md)。
