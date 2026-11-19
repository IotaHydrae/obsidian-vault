// SPDX-License-Identifier: GPL-2.0
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "dummy_i2c_emulator.h"

struct dummy_i2c_of {
	struct i2c_adapter adapter;
	struct dummy_i2c_state state;
};

static int dummy_i2c_of_probe(struct platform_device *pdev)
{
	struct dummy_i2c_of *bus;
	struct i2c_adapter *adapter;

	bus = devm_kzalloc(&pdev->dev, sizeof(*bus), GFP_KERNEL);
	if (!bus)
		return -ENOMEM;
	adapter = &bus->adapter;
	adapter->owner = THIS_MODULE;
	adapter->algo = &dummy_i2c_algorithm;
	adapter->dev.parent = &pdev->dev;
	adapter->dev.of_node = pdev->dev.of_node;
	strscpy(adapter->name, "kb-dummy-i2c-of", sizeof(adapter->name));
	i2c_set_adapdata(adapter, &bus->state);
	return devm_i2c_add_adapter(&pdev->dev, adapter);
}

static const struct of_device_id dummy_i2c_of_match[] = {
	{ .compatible = "example,kb-dummy-i2c" },
	{ }
};
MODULE_DEVICE_TABLE(of, dummy_i2c_of_match);

static struct platform_driver dummy_i2c_of_driver = {
	.probe = dummy_i2c_of_probe,
	.driver = {
		.name = "kb_dummy_i2c_of",
		.of_match_table = dummy_i2c_of_match,
	},
};
module_platform_driver(dummy_i2c_of_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Device-tree educational virtual I2C adapter");
