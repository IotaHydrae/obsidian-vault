
## Prototype

```c

```

## Example

```c
struct mcp23016 {
	struct i2c_client *client;
	struct gpio_chip chip;
};

static inline struct mcp23016 *to_mcp23016(struct gpio_chip *gc)
{

}
```