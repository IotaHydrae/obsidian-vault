# Linux Common Clock Framework：时钟树与驱动接口

> CCF 把 consumer、时钟拓扑和 provider 硬件操作连接起来；获取时钟对象、启用时钟和设置速率是不同步骤。

## TL;DR

- consumer 使用 `struct clk` 和设备树时钟引用；provider 实现 `struct clk_hw` / `clk_ops`，框架协调树形关系与引用。
- `clk_prepare_enable()` 可能睡眠，成功后需要配对 `clk_disable_unprepare()`；不要在硬中断中调用组合接口。
- 多个设备可能共享父时钟；修改速率或 parent 要考虑 provider 能力、其他 consumer 和硬件约束。
- 框架报告速率或 enable 状态，不单独证明引脚上存在有效波形，也不证明外设已经上电或解除复位。

范围：Linux 6.18.62 LTS 的 Common Clock Framework。

## 对象与时钟树

```text
设备驱动 consumer ── struct clk
                       ↓
CCF：时钟对象、parent、速率、引用计数
                       ↓
provider：struct clk_hw / clk_ops → gate、mux、divider、PLL 等硬件
```

设备树的 `clocks`、`clock-names` 和 provider 的 `#clock-cells` 共同描述引用；consumer 传入的名称需匹配设备 binding，而不是依赖全局时钟名称猜测。

| consumer 接口 | 用途与注意 |
| --- | --- |
| `devm_clk_get()` | 取得受设备管理的 clock handle；失败用 `IS_ERR()` / `PTR_ERR()`，并传播 `-EPROBE_DEFER` |
| `clk_prepare_enable()` | 准备并使能；检查返回值，失败路径不要把未成功启用的时钟当成可用 |
| `clk_disable_unprepare()` | 对成功的准备/使能配对；硬件停止访问后再释放依赖 |
| `clk_get_rate()` | 读取框架报告的速率；单位 Hz |
| `clk_set_rate()` / `clk_set_parent()` | 请求改变速率或父时钟；实际可用值和约束由时钟树与 provider 决定 |
| `devm_clk_get_enabled()` | 获取并启用的受管便利接口；退出仍需先停止硬件和异步用户 |

可选时钟使用对应 optional 接口并分别处理 `NULL` 与错误指针；不能把 `-EPROBE_DEFER` 当作“硬件没有这个时钟”。devres 不负责停止设备的 DMA 或 IRQ。

## prepare 与 enable

框架将操作分为 prepare 与 enable 两类：前者使用可睡眠的 prepare 锁，后者使用保护 enable 路径的自旋锁。provider 的 `prepare` 与 `enable` 回调必须遵守所属上下文；普通 consumer 应使用匹配接口，而不是绕过框架直接改时钟寄存器。

框架维护共享使用情况，consumer 成功启用后只撤销自己的使用；某个设备调用 disable 后，共享时钟是否真正关闭还与其他 consumer 有关。设备电源、复位和 Runtime PM 的顺序由驱动和硬件协议共同决定，见 [[Linux 时钟与电源管理]]。

## 验证和故障定位

1. 确认实际 DTB 中的 `clocks` / `clock-names`、provider 驱动和 `-EPROBE_DEFER`。
2. 判断 handle 获取、prepare/enable、set rate 和实际 I/O 分别在哪一步失败。
3. 在支持 `CONFIG_COMMON_CLK_DEBUG` 的系统查看 debugfs `clk/clk_summary`；记录时钟、父节点、速率及计数，但它是软件状态快照。
4. 用硬件允许的方式验证外设可访问性，必要时测量时钟和电源；检查运行/休眠切换后是否恢复。

## 参考

- Linux 6.18.62：[时钟框架](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/clk.rst)、[`include/linux/clk.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/clk.h)、[`drivers/clk/clk.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/clk/clk.c)。
- 蜗窝科技：[CCF 系统结构](https://www.wowotech.net/pm_subsystem/ccf-arch.html)、[clock provider](https://www.wowotech.net/pm_subsystem/clock_provider.html)。
