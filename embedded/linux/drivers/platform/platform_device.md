
```c
struct platform_device {
	const char *name;
	u32 id;
	struct device dev;
	u32 num_resource;
	struct resource *resource;
};
```

对于 platform_driver，在驱动程序和设备匹配之前，`struct platform_device.name` 和 `static struct platform_driver.name` 字段必须相同

