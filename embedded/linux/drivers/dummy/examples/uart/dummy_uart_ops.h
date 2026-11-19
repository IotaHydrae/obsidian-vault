/* SPDX-License-Identifier: GPL-2.0 */
#ifndef KB_DUMMY_UART_OPS_H
#define KB_DUMMY_UART_OPS_H

#include <linux/module.h>
#include <linux/serial_core.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

/* A private nonzero marker for this lab, not a registered UART UAPI type. */
#define DUMMY_UART_TYPE	0xff

static void dummy_uart_guard_module_unload(const struct uart_driver *driver)
{
#ifdef CONFIG_MODULE_UNLOAD
	if (!driver->owner || driver->tty_driver->owner == driver->owner)
		return;

	/* TTY hangup does not wait for all file descriptors to close. */
	__module_get(driver->owner);
	pr_warn("%s: serial_core did not set the TTY owner; unload disabled until reboot\n",
		driver->driver_name);
#endif
}

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

#endif
