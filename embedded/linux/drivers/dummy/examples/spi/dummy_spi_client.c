// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

struct dummy_spi_data {
	struct mutex lock;
	u8 *tx;
	u8 *rx;
};

static int dummy_spi_loopback(struct spi_device *spi,
			      struct dummy_spi_data *data, u8 value)
{
	struct spi_transfer transfer = {
		.tx_buf = data->tx,
		.rx_buf = data->rx,
		.len = 1,
	};
	int ret;

	*data->tx = value;
	*data->rx = 0;
	ret = spi_sync_transfer(spi, &transfer, 1);
	if (ret)
		return ret;
	return *data->rx == value ? 0 : -EIO;
}

static ssize_t value_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct dummy_spi_data *data = spi_get_drvdata(to_spi_device(dev));
	u8 value;

	mutex_lock(&data->lock);
	value = *data->rx;
	mutex_unlock(&data->lock);
	return sysfs_emit(buf, "%u\n", value);
}

static ssize_t value_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct dummy_spi_data *data = spi_get_drvdata(spi);
	u8 value;
	int ret;

	ret = kstrtou8(buf, 0, &value);
	if (ret)
		return ret;
	mutex_lock(&data->lock);
	ret = dummy_spi_loopback(spi, data, value);
	mutex_unlock(&data->lock);
	if (ret)
		return ret;
	return count;
}
static DEVICE_ATTR_RW(value);

static struct attribute *dummy_spi_attrs[] = {
	&dev_attr_value.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dummy_spi);

static int dummy_spi_probe(struct spi_device *spi)
{
	struct dummy_spi_data *data;
	int ret;

	data = devm_kzalloc(&spi->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->tx = devm_kmalloc(&spi->dev, 1, GFP_KERNEL);
	data->rx = devm_kmalloc(&spi->dev, 1, GFP_KERNEL);
	if (!data->tx || !data->rx)
		return -ENOMEM;
	mutex_init(&data->lock);
	spi_set_drvdata(spi, data);
	ret = dummy_spi_loopback(spi, data, 0x5a);
	if (ret)
		return ret;
	dev_info(&spi->dev, "virtual loopback returned 0x%02x\n", *data->rx);
	return 0;
}

static const struct spi_device_id dummy_spi_ids[] = {
	{ "kb_spi_demo", 0 },
	{ "kb-spi-demo", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, dummy_spi_ids);

static const struct of_device_id dummy_spi_of_match[] = {
	{ .compatible = "example,kb-spi-demo" },
	{ }
};
MODULE_DEVICE_TABLE(of, dummy_spi_of_match);

static struct spi_driver dummy_spi_driver = {
	.driver = {
		.name = "kb_dummy_spi_client",
		.dev_groups = dummy_spi_groups,
		.of_match_table = dummy_spi_of_match,
	},
	.probe = dummy_spi_probe,
	.id_table = dummy_spi_ids,
};
module_spi_driver(dummy_spi_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Educational virtual SPI client driver");
