# ARM64 Linux 启动与设备树

> 启动链把固件、引导加载器、内核和根文件系统连接起来；定位故障先确定最后一个可靠输出属于哪个阶段。

## 启动层次

```text
上电 / Boot ROM → 平台固件与 bootloader → Linux arm64 内核入口
→ 架构与内存初始化 → 驱动探测 → 挂载根文件系统 → 用户空间 init
```

实际平台可能包含 SPL、Trusted Firmware 或不同 bootloader；每一阶段的加载位置、异常级和职责由 SoC 与固件决定。设备树、initramfs 和内核命令行是否由 bootloader 提供也取决于启动方式。

## 分阶段检查

| 停止位置 | 优先检查 |
| --- | --- |
| 无 bootloader 输出 | 供电、启动介质与模式、串口电平和早期固件 |
| 已加载内核但无内核输出 | 内核镜像、入口参数、DTB、earlycon 与内存布局 |
| 内核启动后设备缺失 | 使用中的 DTB、`compatible`、驱动配置、资源依赖及延迟探测 |
| 无法挂载根文件系统 | `root=`、存储驱动、分区 / 文件系统支持、initramfs 与识别时序 |
| 已进入用户空间但功能未就绪 | init / 服务、权限、设备节点与依赖，而非假定内核未启动 |

结合启动日志、实际 DTB 和内核配置分析；具体入口和调用路径以 Linux 6.18.62 源码及平台固件为准。设备树创建与匹配见 [[platform|设备树与 Platform 设备]]；启动参数见 [[bootargs]]；异常级见 [[Armv7-A 与 Armv8-A - 处理器模式与异常级]]。

## 参考

- 蜗窝科技：[ARM64 启动时 CPU 初始化](https://www.wowotech.net/armv8a_arch/__cpu_setup.html)、[U-Boot 启动流程](https://www.wowotech.net/u-boot/boot_flow_1.html)、[X Project](https://www.wowotech.net/sort/x_project/)。
- Linux 6.18.62：[Booting AArch64 Linux](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/arch/arm64/booting.rst)。
