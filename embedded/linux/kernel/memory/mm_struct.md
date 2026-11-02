# Linux `mm_struct` 与进程地址空间

> `mm_struct` 描述一个用户虚拟地址空间；多个任务可共享它，而 VMA 描述该地址空间中的各个区域。

- 用户任务的 `task_struct->mm` 指向地址空间；使用 `CLONE_VM` 的线程可以共享同一个 `mm_struct`，不是每个任务都独占一个。
- 内核线程通常没有自己的用户 `mm`，其 `mm` 为 `NULL`；`active_mm` 与地址空间切换有关，具体借用和切换行为依赖内核实现。
- `pgd` 是内核持有的顶级页表指针；CPU 使用的页表基址和切换操作依赖架构。VMA 与页表分别描述映射属性和地址转换，不是一回事。
- `start_code`、`end_code`、`start_data`、`end_data`、`start_brk`、`brk`、`start_stack` 等字段记录地址空间相关边界；`total_vm` 是虚拟内存页数统计。

## 版本和边界

Linux 4.19 使用 VMA 链表和红黑树；当前内核文档描述的是 Maple Tree。排查时查看目标版本的 `include/linux/mm_types.h` 和 `mm/` 源码，不要假设 `mmap` / `mm_rb` 字段存在。

普通 `fork()` 通常为子进程建立独立的 `mm_struct`，并对适用的私有映射采用写时复制；页表项和映射类型的细节依赖实现。共享映射和 `CLONE_VM` 不能一概套用这个结论。

ELF 的 `.bss` 不能简单等同于 `[end_data, start_brk]`：对齐、加载布局和随机化等会影响边界。定位时结合 ELF program headers、对应内核加载代码和 `/proc/<pid>/maps`；解析 `/proc/<pid>/stat` 时按接口说明核对字段号。

相关条目：[[VMA]]、[[virt to phy]]。

参考：[Linux 进程地址空间与 VMA](https://docs.kernel.org/mm/process_addrs.html)。
