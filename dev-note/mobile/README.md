# 手机诊断与远程使用

> 先确定权限、观测口径和故障时间，再决定调参、换组件或修复系统；每项结论限定到实际验证的功能。

## TL;DR

- Android 先检查应用网络策略、内存压力和启动脚本，避免把正常缓存或无效调参当作故障。
- Termux、SSH、投屏和 Linux 桌面各有用途，分别验收输入、权限与会话生命周期。
- iOS 的 watchdog、Jetsam 和资源报告回答不同问题，必须与无响应发生时刻对应。

| 遇到的问题 | 文章 |
| --- | --- |
| 浏览器能联网，Play 商店仍显示离线 | [应用网络权限与 UID 策略](android-app-network-policy.md) |
| 内存只剩几百 MB，zram 接近满 | [Android 内存压力与 zram](android-memory-pressure.md) |
| 调优模块显示成功，重启却失效 | [启动模块审查与持久回退](android-tuning-module-audit.md) |
| 改调度器或降分辨率是否更快省电 | [I/O 与显示调参的收益验证](android-tuning-benefit-validation.md) |
| 电流差 1000 倍，充电功率算不对 | [电池电流单位与 Health HAL](android-battery-current-units.md) |
| 手机发热、耗电，温控状态却为 0 | [发热、温控和耗电的观测边界](android-thermal-and-battery-diagnostics.md) |
| 想刷 LineageOS，怎样选二手机 | [型号、解锁与业务检查](custom-rom-device-selection.md) |
| A/B OTA 后如何判断 Root 是否保留 | [OTA 与启动镜像匹配](android-ab-ota-root.md) |
| Root 模块已启用，目标应用仍报错 | [Root 组件兼容与有效状态](root-module-compatibility.md) |
| 电脑操作 Termux，投屏中文输入异常 | [USB SSH、投屏与输入路径](termux-usb-ssh-and-scrcpy.md) |
| 手机上运行 Linux 桌面并从电脑连接 | [原生 Termux、PRoot 与 VNC](termux-linux-desktop.md) |
| 视频挂断后界面长时间无响应 | [iOS watchdog 与系统事件对齐](ios-unresponsiveness-evidence.md) |

热点共享参见 [Android VPN 热点共享](../network/android-vpn-hotspot-sharing.md)。跨主题入口见 [通用排障方法](../通用排障方法.md)。
