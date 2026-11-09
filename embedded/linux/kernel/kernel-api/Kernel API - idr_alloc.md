# Linux `idr_alloc()`：ID 分配、查找与对象生命周期

> IDR 将整数 ID 映射到指针；分配映射、保护并发和管理对象生命周期是三件需要分别保证的事。

## TL;DR

- `idr_alloc()` 成功返回非负 ID，失败返回负错误码；`0` 是有效 ID，不能按布尔值判断成功。
- 范围为 `[start, end)`，正的 `end` 不包含在内；`end <= 0` 时上限为 `INT_MAX`。
- 所有修改由调用方串行化；删除 ID 不等于回收所有已经取得对象指针的读者。
- **6.18.62 文档已将 IDR 接口标为弃用，新设计优先评估 XArray；只分配编号、不关联指针时考虑 IDA。**

源码基准：Linux 6.18 LTS / 6.18.62。本文用于理解与维护现有 IDR 代码。

## 接口与返回值

```c
int idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp);
```

| 参数或结果 | 语义 |
| --- | --- |
| `idr` | 经 `idr_init()` 或 `DEFINE_IDR()` 初始化的映射表 |
| `ptr` | 关联对象；可用 `NULL` 预留 ID，但简单 `idr_find()` 无法区分这种预留和无映射 |
| `start` | 包含的最小 ID；负值会触发警告并返回 `-EINVAL` |
| `end` | 正值为不包含的上界；`end <= 0` 不是“空范围” |
| `gfp` | 内部节点分配方式，必须符合调用上下文 |
| `>= 0` | 分配成功，ID 后续可以复用，不是永久对象身份 |
| `-ENOSPC` / `-ENOMEM` | 范围无可用编号 / 内存分配失败 |

`idr_alloc_u32()` 的最大 ID 参数为**包含上界**，且返回 `0` 表示成功、ID 由输出参数传出，不能照搬 `idr_alloc()` 的判断方式。

## 示例：任务上下文中由 mutex 保护

```c
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/types.h>

struct registry {
    struct mutex lock;
    struct idr objects;
};

static void registry_init(struct registry *registry)
{
    mutex_init(&registry->lock);
    idr_init(&registry->objects);
}

static int registry_add(struct registry *registry, void *object)
{
    int id;

    mutex_lock(&registry->lock);
    id = idr_alloc(&registry->objects, object, 1, 1024, GFP_KERNEL);
    mutex_unlock(&registry->lock);
    return id;
}

static void *registry_remove(struct registry *registry, int id)
{
    void *object;

    mutex_lock(&registry->lock);
    object = idr_remove(&registry->objects, id);
    mutex_unlock(&registry->lock);
    return object;
}
```

示例分配 `1..1023`，允许在 mutex 内以 `GFP_KERNEL` 分配，所有查找和修改路径都遵守同一锁规则。对象须在发布前初始化；若其他访问者需要通过 `object->id` 读取编号，应在仍持有发布锁时写入，不能先暴露对象再初始化 ID。

查找后若要在解锁后继续使用对象，必须在保护范围内取得对象引用，或采用其他明确的生命周期方案；仅复制一个裸指针不够。示例的 remove 只移除映射并交回指针，不会释放对象，也不保证已有使用者已经结束。

## 自旋锁、RCU 与销毁

- 在不可睡眠的自旋锁临界区不能使用 `GFP_KERNEL`。现有 IDR 代码可在锁外 `idr_preload(GFP_KERNEL)`，锁内用合适的非睡眠分配标志执行 `idr_alloc()`，解锁后 `idr_preload_end()`；预加载不免除错误检查和目标内核上下文约束。
- `idr_find()` 可以在 `rcu_read_lock()` 内无锁查找，但写者仍须串行化，对象也要符合 RCU 发布与回收规则。移除后通常通过延迟回收或等待 grace period 才释放；RCU 读锁外继续使用还需要引用计数等协议。
- `idr_replace()` 可替换映射，错误返回为错误指针，不能按分配 ID 的整数返回值处理。
- 停止新增和查找、移除映射、等待旧使用者、释放各对象，最后 `idr_destroy()`。`idr_destroy()` 只回收 IDR 自身结构，不会释放其指向的对象。
- ID 可能被再次分配；异步请求若只保存旧整数 ID，必须防止误操作后来占用同一 ID 的新对象。

排查时区分范围耗尽、节点分配失败、写者并发和对象释放过早。迁移到 XArray 时重新审查编号范围、返回值、锁和回收协议，不能只按函数名机械替换。

## 源码与参考

- [6.18.62 ID 分配文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/idr.rst)：弃用建议、IDR/IDA 和清理规则。
- [lib/idr.c](https://github.com/gregkh/linux/blob/v6.18.62/lib/idr.c)、[include/linux/idr.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/idr.h)：范围、错误码、同步及 RCU 生命周期。
- 相关：[spin_lock_irqsave](Kernel%20API%20-%20spin_lock_irqsave.md)。
