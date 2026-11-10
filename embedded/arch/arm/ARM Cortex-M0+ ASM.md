# Cortex-M0+ 汇编：AAPCS32 调用、栈帧与返回

> 读懂一次 C 函数调用，要沿着参数寄存器、LR 返回地址和 SP 栈帧三条线看：本例用 `r0`～`r3` 传入四个 `int`，在 `r0` 返回总和，再恢复调用者的栈与返回地址。

## TL;DR

- **Cortex-M0+ 属于 Armv6-M**，执行 Thumb 指令；多数指令为 16 位，`BL` 等少数指令为 32 位，不支持 A32 或 A64。
- **本例遵循 AAPCS32**：四个 `int` 参数依次放入 `r0`～`r3`，`int` 返回值放入 `r0`；寄存器分工是 ABI 约定。
- **`BL` 会覆盖 LR**：`mymain()` 先保存自己的返回地址；叶子函数 `add_val()` 直接 `BX LR` 返回。
- **栈帧共 32 字节**：`PUSH {LR}` 占 4 字节，`SUB SP, #28` 再占 28 字节；调用点保持 8 字节对齐。

范围：GNU 工具链、Cortex-M0+、AAPCS32 基础调用约定。反汇编中的地址、局部变量偏移和指令排列仅对应这一组构建结果，不能当作所有编译器的固定输出。

## 先分清架构寄存器与调用约定

| 寄存器 | 本文需要记住的用途 |
| --- | --- |
| `r0`～`r3` | 参数、结果或临时值；调用者不能假设调用后保持原值 |
| `r4`～`r8`、`r10`～`r11` | 被调用者使用后必须恢复；`r9` 的角色依平台 ABI 而定 |
| `r12` / IP | 临时寄存器，调用后不保证保持；链接器也可能使用 |
| `r13` / SP | 当前栈指针；函数返回时须恢复入口值 |
| `r14` / LR | 链接寄存器，普通调用中保存返回地址 |
| `r15` / PC | 程序计数器；写入合法目标可改变执行位置 |

“前四个参数用四个寄存器”适用于这里的四个 32 位 `int`。64 位值、结构体和隐藏参数等须按 ABI 的大小与对齐规则分配；例如第五个 `int` 参数通常放在**被调函数入口 SP 指向的栈位置**。

Cortex-M0+ 的状态寄存器是 xPSR，其 APSR 部分保存条件标志。它采用 **Thread / Handler** 模式与 MSP/PSP，不采用 A-profile 的 SVC/IRQ 处理器模式或 EL0～EL3；`SVC` 指令触发的是进入 Handler 模式的异常。对照：[[Armv7-A 与 Armv8-A - 处理器模式与异常级]]。

## C 示例：区分两层返回值

```c
__attribute__((noinline)) int add_val(int a, int b, int c, int d)
{
    return a + b + c + d;
}

__attribute__((noinline)) int mymain(void)
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

`noinline` 用于保留可观察的调用边界；局部对象上的 `volatile` 让这里的读写保留为可观察访问，便于分析栈上的加载与存储。它不提供原子性或线程同步，也不能代替内存屏障；修饰标量返回类型没有同样作用。

本例 `add_val()` 返回 **10**，随后写入 `sum`；`mymain()` 自己返回 **0**。若想返回总和，可改成 `return sum;`，此时会增加对 volatile 对象 `sum` 的读取。这里故意只写 `sum`，开启 `-Wall` 时可能出现 `-Wunused-but-set-variable` 提示。

一般化到其他输入时，C 的有符号 `int` 加法须避免溢出；不能用底层加法指令的截断行为推断 C 有符号溢出具有定义。

## 被调用者：参数直接在寄存器中相加

```text
10000468 <add_val>:
10000468:       1840            adds    r0, r0, r1
1000046a:       1880            adds    r0, r0, r2
1000046c:       18c0            adds    r0, r0, r3
1000046e:       4770            bx      lr
```

入口处 `r0=a`、`r1=b`、`r2=c`、`r3=d`。三条 `ADDS` 依次令 `r0` 变为 `a+b`、`a+b+c`、`a+b+c+d`，最后直接作为返回值。

`add_val()` 没有继续调用其他函数，也没有使用需要恢复的寄存器或栈空间，因此这组输出没有建栈帧。`BX LR` 跳回调用点之后的指令。

`ADDS` 会更新 APSR 的 N/Z/C/V 标志；S 后缀表示更新标志，与 Supervisor 模式无关。调用约定不保证函数调用前后的条件标志保持一致。

## 调用者：保存 LR、准备参数、取回结果

```text
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

按数据流阅读即可：

1. `PUSH {LR}` 保存 `mymain()` 返回其调用者的地址，`SUB SP, #28` 分配局部区。
2. `MOVS` 和三次递增生成 1、2、3、4，分别用 `STR` 写入局部变量。
3. 四条 `LDR` 把参数放入 `r0`～`r3`；`[sp, #4]` 表示地址 `SP+4` 处的内存，而不是立即数 4。
4. `BL` 调用 `add_val()`；返回后，`STR r0, [sp, #20]` 将结果 10 写入 `sum`。
5. `MOVS r0, #0` 设置 `mymain()` 的返回值；释放局部区后，`POP {PC}` 取出保存的返回地址并返回。

