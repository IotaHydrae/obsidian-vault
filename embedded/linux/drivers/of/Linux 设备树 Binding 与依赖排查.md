# Linux 设备树 Binding、资源依赖与探测

> 设备树节点的语法、binding、总线创建、驱动匹配和硬件资源是不同检查点；schema 通过不等于硬件一定能工作。

## TL;DR

- binding 定义 `compatible`、属性、单位和约束；先查目标内核的 YAML，再写 DTS。
- `dt_binding_check` 检查 binding schema 本身及其中示例；`dtbs_check` 检查编译后的 DTB 对 binding 的符合性。
- `-EPROBE_DEFER` 常表示供应者暂未就绪，但不证明依赖最终能满足；不要误当作可选资源不存在。
- 总线节点、GPIO、时钟、reset、电源和中断必须与实际板级连接一致，schema 无法证明电气正确。

范围：Linux 6.18.62 LTS 的设备树与驱动核心；具体 binding 和板级连线按设备选择。

## 从 binding 到设备

```text
硬件手册 / binding → DTS + DTSI → DTC 编译 DTB
→ 引导加载器传递实际 DTB → 内核解析节点和资源
→ 对应总线创建设备 → compatible / ID 匹配 → probe 获取资源
```

根节点或适合 platform 总线的节点与 I²C/SPI 控制器下的子节点由不同路径创建设备，不能把 `device_node`、`platform_device` 和 `i2c_client` 当成同一个对象。详见 [[platform|Linux 设备树与 Platform 设备]]。

## 关键属性如何检查

| 属性 | 检查点 |
| --- | --- |
| `compatible` | 设备真实兼容性、顺序、binding 与驱动 match table |
| `reg` | 父总线的地址/大小单元和资源含义；I²C 子节点是总线地址，SPI 子节点通常是片选 |
| `interrupts` / `interrupt-parent` | 控制器定义的 specifier、极性和触发类型；不是直接填写 Linux IRQ 号 |
| `clocks` / `clock-names` | provider 和 consumer 名称、时钟数量与依赖 |
| `*-supply` / `resets` | 实际电源轨、复位控制器与对应名称 |
| `pinctrl-*` / `*-gpios` | 引脚复用、active-low 语义、驱动 consumer 名称 |
| `status` | 节点是否可用；还应关注父总线及实际运行的 DTB |

`reg` 和 `ranges` 的解释取决于父总线 binding；驱动通常应使用资源和总线 API 获取结果，不把 DTS 原始值直接强转为 CPU 指针。`aliases` 的作用和限制见 [[alias|Linux 设备树 aliases 与设备编号]]。

## schema 验证

在配置好 Linux 6.18 内核构建环境、设备树编译器及 `dtschema` 的前提下，可以限定一个真实 binding 文件验证：

```sh
make ARCH=arm64 DT_SCHEMA_FILES=spi/spi-controller.yaml dt_binding_check
make ARCH=arm64 DT_SCHEMA_FILES=spi/spi-controller.yaml dtbs_check
```

示例只选择 SPI controller schema；实际工程应改成目标 binding，并使用正确的内核配置、架构和构建输出目录。`dt_binding_check` 验证 schema 与示例，`dtbs_check` 验证被构建的 DTB；没有构建到目标 DTB 不等于目标板已验证。

## 从现象定位

1. 检查运行中的 `/sys/firmware/devicetree/base/` 与 bootloader 加载路径，确认实际 DTB 和节点状态。
2. 检查相应总线上的设备是否创建，再检查驱动匹配、probe 返回值和供应者。
3. 对 `-EPROBE_DEFER`，从日志和可用时的 `devices_deferred` 反查 clock、regulator、GPIO、IRQ、PHY 等依赖。
4. schema 全部通过后仍要核对引脚、电压、复位、时钟和线缆；软件检查不能证明板级时序。

## 参考

- Linux 6.18.62：[编写 schema](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/writing-schema.rst)、[设备树使用模型](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/usage-model.rst)、[`drivers/of/platform.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/of/platform.c)。
- 蜗窝科技：[Device Tree 基本概念](https://www.wowotech.net/device_model/dt_basic_concept.html)、[设备树文件结构](https://www.wowotech.net/device_model/dt-code-file-struct-parse.html)。
