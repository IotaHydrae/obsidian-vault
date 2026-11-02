# uConsole Linux 工程知识库

> 汇集 uConsole 的 CM4／CM5 镜像构建、内核适配、桌面、启动与电源实验，以及其他计算模块的接口和移植参考。

## TL;DR

- 按问题阅读[专题知识索引](knowledge/README.md)，实施前核对板型、系统版本及证据条件。
- 硬件资料清单与引脚表在 `reference/`，架构、构建入口和有日期的开发记录在 `project/`。
- 构建成功、设备启动和功能验收分别判断；资料收集或设计预留不代表板型已完成适配。

## 阅读入口

| 要解决的问题 | 入口 |
| --- | --- |
| 查看全部专题 | [知识库索引](knowledge/README.md) |
| 构建镜像、选择配置档和板型 | [构建与使用](project/build-and-usage.md)、[镜像组合设计](project/image-design.md) |
| 理解工程分层、板型与后端 | [架构](project/architecture.md)、[板型设计](project/board-design.md)、[路线图](project/roadmap.md) |
| CM4／CM5 内核与设备树 | [CM4 移植](knowledge/kernel/cm4-kernel-port.md)、[CM5 移植](knowledge/kernel/cm5-kernel-port.md)、[设备树验证链](knowledge/kernel/device-tree-artifact-lifecycle.md) |
| 启动优化与固件 | [CM4 启动测量](knowledge/boot/cm4-boot-time.md)、[固件更新](knowledge/boot/cm4-firmware-update.md)、[计时时钟域](knowledge/boot/measurement-clock-domains.md) |
| 桌面、模拟器及电源策略 | [桌面应用](knowledge/desktop/desktop-applications.md)、[模拟器](knowledge/desktop/uconsole-game-emulators.md)、[电源策略](knowledge/power/desktop-power-policy.md) |
| USB、网络与介质 | [CM5 IO Board USB](knowledge/usb/cm5-io-board-usb-debug.md)、[CM4 Wi-Fi](knowledge/network/cm4-uconsole-wifi.md)、[克隆介质身份](knowledge/storage/cloned-media-identities.md) |
| 核心板接口与硬件选型 | [硬件资料索引](reference/README.md)、[uConsole 核心板接口](knowledge/hardware/uconsole-custom-core-interface.md) |
| 查看有日期的实验进展 | [开发状态](project/status.md) |

## 环境与引用

- `<HOST_01>` 等代替实际网络地址，`/home/board` 为示例家目录；使用命令前替换为自己的环境。
- `00000000-0000-4000-8000-...` 为示例启动 UUID，同一示例值对应同一组观测，不是本机的实际身份。
- 工程公开的开发镜像默认配置按文档保留；现场口令和被忽略的本地配置不随知识库分发。
- “工程源码”路径用于定位实现、测试和构建工具；相关命令在镜像工程中运行。PDF、CAD 和模型可按参考清单中的官方入口及摘要查找。

7 份 JSON 来源清单与 5 份 CSV 引脚表位于各参考资料目录；对应的版本、仓库固定提交、许可说明与适用边界一并保留。
