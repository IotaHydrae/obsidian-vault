
在 ARM 架构中，CPU 访问一个虚拟地址（VA）并将其转化为物理地址（PA）的过程，是一场 **MMU（内存管理单元）** 硬件与 **`mm_struct`** 数据结构之间的接力赛。

以现代 **ARMv8-A (AArch64)** 架构、4KB 页大小、48 位虚拟地址空间为例，这个过程通常涉及 **4 级页表** 查询。

### 1. 根基：`mm_struct->pgd`

`mm_struct->` 指向**全局页目录**（Page Global Directory）。这是硬核所在，它指向了该进程的物理页表。

当 Linux 切换进程时，内核会将新进程的 `mm_struct->pgd` 写入 ARM 的系统寄存器 **`TTBR0_EL1`** (Translation Table Base Register 0)。

- **`TTBR0_EL1`**：存放用户空间的页表基地址。
    
- **`TTBR1_EL1`**：存放内核空间的页表基地址（Linux 巧妙地将地址空间一分为二）。