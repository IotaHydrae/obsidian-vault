# Linux IRQ Domain 与中断控制器

> 设备树里的中断说明先由中断控制器解析并映射为 Linux IRQ；驱动通过总线接口取得 Linux IRQ，再注册处理函数。

## TL;DR

- 硬件 IRQ 编号、设备树的中断 specifier 和 Linux IRQ 编号属于不同命名空间，不能默认数值相同。
- IRQ domain 描述映射关系，irqchip 提供控制器操作，flow handler 按触发方式协调 action。
- GPIO 扩展器等设备可能形成级联中断：父控制器接收汇总信号，子控制器再识别和分派来源。
- 中断恢复需要控制器与设备两边配合；对电平中断尤其要消除设备侧持续有效的原因。

适用范围：Linux 6.18 LTS。

## 从描述到处理

```text
设备树 interrupt-parent / interrupts
    → 对应控制器解析 specifier 和触发类型
    → IRQ domain 建立 hwirq 与 Linux IRQ 的映射
    → 驱动获取 Linux IRQ 并 request_irq / request_threaded_irq
    → 控制器收到中断 → 通用 IRQ 分派 → 已注册的处理函数
```

实际可能是多级 IRQ domain、MSI 或总线提供的 IRQ；不能把图当作所有架构唯一的调用栈。`platform_get_irq()` 的结果要检查负错误码，不要直接把设备树单元格当作参数。

## 中断控制器与分派

| 层次 | 负责什么 |
| --- | --- |
| 设备 | 产生事件，保存状态或数据；按照器件规则屏蔽或清除中断源 |
| irqchip | 实现控制器的 mask、unmask、ack、EOI 和触发类型等操作，具体能力取决于控制器 |
| IRQ domain | 把固件描述和控制器的 hwirq 对应到 Linux IRQ；分层 domain 可表达多级控制器 |
| 通用 IRQ 层 | 查找该 IRQ 的描述符和处理流程，调用已注册的 action |
| 设备驱动 | 判断共享中断归属、处理设备状态并安排后续工作 |

电平触发时，只在中断控制器执行 EOI 不一定使设备侧信号失效；设备未清除原因可能在解除屏蔽后再次触发。边沿触发则要关注屏蔽期间硬件是否能够保留事件。具体 ACK / EOI 顺序由 irqchip、flow handler 和设备手册共同决定。

### 级联示例

GPIO 控制器可以把多路 GPIO 汇总到一个父 IRQ。父 IRQ 到达后，GPIO 控制器读取自己的 pending 状态，再将有效的子线路分派到对应 Linux IRQ。若控制器通过 I²C / SPI 访问，其状态读取可能需要可睡眠的线程上下文，不能直接套用普通 MMIO 控制器的处理方式。

设备树的 `interrupt-parent` 和 `interrupts` 描述目标控制器及其定义的 specifier；specifier 中的单元不一定就是 `/proc/interrupts` 中的数字。使用 `/proc/interrupts` 时应按设备名称、控制器和计数变化定位。

## Linux 6.18 源码入口

- `kernel/irq/irqdomain.c` 的 `irq_create_fwspec_mapping()` 根据固件 specifier 查找 domain 并处理映射；分层 domain 还涉及相应的层次分配。
- `kernel/irq/irqdesc.c` 的 `generic_handle_domain_irq()` 是按 domain 与 hwirq 分派的入口之一；具体控制器可能使用不同入口。
- `kernel/irq/chip.c` 包含 `handle_level_irq()`、`handle_fasteoi_irq()` 等 flow handler；控制器选择哪一种取决于硬件和 irqchip 设置。
- 注册、线程唤醒及共享处理的具体实现见 [[threaded-irq]]，不应仅根据 domain 映射推断线程一定能执行。

| 观察 | 可能的检查方向 |
| --- | --- |
| 没有 Linux IRQ | 设备节点是否启用、interrupt-parent、控制器驱动和映射是否成功 |
| `/proc/interrupts` 不增长 | 接线、pinmux、屏蔽、触发类型、供电、设备中断使能 |
| 计数持续快速增长 | 电平源是否已清除、极性、寄存器 ACK / W1C 顺序 |
| 父计数增长但子功能无响应 | 级联控制器状态读取、掩码及子 IRQ 分派 |

不可睡眠的 primary handler 与可睡眠的 IRQ 线程、共享 IRQ、`IRQF_ONESHOT` 详见 [[threaded-irq]]。

## 参考

- 蜗窝科技：[IRQ Domain](https://www.wowotech.net/irq_subsystem/irq-domain.html)、[IRQ number 和描述符](https://www.wowotech.net/irq_subsystem/interrupt_descriptor.html)。
- Linux 6.18.62：[IRQ domain 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/irq/irq-domain.rst)、[通用 IRQ](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/genericirq.rst)。
- Linux 6.18.62：[`irqdomain.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/irq/irqdomain.c)、[`chip.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/irq/chip.c)、[`irqdesc.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/irq/irqdesc.c)。
