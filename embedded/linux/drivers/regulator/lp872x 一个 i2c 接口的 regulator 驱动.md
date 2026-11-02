# LP872x：I2C Regmap 访问示例

> LP872x 驱动使用 I2C regmap 抽象寄存器读写；调试时先检查 regmap 初始化及每次访问的错误码。

```c
static const struct regmap_config lp872x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = MAX_REGISTERS,
};

lp->regmap = devm_regmap_init_i2c(client, &lp872x_regmap_config);
if (IS_ERR(lp->regmap))
    return PTR_ERR(lp->regmap);

ret = regmap_read(lp->regmap, LP872X_GENERAL_CFG, &value);
if (ret)
    return ret;
```

此驱动示例中 `LP872X_GENERAL_CFG` 为 `0x00`，LDO1–LDO5 电压寄存器为 `0x01`–`0x05`。实际寄存器定义、可写范围、variant 和 regulator 注册流程以目标驱动版本及芯片手册为准。
