## resource

```c
#define IORESOURCE_IO  0x00000100 /* PCI/ISA IO port */
#define IORESOURCE_MEM 0x00000200 /* Memory region */
#define IORESOURCE_REG 0x00000300 /*  */
#define IORESOURCE_IRQ 0x00000400 /* IRQ line */
#define IORESOURCE_DMA 0x00000800 /* DMA channel */
#define IORESOURCE_BUS 0x00001000 /* Bus */

struct resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
	unsigned long flags;
};

static struct platform_device dummy_pdev = {
	.name = "dummy-platform-device",
	.id = 0,
	.dev = {
		.platform_data = NULL,
	},
	.resource = needed_resources,
	.num_re
};
```

### via DTS

```c
```

### 获取 resource

```c
struct resource *platform_get_resource(struct platform_device *pdev,
					unsigned int type, unsigned int num);
int platform_get_irq(struct platform_device *pdev, unsigned int num);

struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
int irq = platform_get_irq(pdev, 0);
```

## platform_data

### 获取 platform_data

```c
void *dev_get_platdata(const struct device *dev);

```