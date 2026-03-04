
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
```

```c
struct resource *platform_get_resource(struct platform_device *dev,
					unsigned int type, unsigned int num);
int platform_get_irq()

struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
```