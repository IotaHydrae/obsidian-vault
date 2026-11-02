# Linux Kprobe 定位内核执行路径

> Kprobe 可在受支持的内核位置安装探针并运行回调；符号、架构、内核配置和可探测范围均有约束。

## 最小模块示例

```c
#include <linux/kprobes.h>
#include <linux/module.h>

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    pr_info("kprobe hit: %p\n", p->addr);
    return 0;
}

static struct kprobe kp = {
    .symbol_name = "target_function_name",
    .pre_handler = handler_pre,
};

static int __init probe_init(void)
{
    return register_kprobe(&kp);
}

static void __exit probe_exit(void)
{
    unregister_kprobe(&kp);
}

module_init(probe_init);
module_exit(probe_exit);
MODULE_LICENSE("GPL");
```

将目标名称替换为目标内核中可探测的符号，在匹配内核头文件及配置下构建。此处未加载模块或验证目标符号。回调可能处于不能睡眠的上下文，应控制开销并避免递归；生产系统先评估风险。

经典实现可用断点触发回调并执行原指令，但优化探针和架构实现可能不同。在 ARM64 上，`regs->regs[0]` 可对应第一个参数，具体含义取决于调用约定和探针位置。修改寄存器或指令指针前，须核对回调返回值、架构限制和目标内核 Kprobes 文档。
