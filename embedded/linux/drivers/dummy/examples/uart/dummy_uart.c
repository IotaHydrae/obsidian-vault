// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/module.h>
#include <linux/serial_core.h>

#include "dummy_uart_ops.h"

static struct device *dummy_parent;

static struct uart_driver dummy_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = "kb_dummy_uart",
	.dev_name = "ttyDUMMY",
	.nr = 1,
};

static struct uart_port dummy_uart_port = {
	.iotype = UPIO_UNKNOWN,
	.fifosize = 1,
	.type = DUMMY_UART_TYPE,
	.flags = UPF_FIXED_TYPE | UPF_FIXED_PORT,
	.ops = &dummy_uart_ops,
	.line = 0,
};

static int __init dummy_uart_init(void)
{
	int ret;

	dummy_parent = root_device_register("kb-dummy-uart-parent");
	if (IS_ERR(dummy_parent))
		return PTR_ERR(dummy_parent);
	ret = uart_register_driver(&dummy_uart_driver);
	if (ret)
		goto unregister_parent;

	dummy_uart_port.dev = dummy_parent;
	ret = uart_add_one_port(&dummy_uart_driver, &dummy_uart_port);
	if (ret)
		goto unregister_driver;
	pr_info("dummy UART: ttyDUMMY0\n");
	return 0;

unregister_driver:
	uart_unregister_driver(&dummy_uart_driver);
unregister_parent:
	root_device_unregister(dummy_parent);
	return ret;
}

static void __exit dummy_uart_exit(void)
{
	uart_remove_one_port(&dummy_uart_driver, &dummy_uart_port);
	uart_unregister_driver(&dummy_uart_driver);
	root_device_unregister(dummy_parent);
}

module_init(dummy_uart_init);
module_exit(dummy_uart_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Educational serial core UART loopback");
