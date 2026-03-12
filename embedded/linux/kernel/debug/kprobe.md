Kprobes（Kernel Probes）是 Linux 内核的一种调试机制，它能让你在几乎任何内核指令处设置“陷阱”，而无需重启或修改内核源码。

### 1. 注册与备份 (Registration)

当你指定要探测的函数地址时，内核会先做两件事：

- **读取原始指令**：记录该地址处原本的指令内容。
    
- **保存副本**：将原始指令复制到一个专门的缓冲区（Slot）中，以便后续恢复执行。
    

### 2. 注入“断点” (Breakpoint Insertion)

这是最关键的一步。Kprobes 会利用 CPU 的断点指令（在 x86 架构上通常是 `int3` 指令，大小只有 1 字节）来覆盖目标位置的原始指令。

### 3. 触发陷阱 (The Trap)

当内核执行流到达这个位置时：

1. **触发异常**：CPU 执行到 `int3`，引发一个断点异常（Breakpoint Exception）。
    
2. **夺取控制权**：CPU 暂停正常业务，跳转到内核的异常处理程序。
    
3. **执行 Pre-handler**：Kprobes 找到对应的钩子，执行你自定义的 `pre_handler` 函数。这时候你可以读取寄存器、查看变量等。

### 4. 单步执行与恢复 (Single-stepping)

处理完你的逻辑后，内核需要让程序继续跑下去，但断点处已经被改成了 `int3`，原始指令不见了。

- **单步执行副本**：Kprobes 会让 CPU 去执行之前备份在缓冲区里的那条“原始指令”。
    
- **执行 Post-handler**：如果定义了 `post_handler`，它会在原始指令执行完后立即触发。
    

### 5. 归位 (Resuming)

最后，内核将指令指针（IP）指向断点之后的下一条指令，让系统像什么都没发生过一样继续运行。


## Example

```c
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>

/* 定义我们的探测实例 */
static struct kprobe kp = {
    .symbol_name = "do_execve", // 想要钩住的函数名
};

/* 在目标指令执行前运行 */
static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
    /* 在 x86_64 架构下，do_execve 的第一个参数通常在 di 寄存器中 */
    pr_info("<kprobe> pre_handler: p->addr = 0x%p, ip = %lx, flags = 0x%lx\n",
            p->addr, regs->ip, regs->flags);
    return 0;
}

/* 在目标指令执行后运行 */
static void handler_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags) {
    pr_info("<kprobe> post_handler: p->addr = 0x%p, flags = 0x%lx\n",
            p->addr, regs->flags);
}

static int __init kprobe_init(void) {
    kp.pre_handler = handler_pre;
    kp.post_handler = handler_post;

    int ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("register_kprobe failed, returned %d\n", ret);
        return ret;
    }
    pr_info("Planted kprobe at %p\n", kp.addr);
    return 0;
}

static void __exit kprobe_exit(void) {
    unregister_kprobe(&kp);
    pr_info("kprobe at %p unregistered\n", kp.addr);
}

module_init(kprobe_init)
module_exit(kprobe_exit)
MODULE_LICENSE("GPL");
```

### LivePatch

```c

```