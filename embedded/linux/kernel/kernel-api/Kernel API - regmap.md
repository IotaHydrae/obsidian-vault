# Linux Regmap：寄存器读写与位更新

> Regmap 统一寄存器访问、内部锁和可选缓存；配置必须反映硬件语义，尤其是易变、读清和写 1 清除寄存器。

## TL;DR

- I²C/SPI 寄存器设备可使用对应的 `devm_regmap_init_*()`，失败返回错误指针。
- `regmap_read()` 返回的是状态码，寄存器值经 `unsigned int *` 输出。
- `regmap_update_bits()` 通常执行读改写，适合普通配置字段，不适合直接处理 W1C 状态位。
- 总线访问常可睡眠；内部锁不保证整个多寄存器业务事务不可交错。

源码基准：Linux 6.18 LTS / 6.18.62。

## 常用接口

| 接口 | 行为 |
| --- | --- |
| `regmap_read(map, reg, &value)` | 成功 0，失败负错误；失败后不能使用输出值 |
| `regmap_write(map, reg, value)` | 写整个寄存器，成功 0 或负错误 |
| `regmap_update_bits(map, reg, mask, value)` | 通常读改写：`(old & ~mask) \| (value & mask)`，无变化可不写 |
| `regmap_bulk_read/write()` | 连续寄存器批量访问，数量以寄存器值为单位 |
| `regcache_cache_only()` / `regcache_mark_dirty()` / `regcache_sync()` | 缓存隔离、标脏和同步，须与掉电/恢复协议配套 |

`reg_bits` 是寄存器地址位宽，`val_bits` 是寄存器值位宽，`reg_stride` 是地址步长。它们与设备总线数据格式共同决定传输，不能只根据 CPU 位宽配置。

## 示例：I²C 普通配置位

```c
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#define SENSOR_CONTROL 0x10
#define SENSOR_ENABLE  BIT(0)

static const struct regmap_config sensor_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = 0x3f,
    .cache_type = REGCACHE_NONE,
};

static int sensor_setup_regmap(struct i2c_client *client, struct regmap **out)
{
    struct regmap *map;

    map = devm_regmap_init_i2c(client, &sensor_regmap_config);
    if (IS_ERR(map))
        return dev_err_probe(&client->dev, PTR_ERR(map), "regmap init failed\n");

    *out = map;
    return 0;
}

static int sensor_set_enabled(struct regmap *map, bool enabled)
{
    return regmap_update_bits(map, SENSOR_CONTROL, SENSOR_ENABLE,
                              enabled ? SENSOR_ENABLE : 0);
}
```

地址、位宽和范围均为示例，控制寄存器假设为无读写副作用的普通 RW 寄存器。此处禁用缓存，方便直接观察总线访问；需要缓存时应先列出所有 volatile、precious、可读和可写寄存器。

驱动的 Kconfig 应依赖 `I2C` 并选择 `REGMAP_I2C`；头文件能编译不代表所需总线后端已启用。

## 缓存与副作用

- W1C 寄存器通常按手册直接 `regmap_write(map, status_reg, bits_to_clear)`；读改写可能误清其他位。
- `regmap_write_bits()` 的“强制写”仍可能先读寄存器，不能用它绕开读副作用或直接替代 W1C 写入。
- 硬件自行更新的状态应标为 volatile，防止从缓存得到旧值；读清等寄存器还应评估 precious，避免调试转储误读。
- 断电后缓存可能仍有旧值，恢复时按器件要求退出 cache-only、标脏并同步；不可缓存的状态不能靠 sync 恢复。
- 内部锁只保护对应 regmap 操作。需要“解锁寄存器 → 连续配置 → 上锁”等完整协议时，还需驱动层串行化。

实际案例：[syscon](../../drivers/mfd/syscon.md)、[LP872x regulator](../../drivers/regulator/lp872x%20%E4%B8%80%E4%B8%AA%20i2c%20%E6%8E%A5%E5%8F%A3%E7%9A%84%20regulator%20%E9%A9%B1%E5%8A%A8.md)。

## 源码与参考

- [regmap.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/regmap.h)：初始化配置、接口和缓存控制。
- [regmap.c](https://github.com/gregkh/linux/blob/v6.18.62/drivers/base/regmap/regmap.c)：读改写、force_write 和内部加锁。
