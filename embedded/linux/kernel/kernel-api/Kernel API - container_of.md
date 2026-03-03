
## Prototype

```rust
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 * WARNING: any const qualifier of @ptr is lost.
 * Do not use container_of() in new code.
 */

#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	static_assert(__same_type(*(ptr), ((type *)0)->member) ||	\
		      __same_type(*(ptr), void),			\
		      "pointer type mismatch in container_of()");	\
	((type *)(__mptr - offsetof(type, member))); })
```

## Example

```c
struct mcp23016 {
	struct i2c_client *client;
	struct gpio_chip chip;
};

static inline struct mcp23016 *to_mcp23016(struct gpio_chip *gc)
{
	return container_of(gc, struct mcp23016, chip);
}
```