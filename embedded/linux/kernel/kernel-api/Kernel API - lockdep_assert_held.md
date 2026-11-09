# Linux 内核 `lockdep_assert_held()`

> 在要求调用者已经持锁的 helper 中加入 `lockdep_assert_held()`，把锁约束转为可检查的断言；它不会加锁，也不能在运行时替代同步协议。

## TL;DR

- 接收锁对象指针，例如 `lockdep_assert_held(&state->lock)`。
- 检查的是当前执行上下文的持锁记录，不是“某个 CPU 正持有这把锁”。
- 需要 lockdep 配置及运行中的跟踪支持；禁用时断言不提供保护，不能作为权限判断或分支条件。

源码基准：Linux 6.18 LTS / 6.18.62。

## 示例：明确 helper 的持锁前提

```c
#include <linux/lockdep.h>
#include <linux/mutex.h>

struct device_state {
    struct mutex lock;
    unsigned int count;
};

static void state_add_locked(struct device_state *state)
{
    lockdep_assert_held(&state->lock);
    state->count++;
}

static void state_add(struct device_state *state)
{
    mutex_lock(&state->lock);
    state_add_locked(state);
    mutex_unlock(&state->lock);
}
```

示例要求对象发布前完成 `mutex_init()` 和字段初始化。`_locked` 表达“调用者必须持锁”，断言用于发现漏锁调用；不能在 helper 内再次获取同一个非递归锁来补救错误调用。

## 实现与相关断言

6.18.62 中，启用 `CONFIG_LOCKDEP` 时，宏通过 `lockdep_is_held()` 检查当前持锁记录，并在 lockdep 跟踪仍有效而条件不满足时告警；关闭该配置时宏不会执行实际的持锁校验。告警通常不会阻止后续语句继续执行。

| 接口 | 表达的约束 |
| --- | --- |
| `lockdep_assert_held()` | 持有指定锁 |
| `lockdep_assert_not_held()` | 不持有指定锁 |
| `lockdep_assert_held_write()` / `_read()` | 需要特定读写持锁方式，按实际锁类型使用 |
| `lockdep_assert_held_once()` | 对应持锁断言使用一次性告警形式 |

对可读可写的锁，修改数据时不要只依赖不能区分读写模式的一般持锁断言。断言是否可用于某种锁类型，还需核对该类型的 lockdep 接口与实际配置。

## 验证与局限

调试内核启用 `CONFIG_PROVE_LOCKING` 等相应配置，在正常测试中覆盖每个 helper 的调用路径；发现告警时检查完整调用栈和锁的获取/释放位置。不要在真实设备上专门触发有破坏性的数据竞争来证明断言有效。

- 无告警不证明所有访问者都加锁，也不证明没有未覆盖的锁顺序错误。
- 锁已经被其他 CPU 持有，不代表当前调用者可以访问受保护数据。
- `spin_is_locked()` 检查某一时刻的锁状态，不能替代当前上下文持锁证明。
- 即使持锁断言通过，也仍须遵守不可睡眠、IRQ 上下文和对象生命周期规则。

## 源码与参考

- [6.18.62 lockdep.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/lockdep.h)：断言展开及禁用配置下的形式。
- [Lockdep 设计](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/locking/lockdep-design.rst)：锁依赖模型及覆盖范围。
- 相关：[spin_lock_irqsave](Kernel%20API%20-%20spin_lock_irqsave.md)。
