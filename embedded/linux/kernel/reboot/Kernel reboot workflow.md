
```c
/**
 *	kernel_restart - reboot the system
 *	@cmd: pointer to buffer containing command to execute for restart
 *		or %NULL
 *
 *	Shutdown everything and perform a clean reboot.
 *	This is not safe to call in interrupt context.
 */
void kernel_restart(char *cmd)
{
	kernel_restart_prepare(cmd);
	do_kernel_restart_prepare();
	migrate_to_reboot_cpu();
	syscore_shutdown();
	if (!cmd)
		pr_emerg("Restarting system\n");
	else
		pr_emerg("Restarting system with command '%s'\n", cmd);
	kmsg_dump(KMSG_DUMP_SHUTDOWN);
	machine_restart(cmd);
}
EXPORT_SYMBOL_GPL(kernel_restart);
```

`machine_restart` 取决于具体架构的实现，以 ARM 架构为例，`machine_restart` 会关闭中断等，然后调用 `do_kernel_restart`。如果重启失败，则会打印内核日志 `"Reboot failed -- System halted\n"`，然后进入 `while(1);` 循环。下面看看 `do_kernel_restart` 的实现

```c
/**
 *	do_kernel_restart - Execute kernel restart handler call chain
 *
 *	@cmd: pointer to buffer containing command to execute for restart
 *		or %NULL
 *
 *	Calls functions registered with register_restart_handler.
 *
 *	Expected to be called from machine_restart as last step of the restart
 *	sequence.
 *
 *	Restarts the system immediately if a restart handler function has been
 *	registered. Otherwise does nothing.
 */
void do_kernel_restart(char *cmd)
{
	atomic_notifier_call_chain(&restart_handler_list, reboot_mode, cmd);
}
```

利用内核的通知机制，向注册进`restart_handler_list`通知链的所有对象发起调用，简单看下