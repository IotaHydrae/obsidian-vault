
It's just an ordinary backtrace. Those functions are called in reverse order; the first function listed was called by the next. Below I added `[was called]` to further clarify:

```
(unwind_backtrace+0x0/0xf8) [was called] from (warn_slowpath_common+0x50/0x60)
(warn_slowpath_common+0x50/0x60) [was called] from (warn_slowpath_null+0x1c/0x24)
(warn_slowpath_null+0x1c/0x24) [was called] from (local_bh_enable_ip+0xa0/0xac)
(local_bh_enable_ip+0xa0/0xac) [was called] from (bdi_register+0xec/0x150)
```

The `bdi_register+0xec/0x150` is the symbol + the offset/length. There's more information about this in [Understanding a Kernel Oops](https://opensourceforu.com/2011/01/understanding-a-kernel-oops/) and how you can debug a kernel oops. Also, there's this excellent tutorial on [Debugging the Kernel](http://www.freebsd.org/doc/en/books/developers-handbook/kerneldebug.html).

Note: as suggested below by Eugene, you may want to try [addr2line](http://linux.die.net/man/1/addr2line) first. It will need an image with debugging symbols though. For example:

`addr2line -e vmlinux_with_debug_info 0019594c(+offset)`

**Use `objdump`:**

1. locate your `vmlinux` or the `.ko` file under the kernel root directory, then disassemble the object file :
    
    ```c
    objdump -dS vmlinux > /tmp/kernel.s
    ```
    
2. Open the generated assembly file, `/tmp/kernel.s`. with a text editor such as `vim`. Go to `unwind_backtrace+0x0/0xf8`, i.e. search for the address of `unwind_backtrace` + the `offset`. Finally, you have located the problematic part in your source code.

**Use `gdb`:**

IMO, an even more elegant option is to use the one and only `gdb`. Assuming you have the suitable toolchain on your host machine:

1. Run `gdb <path-to-vmlinux>`.
2. Execute in gdb's prompt: `list *(unwind_backtrace+0x10)`.


## Understanding the Oops dump

Let’s have a closer look at the above dump, to understand some of the important bits of information.

|   |
|---|
|`BUG: unable to handle kernel NULL pointer dereference at (null)`|

The first line indicates a pointer with a NULL value.

|                                                          |
| -------------------------------------------------------- |
| `IP: [<ffffffffa03e1012>] my_oops_init+0x12/0x21 [oops]` |

IP is the instruction pointer.

|   |
|---|
|`Oops: 0002 [#1] SMP`|

This is the error code value in hex. Each bit has a significance of its own:

- `bit 0` == 0 means no page found, 1 means a protection fault
- `bit 1` == 0 means read, 1 means write
- `bit 2` == 0 means kernel, 1 means user-mode
- `[#1]` — this value is the number of times the Oops occurred. Multiple Oops can be triggered as a cascading effect of the first one.

|   |
|---|
|`CPU 1`|

This denotes on which CPU the error occurred.

|   |
|---|
|`Pid: 2248, comm: insmod Tainted: P           2.6.33.3-85.fc13.x86_64`|

The `Tainted` flag points to `P` here. Each flag has its own meaning. A few other flags, and their meanings, picked up from `kernel/panic.c`:

- `P` — Proprietary module has been loaded.
- `F` — Module has been forcibly loaded.
- `S` — SMP with a CPU not designed for SMP.
- `R` — User forced a module unload.
- `M` — System experienced a machine check exception.
- `B` — System has hit bad_page.
- `U` — Userspace-defined naughtiness.
- `A` — ACPI table overridden.
- `W` — Taint on warning.

|   |
|---|
|`RIP: 0010:[<ffffffffa03e1012>]  [<ffffffffa03e1012>] my_oops_init+0x12/0x21 [oops]`|

`RIP` is the CPU register containing the address of the instruction that is getting executed. `0010` comes from the code segment register. `my_oops_init+0x12/0x21` is the _<symbol> + the offset/length_.

|   |
|---|
|`RSP: 0018:ffff88007ad4bf08  EFLAGS: 00010292`<br><br>`RAX: 0000000000000018 RBX: ffffffffa03e1000 RCX: 00000000000013b7`<br><br>`RDX: 0000000000000000 RSI: 0000000000000046 RDI: 0000000000000246`<br><br>`RBP: ffff88007ad4bf08 R08: ffff88007af1cba0 R09: 0000000000000004`<br><br>`R10: 0000000000000000 R11: ffff88007ad4bd68 R12: 0000000000000000`<br><br>`R13: 00000000016b0030 R14: 0000000000019db9 R15: 00000000016b0010`|

This is a dump of the contents of some of the CPU registers.


|   |
|---|
|`Stack:`<br><br>`ffff88007ad4bf38 ffffffff8100205f ffffffffa03de060 ffffffffa03de060`<br><br> `0000000000000000 00000000016b0030 ffff88007ad4bf78 ffffffff8107aac9`<br><br> `ffff88007ad4bf78 00007fff69f3e814 0000000000019db9 0000000000020000`|

The above is the stack trace.

|   |
|---|
|`Call Trace:`<br><br>`[<ffffffff8100205f>] do_one_initcall+0x59/0x154`<br><br>`[<ffffffff8107aac9>] sys_init_module+0xd1/0x230`<br><br>`[<ffffffff81009b02>] system_call_fastpath+0x16/0x1b`|

The above is the call trace — the list of functions being called just before the Oops occurred.

|   |
|---|
|`Code: <c7> 04 25 00 00 00 00 00 00 00 00 31 c0 c9 c3 00 00 00 00 00 00 00`|

The `Code` is a hex-dump of the section of machine code that was being run at the time the Oops occurred.

## Debugging an Oops dump

The first step is to load the offending module into the GDB debugger, as follows:

|   |
|---|
|`[root@DELL-RnD-India oops]# gdb oops.ko`<br><br>`GNU gdb (GDB) Fedora (7.1-18.fc13)`<br><br>`Reading symbols from /code/oops/oops.ko...done.`<br><br>`(gdb) add-symbol-file oops.o 0xffffffffa03e1000`<br><br>`add symbol table from file "oops.o" at`<br><br>    `.text_addr = 0xffffffffa03e1000`|

Next, add the symbol file to the debugger. The `add-symbol-file` command’s first argument is `oops.o` and the second argument is the address of the text section of the module. You can obtain this address from `/sys/module/oops/sections/.init.text` (where `oops` is the module name):

|   |
|---|
|`(gdb) add-symbol-file oops.o 0xffffffffa03e1000`<br><br>`add symbol table from file "oops.o" at`<br><br>    `.text_addr = 0xffffffffa03e1000`<br><br>`(y or n) y`<br><br>`Reading symbols from /code/oops/oops.o...done.`|

From the `RIP` instruction line, we can get the name of the offending function, and disassemble it.

|   |
|---|
|`(gdb) disassemble my_oops_init`<br><br>`Dump of assembler code for function my_oops_init:`<br><br>   `0x0000000000000038 <+0>:    push   %rbp`<br><br>   `0x0000000000000039 <+1>:    mov    $0x0,%rdi`<br><br>   `0x0000000000000040 <+8>:    xor    %eax,%eax`<br><br>   `0x0000000000000042 <+10>:    mov    %rsp,%rbp`<br><br>   `0x0000000000000045 <+13>:    callq  0x4a <my_oops_init+18>`<br><br>   `0x000000000000004a <+18>:    movl   $0x0,0x0`<br><br>   `0x0000000000000055 <+29>:    xor    %eax,%eax`<br><br>   `0x0000000000000057 <+31>:    leaveq`<br><br>   `0x0000000000000058 <+32>:    retq`<br><br>`End of assembler dump.`|

Now, to pin point the actual line of offending code, we add the starting address and the offset. The offset is available in the same `RIP` instruction line. In our case, we are adding `0x0000000000000038 + 0x012 =  0x000000000000004a`. This points to the `movl` instruction.

|   |
|---|
|`(gdb) list *0x000000000000004a`<br><br>`0x4a is in my_oops_init (/code/oops/oops.c:6).`<br><br>`1    #include <linux/kernel.h>`<br><br>`2    #include <linux/module.h>`<br><br>`3    #include <linux/init.h>`<br><br>`4`    <br><br>`5    static void create_oops() {`<br><br>`6        *(int *)0 = 0;`<br><br>`7    }`|

This gives the code of the offending function.

### References

The [kerneloops.org](http://www.kerneloops.org/) website can be used to pick up a lot of Oops messages to debug. The Linux kernel documentation directory has information about Oops — `kernel/Documentation/oops-tracing.txt`. This, and numerous other online resources, were used while creating this article.