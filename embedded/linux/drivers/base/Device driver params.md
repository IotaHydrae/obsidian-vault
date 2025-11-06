## device attribute

```c
static ssize_t dither_type_show(struct device *dev,
                                struct device_attribute *attr, char *buf)

{
	struct st7305 *st7305 = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u\n", st7305->dither_type);
}

static ssize_t dither_type_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct st7305 *st7305 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val >= DITHER_TYPE_MAX)
		return -EINVAL;

	st7305->dither_type = val;

	dev_info(dev, "set dither type to %lu\n", val);

	return count;
}

static DEVICE_ATTR_RW(dither_type);

static struct attribute *st7305_attrs[] = {
        &dev_attr_dither_type.attr,
        NULL,
};

static const struct attribute_group st7305_attr_group = {
        .name = "config",
        .attrs = st7305_attrs
};

ret = sysfs_create_group(&dev->kobj, &st7305_attr_group);
	if (ret)
		dev_err(dev, "Failed to create device attrs\n");

sysfs_remove_group(&st7305->dev->kobj, &st7305_attr_group);
```

## debugfs