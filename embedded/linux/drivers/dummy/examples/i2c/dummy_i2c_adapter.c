// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/module.h>

#define DUMMY_I2C_ADDR		0x42
#define DUMMY_I2C_ID_REG	0x00
#define DUMMY_I2C_VALUE_REG	0x01
#define DUMMY_I2C_ID		0xa5

static struct device *dummy_parent;
static struct i2c_client *dummy_client;
static u8 dummy_value;

static int dummy_i2c_xfer(struct i2c_adapter *adapter,
			  struct i2c_msg *msgs, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		if (msgs[i].addr != DUMMY_I2C_ADDR)
			return -ENXIO;
	}

	if (num == 1 && msgs[0].flags == 0 && msgs[0].len == 2) {
		if (msgs[0].buf[0] == DUMMY_I2C_ID_REG)
			return -EACCES;
		if (msgs[0].buf[0] != DUMMY_I2C_VALUE_REG)
			return -EINVAL;
		dummy_value = msgs[0].buf[1];
		return 1;
	}

	if (num == 2 && msgs[0].flags == 0 && msgs[0].len == 1 &&
	    msgs[1].flags == I2C_M_RD && msgs[1].len == 1) {
		switch (msgs[0].buf[0]) {
		case DUMMY_I2C_ID_REG:
			msgs[1].buf[0] = DUMMY_I2C_ID;
			return 2;
		case DUMMY_I2C_VALUE_REG:
			msgs[1].buf[0] = dummy_value;
			return 2;
		default:
			return -EINVAL;
		}
	}

	return -EOPNOTSUPP;
}

static u32 dummy_i2c_functionality(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C;
}

static const struct i2c_algorithm dummy_i2c_algorithm = {
	.xfer = dummy_i2c_xfer,
	.functionality = dummy_i2c_functionality,
};

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
