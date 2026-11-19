# Linux 设备树与 Platform 设备

> 设备树描述硬件拓扑和资源，内核按总线类型创建设备对象，再由驱动核心进行匹配与探测。

## 关键路径

```text
DTS / DTSI → 编译为 DTB → 启动加载并传给内核
→ 内核建立 device_node → 对应总线创建设备对象
→ compatible 等信息参与匹配 → 驱动 probe 获取资源并初始化
```

`device_node` 不等于已经成功绑定的设备。根节点和可由 platform 总线处理的节点可能生成 `platform_device`；I²C、SPI 等总线下的子节点通常由相应总线控制器枚举为自己的设备类型，不应假设每个节点都会变成 platform 设备。

| 属性 / 机制 | 用途与边界 |
| --- | --- |
| `compatible` | 按 binding 声明硬件兼容性；驱动使用匹配表匹配，具体顺序见 [[platform_match]] |
| `status` | `disabled` 节点通常不会作为可用设备创建；还要检查父节点和实际运行的 DTB |
| `reg` / `ranges` | 总线地址和转换由父总线规则决定；驱动获取已解析资源，不能把原始单元格直接当 CPU 指针 |
| `interrupts` / `interrupt-parent` | 指向中断控制器及其格式；取得 Linux IRQ 后再注册处理，见 [[Linux IRQ Domain 与中断控制器]] |
| `clocks`、`resets`、`*-supply`、`pinctrl-*` | 描述资源或供应者；获取失败需区分可选资源、配置错误与延迟探测 |

`-EPROBE_DEFER` 表示当前无法完成探测，常见于依赖的供应者尚未就绪；它不证明依赖最终一定可用。排查时核对内核日志、对应供应者驱动及设备树绑定。

设备树 bindings 描述属性的约束和单位；新增或修改节点时应以 Linux 6.18 的 binding 与实际硬件为准。`devm_*` 的自动回收也不替代停止中断、DMA 和工作线程，见 [[Kernel API - devres]]。

## 参考

- 蜗窝科技：[Device Tree 基本概念](https://www.wowotech.net/device_model/dt_basic_concept.html)、[Device Tree 代码分析](https://www.wowotech.net/device_model/dt-code-analysis.html)、[设备模型栏目](https://www.wowotech.net/sort/device_model/)。
- Linux 6.18.62：[设备树使用模型](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/usage-model.rst)、[Platform 驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/platform.rst)。
