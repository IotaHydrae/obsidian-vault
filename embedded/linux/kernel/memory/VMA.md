# Linux VMA、Maple Tree 与 `find_vma()`

> VMA 描述一段具有共同属性的虚拟地址区间；它存在不代表物理页已分配，也不保证地址落在 `find_vma()` 返回的区域内。

## TL;DR

- VMA 使用左闭右开区间 `[vm_start, vm_end)`；映射权限与后备对象通过标志、`vm_file` 和相关操作描述。
- Linux 6.18 的 `find_vma()` 返回包含该地址的 VMA **或下一个 VMA**；判断命中仍要检查 `addr >= vm_start`。
- 读取 VMA 时必须遵守 mm 生命周期和锁要求；释放保护后不能继续随意使用 VMA 指针。
- VMA、页表、驻留物理页是不同层次：按需分配、COW 和文件映射可能导致稍后缺页。

范围：Linux 6.18.62 LTS，普通用户进程地址空间。

## VMA 表示什么

| 字段 / 对象 | 意义 |
| --- | --- |
| `vm_start`、`vm_end` | 左闭右开的虚拟地址范围 |
| `vm_flags` | 区域权限、共享性等标志；修改需遵守内核接口和锁规则 |
| `vm_file` | 相关文件映射；匿名映射通常没有普通文件后备 |
| `vm_pgoff` | 映射在后备对象中的页偏移，解释取决于映射类型 |
| `vm_ops` | 特定映射支持的操作，例如适用时处理 fault |

同一个文件、heap 或 stack 不保证对应唯一 VMA；权限变化、映射建立或删除可以导致区域拆分、合并或消失。

## `find_vma()` 的边界

给定区域 `[0x1000, 0x2000)` 和 `[0x4000, 0x5000)`：

| 查找地址 | 返回结果 | 地址是否在返回 VMA 内 |
| --- | --- | --- |
| `0x1800` | 第一个区域 | 是 |
| `0x3000` | 第二个区域 | 否，地址在空洞中 |
| `0x5000` | 后面无区域时返回 `NULL` | 否；结束地址不属于区间 |

6.18.62 的 `find_vma()` 在持有适用 mmap 锁的前提下，通过 `mm->mm_mt` 查找；普通读取可使用 `mmap_read_lock()`。以下示例只返回判断结果，不把 VMA 指针带出锁；调用者必须先保证 `mm` 有效，且处于允许睡眠的上下文：

```c
#include <linux/mm.h>
#include <linux/mmap_lock.h>

static bool address_has_vma(struct mm_struct *mm, unsigned long addr)
{
    struct vm_area_struct *vma;
    bool found;

    mmap_read_lock(mm);
    vma = find_vma(mm, addr);
    found = vma && addr >= vma->vm_start;
    mmap_read_unlock(mm);
    return found;
}
```

此结果只表示查询时存在覆盖该地址的 VMA，不证明读写权限、页表映射、页面驻留或后续访问仍会成功。不同操作可能需要写锁、VMA 锁或其他约束；不能把读锁当作修改任意 VMA 字段的许可。

## 用户态观察

```sh
target_pid=1234  # 改成实际进程号
cat "/proc/$target_pid/maps"
cat "/proc/$target_pid/smaps"
```

访问受权限和配置限制。`maps` 是区域及权限快照，`smaps` 提供更多内存统计，均不能保证数据在读取后保持不变。缺页和 COW 见 [[Linux 缺页与写时复制]]，页表翻译见 [[virt to phy]]，地址空间对象见 [[mm_struct]]。

## 参考

- Linux 6.18.62：[`mm/mmap.c`](https://github.com/gregkh/linux/blob/v6.18.62/mm/mmap.c) 的 `find_vma()`、[进程地址空间和锁](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/mm/process_addrs.rst)、[`mm_types.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/mm_types.h)。
- 蜗窝科技：[内存管理基本概念](https://www.wowotech.net/memory_management/concept.html)、[逆向映射](https://www.wowotech.net/memory_management/reverse_mapping.html)。