### 栈帧为什么是 32 字节

设 `mymain()` 入口 SP 为 `S`，执行完两条分配指令后：`SP = S - 4 - 28 = S - 32`。在调用 `add_val()` 前，栈布局如下：

| 相对当前 SP 的字节偏移 | 内容 |
| --- | --- |
| `+0` | 此构建未使用的分配空间 |
| `+4` | `a = 1` |
| `+8` | `b = 2` |
| `+12` | `c = 3` |
| `+16` | `d = 4` |
| `+20` | `sum`，调用前尚未初始化 |
| `+24` | 此构建未使用的分配空间 |
| `+28` | 保存的入口 LR，即返回 `mymain()` 调用者的地址 |

AAPCS32 要求 SP 始终满足 4 字节对齐，在公共调用接口满足 **8 字节对齐**。入口 `S` 已按 8 字节对齐时，先压入 LR 会暂时变成 `S-4`，再减 28 后就在 `BL` 处恢复 8 字节对齐；并不要求每条指令之后都保持 8 字节对齐。

表中的两个未使用位置不是 ABI 规定的固定槽位。换优化选项后，变量可以重排，栈空间也可以改变；本例总栈帧大小应由 SP 的实际变化计算。

### 两个返回地址不能混用

`0x1000048c` 处的 `BL` 占 **4 字节**，下一条指令在 `0x10000490`，所以写入 LR 的普通 Thumb 返回地址是 **`0x10000491`**。最低位 1 是 Thumb 状态标记，实际指令地址仍是偶数。

`add_val()` 的 `BX LR` 据此返回 `0x10000490`；`mymain()` 最后的 `POP {PC}` 则从栈里取回更早保存的入口 LR，返回上一层调用者。Cortex-M0+ 只支持 Thumb，不能通过清零目标地址最低位来切到 ARM 状态。

普通 `BL` 调用不会自动压入异常栈帧。异常进入时，硬件写入 Handler LR 的是 **EXC_RETURN**，例如 `0xfffffffd`，其返回机制与这里的普通地址不同。任务切换见 [[PendSV psp force lr return]] 和 [[svc load task]]。

## 本例涉及的指令速查

| 指令 | 本例含义 | 条件标志 |
| --- | --- | --- |
| `MOVS r3, #1` | 将立即数 1 写入寄存器 | 更新 N/Z |
| `ADDS r0, r0, r1` | 寄存器相加，结果写入 `r0` | 更新 N/Z/C/V |
| `LDR` / `STR` | 加载 / 存储一个 32 位字 | 不变 |
| `PUSH {LR}` / `POP {PC}` | 向低地址压栈 / 出栈并返回 | 不变 |
| `SUB SP, #28` / `ADD SP, #28` | 分配 / 释放局部区 | 不变 |
| `BL` / `BX LR` | 带返回地址的调用 / 经 LR 返回 | 不变 |

N/Z/C/V 分别表示负数、零、进位和有符号溢出。具体指令形式影响哪些标志，应查对应编码，不能仅凭助记符概括全部变体。

移植汇编时还要注意 Armv6-M 的限制：`PUSH` / `POP` 的通用寄存器列表仅支持 `r0`～`r7`，分别可额外带 LR / PC；`LDM` / `STM` 也不能直接搬运 `r8`～`r11`，需要低寄存器中转。`IT`、`CBZ`、`CBNZ`、`SDIV`、`UDIV` 均不受支持，不能直接照搬 Cortex-M3/M4 的代码。

## 用工具链验证自己的构建

将上面的 C 示例保存为 `call.c`，生成目标文件后查看反汇编与重定位：

```sh
arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -O1 -c call.c -o call.o
arm-none-eabi-objdump -dr call.o
```

已用 GCC **13.2.1** 和 GNU Binutils **2.42** 检查：可生成 Cortex-M0+ 代码，保留四寄存器传参、三条加法、32 字节栈帧和返回值存储。局部变量偏移与上面的示例构建不同；`.o` 中的 `BL` 还可能等待链接重定位，最终地址应查看已链接 ELF 的反汇编。

排查时优先核对 **参数位置 → LR 保存 → 调用点 SP 对齐 → 返回值 → SP 恢复**。汇编或编译通过只验证目标指令与代码生成，不能代替目标板上的运行验证。

## 参考资料

- Arm《Cortex-M0+ Devices Generic User Guide》，**DUI 0662B**：[指令集总览](https://support.arm.com/documentation/dui0662/b/The-Cortex-M0--Instruction-Set/Instruction-set-summary)、[处理器模式与权限](https://support.arm.com/documentation/dui0662/b/The-Cortex-M0--Processor/Programmers-model/Processor-modes-and-privilege-levels-for-software-execution)。
- Arm [AAPCS32，2025Q1](https://github.com/ARM-software/abi-aa/blob/2025Q1/aapcs32/aapcs32.rst)：重点查 `Core registers`、`The Stack`、`Parameter Passing` 和 `Result Return`。
- Arm [CMSIS 6.1.0 的 `core_cm0plus.h`](https://github.com/ARM-software/CMSIS_6/blob/v6.1.0/CMSIS/Core/Include/core_cm0plus.h)：xPSR、CONTROL 与 EXC_RETURN 定义。
