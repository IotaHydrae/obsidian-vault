
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

（如果对 `atomic_notifier_call_chain` 实现感兴趣的，可以看这里[https://elixir.bootlin.com/linux/v6.19.8/source/kernel/notifier.c#L217](https://elixir.bootlin.com/linux/v6.19.8/source/kernel/notifier.c#L217)）

利用内核的通知机制，向注册进`restart_handler_list`通知链的所有`notifier_block`发起调用

```c
typedef	int (*notifier_fn_t)(struct notifier_block *nb,
			unsigned long action, void *data);

// https://elixir.bootlin.com/linux/v6.19.8/source/include/linux/notifier.h#L54
struct notifier_block {
	notifier_fn_t notifier_call;
	struct notifier_block __rcu *next;
	int priority;
};

// https://elixir.bootlin.com/linux/v6.19.8/source/arch/arm/kernel/setup.c#L1091
static struct notifier_block arm_restart_nb = {
	.notifier_call = arm_restart,
	.priority = 128,
};
```

简单看下 `restart_handler_list` 的原型、注册、销毁实现

[https://elixir.bootlin.com/linux/v6.19.8/source/kernel/reboot.c#L170](https://elixir.bootlin.com/linux/v6.19.8/source/kernel/reboot.c#L170)

```c
/*
 *	Notifier list for kernel code which wants to be called
 *	to restart the system.
 */
static ATOMIC_NOTIFIER_HEAD(restart_handler_list);

/**
 *	register_restart_handler - Register function to be called to reset
 *				   the system
 *	@nb: Info about handler function to be called
 *	@nb->priority:	Handler priority. Handlers should follow the
 *			following guidelines for setting priorities.
 *			0:	Restart handler of last resort,
 *				with limited restart capabilities
 *			128:	Default restart handler; use if no other
 *				restart handler is expected to be available,
 *				and/or if restart functionality is
 *				sufficient to restart the entire system
 *			255:	Highest priority restart handler, will
 *				preempt all other restart handlers
 *
 *	Registers a function with code to be called to restart the
 *	system.
 *
 *	Registered functions will be called from machine_restart as last
 *	step of the restart sequence (if the architecture specific
 *	machine_restart function calls do_kernel_restart - see below
 *	for details).
 *	Registered functions are expected to restart the system immediately.
 *	If more than one function is registered, the restart handler priority
 *	selects which function will be called first.
 *
 *	Restart handlers are expected to be registered from non-architecture
 *	code, typically from drivers. A typical use case would be a system
 *	where restart functionality is provided through a watchdog. Multiple
 *	restart handlers may exist; for example, one restart handler might
 *	restart the entire system, while another only restarts the CPU.
 *	In such cases, the restart handler which only restarts part of the
 *	hardware is expected to register with low priority to ensure that
 *	it only runs if no other means to restart the system is available.
 *
 *	Currently always returns zero, as atomic_notifier_chain_register()
 *	always returns zero.
 */
int register_restart_handler(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&restart_handler_list, nb);
}
EXPORT_SYMBOL(register_restart_handler);

/**
 *	unregister_restart_handler - Unregister previously registered
 *				     restart handler
 *	@nb: Hook to be unregistered
 *
 *	Unregisters a previously registered restart handler function.
 *
 *	Returns zero on success, or %-ENOENT on failure.
 */
int unregister_restart_handler(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&restart_handler_list, nb);
}
EXPORT_SYMBOL(unregister_restart_handler);
```

根据注释中的说明我们可以得知，`restart_handler` 被调用时需要重启系统。被注册的函数会在作为整个重启流程的最后一个阶段，在machine_restart 函数中被调用。

还说了 `notifier_block` 的优先级设置，被注册的函数应当立即重启系统，如果有多个函数被注册，则按照优先级决定谁先被调用。

`restart_handler` 应该被注册在与架构无关的代码中，例如驱动中。一个典型的用例就是系统的重启能力由看门狗提供。

系统中有可能存在多个 `restart_handler`，举个例子，其中一个负责重启整个系统，另一个只重启cpu。在这种情况下，那些只重启部分硬件的 `restart_handler` 应该以更低的优先级进行注册，以确保在**没有其他重启系统的方法**时才能够运行。

接下来看一下注册 `restart_handler` 的例子

[https://elixir.bootlin.com/linux/v6.19.8/source/arch/arm/kernel/setup.c#L1163](https://elixir.bootlin.com/linux/v6.19.8/source/arch/arm/kernel/setup.c#L1163)

如果你的dt提供了

```c
static void (*__arm_pm_restart)(enum reboot_mode reboot_mode, const char *cmd);

static int arm_restart(struct notifier_block *nb, unsigned long action,
		       void *data)
{
	__arm_pm_restart(action, data);
	return NOTIFY_DONE;
}

static struct notifier_block arm_restart_nb = {
	.notifier_call = arm_restart,
	.priority = 128,
};

void __init setup_arch(char **cmdline_p)
{
	...
	
	if (mdesc->restart) {
			__arm_pm_restart = mdesc->restart;
			register_restart_handler(&arm_restart_nb);
	}
	
	...
	
}
```

这里有一个疑问，`mdesc->restart` 是如何拿到的？
## 补充

### struct machine_desc

```c
struct machine_desc {
	unsigned int		nr;		/* architecture number	*/
	const char		*name;		/* architecture name	*/
	unsigned long		atag_offset;	/* tagged list (relative) */
	const char *const 	*dt_compat;	/* array of device tree
						 * 'compatible' strings	*/

	unsigned int		nr_irqs;	/* number of IRQs */

#ifdef CONFIG_ZONE_DMA
	phys_addr_t		dma_zone_size;	/* size of DMA-able area */
#endif

	unsigned int		video_start;	/* start of video RAM	*/
	unsigned int		video_end;	/* end of video RAM	*/

	unsigned char		reserve_lp0 :1;	/* never has lp0	*/
	unsigned char		reserve_lp1 :1;	/* never has lp1	*/
	unsigned char		reserve_lp2 :1;	/* never has lp2	*/
	enum reboot_mode	reboot_mode;	/* default restart mode	*/
	unsigned		l2c_aux_val;	/* L2 cache aux value	*/
	unsigned		l2c_aux_mask;	/* L2 cache aux mask	*/
	void			(*l2c_write_sec)(unsigned long, unsigned);
	const struct smp_operations	*smp;	/* SMP operations	*/
	bool			(*smp_init)(void);
	void			(*fixup)(struct tag *, char **);
	void			(*dt_fixup)(void);
	long long		(*pv_fixup)(void);
	void			(*reserve)(void);/* reserve mem blocks	*/
	void			(*map_io)(void);/* IO mapping function	*/
	void			(*init_early)(void);
	void			(*init_irq)(void);
	void			(*init_time)(void);
	void			(*init_machine)(void);
	void			(*init_late)(void);
	void			(*restart)(enum reboot_mode, const char *);
};

/*
 * Set of macros to define architecture features.  This is built into
 * a table by the linker.
 */
#define MACHINE_START(_type,_name)			\
static const struct machine_desc __mach_desc_##_type	\
 __used							\
 __section(".arch.info.init") = {			\
	.nr		= MACH_TYPE_##_type,		\
	.name		= _name,

#define MACHINE_END				\
};

#define DT_MACHINE_START(_name, _namestr)		\
static const struct machine_desc __mach_desc_##_name	\
 __used							\
 __section(".arch.info.init") = {			\
	.nr		= ~0,				\
	.name		= _namestr,

#endif
```