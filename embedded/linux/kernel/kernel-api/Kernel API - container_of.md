# Linux 内核 `container_of()`：从成员找回对象

> 已知一个仍然存活的对象及其成员地址，`container_of()` 可按成员偏移还原对象地址；它不会分配、查找、验证或延长对象的生命周期。

## TL;DR

- 参数为成员指针、容器类型、成员名；必须传 `&obj->member` 所指的实际成员，不能传一个类型相同但无关的对象。
- **Linux 6.18.62 的头文件建议新代码优先使用 `container_of_const()`**，它对非 const 指针同样适用，并保留 const 输入的限定。
- 常见用途是从 `work_struct`、链表节点或嵌入式子系统对象找回驱动私有状态。

源码基准：Linux 6.18 LTS / 6.18.62。

## 机制与类型检查

```text
member_ptr = object_address + offsetof(type, member)
object_address = member_ptr - offsetof(type, member)
```

实际宏使用 GNU C 扩展、`typeof` 和编译期类型检查，不能把上面的地址关系当作可直接用于所有标准 C 环境的实现。成员类型不匹配通常会触发编译错误；成员类型相同、指针却来自错误对象时，编译器通常无法识别。

| 参数 | 含义 | 示例 |
| --- | --- | --- |
| `ptr` | 指向成员本身的指针 | `struct gpio_chip *gc` |
| `type` | 外层结构体类型 | `struct mcp23016` |
| `member` | 嵌入成员的名称 | `chip` |

若成员是 `struct foo *child`，它是一个指针字段：字段地址为 `&obj->child`，而 `obj->child` 指向的是另一个对象。后者不能用于还原 `obj`。

## 示例：从 gpio_chip 取得私有状态

```c
#include <linux/container_of.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>

struct mcp23016 {
    struct i2c_client *client;
    struct gpio_chip chip;
};

static inline struct mcp23016 *to_mcp23016(struct gpio_chip *gc)
{
    return container_of_const(gc, struct mcp23016, chip);
}

static inline const struct mcp23016 *to_mcp23016_const(
    const struct gpio_chip *gc)
{
    return container_of_const(gc, struct mcp23016, chip);
}
```

前提是 `gc` 确实来自一个 `struct mcp23016` 的 `chip` 成员。旧代码常见 `container_of(gc, struct mcp23016, chip)`，对非 const 输入可得到同样的容器地址；对 const 输入却会丢弃限定符。`container_of_const()` 保留的是成员指针所表达的 const，不会推断其他字段的所有权。

## 使用边界

- 不能把 `NULL`、`ERR_PTR()`、已经释放的成员指针或任意同类型指针交给宏期待运行时验证；结果可能只是一个无效地址。
- 指向数组内部元素的指针不等于指向整个数组成员的指针；需要先明确真实成员地址，不能随意偏移后还原。
- `container_of()` 不加锁、不增加引用计数，也不提供 RCU 保护。异步回调需要保证回调执行期间容器仍存活。
- 同一种成员类型可在容器里出现多次，传错成员名可能仍能编译；应让转换 helper 的名字和调用位置表达对象关系。
- 不通过手工减常数代替宏，结构体布局和对齐改变后常数会失效。

验证时同时检查编译期类型和运行时来源：回调是由谁注册、传来的成员属于谁、注销/取消回调之后何时释放容器。不能仅因返回地址“看起来合理”就认为对象有效。

相关：[workqueue](Kernel%20API%20-%20workqueue.md)、[tasklet](Kernel%20API%20-%20tasklet.md)。

## 源码与参考

- [6.18.62 container_of.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/container_of.h)：类型检查、const 丢失警告和新代码建议。
- [驱动对象容器模式](https://docs.kernel.org/driver-api/driver-model/design-patterns.html)：嵌入对象与回调中的私有状态恢复。
