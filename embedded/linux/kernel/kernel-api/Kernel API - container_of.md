# Linux 内核 `container_of`

> `container_of` 根据结构体成员指针和成员偏移取得外层结构体指针；指针必须指向正确类型的成员。

## 示例

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

## 注意

`container_of()` 可能丢失成员指针的 `const` 限定；需要保留限定符时，检查目标内核是否提供 `container_of_const()`。不能对不属于该容器的指针使用。
