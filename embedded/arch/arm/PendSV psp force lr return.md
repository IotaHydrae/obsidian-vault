```c
void task_init(struct task_struct *task, void (*entry)(void), u32 *sp_top)
{
	u32 *sp = sp_top;

	/* r0, r1, r2, r3, r12, lr, pc, xpsr */
	*(--sp) = 0x01000000;
	*(--sp) = (u32)entry;
	*(--sp) = 0xFFFFFFFD; /* Return to thread mode and use PSP */
	*(--sp) = 0x12; // r12
	*(--sp) = 0x03; // r3
	*(--sp) = 0x02; // r2
	*(--sp) = 0x01; // r1
	*(--sp) = 0; // r0

	/* r4 ~ r11 */
	for (int i = 0; i < 8; i++) {
		// printf("%s, %p\n", __func__, sp);
		*(--sp) = 4 + i;
	}

	// dump_regs(sp);
	task->sp = sp;
}

void isr_pendsv(void)
{
	asm volatile(
		".syntax unified\n\t"
		"mrs r0, psp\n\t"
		"adds r0, r0, #32\n\t"
		"msr psp, r0\n\t"
		"bx lr\n\t"
	);
}

void task1_func()
{
	static uint count = 0;
	for (;;) {
		printf("++++++++++ %s, %04d +++++++++++++++\n", __func__, count++);
		for (volatile uint i = 0; i < 1000000; i++)
			;
	}
}

task_init(&tasks[0], task1_func, task1_stack + STACK_SIZE);

__set_PSP((u32)tasks[0].sp);
__set_CONTROL(0x02);
__ISB();

SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
```