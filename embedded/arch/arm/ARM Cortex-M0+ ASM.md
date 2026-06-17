## AAPCS func call example

```c
int __attribute__((noinline)) add_val(int a, int b, int c, int d)
{
	return a + b + c + d;
}
```

```c
10000468 <add_val>:
10000468:       1840            adds    r0, r0, r1
1000046a:       1880            adds    r0, r0, r2
1000046c:       18c0            adds    r0, r0, r3
1000046e:       4770            bx      lr
```

```c
volatile int __attribute__((noinline)) mymain()
{
	volatile int a = 1;
	volatile int b = 2;
	volatile int c = 3;
	volatile int d = 4;
	volatile int sum;
	sum = add_val(a, b, c, d);
	return 0;
}
```

```c
10000470 <mymain>:
10000470:       2301            movs    r3, #1
10000472:       b500            push    {lr}
10000474:       b087            sub     sp, #28
10000476:       9301            str     r3, [sp, #4]
10000478:       3301            adds    r3, #1
1000047a:       9302            str     r3, [sp, #8]
1000047c:       3301            adds    r3, #1
1000047e:       9303            str     r3, [sp, #12]
10000480:       3301            adds    r3, #1
10000482:       9304            str     r3, [sp, #16]
10000484:       9801            ldr     r0, [sp, #4]
10000486:       9902            ldr     r1, [sp, #8]
10000488:       9a03            ldr     r2, [sp, #12]
1000048a:       9b04            ldr     r3, [sp, #16]
1000048c:       f7ff ffec       bl      10000468 <add_val>
10000490:       9005            str     r0, [sp, #20]
10000492:       2000            movs    r0, #0
10000494:       b007            add     sp, #28
10000496:       bd00            pop     {pc}
```