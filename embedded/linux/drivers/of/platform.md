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

可由 platform 总线处理的节点可能生成 `platform_device`；I²C、SPI 等总线下的子节点通常由相应总线控制器枚举为自己的设备类型。

| 属性 / 机制 | 用途与边界 |
| --- | --- |
| `compatible` | 按 binding 声明硬件兼容性；驱动使用匹配表匹配，具体顺序见 [[platform_match]] |
| `status` | `disabled` 节点通常不会作为可用设备创建；还要检查父节点和实际运行的 DTB |
| `reg` / `ranges` | 总线地址和转换由父总线规则决定；驱动获取已解析资源，不能把原始单元格直接当 CPU 指针 |
| `interrupts` / `interrupt-parent` | 指向中断控制器及其格式；取得 Linux IRQ 后再注册处理，见 [[Linux IRQ Domain 与中断控制器]] |
| `clocks`、`resets`、`*-supply`、`pinctrl-*` | 描述资源或供应者；获取失败需区分可选资源、配置错误与延迟探测 |

`-EPROBE_DEFER` 不证明依赖最终一定可用；排查时核对对应供应者驱动及设备树 binding。

设备树 bindings 描述属性的约束和单位；新增或修改节点时应以 Linux 6.18 的 binding 与实际硬件为准。`devm_*` 的自动回收也不替代停止中断、DMA 和工作线程，见 [[Kernel API - devres]]。

## 匹配、资源与依赖

同一硬件节点通常包含可匹配的 `compatible` 和资源引用。匹配由对应总线完成；platform、I²C 和 SPI 驱动获得的设备对象类型不同。节点名称只是组织名称，不能替代 binding 规定的 `compatible`。

`reg` 的地址和长度由父节点的 `#address-cells`、`#size-cells` 解释；经过必要的 `ranges` 转换后，驱动通过总线资源 API 获取。中断 specifier 的单元数和含义则由对应中断控制器的 `#interrupt-cells` 定义。

对于 clock、reset、regulator、GPIO 等引用，驱动应调用所属框架的 consumer API，而不是自己解析供应者的寄存器。供应者未就绪时，资源获取可能返回 `-EPROBE_DEFER`；必须把该错误传播到 probe，不能把它一律当作“可选资源不存在”。

## 排查设备没有 probe

1. 确认 bootloader 实际加载的 DTB 和运行时 `/sys/firmware/devicetree/base/` 中的节点，而不只检查工程里的 DTS。
2. 核对节点与父节点可用、`compatible` 匹配以及对应内核驱动已编译或加载。
3. 查看所属总线的 sysfs 设备、驱动链接和内核日志；设备不存在与匹配后 probe 失败是不同阶段。
4. 若为延迟探测，继续定位未就绪供应者和 binding 所需属性；开启 debugfs 时可查看 `devices_deferred`。

设备树只是硬件描述，不负责验证板级电路是否接对；资源解析成功也不能证明时钟和电源已正确使能。

## Linux 6.18 源码入口

| 设备类型 | 入口 | 边界 |
| --- | --- | --- |
| Platform | `drivers/of/platform.c` 的 `of_platform_populate()` | 按节点和总线规则创建 platform 设备，不遍历所有节点为同一种设备 |
| I²C | `drivers/i2c/i2c-core-of.c` 的 `of_i2c_register_devices()` | I²C adapter 就绪后按其子节点创建设备 |
| SPI | `drivers/spi/spi.c` 的 `of_register_spi_devices()` | 依托 SPI controller 创建其子设备 |

对应 driver `probe()` 中，应先验证资源获取结果，再启用硬件；probe 中任何一步失败都要回收已取得的状态，并保留正确的错误码。

## 参考

- 蜗窝科技：[Device Tree 基本概念](https://www.wowotech.net/device_model/dt_basic_concept.html)、[Device Tree 代码分析](https://www.wowotech.net/device_model/dt-code-analysis.html)、[设备模型栏目](https://www.wowotech.net/sort/device_model/)。
- Linux 6.18.62：[设备树使用模型](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/usage-model.rst)、[Platform 驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/platform.rst)。
- Linux 6.18.62：[`drivers/of/platform.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/of/platform.c)、[`drivers/i2c/i2c-core-of.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/i2c/i2c-core-of.c)、[`drivers/spi/spi.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/spi/spi.c)。
