## device attribute

```c
static DEVICE_ATTR_RW(dither_type);

static struct attribute *st7305_attrs[] = {
        &dev_attr_dither_type.attr,
        NULL,
};

static const struct attribute_group st7305_attr_group = {
        .name = "config",
        .attrs = st7305_attrs
};
```