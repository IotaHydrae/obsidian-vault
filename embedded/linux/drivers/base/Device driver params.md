## device attribute

```c
static ssize_t dither_type_show(struct device *dev,
                                struct device_attribute *attr, char *buf)

{
	struct st7305 *st7305 = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u\n", st7305->dither_type);
}

static ssize_t dither_type_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct st7305 *st7305 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val >= DITHER_TYPE_MAX)
		return -EINVAL;

	st7305->dither_type = val;

	dev_info(dev, "set dither type to %lu\n", val);

	return count;
}

static DEVICE_ATTR_RW(dither_type);

static struct attribute *st7305_attrs[] = {
        &dev_attr_dither_type.attr,
        NULL,
};

static const struct attribute_group st7305_attr_group = {
        .name = "config",
        .attrs = st7305_attrs
};

static int st7305_probe(struct spi_device *spi)
{
	...
	dev_set_drvdata(dev, st7305);

	ret = sysfs_create_group(&dev->kobj, &st7305_attr_group);
		if (ret)
			dev_err(dev, "Failed to create device attrs\n");
	...
}

static void st7305_remove(struct spi_device *spi)
{
	...
	sysfs_remove_group(&st7305->dev->kobj, &st7305_attr_group);
	...
}
```

## debugfs

drm_mipi_dbi.c - kernel 6.12

```c
static ssize_t mipi_dbi_debugfs_command_write(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct mipi_dbi_dev *dbidev = m->private;
	u8 val, cmd = 0, parameters[64];
	char *buf, *pos, *token;
	int i, ret, idx;

	if (!drm_dev_enter(&dbidev->drm, &idx))
		return -ENODEV;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto err_exit;
	}

	/* strip trailing whitespace */
	for (i = count - 1; i > 0; i--)
		if (isspace(buf[i]))
			buf[i] = '\0';
		else
			break;
	i = 0;
	pos = buf;
	while (pos) {
		token = strsep(&pos, " ");
		if (!token) {
			ret = -EINVAL;
			goto err_free;
		}

		ret = kstrtou8(token, 16, &val);
		if (ret < 0)
			goto err_free;

		if (token == buf)
			cmd = val;
		else
			parameters[i++] = val;

		if (i == 64) {
			ret = -E2BIG;
			goto err_free;
		}
	}

	ret = mipi_dbi_command_buf(&dbidev->dbi, cmd, parameters, i);

err_free:
	kfree(buf);
err_exit:
	drm_dev_exit(idx);

	return ret < 0 ? ret : count;
}

static int mipi_dbi_debugfs_command_show(struct seq_file *m, void *unused)
{
	struct mipi_dbi_dev *dbidev = m->private;
	struct mipi_dbi *dbi = &dbidev->dbi;
	u8 cmd, val[4];
	int ret, idx;
	size_t len;

	if (!drm_dev_enter(&dbidev->drm, &idx))
		return -ENODEV;

	for (cmd = 0; cmd < 255; cmd++) {
		if (!mipi_dbi_command_is_read(dbi, cmd))
			continue;

		switch (cmd) {
		case MIPI_DCS_READ_MEMORY_START:
		case MIPI_DCS_READ_MEMORY_CONTINUE:
			len = 2;
			break;
		case MIPI_DCS_GET_DISPLAY_ID:
			len = 3;
			break;
		case MIPI_DCS_GET_DISPLAY_STATUS:
			len = 4;
			break;
		default:
			len = 1;
			break;
		}

		seq_printf(m, "%02x: ", cmd);
		ret = mipi_dbi_command_buf(dbi, cmd, val, len);
		if (ret) {
			seq_puts(m, "XX\n");
			continue;
		}
		seq_printf(m, "%*phN\n", (int)len, val);
	}

	drm_dev_exit(idx);

	return 0;
}

static int mipi_dbi_debugfs_command_open(struct inode *inode, struct file *file)
{
	return single_open(file, mipi_dbi_debugfs_command_show,
			   inode->i_private);
}

static const struct file_operations mipi_dbi_debugfs_command_fops = {
	.owner = THIS_MODULE,
	.open = mipi_dbi_debugfs_command_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mipi_dbi_debugfs_command_write,
};

void mipi_dbi_debugfs_init(struct drm_minor *minor)
{
	struct mipi_dbi_dev *dbidev = drm_to_mipi_dbi_dev(minor->dev);
	umode_t mode = S_IFREG | S_IWUSR;

	if (dbidev->dbi.read_commands)
		mode |= S_IRUGO;
	debugfs_create_file("command", mode, minor->debugfs_root, dbidev,
			    &mipi_dbi_debugfs_command_fops);
}
```