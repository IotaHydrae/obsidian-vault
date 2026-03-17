
syscon 即 System Control Driver，用来给一系列 driver 提供统一的寄存器访问机制，例如 clk、reset。

```c
static int st_reset_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;

	match = of_match_device(st_reset_of_match, dev);
	if (!match)
		return -ENODEV;

	st_restart_syscfg = (struct reset_syscfg *)match->data;

	st_restart_syscfg->regmap =
		syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(st_restart_syscfg->regmap)) {
		dev_err(dev, "No syscfg phandle specified\n");
		return PTR_ERR(st_restart_syscfg->regmap);
	}

	return register_restart_handler(&st_restart_nb);
}
```

```c
struct regmap *syscon_regmap_lookup_by_phandle(struct device_node *np,
					const char *property)
{
	struct device_node *syscon_np;
	struct regmap *regmap;

	if (property)
		syscon_np = of_parse_phandle(np, property, 0);
	else
		syscon_np = np;

	if (!syscon_np)
		return ERR_PTR(-ENODEV);

	regmap = syscon_node_to_regmap(syscon_np);

	if (property)
		of_node_put(syscon_np);

	return regmap;
}
EXPORT_SYMBOL_GPL(syscon_regmap_lookup_by_phandle);

/**
 * syscon_node_to_regmap() - Get or create a regmap for specified syscon device node
 * @np: Device tree node
 *
 * Get a regmap for the specified device node. If there's not an existing
 * regmap, then one is instantiated if the node is a generic "syscon". This
 * function is safe to use for a syscon registered with
 * of_syscon_register_regmap().
 *
 * Return: regmap ptr on success, negative error code on failure.
 */
struct regmap *syscon_node_to_regmap(struct device_node *np)
{
	return device_node_get_regmap(np, of_device_is_compatible(np, "syscon"), true);
}
EXPORT_SYMBOL_GPL(syscon_node_to_regmap);

static struct regmap *device_node_get_regmap(struct device_node *np,
					     bool create_regmap,
					     bool check_res)
{
	struct syscon *entry, *syscon = NULL;

	mutex_lock(&syscon_list_lock);

	list_for_each_entry(entry, &syscon_list, list)
		if (entry->np == np) {
			syscon = entry;
			break;
		}

	if (!syscon) {
		if (create_regmap)
			syscon = of_syscon_register(np, check_res);
		else
			syscon = ERR_PTR(-EPROBE_DEFER);
	}
	mutex_unlock(&syscon_list_lock);

	if (IS_ERR(syscon))
		return ERR_CAST(syscon);

	return syscon->regmap;
}
```

这是一个设备树节点的例子

```c
syscfg: syscon@50000000 {
    compatible = "syscon";
    reg = <0x50000000 0x1000>;
};
```

```c
static struct syscon *of_syscon_register(struct device_node *np, bool check_res)
{
	struct regmap *regmap;
	...
	
	regmap = regmap_init_mmio(NULL, base, &syscon_config);
	if (IS_ERR(regmap)) {
		pr_err("regmap init failed\n");
		ret = PTR_ERR(regmap);
		goto err_regmap;
	}
	
	...
	
	syscon->regmap = regmap;
	syscon->np = np;
	
	...
}
```

在 st_reset_probe 函数中有 `st_restart_syscfg = (struct reset_syscfg *)match->data;`，这里的data就是通过 of_match_device 拿到的

```c
static struct reset_syscfg stih407_reset = {
	.offset_rst = STIH407_SYSCFG_4000,
	.mask_rst = BIT(0),
	.offset_rst_msk = STIH407_SYSCFG_4008,
	.mask_rst_msk = BIT(0)
};

static const struct of_device_id st_reset_of_match[] = {
	{
		.compatible = "st,stih407-restart",
		.data = (void *)&stih407_reset,
	},
	{}
};
```

然后在具体的 restart 函数中，使用驱动程序自定义的 reset_syscfg 进行 regmap 访问

```c
static struct reset_syscfg *st_restart_syscfg;

static int st_restart(struct notifier_block *this, unsigned long mode,
		      void *cmd)
{
	/* reset syscfg updated */
	regmap_update_bits(st_restart_syscfg->regmap,
			   st_restart_syscfg->offset_rst,
			   st_restart_syscfg->mask_rst,
			   0);

	/* unmask the reset */
	regmap_update_bits(st_restart_syscfg->regmap,
			   st_restart_syscfg->offset_rst_msk,
			   st_restart_syscfg->mask_rst_msk,
			   0);

	return NOTIFY_DONE;
}
```

## 补充

### syscon 驱动为什么放在 MFD 子系统

syscon 是一个没有明确功能，但被多个子功能共享的 ”寄存器集合“，本质上是一个寄存器资源提供者，而不是一个单一功能设备。

MFD 的典型模型是：一个物理设备，内部包含多个逻辑子模块，每个子模块由不同驱动负责。举个例子： 一个 PMIC 的驱动通常包含 regulator、gpio、rtc等子模块。这些子模块共享同一块寄存器区域，但在逻辑上是不同功能的驱动。

我们再来看前面的 syscon 设备树节点

```c
syscfg: syscon@50000000 {
    compatible = "syscon";
    reg = <0x50000000 0x1000>;
};
```

这块寄存器区域可能同时被如下模块调用：

- clock
- reset controller
- pinctrl
- reboot
- power domain

syscon 没有且不主动创建子设备，内核不知道有哪些功能会用到这块寄存器，让 driver 自己通过 phandle 拿 regmap。syscon 不定义协议，只提供访问机制，如何访问应当由 driver 自行决定。

## 提问

### `of_platform_device_create()` 什么时候触发

### `EPROBE_DEFER` 在 driver model 里的传播路径
### `regmap` 的锁机制（spinlock vs mutex）

### `syscon` vs `simple-mfd` 的区别

### 为什么 `reset-controller` 有时直接挂在 syscon 上

### `regmap-mmio` 的 cache / lock 实现

### 多个 driver 同时访问 regmap 如何保证一致性