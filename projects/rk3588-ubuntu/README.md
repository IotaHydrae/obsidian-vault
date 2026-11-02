# RK3588 Ubuntu 镜像知识库

> 围绕 LubanCat-5 系列的 Ubuntu 24.04 ARM64 镜像，按构建、硬件、存储、系统集成和 USB 主题查阅问题、方法与验证依据。

文档采用各篇注明的软件版本和测试条件；Rockchip 6.1 BSP 的结论不能直接套到主线内核，也不能把一块板的测量结果推广到所有硬件。

## 常用入口

| 要解决的问题 | 入口 |
| --- | --- |
| 构建镜像与复用缓存 | [构建详解](build/build-guide.md)、[构建内部机制](build/build-internals.md) |
| 增加或打包应用 | [应用构建](build/apps.md)、[Rust deb](build/rust-app-debs.md) |
| 板级适配、内核与 U-Boot | [板级边界](build/derived-board-bringup.md)、[内核记录](build/kernel-changelog.md)、[U-Boot 记录](build/uboot-changelog.md) |
| 烧录、恢复与启动异常 | [烧录维护](build/flashing.md)、[rkdeveloptool](build/rkdeveloptool.md)、[RK3588 启动链](hardware/rk3588-boot-flow.md) |
| ATF、BL31、OP-TEE 与安全启动 | [BL31](hardware/bl31-mechanism.md)、[OP-TEE](hardware/bl32-optee.md)、[OTP 与 Secure Boot](hardware/otp-secure-boot.md) |
| CPU、GPU、RGA 与性能测量 | [CoreMark](hardware/cpu-coremark.md)、[GPU](hardware/gpu.md)、[RGA](hardware/rga.md) |
| SD、NVMe 与数据可靠性 | [数据卡](storage/init-sdcard.md)、[NVMe](storage/nvme-performance.md)、[数据耐久性](storage/data-card-durability.md) |
| Kiosk、权限与系统集成 | [会话设计](system/kiosk/session-design.md)、[权限边界](system/kiosk/sudo-policy.md)、[系统集成](system/system-integration.md) |
| USB 拓扑、Type-C 与摄像头 | [端口拓扑](usb/topology-lubancat-5-v2.md)、[Type-C 决策](usb/typec-rollout-and-decision.md)、[UVC stall](usb/camera-uvc-stall.md) |
| 查看所有专题 | [完整主题索引](reference/source-index.md) |

## 阅读代码和环境示例

- `<HOST_01>` 等编号占位符代替实际内网主机，`host-01.example.com` 和 `code.example.com` 是示例域名；运行命令前替换为自己的环境。
- `board`、`/home/board`、`/path/to/image-project` 为示例账户和路径；镜像专用的 `lsp-*` 命令、配置变量及设备树标识仍保留其接口名称。
- `<SECRET>` 不包含实际口令或令牌。Wi-Fi 名称、设备序列号和 UUID 使用示例值；以 `02:00:00:00:` 开头的 MAC 用于区分示例设备，不能直接用于现场绑定。
- 标为“镜像源码”的路径指向镜像工程中的实现、测试或资料，不代表此 Vault 内安装了对应工具。

测量 JSON 与 3 张原理图保留在各专题的相对附件路径下；测量数值、寄存器常量和固件摘要用于核对技术结论，不包含现场访问凭据。
