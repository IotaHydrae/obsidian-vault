```c
#define IORESOURCE_IO

struct resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
	unsigned long flags;
};
```