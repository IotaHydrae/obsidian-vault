# Linux List：嵌入式双向链表与节点所有权

> `list_head` 只维护节点之间的链接；并发互斥、对象释放和摘链后的所有权必须由调用者定义。

## TL;DR

- 链表头和需要独立存在的节点都用 `INIT_LIST_HEAD()` 初始化。
- `list_add_tail()` 入队，`list_del_init()` 摘链并恢复自链接状态。
- `list_for_each_entry_safe()` 允许当前遍历者删除当前节点，不提供跨 CPU 并发安全。
- 同一个 `list_head` 成员同一时刻只能属于一条链表；多条链表需要不同成员。

源码基准：Linux 6.18 LTS / 6.18.62。

## 常用接口

| 接口 | 用途 |
| --- | --- |
| `LIST_HEAD(name)` / `INIT_LIST_HEAD()` | 静态/运行时初始化循环链表 |
| `list_add()` / `list_add_tail()` | 插入头部/尾部 |
| `list_empty()` | 判断是否为空，仍需遵守同步协议 |
| `list_first_entry()` | 取得首个外层对象，要求链表非空 |
| `list_del_init()` | 摘链并将节点重新初始化 |
| `list_for_each_entry_safe()` | 保存下一个节点，供本次遍历删除当前项 |

节点嵌入业务对象，通过 [container_of](Kernel%20API%20-%20container_of.md) 一类转换找回外层对象。插入和删除修改多个指针，不能把它们视为天然原子操作。

## 示例：锁保护的队列出队

```c
#include <linux/list.h>
#include <linux/mutex.h>

struct request {
    struct list_head node;
    unsigned int command;
};

struct request_queue {
    struct mutex lock;
    struct list_head pending;
};

static void request_queue_init(struct request_queue *queue)
{
    mutex_init(&queue->lock);
    INIT_LIST_HEAD(&queue->pending);
}

static void request_enqueue(struct request_queue *queue, struct request *req)
{
    mutex_lock(&queue->lock);
    list_add_tail(&req->node, &queue->pending);
    mutex_unlock(&queue->lock);
}

static struct request *request_dequeue(struct request_queue *queue)
{
    struct request *req = NULL;

    mutex_lock(&queue->lock);
    if (!list_empty(&queue->pending)) {
        req = list_first_entry(&queue->pending, struct request, node);
        list_del_init(&req->node);
    }
    mutex_unlock(&queue->lock);
    return req;
}
```

新请求在发布前初始化 `req->node` 和数据。约定入队时把对象所有权交给队列，禁止重复入队；出队时把所有权转给调用者，由调用者处理并最终释放。其他路径不得保留无保护的裸指针访问对象。此例仅适用于可睡眠上下文。

## 删除、遍历与释放

- `list_del()` 可能将指针写成 poison，不能随意再检查其 `list_empty()`；需要可复用节点时用 `list_del_init()`。
- safe 遍历期间若释放锁，其他 CPU 仍可能删除并释放已保存的“下一个节点”；宏名中的 safe 不覆盖这种竞态。
- 退出时先禁止新入队，再锁住或独占地摘除剩余节点，最后按所有权释放对象。
- 锁只保护持锁期间的链接和状态；出锁后继续共享对象需要额外的 [引用计数](Kernel%20API%20-%20kref.md)。RCU 链表有独立读写和回收协议，不能只换一个遍历宏。

## 源码与参考

- [list.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/list.h)：插入、删除、外层对象转换和遍历宏。
- [List 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/list.rst)：嵌入式链表、迭代与并发注意事项。
