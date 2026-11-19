// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include "dummy_spi_loopback.h"

static int dummy_spi_of_probe(struct platform_device *pdev)
{
	struct spi_controller *controller;

	controller = devm_spi_alloc_host(&pdev->dev, 0);
	if (!controller)
		return -ENOMEM;
	controller->dev.of_node = pdev->dev.of_node;
	controller->bus_num = -1;
	controller->num_chipselect = 1;
	controller->mode_bits = 0;
	controller->bits_per_word_mask = SPI_BPW_MASK(8);
	controller->transfer_one = dummy_spi_transfer_one;
	return devm_spi_register_controller(&pdev->dev, controller);
}

static const struct of_device_id dummy_spi_of_match[] = {
	{ .compatible = "example,kb-dummy-spi" },
	{ }
};
MODULE_DEVICE_TABLE(of, dummy_spi_of_match);

static struct platform_driver dummy_spi_of_driver = {
	.probe = dummy_spi_of_probe,
	.driver = {
		.name = "kb_dummy_spi_of",
		.of_match_table = dummy_spi_of_match,
	},
};
module_platform_driver(dummy_spi_of_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Device-tree educational virtual SPI controller");
