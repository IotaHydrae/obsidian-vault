当你访问总线上不存在的一片内存地址空间时，会触发 HardFault，我们可以利用这个机制，将一个 spi psram 模拟的映射到总线上，只需要在 Hardfault 的处理程序中，保存现场，以及触发 hardfault 的那一条指令，必须向某一个地址写了某个值，读取某个地址的值，我们解析这些指令，就可以在一个wai

https://github.com/IotaHydrae/rpi-pico-lab/blob/main/psram-mpu/main.c

```
 asm volatile(
        ".syntax unified    \n\t"
        ".global HardFault_Handler \n\t"
        ".func HardFault_Handler \n\t"
        ".type HardFault_Handler function \n\t"
        "HardFault_Handler: \n\t"
        "   mov r0, lr \n\t"
        "   lsrs r0, #3 \n\t"
        "   bcs 1f  \n\t"
		"	mov   r0, sp									\n\t"
		"	b     2f										\n\t"
		"1:													\n\t"
		"	mrs   r0, psp									\n\t"
		"2:													\n\t"
		
		//to emulate-for-write fast, we must assume that PC points somewhere valid
		//otherwise we'd have to take the penalty of switching to out safe mode, and then wrangling the MPU
		//whereas now we can use "hard fault uses default map" mode
		
		// "	ldr		r2, [r0, #4 * 6]						\n\t"
		// "	ldrh	r1, [r2]								\n\t"
		// "	lsrs	r3, r1, #8								\n\t"
		// "	add		pc, r3									\n\t"
		// "	nop												\n\t"
        "   push     {r4, lr}                               \n\t"
        "   b      call_handler                             \n\t"
		// ".rept 35											\n\t"
		// "	b		report_some_fault						\n\t"
        // ".endr                                              \n\t"
        "call_handler:                                      \n\t"
        "	mov		r12, r0									\n\t"
		"	mov		r0, r8									\n\t"
		"	mov		r1, r9									\n\t"
		"	mov		r2, r10									\n\t"
		"	mov		r3, r11									\n\t"
		"	push	{r0-r7}									\n\t"
		"	mov		r0, r12									\n\t"
		"	mov		r1, sp									\n\t"
		"	mov		r2, lr									\n\t"
        "   ldr     r3, =hard_fault_handler_c               \n\t"
        "   blx      r3                                      \n\t"

        "   pop     {r0-r7}                                 \n\t"
		"	mov		r8,  r0									\n\t"
		"	mov		r9,  r1									\n\t"
		"	mov		r10, r2									\n\t"
		"	mov		r11, r3 								\n\t"

        "   pop     {r4}                                    \n\t"
        "   pop     {r3}                                    \n\t"
        "   mov     lr, r3                                  \n\t"
        "   bx      lr                                      \n\t"
```