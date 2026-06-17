```c
.syntax unified
.thumb
.cpu cortex-m0plus

.global isr_svcall
.type isr_svcall, %function
isr_svcall:
        /* Restore the context. "current" points to the ready-to-run TCB. */
        ldr r3, =current /* the address of "current" var : r3 = &current */
        ldr r1, [r3] /* r1 = current */
        ldr r0, [r1] /* r0 = *current */
        ldmia r0!, {r4-r7}

        /* arm cortex-m0plus doesn't support ldmia with r8~r11 regs */
        mov r4, r8
        mov r5, r9
        mov r6, r10
        mov r7, r11
        ldmia r0!, {r4-r7}

        /* restore the task stack pointer (PSP) */
        msr psp, r0
        isb /* Instruction Synchronization Barrier, flush pipeline */

        bx lr
```