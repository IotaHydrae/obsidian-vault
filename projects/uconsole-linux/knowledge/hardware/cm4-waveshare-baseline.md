# 微雪底板上的 CM4：原系统基线

> 微雪载板上的 CM4/SD 基线只验证该载板，不能代替 uConsole 接线验证。

## TL;DR

- 本模块是 CM4 Rev 1.1、4 GB，原生根存储报告 SD；不要称为 eMMC。
- 底板准确型号未确定，保存 DT 和存储/内核观察后再选择测试配置。
- 迁移 uConsole 时独立验证 PMIC、显示、USB 和 Wi-Fi。


## 方法：先按载板建立配置边界

准备内核启动测试时，应使用现有的通用 CM4 DTB，以及该底板实际采用的 USB 和启动设置。不要把 uConsole 的 PMIC/显示 overlay 或 GPIO 音频服务复制到这块底板。这里的成功测试可以验证 CM4 的 CPU、存储和网络行为，不能证明 uConsole 的面板、背光、充电或音频行为正常。

## 设备观察

| 项目 | 观察结果 |
| --- | --- |
| 核心板 | Raspberry Pi Compute Module 4 Rev 1.1，修订号 `c03141`，BCM2711 |
| CPU 与内存 | 四个 Cortex-A72 核心；安装 4 GB，系统报告可用 3.7 GiB |
| 原系统 | Debian 13.4 Trixie，带 Raspberry Pi 软件包 |
| 运行内核 | `6.12.75+rpt-rpi-v8`，aarch64，4096 字节页大小 |
| 根存储 | 原生 MMC 设备报告类型为 `SD`，容量 29.1 GiB；未列出 eMMC 启动分区 |
| 分区 | 512 MiB FAT `/boot/firmware`；28.6 GiB ext4 `/` |
| 可用空间 | 检查时启动分区约有 445 MiB，根分区约有 18 GiB |
| 网络 | `eth0` 已启用；`wlan0` 未启用；启动日志中无线固件已初始化 |
| USB 与 PCIe | Terminus `1a40:0101` USB 集线器；未枚举到 PCI 设备 |
| 显示 | 两个 HDMI 连接器均报告未连接；没有 HAT 身份节点 |
| 固件 | 启动固件日期 2025-08-20；EEPROM 引导程序日期 2023-01-11 |
| 电源与温度 | `get_throttled=0x0`；检查时温度为 33.6 °C |

原有配置启用了通用 `vc4-kms-v3d`、显示和摄像头自动检测、`auto_initramfs=1`、64 位模式及 UART。`[cm4]` 段设置了 `otg_mode=1`、`over_voltage=6` 和 `arm_freq=2000`。这些是原系统的设置，不是工程默认值，也不是已验证的必要条件。EEPROM 报告 `BOOT_ORDER=0xf25641`，配置为先尝试 SD；回退恢复路径尚未测试。

## 历史基线与后续验证边界

此次基线检查时，工程内核还没有在该设备上启动。检查期间没有修改软件包、模块、启动配置或 EEPROM 设置，也没有重启或烧写存储。用户之后已成功启动单独准备的 SD 卡，详见[工程内核启动记录](../storage/cm4-carrier-sd-test.md)。底板确切型号、所接扩展硬件以及经过测试的恢复路径仍不明确。
