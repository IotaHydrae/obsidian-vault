
在这个驱动中，你可以了解到 i2c + regmap + regulator 的组合

这是驱动的 regmap_config

```c
static const struct regmap_config lp872x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MAX_REGISTERS,
};
```

然后驱动定义了