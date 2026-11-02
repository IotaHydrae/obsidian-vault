# Kindle Linux 工程知识库

> 围绕 Kindle KT4 的用户区 Alpine chroot、墨水屏绘制和 KOReader 应用开发，保留原厂启动链、签名内核与系统根分区。

## TL;DR

- 当前开发路线是手动运行用户存储区的独立 Alpine ARMv7 镜像，不是替换 Kindle 内核或根文件系统。
- USB 自动启动试验已回滚，自动启用命令保持禁用；手动 USB 连接与开机自动连接分别判断。
- 离线检查、设备探测、安装部署及恢复是不同操作；构建或用户态 root 权限不能证明设备具备可靠恢复路径。

## 阅读入口

| 要解决的问题 | 入口 |
| --- | --- |
| 工程使用方法和命令入口 | [构建与使用](project/build-and-usage.md) |
| 手动安装、运行和退出 Alpine | [Alpine chroot](knowledge/chroot.md) |
| 墨水屏绘制和应用路线 | [第三方应用](knowledge/apps.md) |
| KOReader 插件界面、状态与部署 | [插件开发](knowledge/koreader-plugin.md)、[插件说明](reference/kindledev-plugin-readme.md) |
| USB 存储更新与手动 USB SSH | [USB 开发](knowledge/usb.md) |
| 刷写、备份和恢复的能力边界 | [刷写与恢复](knowledge/flashing.md) |
| 理解架构和自动化接口 | [架构](project/architecture.md)、[工程接口契约](project/agent.md) |
| 按已有证据查看验证范围 | [开发状态](project/status.md) |

## 环境与证据

- `<HOST_01>` 等是网络地址占位符，使用命令前按实际环境替换，并核对主机端与 Kindle 端的地址角色。
- 项目中的版本、文件摘要、挂载参数和验证数字按各篇条件保留；历史试验不自动代表当前设备状态。
- “工程源码”路径用于定位 Kindle 工程中的实现与测试，对应命令在该工程中运行。
- 设备本地配置、原始探测、凭据、二进制备份和工作目录不随知识库分发。
