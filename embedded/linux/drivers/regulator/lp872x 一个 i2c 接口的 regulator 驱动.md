
在这个驱动中，你可以了解到 i2c + regmap 的使用，以及 regulator 驱动的结构

这是驱动的 regmap_config

```c
static const struct regmap_config lp872x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MAX_REGISTERS,
};
```

驱动的 probe 函数里 alloc 了一个 i2c regmap

```c
static int lp872x_probe(struct i2c_client *cl)
{
	struct lp872x *lp;
	...
	lp->regmap = devm_regmap_init_i2c(cl, &lp872x_regmap_config);
	if (IS_ERR(lp->regmap)) {
		ret = PTR_ERR(lp->regmap);
		dev_err(&cl->dev, "regmap init i2c err: %d\n", ret);
		return ret;
	}
	...
}
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

这是驱动定义的寄存器的一部分

```c
/* Registers : LP8720/8725 shared */
#define LP872X_GENERAL_CFG		0x00
#define LP872X_LDO1_VOUT		0x01
#define LP872X_LDO2_VOUT		0x02
#define LP872X_LDO3_VOUT		0x03
#define LP872X_LDO4_VOUT		0x04
#define LP872X_LDO5_VOUT		0x05
```

上面讲的这些就是 lp872x 这个驱动程序是如何通过 i2c regmap 读写设备寄存器的，接下来看看 regulator 驱动的部分

