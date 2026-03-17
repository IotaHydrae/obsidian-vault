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
```