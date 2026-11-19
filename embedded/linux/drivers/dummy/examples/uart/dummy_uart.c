// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/module.h>
#include <linux/serial_core.h>
#include <linux/tty_flip.h>

/* A private nonzero marker for this lab, not a registered UART UAPI type. */
#define DUMMY_UART_TYPE	0xff

static struct device *dummy_parent;

static unsigned int dummy_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT;
}

static void dummy_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static unsigned int dummy_get_mctrl(struct uart_port *port)
{
	return TIOCM_CAR | TIOCM_CTS | TIOCM_DSR;
}

static void dummy_stop_tx(struct uart_port *port)
{
}

static void dummy_receive_char(struct uart_port *port, u8 ch)
{
	if (tty_insert_flip_char(&port->state->port, ch, TTY_NORMAL))
		port->icount.rx++;
	else
		port->icount.buf_overrun++;
}

static void dummy_start_tx(struct uart_port *port)
{
	u8 ch;

	uart_port_tx(port, ch, true, dummy_receive_char(port, ch));
	tty_flip_buffer_push(&port->state->port);
}

static void dummy_stop_rx(struct uart_port *port)
{
}

static int dummy_startup(struct uart_port *port)
{
	return 0;
}

static void dummy_shutdown(struct uart_port *port)
{
}

static void dummy_set_termios(struct uart_port *port, struct ktermios *new,
			      const struct ktermios *old)
{
}

static const char *dummy_type(struct uart_port *port)
{
	return "kb-dummy-uart";
}

static void dummy_release_port(struct uart_port *port)
{
}

static int dummy_request_port(struct uart_port *port)
{
	return 0;
}

static void dummy_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = DUMMY_UART_TYPE;
}

static const struct uart_ops dummy_uart_ops = {
	.tx_empty = dummy_tx_empty,
	.set_mctrl = dummy_set_mctrl,
	.get_mctrl = dummy_get_mctrl,
	.stop_tx = dummy_stop_tx,
	.start_tx = dummy_start_tx,
	.stop_rx = dummy_stop_rx,
	.startup = dummy_startup,
	.shutdown = dummy_shutdown,
	.set_termios = dummy_set_termios,
	.type = dummy_type,
	.release_port = dummy_release_port,
	.request_port = dummy_request_port,
	.config_port = dummy_config_port,
};

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
