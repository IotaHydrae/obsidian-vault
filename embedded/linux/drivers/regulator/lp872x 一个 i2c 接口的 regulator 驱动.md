
在这个驱动中，你可以了解到 i2c + regmap + regulator 的组合

这是驱动的 regmap_config

```c
static const struct regmap_config lp872x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MAX_REGISTERS,
};
```

然后驱动定义了通过 regmap 的读、写函数

```c
static int lp872x_read_byte(struct lp872x *lp, u8 addr, u8 *data)
{
	int ret;
	unsigned int val;

	ret = regmap_read(lp->regmap, addr, &val);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", addr);
		return ret;
	}

	*data = (u8)val;
	return 0;
}

static inline int lp872x_write_byte(struct lp872x *lp, u8 addr, u8 data)
{
	return regmap_write(lp->regmap, addr, data);
}
```

然后在其他地方调用的例子

```c
ret = lp872x_read_byte(lp, LP872X_GENERAL_CFG, &val);
```