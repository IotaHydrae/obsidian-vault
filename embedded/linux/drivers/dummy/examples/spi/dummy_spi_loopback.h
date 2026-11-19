/* SPDX-License-Identifier: GPL-2.0 */
#ifndef KB_DUMMY_SPI_LOOPBACK_H
#define KB_DUMMY_SPI_LOOPBACK_H

#include <linux/spi/spi.h>

static int dummy_spi_transfer_one(struct spi_controller *controller,
				  struct spi_device *spi,
				  struct spi_transfer *transfer)
{
	const u8 *tx = transfer->tx_buf;
	u8 *rx = transfer->rx_buf;
	unsigned int i;

	for (i = 0; i < transfer->len; i++) {
		if (rx)
			rx[i] = tx ? tx[i] : 0xff;
	}
	return 0;
}

#endif
