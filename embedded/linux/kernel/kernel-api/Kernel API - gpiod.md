# Linux GPIO Descriptor：逻辑电平与可睡眠访问

> 使用 descriptor 接口按功能获取 GPIO；逻辑值考虑 active-low，访问上下文取决于 GPIO 控制器是否需要睡眠。

## TL;DR

- `devm_gpiod_get(dev, "reset", flags)` 对应设备属性 `reset-gpios`，失败返回错误指针。
- `_optional` 在没有可选 GPIO 映射时可返回 NULL；真实错误仍要传播，尤其 `-EPROBE_DEFER`。
- 逻辑 1 表示 asserted；active-low GPIO 的逻辑 1 对应物理低电平。
- I²C/SPI 扩展 GPIO 使用 `_cansleep` 接口，并在可睡眠上下文调用。

源码基准：Linux 6.18 LTS / 6.18.62。此版本 `gpiod_set_value()` 和 `_cansleep()` 返回 int，不能直接照搬旧内核的 void 返回约定。

## 接口与返回值

| 接口 | 行为 |
| --- | --- |
| `devm_gpiod_get()` / `_optional()` | 取得并配置 descriptor；方向 flags 使用逻辑值 |
| `GPIOD_IN` / `GPIOD_OUT_LOW` / `GPIOD_OUT_HIGH` | 输入 / 输出逻辑 0 / 输出逻辑 1 |
| `gpiod_get_value_cansleep()` | 返回逻辑 0、1 或负错误 |
| `gpiod_set_value_cansleep()` | 写逻辑值，6.18.62 返回 0 或负错误 |
| `gpiod_cansleep()` | 判断控制器访问是否可能睡眠 |
| `gpiod_get/set_raw_value*()` | 绕过 active-low 极性转换，通常不用于普通功能驱动 |

Descriptor 包含消费者连接和极性等信息，避免驱动硬编码全局 GPIO 编号。能否睡眠由控制器决定；加自旋锁不会使需要总线访问的 GPIO 变成原子操作。

## 示例：获取可选复位线并释放复位

```c
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>

struct reset_line {
    struct gpio_desc *gpio;
};

static int reset_line_get(struct device *dev, struct reset_line *line)
{
    struct gpio_desc *gpio;

    gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(gpio))
        return dev_err_probe(dev, PTR_ERR(gpio), "reset GPIO unavailable\n");

    line->gpio = gpio;
    return 0;
}

static int reset_line_deassert(struct reset_line *line)
{
    if (!line->gpio)
        return 0;

    return gpiod_set_value_cansleep(line->gpio, 0);
}
```

该例假设“没有复位线”合法，有线时初始逻辑 1 代表保持复位。设备树若声明 active-low，初始物理电平就是低，释放复位写逻辑 0 后变高。两步之间的电源、时钟、保持时间和就绪等待须按硬件要求实现；示例不构成完整复位时序。

## 常见边界

- 不要把 `IS_ERR_OR_NULL()` 的结果统一转成成功：NULL 表示可选资源缺失，错误指针可能表示控制器尚未就绪或配置错误。
- 读取值先检查 `< 0`，再判断真假；负错误不能当作 GPIO 高电平。
- `_cansleep` 不用于硬中断；需要读取扩展 GPIO 的中断处理可放入 [threaded IRQ](../irq/threaded-irq.md)。
- 即使 `gpiod_cansleep()` 为 false，也要遵守 descriptor 的生命周期及所用接口约定。
- managed descriptor 会在解绑时归还，但 GPIO 归还不自动替代安全停机电平；先结束使用者并按手册处理输出状态。
- GPIO 操作成功只表示驱动层操作成功，不保证外部器件已经完成复位或电源转换。

## 源码与参考

- [GPIO Consumer 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/gpio/consumer.rst)：逻辑电平、连接命名和睡眠访问。
- [consumer.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/gpio/consumer.h)、[gpiolib.c](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpio/gpiolib.c)：6.18.62 getter/setter 返回值和极性转换。
