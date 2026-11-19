# Linux `mm_struct` 与进程地址空间

> `mm_struct` 描述一个用户虚拟地址空间；多个任务可以共享它。VMA 记录区域语义，页表记录地址翻译，两者不能互相替代。

## TL;DR

- `task_struct->mm` 指向任务的用户地址空间；`CLONE_VM` 任务共享同一 `mm_struct`。
- Linux 6.18 使用 `mm_mt` 保存 VMA 索引；`pgd` 指向内核持有的顶级页表对象。
- `mm_users` 与 `mm_count` 对应不同层次的生命周期；持有对象引用不代表可以无锁访问 VMA。
- 内核线程通常没有自己的用户 `mm`；`active_mm` 与调度时地址空间切换有关。

范围：Linux 6.18.62 LTS，启用 MMU 的常见系统。

## 对象关系

```text
task_struct A ─┐
               ├→ mm_struct → mm_mt → VMA 区间与映射属性
task_struct B ─┘            → pgd   → 多级页表 / CPU 地址翻译
```

使用 `CLONE_VM` 的任务共享地址空间；普通 `fork()` 通常为子进程建立独立的 `mm_struct`，适用的私有映射采用写时复制。共享映射和 `CLONE_VM` 不应一概套用私有 COW 的结论。`exec` 和进程退出也会改变地址空间的关联和生命周期。

| 成员 | 含义与边界 |
| --- | --- |
| `mm_mt` | Maple Tree：组织该地址空间中的 VMA；访问需满足相应锁和生命周期规则 |
| `mmap_lock` | 整个地址空间的读写同步之一；VMA / 页表还有其他细粒度锁 |
| `pgd` | 内核持有的顶级页表指针；CPU 的物理页表基址由架构切换代码处理 |
| `mm_users` | 用户地址空间的使用者计数；归零后释放地址空间相关资源 |
| `mm_count` | `mm_struct` 对象的引用计数；`mm_users` 整体占一个引用 |
| `total_vm` | 虚拟地址空间页数统计，不等于驻留物理页数 |
| `start_code` / `end_code`、`start_data` / `end_data`、`start_brk` / `brk`、`start_stack` | 记录代码、数据、heap、stack 等边界，不替代读取实际映射 |

## 引用与同步

| 手头已有的对象 | 下一步操作 | 能保证什么 |
| --- | --- | --- |
| 持有有效 `task_struct` 引用，要访问该任务的地址空间 | `get_task_mm()`；非 `NULL` 时最终 `mmput()` | 在任务锁下取到 `mm_users` 引用，保护取到的 mm；`exec` 后该任务可能已换成另一个 mm |
| 已有有效的 mm，只需保留 `mm_struct` 对象 | `mmgrab()` / `mmdrop()` | 持有 `mm_count`；`mm_users` 可归零，不能据此访问其用户映射 |
| 只有 `mm_count` 引用，想使用尚未退出的地址空间 | `mmget_not_zero()` 成功后配对 `mmput()`；失败则停止访问 | 不会把已归零的 `mm_users` 重新复活 |

拿到 `mm_users` 也要按操作获取 `mmap_lock` 或其他适用保护，才能访问 VMA。不能裸读 `task->mm` 后直接加引用，也不能用对象引用替代映射同步。

VMA 的查找、有效区间和锁见 [[VMA]]；ARM64 页表与地址转换见 [[virt to phy]]；缺页、COW 和实际物理页建立见 [[Linux 缺页与写时复制]]。

## 观察与常见误解

- `/proc/<pid>/maps` 展示 VMA 范围和权限，不能证明每个地址已对应驻留物理页。
- 同一 VMA 内可能存在未分配页面、只读 COW 页面或尚未填充的文件页；访问结果取决于页表、权限和缺页处理。
- ELF `.bss` 不能简单等同于 `[end_data, start_brk]`；对齐、加载布局和随机化会影响边界，应结合 ELF program headers 和实际 maps。解析 `/proc/<pid>/stat` 时应按接口说明核对字段号。
- 内核线程的 `mm == NULL` 不表示 CPU 没有地址空间；`active_mm` 的具体切换遵循架构和内核调度实现。

## 参考

- Linux 6.18.62：[`mm_types.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/mm_types.h)、[进程地址空间](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/mm/process_addrs.rst)、[`sched/mm.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/sched/mm.h)。
- [`kernel/fork.c`](https://github.com/gregkh/linux/blob/v6.18.62/kernel/fork.c) 的 `get_task_mm()`、`mmput()`。
- 蜗窝科技：[内存管理基本概念](https://www.wowotech.net/memory_management/concept.html)、[逆向映射](https://www.wowotech.net/memory_management/reverse_mapping.html)。
