# Linux Regulator、Reset 与电源域

> regulator 管理供电资源，reset 控制复位信号，PM domain 协调设备所属电源域；驱动必须按设备依赖顺序使用它们。

## TL;DR

- `*-supply` 引用 regulator，`resets` 引用 reset controller，`power-domains` 描述电源域关系；三者职责不同。
- regulator 常被多个设备共享：自己的 disable 不保证电源轨实际关闭，引用和 enable 必须按成功路径配对。
- `devm_*` 管理资源释放，不替代停止硬件、同步回调和按手册执行上电 / 复位时序。
- 依赖尚未就绪时保留 `-EPROBE_DEFER`；真正可选资源才使用 optional 接口。

范围：Linux 6.18 LTS；电源电压、复位时序和共享关系以硬件及 binding 为准。

## 资源关系

```text
设备树 / 固件描述
  ├→ regulator provider → 设备电源轨
  ├→ reset controller   → 设备复位控制
  └→ PM domain provider  → 可由多个设备共享的电源域
```

regulator 框架按 consumer 维护使用关系；provider 负责实际电压、使能等硬件操作。电源域可能管理一组设备以及其平台相关状态；它不等同于单条 `*-supply`，也不能替代独立的 reset 或 clock 配置。

## 消费者常用接口

| 接口 | 用途 | 边界 |
| --- | --- | --- |
| `devm_regulator_get(dev, "vdd")` | 获取名为 `vdd` 的供电资源 | 设备树常对应 `vdd-supply`；检查错误指针和延迟探测 |
| `regulator_enable()` / `regulator_disable()` | 对该 consumer 的使能操作配对 | 共享轨是否实际关闭取决于所有 consumer 和约束 |
| `devm_regulator_get_enable()` | 获取并启用的受管便利接口 | 检查返回值；退出前仍需停止设备使用 |
| `devm_reset_control_get*()` | 获取对应 reset handle | 选择 exclusive / shared 和 optional 的语义需符合硬件 |
| `reset_control_assert()` / `reset_control_deassert()` | 请求复位状态 | 成功与否、时序和共享约束不能忽略 |

probe 常按硬件要求获取资源、供电、使能时钟、释放复位、等待稳定后访问寄存器；**具体顺序不是框架统一规定**。失败路径只撤销已经成功执行的步骤，避免重复 disable 或在设备还运行时关电。

### 共享 reset 不等同于主动打复位脉冲

| reset 使用方式 | 哪些操作会改变硬件 |
| --- | --- |
| exclusive | 对本控制线直接 assert / deassert；用于驱动确实拥有控制权且需要确定复位时序的设备 |
| shared，显式 assert / deassert | 第一次成功 deassert 解除复位，最后一份解除请求配对归还时才 assert。其他消费者仍持有请求时，本驱动的 assert 不保证线路发生变化 |
| shared，自释放脉冲 | 使用 `reset_control_reset()` / `reset_control_rearm()` 协议；首次触发后，所有消费者重新 arm 前不会再实际发脉冲。不要和上一种共享计数语义混用 |

例如两个设备共用一条复位线，设备 A、B 都已 deassert，此时 A 解绑并执行配对 assert，B 仍可继续使用硬件。若 A 必须独立得到自己的复位脉冲才能恢复，这条共享线不能当成独占线使用。optional reset 未描述可返回 `NULL`；`-EPROBE_DEFER` 仍要传播。

## PM domain 与 Runtime PM

设备通过固件或平台关系关联到 PM domain 后，domain 参与设备电源状态管理；驱动继续使用 Runtime PM 和设备 PM 接口表达活动状态。多个设备共享 domain 时，一个设备进入 runtime suspend 不必然使整个 domain 断电。

系统休眠还涉及唤醒源与父子供应者顺序，见 [[Linux 系统休眠与中断唤醒]]；单设备活动引用见 [[Kernel API - pm_runtime]]；时钟使用见 [[Linux CCF 时钟框架]]。

## 诊断

1. 检查 DT binding、`*-supply` / `resets` / `power-domains` 引用，以及 provider 是否已注册。
2. 区分资源获取失败、使能失败、复位失败和硬件访问失败；记录实际 errno。
3. 软件状态正常但寄存器不可访问时，结合允许的测量确认电压、电平和时序。
4. 休眠后故障同时检查 domain、regulator、clock、reset 和寄存器是否需要重新初始化。

## 参考

- Linux 6.18.62：[regulator consumer](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/power/regulator/consumer.rst)、[reset framework](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/reset.rst)、[设备 PM](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/pm/devices.rst)。
- [`regulator/consumer.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/regulator/consumer.h)、[`drivers/pmdomain/core.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/pmdomain/core.c)。
- 蜗窝科技：[regulator 框架](https://www.wowotech.net/pm_subsystem/regulator_framework_overview.html)、[PM domain](https://www.wowotech.net/pm_subsystem/pm_domain_overview.html)。
