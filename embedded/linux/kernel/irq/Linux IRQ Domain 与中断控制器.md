# Linux IRQ Domain 与中断控制器

> 设备树里的中断说明先由中断控制器解析并映射为 Linux IRQ；驱动通过总线接口取得 Linux IRQ，再注册处理函数。

## TL;DR

- 硬件 IRQ 编号、设备树的中断 specifier 和 Linux IRQ 编号属于不同命名空间，不能默认数值相同。
- IRQ domain 描述映射关系，irqchip 提供控制器操作，flow handler 按触发方式协调 action。
- GPIO 扩展器等设备可能形成级联中断：父控制器接收汇总信号，子控制器再识别和分派来源。
- 中断恢复需要控制器与设备两边配合；对电平中断尤其要消除设备侧持续有效的原因。

## 从描述到处理

```text
设备树 interrupt-parent / interrupts
    → 对应控制器解析 specifier 和触发类型
    → IRQ domain 建立 hwirq 与 Linux IRQ 的映射
    → 驱动获取 Linux IRQ 并 request_irq / request_threaded_irq
    → 控制器收到中断 → 通用 IRQ 分派 → 已注册的处理函数
```

实际可能是多级 IRQ domain、MSI 或总线提供的 IRQ；不能把图当作所有架构唯一的调用栈。`platform_get_irq()` 的结果要检查负错误码，不要直接把设备树单元格当作参数。

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
