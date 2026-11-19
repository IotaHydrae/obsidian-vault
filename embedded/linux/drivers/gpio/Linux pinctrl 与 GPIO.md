# Linux pinctrl 与 GPIO

> pinctrl 管引脚复用和电气配置，GPIO 管通用数字输入输出；同一引脚能否作为 GPIO 使用取决于硬件和当前引脚状态。

## 分工

| 层次 | 主要职责 |
| --- | --- |
| pin controller / pinctrl | 选择复用功能和引脚组，配置 pull、drive strength 等芯片支持的参数 |
| GPIO controller / gpiolib | 提供输入、输出、电平和部分中断相关能力 |
| consumer 驱动 | 按 binding 获取描述符，处理逻辑极性和设备实际时序 |

设备树常通过 `pinctrl-names`、`pinctrl-0` 等关联状态；状态名称、可用的复用组及电气参数以控制器 binding 为准。驱动核心或设备驱动可在适当阶段选择状态，休眠和恢复时可能使用不同状态。

GPIO 描述符 API 以**逻辑值**表达 active / inactive，active-low 转换由框架处理。GPIO 控制器可能挂在 I²C / SPI 等可睡眠总线上，必须按 `gpiod_cansleep()` 和具体上下文选择访问方式；取得 GPIO 也不会自动保证复用、供电或外部上拉正确。接口细节见 [[Kernel API - gpiod]]。

## 排查

1. 核对实际 DTB、引脚复用、GPIO 控制器及设备的 `*-gpios` 属性。
2. 按芯片手册核对输入 / 输出、上下拉、驱动强度、电压域与外部电路。
3. 区分逻辑电平与引脚测得的物理电平；对 active-low 不要重复反相。
4. 中断场景继续检查中断映射、触发极性和设备侧清除，见 [[Linux IRQ Domain 与中断控制器]]。

debugfs 的 pinctrl / GPIO 信息依赖内核配置与权限，寄存器状态、示波器测量和设备树之间可能还存在固件或其他消费者的影响。

## 参考

- 蜗窝科技：[pinctrl 子系统](https://www.wowotech.net/gpio_subsystem/pin-control-subsystem.html)、[pinctrl 与 GPIO 的关系](https://www.wowotech.net/gpio_subsystem/pinctrl-and-gpio.html)。
- Linux 6.18.62：[pinctrl](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/pin-control.rst)、[GPIO consumer](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/gpio/consumer.rst)。
