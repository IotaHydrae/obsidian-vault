// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/module.h>

#define DUMMY_I2C_ID_REG	0x00
#define DUMMY_I2C_VALUE_REG	0x01
#define DUMMY_I2C_ID		0xa5

static int dummy_i2c_read_reg(struct i2c_client *client, u8 reg, u8 *value)
{
	struct i2c_msg msgs[] = {
		{ .addr = client->addr, .len = 1, .buf = &reg },
		{ .addr = client->addr, .flags = I2C_M_RD, .len = 1,
		  .buf = value },
	};
	int ret;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret == ARRAY_SIZE(msgs))
		return 0;
	return ret < 0 ? ret : -EIO;
}

static ssize_t value_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 value;
	int ret;

	ret = dummy_i2c_read_reg(client, DUMMY_I2C_VALUE_REG, &value);
	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", value);
}

static ssize_t value_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 data[] = { DUMMY_I2C_VALUE_REG, 0 };
	int ret;

	ret = kstrtou8(buf, 0, &data[1]);
	if (ret)
		return ret;
	ret = i2c_master_send(client, data, sizeof(data));
	if (ret != sizeof(data))
		return ret < 0 ? ret : -EIO;
	return count;
}
static DEVICE_ATTR_RW(value);

static struct attribute *dummy_i2c_attrs[] = {
	&dev_attr_value.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dummy_i2c);

static int dummy_i2c_probe(struct i2c_client *client)
{
	u8 id;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EOPNOTSUPP;
	ret = dummy_i2c_read_reg(client, DUMMY_I2C_ID_REG, &id);
	if (ret)
		return ret;
	if (id != DUMMY_I2C_ID)
		return -ENODEV;
	dev_info(&client->dev, "virtual device ID 0x%02x\n", id);
	return 0;
}

static const struct i2c_device_id dummy_i2c_ids[] = {
	{ "kb_i2c_demo", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dummy_i2c_ids);

static struct i2c_driver dummy_i2c_driver = {
	.driver = {
		.name = "kb_dummy_i2c_client",
		.dev_groups = dummy_i2c_groups,
	},
	.probe = dummy_i2c_probe,
	.id_table = dummy_i2c_ids,
};
module_i2c_driver(dummy_i2c_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Educational virtual I2C client driver");
