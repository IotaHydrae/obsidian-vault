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