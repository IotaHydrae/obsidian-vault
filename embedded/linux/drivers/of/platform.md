# Linux 设备树与 Platform 设备

> 设备树描述硬件拓扑和资源，内核按总线类型创建设备对象，再由驱动核心进行匹配与探测。

## TL;DR

- `device_node` 不等于驱动已绑定；设备类型由父总线决定，不是每个节点都会创建 `platform_device`。
- 查不到设备时先核对运行中的 DTB、节点状态、`compatible` 和所属总线。
- `-EPROBE_DEFER` 常表示依赖尚未就绪，应继续检查供应者和日志。

适用范围：Linux 6.18 LTS。

## 关键路径

```text
DTS / DTSI → 编译为 DTB → 启动加载并传给内核
→ 内核建立 device_node → 对应总线创建设备对象
→ compatible 等信息参与匹配 → 驱动 probe 获取资源并初始化
```

内核可为根节点下的设备和受支持的总线（如 `simple-bus`）下的节点创建 `platform_device`；I²C、SPI 控制器下的外设子节点则由各自的总线框架实例化。父设备出现在 `/sys/bus/platform/devices/`，不表示其 I²C 或 SPI 外设也会出现在这个目录。

## 区分实例化和绑定

| 现象 | 应追踪的阶段 |
| --- | --- |
| 运行时有 DT 节点，但在相应总线上找不到设备 | 实例化。先确认节点和父总线可用，并检查父驱动是否已注册相应总线 |
| 有 `platform_device`，但没有 `driver` 链接 | 绑定。核对 `compatible`、模块和 [[platform_match|Platform 匹配顺序]]，再区分匹配不到、probe 失败和延迟探测 |

属性约束、schema 校验和供应者排查见 [[Linux 设备树 Binding 与依赖排查]]。对应驱动的 `probe()` 应在拿齐所需资源以后再启动 I/O；`devm_*` 回收不代替同步 IRQ、DMA 与工作线程，见 [[Kernel API - devres]]。

## Linux 6.18 源码入口

| 设备类型 | 入口 | 边界 |
| --- | --- | --- |
| Platform | `drivers/of/platform.c` 的 `of_platform_populate()` | 按节点和总线规则创建 platform 设备，不遍历所有节点为同一种设备 |
| I²C | `drivers/i2c/i2c-core-of.c` 的 `of_i2c_register_devices()` | I²C adapter 就绪后按其子节点创建设备 |
| SPI | `drivers/spi/spi.c` 的 `of_register_spi_devices()` | 依托 SPI controller 创建其子设备 |

## 参考

- 蜗窝科技：[Device Tree 基本概念](https://www.wowotech.net/device_model/dt_basic_concept.html)、[Device Tree 代码分析](https://www.wowotech.net/device_model/dt-code-analysis.html)、[设备模型栏目](https://www.wowotech.net/sort/device_model/)。
- Linux 6.18.62：[设备树使用模型](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/usage-model.rst)、[Platform 驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/platform.rst)。
- Linux 6.18.62：[`drivers/of/platform.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/of/platform.c)、[`drivers/i2c/i2c-core-of.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/i2c/i2c-core-of.c)、[`drivers/spi/spi.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/spi/spi.c)。
