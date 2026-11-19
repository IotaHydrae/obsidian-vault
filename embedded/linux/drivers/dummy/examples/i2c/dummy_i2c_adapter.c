// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/module.h>

#include "dummy_i2c_emulator.h"

static struct device *dummy_parent;
static struct i2c_client *dummy_client;
static struct dummy_i2c_state dummy_state;

static struct i2c_adapter dummy_adapter = {
	.owner = THIS_MODULE,
	.name = "kb-dummy-i2c",
	.algo = &dummy_i2c_algorithm,
};

static int __init dummy_i2c_adapter_init(void)
{
	const struct i2c_board_info info = {
		I2C_BOARD_INFO("kb_i2c_demo", DUMMY_I2C_ADDR),
	};
	int ret;

	dummy_parent = root_device_register("kb-dummy-i2c-parent");
	if (IS_ERR(dummy_parent))
		return PTR_ERR(dummy_parent);

	dummy_adapter.dev.parent = dummy_parent;
	i2c_set_adapdata(&dummy_adapter, &dummy_state);
	ret = i2c_add_adapter(&dummy_adapter);
	if (ret)
		goto unregister_parent;

	dummy_client = i2c_new_client_device(&dummy_adapter, &info);
	if (IS_ERR(dummy_client)) {
		ret = PTR_ERR(dummy_client);
		goto del_adapter;
	}

	pr_info("dummy I2C device: bus=%d address=0x%02x\n",
		dummy_adapter.nr, DUMMY_I2C_ADDR);
	return 0;

del_adapter:
	i2c_del_adapter(&dummy_adapter);
unregister_parent:
	root_device_unregister(dummy_parent);
	return ret;
}

static void __exit dummy_i2c_adapter_exit(void)
{
	i2c_unregister_device(dummy_client);
	i2c_del_adapter(&dummy_adapter);
	root_device_unregister(dummy_parent);
}

module_init(dummy_i2c_adapter_init);
module_exit(dummy_i2c_adapter_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Educational virtual I2C adapter and device");
