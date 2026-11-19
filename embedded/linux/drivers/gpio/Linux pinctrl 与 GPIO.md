# Linux pinctrl 与 GPIO

> pinctrl 管引脚复用和电气配置，GPIO 管通用数字输入输出；同一引脚能否作为 GPIO 使用取决于硬件和当前引脚状态。

## TL;DR

- 先核对 pinmux、引脚电气配置及电压域，再确认 GPIO 的方向和电平。
- 描述符 API 使用逻辑 active / inactive；active-low 不应由驱动再次手动反相。
- 可能经过 I²C / SPI 的 GPIO 访问可睡眠，按 `gpiod_cansleep()` 选择调用上下文。

适用范围：Linux 6.18 LTS。

## 分工

| 层次 | 主要职责 |
| --- | --- |
| pin controller / pinctrl | 选择复用功能和引脚组，配置 pull、drive strength 等芯片支持的参数 |
| GPIO controller / gpiolib | 提供输入、输出、电平和部分中断相关能力 |
| consumer 驱动 | 按 binding 获取描述符，处理逻辑极性和设备实际时序 |

设备树常通过 `pinctrl-names`、`pinctrl-0` 等关联状态；状态名称、可用的复用组及电气参数以控制器 binding 为准。驱动核心或设备驱动可在适当阶段选择状态，休眠和恢复时可能使用不同状态。

GPIO 描述符 API 以**逻辑值**表达 active / inactive，active-low 转换由框架处理。GPIO 控制器可能挂在 I²C / SPI 等可睡眠总线上，必须按 `gpiod_cansleep()` 和具体上下文选择访问方式；取得 GPIO 也不会自动保证复用、供电或外部上拉正确。接口细节见 [[Kernel API - gpiod]]。

## 从设备树到实际引脚

```text
设备节点 pinctrl 状态 → 选择 pin function / 电气参数
设备节点 <function>-gpios → 找到 GPIO controller 和线路
→ 驱动获取 GPIO descriptor → 配置方向 / 读写逻辑值
```

两个引用解决不同问题：pinctrl 选择引脚功能，GPIO 描述符标识某个控制器提供的线路。芯片支持时两者可以关联，但不能认为申请 GPIO 一定已经自动设置所有电气参数。`default`、`sleep` 等状态的使用取决于设备、核心流程及驱动。

| 实际需求 | 检查要点 |
| --- | --- |
| GPIO 输出控制复位 | active-low、初始逻辑值、复位持续时间及电源顺序 |
| GPIO 输入读取状态 | 是否具备有效上拉 / 下拉、方向、电压兼容和去抖要求 |
| 转成 IRQ | 输入模式、触发极性、IRQ domain 与线路是否具备中断能力 |
| I²C / SPI GPIO 扩展器 | 操作可能睡眠，硬中断不能直接调用可睡眠访问接口 |

`gpiod_to_irq()` 成功仅得到 Linux IRQ，不会自动清除外设中断状态。一个设备的 GPIO 中断可能最终指向级联控制器，排查时要同时观察父 IRQ、GPIO 控制器和设备状态。

## 从症状到原因

- **软件值正确、引脚电平相反**：核对 active-low、外部反相器和测量点；区分逻辑 API 与物理电平。
- **方向配置成功但波形不变**：检查 pinmux 是否仍是外设功能、IO 电压域及其他消费者占用。
- **中断持续触发**：区分边沿和电平触发，确认外设源、GPIO 线路和 IRQ 极性；不要只反复清父 IRQ。
- **睡眠后失效**：核对 pinctrl sleep / default 状态、供电域、唤醒能力和恢复顺序。

## 逻辑极性示例

假设设备树将 `reset-gpios` 标记为 active-low，驱动以 `reset` 作为 consumer 名称获取该 GPIO：

| 驱动请求的逻辑状态 | 引脚物理电平 | 设备含义 |
| --- | --- | --- |
| `1` / active | 低 | 复位有效 |
| `0` / inactive | 高 | 复位释放 |

初始化输出时也应按逻辑语义选择 `GPIOD_OUT_HIGH` / `GPIOD_OUT_LOW`，并按器件要求等待复位与供电时序。外部还存在反相器时，设备树极性必须反映驱动看到的控制语义，不能只看芯片引脚名称。

## 排查

1. 核对实际 DTB、引脚复用、GPIO 控制器及设备的 `*-gpios` 属性。
2. 按芯片手册核对输入 / 输出、上下拉、驱动强度、电压域与外部电路。
3. 区分逻辑电平与引脚测得的物理电平；对 active-low 不要重复反相。
4. 中断场景继续检查中断映射、触发极性和设备侧清除，见 [[Linux IRQ Domain 与中断控制器]]。

debugfs 的 pinctrl / GPIO 信息依赖内核配置与权限，寄存器状态、示波器测量和设备树之间可能还存在固件或其他消费者的影响。

## 参考

- 蜗窝科技：[pinctrl 子系统](https://www.wowotech.net/gpio_subsystem/pin-control-subsystem.html)、[pinctrl 与 GPIO 的关系](https://www.wowotech.net/gpio_subsystem/pinctrl-and-gpio.html)。
- Linux 6.18.62：[pinctrl](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/pin-control.rst)、[GPIO consumer](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/gpio/consumer.rst)。
