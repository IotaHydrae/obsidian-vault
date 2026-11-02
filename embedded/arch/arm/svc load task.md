# Cortex-M0+：恢复任务寄存器与 PSP

> 在异常上下文中恢复软件保存的 `r4`–`r11`，再令 PSP 指向硬件栈帧，通过匹配的 `EXC_RETURN` 返回。

## 栈布局约定

以下示例约定 `r0` 指向软件帧，低地址到高地址依次为：

```text
r4 r5 r6 r7 | r8 r9 r10 r11 | r0 r1 r2 r3 r12 lr pc xpsr
<-------- 软件保存 ------->   <------ 硬件基本帧 -------->
```

这是自定义布局，必须与保存路径一致，不是所有 RTOS 的统一格式。Cortex-M0+ 的 `ldmia` 不能直接加载 `r8`–`r11`，需要通过低寄存器中转。

```asm
.syntax unified
.thumb
.cpu cortex-m0plus
.global restore_task_context
.type restore_task_context, %function
.thumb_func
restore_task_context:
    mov r2, r0
    adds r2, #16
    ldmia r2!, {r4-r7}
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    ldmia r0!, {r4-r7}
    msr psp, r2
    isb
    ldr r0, =0xfffffffd
    bx r0
```

该片段只能从适当的 Handler 上下文进入，不能作为普通 C 函数调用。`0xfffffffd` 针对这里假定的 Cortex-M 基本帧，表示返回 Thread mode 并从 PSP 恢复；调度器还需处理异常嵌套、MSP、权限、中断状态和任务生命周期。汇编检查不等于硬件调度验证。

任务初始帧和 LR 的区别见 [[PendSV psp force lr return]]；可对照 [FreeRTOS V11.1.0 Cortex-M0 移植](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.1.0/portable/GCC/ARM_CM0)。
