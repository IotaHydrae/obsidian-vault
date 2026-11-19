# ARM64 Linux 启动与设备树

> 启动链把固件、引导加载器、内核和根文件系统连接起来；定位故障先确定最后一个可靠输出属于哪个阶段。

## TL;DR

- 先区分固件、bootloader、内核、根文件系统和用户空间的最后可见阶段。
- 核对实际加载的 Image、DTB、内核命令行和根文件系统，不能只检查源码目录。
- 内核要求的入口状态和参数以 Linux 6.18 ARM64 boot protocol 及平台启动方式为准。

适用范围：Linux 6.18 LTS；固件和硬件初始化依平台而定。

## 启动层次

```text
上电 / Boot ROM → 平台固件与 bootloader → Linux arm64 内核入口
→ 架构与内存初始化 → 驱动探测 → 挂载根文件系统 → 用户空间 init
```

实际平台可能包含 SPL、Trusted Firmware 或不同 bootloader；每一阶段的加载位置、异常级和职责由 SoC 与固件决定。设备树、initramfs 和内核命令行是否由 bootloader 提供也取决于启动方式。

## 内核如何接管

对于按 ARM64 Linux boot protocol 直接进入内核的情况，引导方需准备符合要求的内存、CPU 状态和设备树，并通过规定寄存器传递参数。内核入口随后完成架构所需的早期设置、内存映射和通用初始化；具体异常级和固件交接取决于平台和启动路径。

设备树提供内存、CPU、总线、外设及其资源等描述，内核使用实际传入的 DTB 创建对应设备。编译产物存在不代表该 DTB 已被加载；多个启动项或固件覆盖可能使运行时内容不同。

内核命令行控制 console、根文件系统和其他启动参数；若根文件系统或存储驱动不在内核内建部分，可能需要 initramfs 先加载模块并准备根设备。模块或 initramfs 是否可用必须按实际镜像验证。

## 直接进入内核时的交接

按 Linux 6.18 ARM64 boot protocol 直接进入内核时，bootloader 需满足 Image 放置、设备树、CPU 状态、缓存和内存等约束。入口参数包括：`x0` 指向系统 RAM 中 DTB 的**物理地址**，`x1`、`x2`、`x3` 为 0。引导方还需在进入内核前停止可能继续写内存的 DMA 设备。

这些只是交接条件的摘要；Image 头部、内存范围、CPU 模式、中断屏蔽和缓存维护等完整要求以引用的 boot protocol 为准。经 EFI stub 启动时，应把 EFI 与之后进入内核的阶段分开分析。

## 从内核输出到根文件系统

1. 内核开始输出后，检查内存识别、设备树和关键控制器 probe；早期输出停止并不自动说明串口故障。
2. 检查存储设备是否出现，以及所需驱动、分区与文件系统是否已可用；`root=` 的目标应与实际设备或稳定标识对应。
3. 若使用 initramfs，检查其是否包含所需模块、用户程序和切换根目录逻辑。
4. 根文件系统可挂载但服务失败时，转向 init、挂载、权限与服务依赖，不重复排查 bootloader 阶段。

## 证据与判断

| 证据 | 可以证明 | 不能单独证明 |
| --- | --- | --- |
| bootloader 显示加载完成 | 对应加载步骤已执行 | 内核入口状态、DTB 内容和后续驱动都正确 |
| 早期 console 有输出 | 已进入能够输出的阶段 | 正常 console、存储和用户空间均已就绪 |
| `/proc/cmdline` | 当前内核运行时收到的命令行 | 每个参数都被目标驱动成功使用 |
| `/sys/firmware/devicetree/base/` | 内核可见的设备树内容 | 每个节点均已绑定驱动 |

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
