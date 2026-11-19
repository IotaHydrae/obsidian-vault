# 嵌入式 Linux 知识地图

> 按启动、内核机制、设备驱动和排障查找；接口细节优先阅读已有专题笔记。内核内容以 Linux 6.18 LTS 为基准。

## 内核与启动

| 主题 | 笔记入口 |
| --- | --- |
| ARM64 启动 | [[ARM64 Linux 启动与设备树]]、[[bootargs]] |
| 调度与任务 | [[Linux 进程调度与等待]]、[[Kernel API - kthread_run]] |
| 锁、RCU 与内存顺序 | [[Linux RCU 与内存顺序]]、[[Kernel API - spin_lock_irqsave]]、[[Kernel API - mutex]] |
| 中断与异步处理 | [[Linux IRQ Domain 与中断控制器]]、[[threaded-irq]]、[[Kernel API - workqueue]] |
| 时间 | [[Linux 时间子系统]]、[[Kernel API - timer_setup]] |
| 内存和设备地址 | [[Kernel Address - Virtual and Physical]]、[[Linux 内存回收与 OOM]]、[[Kernel API - copy_user]]、[[DRAM 组织与时序基础]]、[[virt to phy]]、[[VMA]]、[[Kernel API - dma_map_single]] |
| 常用接口 | [Linux Kernel API 笔记索引](kernel/kernel-api/README.md) |

## 设备与外围

| 主题 | 笔记入口 |
| --- | --- |
| 设备树与设备模型 | [[Linux 设备模型与对象生命周期]]、[设备树与 Platform 设备](drivers/of/platform.md)、[[platform_match]]、[[Kernel API - devres]] |
| 时钟和电源 | [[Linux 时钟与电源管理]]、[[Kernel API - pm_runtime]] |
| 引脚与 GPIO | [[Linux pinctrl 与 GPIO]]、[[Kernel API - gpiod]] |
| DMA 引擎 | [[Linux DMAEngine 数据传输]]、[[Kernel API - dma_map_single]] |
| I²C / IIO | [[如何实例化I2C设备]]、[[i2c-dev]]、[[Trigger]] |
| 存储和文件系统 | [[Linux 存储与文件系统]] |
| 串口和 TTY | [[Linux 串口与 TTY 数据路径]]、[[g_serial - systemd getty@ttyGS0]] |
| 显示 | [[Linux DRM KMS 显示管线]]、[[panel-mipi-dbi]]、[[simple-panel-dsi]] |
| USB | [[USB 2.0 设备的枚举过程]]、[[USB 2.0 设备的传输过程]]、[[Create usb gadget via configfs]] |
| 以太网 | [[Linux 以太网 MAC PHY 与链路]] |
| 蓝牙 | [[Linux 蓝牙协议栈与排查]] |
| 排障 | [Linux 性能和内核调试工具入口](kernel/debug/intro.md)、[[ftrace]]、[[oops panic debug]] |

## 原站专题入口

以下栏目用于按原站的系列查找细节；Linux 接口和实现以本库引用的 6.18 LTS 源码为准。

| 范围 | 来源 |
| --- | --- |
| 内核、调度、同步、内存 | [Linux 内核分析](https://www.wowotech.net/sort/linux_kenrel/)、[进程管理](https://www.wowotech.net/sort/process_management/)、[内核同步](https://www.wowotech.net/sort/kernel_synchronization/)、[内存管理](https://www.wowotech.net/sort/memory_management/) |
| 中断与时间 | [中断子系统](https://www.wowotech.net/sort/irq_subsystem/)、[时间子系统](https://www.wowotech.net/sort/timer_subsystem/) |
| 设备模型、电源和引脚 | [统一设备模型](https://www.wowotech.net/sort/device_model/)、[电源管理](https://www.wowotech.net/sort/pm_subsystem/)、[GPIO 子系统](https://www.wowotech.net/sort/gpio_subsystem/) |
| 总线、存储和终端 | [通信类协议](https://www.wowotech.net/sort/comm/)、[文件系统](https://www.wowotech.net/sort/filesystem/)、[TTY](https://www.wowotech.net/sort/tty_framework/)、[基础技术](https://www.wowotech.net/sort/basic_tech/) |
| 显示、USB、蓝牙 | [图形子系统](https://www.wowotech.net/sort/graphic_subsystem/)、[显示](https://www.wowotech.net/sort/display/)、[USB](https://www.wowotech.net/sort/usb/)、[蓝牙](https://www.wowotech.net/sort/bluetooth/) |
| 架构与实践 | [ARMv8-A](https://www.wowotech.net/sort/armv8a_arch/)、[U-Boot](https://www.wowotech.net/sort/u-boot/)、[Linux 应用](https://www.wowotech.net/sort/linux_application/)、[X Project](https://www.wowotech.net/sort/x_project/) |

固件、BSP 和硬件板级操作应结合目标设备的实际配置验证。
