# Linux Kref：共享对象的最后一次释放

> 引用计数保证“还有使用者时对象不被释放”，锁保证共享状态访问有序；两者各自负责一个问题，不能互相替代。

## TL;DR

- `kref_init()` 建立初始引用 1，分配者必须明确由谁持有它。
- 每个跨线程、异步队列或句柄的持有者在交接前取得引用，使用结束后 `kref_put()`。
- 最后一次 put 会同步调用 release，随后不能再访问对象。
- 从共享表查对象并获取引用必须受锁或其他生命周期协议保护；`kref_get_unless_zero()` 不能挽救已释放的裸指针。

源码基准：Linux 6.18 LTS / 6.18.62。

## 接口与原理

`struct kref` 封装 `refcount_t`，使用引用计数操作增减，降到 0 时执行调用者提供的释放函数。

| 接口 | 前提与行为 |
| --- | --- |
| `kref_init()` | 新对象初始化为 1，不能重置仍在共享的对象 |
| `kref_get()` | 已有有效引用，或外部保护确保对象和计数有效 |
| `kref_get_unless_zero()` | 计数非零才增加；访问计数本身仍需保证内存存活 |
| `kref_put(ref, release)` | 最后一个引用同步调用 release，返回 1；否则返回 0 |

即使 put 返回 0，另一个 CPU 也可能立即释放对象，因此调用者放弃该引用后不能继续解引用。release 必须是类型匹配的回调，不能直接传 `kfree`。

## 示例：不可变结果对象

```c
#include <linux/container_of.h>
#include <linux/kref.h>
#include <linux/slab.h>

struct result_object {
    struct kref ref;
    unsigned int value;
};

static struct result_object *result_create(unsigned int value)
{
    struct result_object *result;

    result = kzalloc(sizeof(*result), GFP_KERNEL);
    if (!result)
        return NULL;

    kref_init(&result->ref);
    result->value = value;
    return result;
}

static void result_release(struct kref *ref)
{
    struct result_object *result;

    result = container_of(ref, struct result_object, ref);
    kfree(result);
}

static void result_put(struct result_object *result)
{
    kref_put(&result->ref, result_release);
}
```

创建者持有初始引用。若把结果共享给异步消费者，应在发布前 `kref_get(&result->ref)`，发布失败则撤销新增引用；消费者和创建者各自在结束时调用 `result_put()`。若交接的是唯一所有权，也可直接转交已有引用，但必须明确发送方之后不再使用。`value` 在发布后保持不变。

## 查找和解绑

```text
持有注册表锁 → 查找对象 → 确认未离线并取得引用 → 解锁 → 使用 → put
解绑：持同一锁标记离线并摘表 → 解锁 → 放弃注册表持有的引用
```

注册表本身应持有引用，且所有查找与摘表遵守同一协议。若使用 RCU，还需保证计数所在内存在读侧临界区内有效，并配套延迟回收；不能只把锁换成 `kref_get_unless_zero()`。

- release 在最后 put 的调用上下文执行；若可能来自 IRQ，不能在其中调用睡眠式销毁操作。
- 不要让对象的最终 release 与 devm 自动回收同时释放同一块内存；解绑后仍可能被持有的对象通常需要独立分配和离线处理。
- 引用计数不保护 `value` 等可变字段，也不自动阻止离线设备的寄存器访问；相关状态仍需锁和访问检查。

## 源码与参考

- [kref.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/kref.h)：初始化、get/put 和释放上下文。
- [Kref 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/kref.rst)：发布前取引用、查找竞态和所有权规则。
