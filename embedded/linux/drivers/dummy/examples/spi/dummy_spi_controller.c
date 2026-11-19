// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include "dummy_spi_loopback.h"

static struct device *dummy_parent;
static struct spi_controller *dummy_controller;
static struct spi_device *dummy_device;

static int __init dummy_spi_controller_init(void)
{
	struct spi_board_info info = {
		.modalias = "kb_spi_demo",
		.max_speed_hz = 1000000,
		.chip_select = 0,
		.mode = SPI_MODE_0,
	};
	int ret;

	dummy_parent = root_device_register("kb-dummy-spi-parent");
	if (IS_ERR(dummy_parent))
		return PTR_ERR(dummy_parent);

	dummy_controller = spi_alloc_host(dummy_parent, 0);
	if (!dummy_controller) {
		ret = -ENOMEM;
		goto unregister_parent;
	}
	dummy_controller->bus_num = -1;
	dummy_controller->num_chipselect = 1;
	dummy_controller->mode_bits = 0;
	dummy_controller->bits_per_word_mask = SPI_BPW_MASK(8);
	dummy_controller->transfer_one = dummy_spi_transfer_one;
	ret = spi_register_controller(dummy_controller);
	if (ret)
		goto put_controller;

	dummy_device = spi_new_device(dummy_controller, &info);
	if (!dummy_device) {
		ret = -ENODEV;
		goto unregister_controller;
	}
	pr_info("dummy SPI device: bus=%d chip-select=0\n",
		dummy_controller->bus_num);
	return 0;

unregister_controller:
	spi_unregister_controller(dummy_controller);
	goto unregister_parent;
put_controller:
	spi_controller_put(dummy_controller);
unregister_parent:
	root_device_unregister(dummy_parent);
	return ret;
}

static void __exit dummy_spi_controller_exit(void)
{
	spi_unregister_device(dummy_device);
	spi_unregister_controller(dummy_controller);
	root_device_unregister(dummy_parent);
}

module_init(dummy_spi_controller_init);
module_exit(dummy_spi_controller_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Educational virtual SPI loopback controller");
