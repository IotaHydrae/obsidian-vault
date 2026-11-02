# Linux VMA 与 `find_vma()`

> VMA 是地址空间中一段具有共同映射属性的虚拟地址区间；它并不保证对应物理页已经分配。

- `vm_start` / `vm_end` 描述左闭右开的地址范围 `[start, end)`；`vm_flags` 表示映射权限和属性。
- `vm_file` 可关联文件映射；匿名映射通常没有对应文件。`vm_ops` 可提供缺页等相关操作。
- 同一文件、堆或栈不保证始终对应唯一 VMA；映射可以因权限变化等被拆分或合并。

## 查看与查找

```bash
cat /proc/<pid>/maps
```

访问受权限和系统配置限制。内核中 `find_vma(mm, addr)` 找到的是第一个满足 `addr < vm_end` 的 VMA，可能位于 `addr` 之后；要判断地址是否实际落在该 VMA，还要检查 `vm_start <= addr`。操作 VMA 时遵守目标内核版本的锁和生命周期要求。

Linux 4.19 的实现使用 VMA 缓存和红黑树；其他版本的查找实现及数据结构需查看目标源码。地址空间整体结构见 [[mm_struct]]。
