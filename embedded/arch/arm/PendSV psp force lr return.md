# Cortex-M：任务初始栈与 PendSV 异常返回

> PSP 必须指向与保存/恢复代码一致的栈帧；仅修改 PSP 或将 `0xfffffffd` 填入任务 LR，不能完成上下文切换。

## 区分两种 LR

- **Handler 中的 LR / EXC_RETURN**：描述异常返回模式和栈选择。此 Cortex-M 基本帧场景中的 `0xfffffffd` 表示返回 Thread mode、使用 PSP。
- **硬件栈帧中的 LR**：异常返回后恢复为任务的 LR，应指向合法的任务退出处理，而不是拿来代替 Handler 的 EXC_RETURN。

## 初始帧

硬件基本帧从低地址到高地址为 `r0, r1, r2, r3, r12, lr, pc, xpsr`。首次启动前需要按相同格式构造，并在前面预留调度器的软件保存区域：

- PC 指向任务入口，LR 指向退出处理，xPSR 的 Thumb 状态位正确。
- SP 满足架构和 ABI 的对齐要求，栈空间覆盖初始帧及运行时使用范围。
- 软件保存区的寄存器顺序与恢复代码完全一致；例如 [[svc load task]] 使用 `r4`–`r7`、`r8`–`r11` 的顺序。

## PendSV 切换流程

1. 异常进入时，CPU 根据被中断上下文的栈选择保存硬件帧；Handler 本身使用 MSP。
2. 软件取得任务 PSP，保存硬件未自动保存的寄存器，并记录任务栈指针。
3. 调度器选择下一任务，恢复其软件寄存器，设置 PSP 指向硬件帧。
4. 使用与目标帧一致的 EXC_RETURN 返回，由 CPU 恢复硬件帧。

修改 PSP 前必须同时考虑异常进入已发生的压栈；固定加上 32 字节并直接 `bx lr`，既不能替代完整寄存器恢复，也可能选择错误栈帧。完整实现还需处理中断屏蔽、调度状态和任务退出。

参考：[FreeRTOS V11.1.0 Cortex-M0 初始栈与上下文实现](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.1.0/portable/GCC/ARM_CM0)。
