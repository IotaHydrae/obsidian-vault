# 定位 Linux Kernel Oops

> 先保留完整 Oops 和对应构建的未剥离符号文件，再用故障指令及调用栈定位源码；地址和错误码解释依赖架构。

## 排查步骤

1. 记录内核版本、配置、模块版本、首个错误、寄存器、`Call Trace` 和 taint 信息。优先分析最早的异常，后续异常可能是连锁反应。
2. 获取与运行内核完全匹配、带调试信息的 `vmlinux` 或 `.ko`，并确认交叉工具链和地址随机化情况。
3. 对 `function+0xOFFSET/0xSIZE`，在 GDB 中定位符号加偏移；符号化结果和优化后的源代码可能不完全一一对应。

```gdb
file vmlinux
list *(function_name+0xOFFSET)
disassemble /m function_name
```

也可使用匹配架构的 `objdump -dS vmlinux` 或 `addr2line -e vmlinux <已换算的链接地址>`。运行时绝对地址受 KASLR 和模块加载地址影响，不能直接当作文件内地址。

模块调试需要正确加载对应的符号文件和各节的运行地址；可从 `/sys/module/<name>/sections/` 获取仍存在的节地址，受权限限制且初始化节可能已经释放。

## 读日志

- `IP` / `RIP` / `PC` 是故障指令位置；`symbol+offset/size [module]` 标示符号及模块。
- 调用栈通常从当前帧向调用者排列，但缺失帧和中断上下文会影响解读。
- `Oops: 0002` 等错误码的含义依异常类型与架构而异，须按目标架构手册和内核版本解释。
- `Tainted` 是内核状态线索，不单独证明根因；标志含义以目标内核文档为准。

参考：[Understanding a Kernel Oops](https://opensourceforu.com/2011/01/understanding-a-kernel-oops/)。
