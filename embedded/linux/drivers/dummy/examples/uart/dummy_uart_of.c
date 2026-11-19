// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/slab.h>

#include "dummy_uart_ops.h"

static struct uart_driver dummy_uart_of_driver = {
	.owner = THIS_MODULE,
	.driver_name = "kb_dummy_uart_of",
	.dev_name = "ttyDUMMY",
	.nr = 1,
};

static int dummy_uart_of_probe(struct platform_device *pdev)
{
	struct uart_port *port;
	int ret;

	port = devm_kzalloc(&pdev->dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;
	port->dev = &pdev->dev;
	port->iotype = UPIO_UNKNOWN;
	port->fifosize = 1;
	port->type = DUMMY_UART_TYPE;
	port->flags = UPF_FIXED_TYPE | UPF_FIXED_PORT;
	port->ops = &dummy_uart_ops;
	port->line = 0;
	ret = uart_add_one_port(&dummy_uart_of_driver, port);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, port);
	return 0;
}

static void dummy_uart_of_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);

	uart_remove_one_port(&dummy_uart_of_driver, port);
}

static const struct of_device_id dummy_uart_of_match[] = {
	{ .compatible = "example,kb-dummy-uart" },
	{ }
};
MODULE_DEVICE_TABLE(of, dummy_uart_of_match);

static struct platform_driver dummy_uart_platform_driver = {
	.probe = dummy_uart_of_probe,
	.remove = dummy_uart_of_remove,
	.driver = {
		.name = "kb_dummy_uart_of",
		.of_match_table = dummy_uart_of_match,
	},
};

static int __init dummy_uart_of_init(void)
{
	int ret;

	ret = uart_register_driver(&dummy_uart_of_driver);
	if (ret)
		return ret;
	ret = platform_driver_register(&dummy_uart_platform_driver);
	if (ret) {
		uart_unregister_driver(&dummy_uart_of_driver);
		return ret;
	}
	dummy_uart_guard_module_unload(&dummy_uart_of_driver);
	return 0;
}

static void __exit dummy_uart_of_exit(void)
{
	platform_driver_unregister(&dummy_uart_platform_driver);
	uart_unregister_driver(&dummy_uart_of_driver);
}

module_init(dummy_uart_of_init);
module_exit(dummy_uart_of_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Device-tree educational serial core UART loopback");
