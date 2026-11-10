# Linux Devres：托管资源与清理动作

> Devres 负责在 probe 失败或设备解绑时回收已登记资源；停止设备、阻止新请求和排空异步活动仍由驱动负责。

## TL;DR

- 与设备绑定同寿命的资源优先考虑 `devm_*`，例如 `devm_kzalloc()`、`devm_request_threaded_irq()`。
- 没有 managed 接口的资源，可用 `devm_add_action_or_reset()` 登记清理动作。
- 登记失败时 `_or_reset` 会立即执行动作；此时不要再手工释放同一资源。
- 清理通常按登记顺序的逆序进行；资源依赖必须按这个顺序设计。

源码基准：Linux 6.18 LTS / 6.18.62。

## 实现与接口

每项 devres 带有释放回调，挂到设备的资源链表。失败回滚或解绑时，核心取下资源并调用相应回调，驱动无需在每个错误分支重复清理。

| 接口 | 行为 |
| --- | --- |
| `devm_kzalloc(dev, size, gfp)` | 分配并登记清零内存，失败为 NULL |
| `devm_add_action_or_reset(dev, action, data)` | 登记失败立即执行 action，返回负错误；成功为 0 |
| `devm_remove_action(dev, action, data)` | 移除已登记动作，不执行 |
| `devm_release_action(dev, action, data)` | 移除并执行动作 |

不要用普通 `kfree()` 释放仍登记在 devres 中的内存，也不要随意在 remove 中重复释放 managed IRQ、映射等资源。确需提前释放时使用对应 managed 释放接口。

## 示例：托管动态 FIFO

```c
#include <linux/device.h>
#include <linux/kfifo.h>
#include <linux/slab.h>

struct event_queue {
    struct kfifo fifo;
};

static void event_queue_free(void *data)
{
    struct event_queue *queue = data;

    kfifo_free(&queue->fifo);
}

static int event_queue_alloc(struct device *dev, struct event_queue **out)
{
    struct event_queue *queue;
    int ret;

    queue = devm_kzalloc(dev, sizeof(*queue), GFP_KERNEL);
    if (!queue)
        return -ENOMEM;

    ret = kfifo_alloc(&queue->fifo, 1024, GFP_KERNEL);
    if (ret)
        return ret;

    ret = devm_add_action_or_reset(dev, event_queue_free, queue);
    if (ret)
        return ret;

    *out = queue;
    return 0;
}
```

先登记 `queue` 的内存，再登记 FIFO 清理，因此回收时先释放 FIFO，随后释放外层对象。动作登记失败时 FIFO 已被立即释放，错误路径直接返回。这里的 `struct kfifo` 按字节计数；事件类型化用法见 [kfifo](Kernel%20API%20-%20kfifo.md)。

## 解绑不是自动停止

```text
关闭新请求入口 → 停止硬件/中断源 → 同步 IRQ、work、DMA 等访问者
→ remove 返回 → 托管资源回收
```

- 若必须在 devres 回收前停止某项活动，应在 remove 中同步停止，或登记依赖顺序正确的停止动作。
- 资源注册顺序可能使 IRQ 在缓冲区之后才被释放；不能仅凭“都是 devm”就认定中断不会访问已释放对象。
- 文件句柄、回调或其他使用者若可能跨越解绑继续持有对象，需要独立的引用和离线状态协议；此类对象不能默认随设备解绑释放，见 [kref](Kernel%20API%20-%20kref.md)。

## 源码与参考

- [Devres 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/devres.rst)：释放顺序、适用边界与资源组。
- [device/devres.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/device/devres.h)、[devres.c](https://github.com/gregkh/linux/blob/v6.18.62/drivers/base/devres.c)：动作登记失败的立即清理。
