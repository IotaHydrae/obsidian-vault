# Linux 用户空间数据复制：copy_from_user 与 copy_to_user

> 用户指针属于调用进程的地址空间，不能当作普通内核指针解引用；使用 uaccess 接口并检查完整复制结果。

## TL;DR

- `copy_from_user()` / `copy_to_user()` 返回**未复制的字节数**，成功为 0，不是常规的负 errno。
- 普通 uaccess 可能缺页并睡眠，不能在硬中断、持有自旋锁或其他禁止睡眠的上下文直接调用。
- 用户地址、长度、结构体字段和对象状态都必须验证；`access_ok()` 成功也不保证后续复制一定成功。
- 不应多次直接读取可变的用户数据并假设它不变；先复制、校验，再在需要的锁保护下更新状态。

适用范围：Linux 6.18 LTS，普通进程上下文与一般用户地址。

## 接口与返回值

| 接口 | 用途 | 返回值 |
| --- | --- | --- |
| `copy_from_user(dst, src, n)` | 用户缓冲区复制到内核缓冲区 | 未复制字节数；0 表示全部复制 |
| `copy_to_user(dst, src, n)` | 内核缓冲区复制到用户缓冲区 | 未复制字节数；0 表示全部复制 |
| `get_user()` / `put_user()` | 读取 / 写入单个受支持类型的用户值 | 成功为 0，失败为负错误码 |
| `memdup_user()` | 复制用户缓冲区并分配内核内存 | 成功返回指针，失败返回错误指针；用 `kfree()` 释放 |

`copy_from_user()` 在短复制时会对未复制的目标尾部补零，但**非零返回仍表示没有完整取得输入**；不能把补零后的缓冲区直接当作有效请求。内部或 atomic 变体的语义和前提不同，不能直接用来绕过上下文约束。

## 为什么不能用普通 memcpy

用户指针在用户地址空间中解释；访问可能发生缺页、权限错误或映射改变。uaccess 路径不仅执行复制，还结合架构访问机制和异常修复，使调用者能够处理未复制的字节。普通 `memcpy()` 不提供同等的用户地址访问契约。

例如 Linux 6.18 的 ARM64 uaccess 实现包含特权用户访问的启用 / 禁用处理，以配合 PAN 相关机制；是否使用硬件能力及具体实现取决于平台和内核配置。驱动不应自行打开该保护，或根据某次 `memcpy()` 实验成功就假定直接访问可移植。

用户指针也不应在系统调用返回后直接交给任意 workqueue 延迟解引用：用户映射可能改变，工作线程也未必处于调用进程的地址空间。简单请求通常应先复制到内核持有的缓冲区；需要长期访问用户页面时，应使用相应页面固定、映射与释放机制并设计完整生命周期。

## 一次安全的输入处理

```text
检查调用允许睡眠 → 验证长度及运算范围 → 复制到内核缓冲区
→ 检查未复制字节数 → 校验字段和权限 → 获取需要的锁并更新设备状态
```

示例中，调用者已保证 `dst` 指向可写的 64 字节内核缓冲区；这里只展示复制和长度校验，不负责解释命令或同步设备：

```c
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/uaccess.h>

static int copy_command(char *dst, const void __user *src, size_t len)
{
    if (len > 64)
        return -EMSGSIZE;
    if (copy_from_user(dst, src, len))
        return -EFAULT;
    return 0;
}
```

复制成功只证明取得了指定字节，仍需验证字段范围、版本、字符串终止、设备状态以及调用者权限。复制失败时目标缓冲区可能已经部分改变；示例要求调用者在成功前不把它发布给其他执行路径。

## 输出与并发

向用户空间返回数据时，先在持有正确同步机制的情况下制作内核快照，再释放不能睡眠的锁并调用 `copy_to_user()`；不能在复制过程中假定源数据不会被其他线程修改。若源含 padding 或未初始化字段，应先初始化，避免泄露内核数据。

用户空间可以在调用期间并发改变自己的内存。对长度、偏移或命令结构反复从用户地址读取会形成检查与使用之间的竞态；一次复制到内核后再验证，并根据协议决定是否允许部分输入。

文件 `read()` / `write()` 的部分复制和返回值策略由接口契约决定；ioctl 的固定结构或本示例可把未完成复制作为 `-EFAULT`，不能把这一策略无条件套到所有读写实现。

## 排查

| 现象 | 优先检查 |
| --- | --- |
| `-EFAULT` | 用户地址、权限、缓冲区长度及进程生命周期；检查实际未复制字节数 |
| “sleeping function called from invalid context” | 是否在 spinlock、硬中断或禁止缺页的路径调用普通 uaccess |
| 结构体字段异常 | 两端 ABI / 对齐、长度和完整复制；是否存在并发修改 |
| 输出包含异常字节 | 核对实际长度、padding 初始化和共享数据快照 |

## 参考

- 蜗窝科技：[copy_to_user / copy_from_user 的思考](https://www.wowotech.net/memory_management/454.html)。
- Linux 6.18.62：[`include/linux/uaccess.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/uaccess.h)、[内存管理 API](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/core-api/mm-api.rst)。
- Linux 6.18.62：[ARM64 uaccess](https://github.com/gregkh/linux/blob/v6.18.62/arch/arm64/include/asm/uaccess.h)。
