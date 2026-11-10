# Linux XArray：编号分配与对象索引

> XArray 管理“整数索引到条目”的映射；数组内部锁保护树结构，不会自动延长条目所指对象的生命周期。

## TL;DR

- 新设计需要 ID 到对象映射时优先评估 XArray；只分配整数 ID 可用 IDA。
- `xa_alloc()` 要用带 `XA_FLAGS_ALLOC` 的数组；成功返回 0，ID 经 `u32 *id` 输出。
- `XA_LIMIT(min, max)` 两端都包含，与 IDR 的 exclusive end 不同。
- `xa_store()` 的编码错误用 `xa_err()` 检查；不要套 `IS_ERR()` / `PTR_ERR()`。

源码基准：Linux 6.18 LTS / 6.18.62。

## 接口与错误

| 接口 | 返回与边界 |
| --- | --- |
| `xa_init()` / `DEFINE_XARRAY()` | 普通数组，用于已知索引的存储 |
| `xa_init_flags(xa, XA_FLAGS_ALLOC)` / `DEFINE_XARRAY_ALLOC()` | 支持分配 ID 的数组 |
| `xa_alloc(xa, &id, entry, limit, gfp)` | 成功 0；内存不足 `-ENOMEM`；区间已满 `-EBUSY` |
| `xa_store(xa, index, entry, gfp)` | 返回旧条目或编码错误；`xa_err(ret)` 为 0 或负错误 |
| `xa_load()` / `xa_erase()` | 返回条目或 NULL；erase 还释放该索引 |
| `xa_destroy()` | 释放数组内部存储，不释放条目指向的对象 |

默认普通 API 自行加锁；`GFP_KERNEL` 操作可能睡眠。不要先拿 `xa_lock` 再调用会重复拿锁的普通 API。需要扩大内部临界区时，必须理解 advanced API、内存分配及上下文约束。

## 示例：任务上下文的注册与取走

```c
#include <linux/mutex.h>
#include <linux/xarray.h>

struct object_registry {
    struct mutex lock;
    struct xarray objects;
};

static void registry_init(struct object_registry *registry)
{
    mutex_init(&registry->lock);
    xa_init_flags(&registry->objects, XA_FLAGS_ALLOC);
}

static int registry_add(struct object_registry *registry, void *object, u32 *id)
{
    int ret;

    mutex_lock(&registry->lock);
    ret = xa_alloc(&registry->objects, id, object,
                   XA_LIMIT(1, 4095), GFP_KERNEL);
    mutex_unlock(&registry->lock);
    return ret;
}

static void *registry_take(struct object_registry *registry, u32 id)
{
    void *object;

    mutex_lock(&registry->lock);
    object = xa_erase(&registry->objects, id);
    mutex_unlock(&registry->lock);
    return object;
}
```

成功注册时把对象所有权交给注册表，失败时仍归调用者；take 成功把所有权转给调用者。示例区间为 1～4095，实际按协议选择。条目须为符合 XArray 编码要求的有效、对齐的非 NULL 对象指针，不能随意存 `ERR_PTR()`。全部初始化须在发布前完成。

## 生命周期与迁移陷阱

- 若增加非破坏性 lookup，需在同一外部锁下完成 `xa_load()` 与 [kref_get](Kernel%20API%20-%20kref.md)，再解锁使用；删除方也须遵守同一锁和引用协议。
- `xa_load()` 内部的树访问保护不让返回对象永久存活。只拿到裸指针、解锁后再取引用可能已经太晚。
- 分配模式下存 NULL 可以保留索引；释放 ID 用 `xa_erase()`，不要把 `xa_store(..., NULL, ...)` 当作通用释放。
- `xa_destroy()` 前先关闭注册/查找入口、取走并释放剩余对象；destroy 不会调用业务析构函数。
- 与 [idr_alloc](Kernel%20API%20-%20idr_alloc.md) 对照：IDR 成功直接返回 ID，XArray 成功返回 0；迁移还需检查编号上界和同步协议。

## 源码与参考

- [xarray.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/xarray.h)：alloc flags、返回值与包含上界。
- [XArray 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/xarray.rst)：普通/advanced API、条目编码和对象生命周期。
