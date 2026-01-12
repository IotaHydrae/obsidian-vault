https://elixir.bootlin.com/linux/v6.18.4/source/drivers/gpio/gpio-dln2.c

```c
static int dln2_gpio_probe(struct platform_device *pdev)
{
	struct dln2_gpio *dln2;
	struct device *dev = &pdev->dev;
	struct gpio_irq_chip *girq;
	int pins;
	int ret;

	pins = dln2_gpio_get_pin_count(pdev);
	if (pins < 0) {
		dev_err(dev, "failed to get pin count: %d\n", pins);
		return pins;
	}
	if (pins > DLN2_GPIO_MAX_PINS) {
		pins = DLN2_GPIO_MAX_PINS;
		dev_warn(dev, "clamping pins to %d\n", DLN2_GPIO_MAX_PINS);
	}

	dln2 = devm_kzalloc(&pdev->dev, sizeof(*dln2), GFP_KERNEL);
	if (!dln2)
		return -ENOMEM;

	mutex_init(&dln2->irq_lock);

	dln2->pdev = pdev;

	dln2->gpio.label = "dln2";
	dln2->gpio.parent = dev;
	dln2->gpio.owner = THIS_MODULE;
	dln2->gpio.base = -1;
	dln2->gpio.ngpio = pins;
	dln2->gpio.can_sleep = true;
	dln2->gpio.set = dln2_gpio_set;
	dln2->gpio.get = dln2_gpio_get;
	dln2->gpio.request = dln2_gpio_request;
	dln2->gpio.free = dln2_gpio_free;
	dln2->gpio.get_direction = dln2_gpio_get_direction;
	dln2->gpio.direction_input = dln2_gpio_direction_input;
	dln2->gpio.direction_output = dln2_gpio_direction_output;
	dln2->gpio.set_config = dln2_gpio_set_config;

	girq = &dln2->gpio.irq;
	gpio_irq_chip_set_chip(girq, &dln2_irqchip);
	/* The event comes from the outside so no parent handler */
	girq->parent_handler = NULL;
	girq->num_parents = 0;
	girq->parents = NULL;
	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_simple_irq;

	platform_set_drvdata(pdev, dln2);

	ret = devm_gpiochip_add_data(dev, &dln2->gpio, dln2);
	if (ret < 0) {
		dev_err(dev, "failed to add gpio chip: %d\n", ret);
		return ret;
	}

	ret = dln2_register_event_cb(pdev, DLN2_GPIO_CONDITION_MET_EV,
				     dln2_gpio_event);
	if (ret) {
		dev_err(dev, "failed to register event cb: %d\n", ret);
		return ret;
	}

	return 0;
}
```

https://elixir.bootlin.com/linux/latest/source/drivers/i2c/busses/i2c-dln2.c

```c
static int dln2_i2c_probe(struct platform_device *pdev)
{
	int ret;
	struct dln2_i2c *dln2;
	struct device *dev = &pdev->dev;
	struct dln2_platform_data *pdata = dev_get_platdata(&pdev->dev);

	dln2 = devm_kzalloc(dev, sizeof(*dln2), GFP_KERNEL);
	if (!dln2)
		return -ENOMEM;

	dln2->buf = devm_kmalloc(dev, DLN2_I2C_BUF_SIZE, GFP_KERNEL);
	if (!dln2->buf)
		return -ENOMEM;

	dln2->pdev = pdev;
	dln2->port = pdata->port;

	/* setup i2c adapter description */
	dln2->adapter.owner = THIS_MODULE;
	dln2->adapter.class = I2C_CLASS_HWMON;
	dln2->adapter.algo = &dln2_i2c_usb_algorithm;
	dln2->adapter.quirks = &dln2_i2c_quirks;
	dln2->adapter.dev.parent = dev;
	ACPI_COMPANION_SET(&dln2->adapter.dev, ACPI_COMPANION(&pdev->dev));
	dln2->adapter.dev.of_node = dev->of_node;
	i2c_set_adapdata(&dln2->adapter, dln2);
	snprintf(dln2->adapter.name, sizeof(dln2->adapter.name), "%s-%s-%d",
		 "dln2-i2c", dev_name(pdev->dev.parent), dln2->port);

	platform_set_drvdata(pdev, dln2);

	/* initialize the i2c interface */
	ret = dln2_i2c_enable(dln2, true);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to initialize adapter\n");

	/* and finally attach to i2c layer */
	ret = i2c_add_adapter(&dln2->adapter);
	if (ret < 0)
		goto out_disable;

	return 0;

out_disable:
	dln2_i2c_enable(dln2, false);

	return ret;
}
```

https://elixir.bootlin.com/linux/v6.18.4/source/drivers/spi/spi-dln2.c

```c
static int dln2_spi_probe(struct platform_device *pdev)
{
	struct spi_controller *host;
	struct dln2_spi *dln2;
	struct dln2_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct device *dev = &pdev->dev;
	int ret;

	host = spi_alloc_host(&pdev->dev, sizeof(*dln2));
	if (!host)
		return -ENOMEM;

	device_set_node(&host->dev, dev_fwnode(dev));

	platform_set_drvdata(pdev, host);

	dln2 = spi_controller_get_devdata(host);

	dln2->buf = devm_kmalloc(&pdev->dev, DLN2_SPI_BUF_SIZE, GFP_KERNEL);
	if (!dln2->buf) {
		ret = -ENOMEM;
		goto exit_free_host;
	}

	dln2->host = host;
	dln2->pdev = pdev;
	dln2->port = pdata->port;
	/* cs/mode can never be 0xff, so the first transfer will set them */
	dln2->cs = 0xff;
	dln2->mode = 0xff;

	/* disable SPI module before continuing with the setup */
	ret = dln2_spi_enable(dln2, false);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to disable SPI module\n");
		goto exit_free_host;
	}

	ret = dln2_spi_get_cs_num(dln2, &host->num_chipselect);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get number of CS pins\n");
		goto exit_free_host;
	}

	ret = dln2_spi_get_speed_range(dln2,
				       &host->min_speed_hz,
				       &host->max_speed_hz);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to read bus min/max freqs\n");
		goto exit_free_host;
	}

	ret = dln2_spi_get_supported_frame_sizes(dln2,
						 &host->bits_per_word_mask);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to read supported frame sizes\n");
		goto exit_free_host;
	}

	ret = dln2_spi_cs_enable_all(dln2, true);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable CS pins\n");
		goto exit_free_host;
	}

	host->bus_num = -1;
	host->mode_bits = SPI_CPOL | SPI_CPHA;
	host->prepare_message = dln2_spi_prepare_message;
	host->transfer_one = dln2_spi_transfer_one;
	host->auto_runtime_pm = true;

	/* enable SPI module, we're good to go */
	ret = dln2_spi_enable(dln2, true);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable SPI module\n");
		goto exit_free_host;
	}

	pm_runtime_set_autosuspend_delay(&pdev->dev,
					 DLN2_RPM_AUTOSUSPEND_TIMEOUT);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = devm_spi_register_controller(&pdev->dev, host);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register host\n");
		goto exit_register;
	}

	return ret;

exit_register:
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	if (dln2_spi_enable(dln2, false) < 0)
		dev_err(&pdev->dev, "Failed to disable SPI module\n");
exit_free_host:
	spi_controller_put(host);

	return ret;
}
```

https://elixir.bootlin.com/linux/v6.18.4/source/drivers/iio/adc/dln2-adc.c

```c
static int dln2_adc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dln2_adc *dln2;
	struct dln2_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct iio_dev *indio_dev;
	int i, ret, chans;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*dln2));
	if (!indio_dev)
		return -ENOMEM;

	dln2 = iio_priv(indio_dev);
	dln2->pdev = pdev;
	dln2->port = pdata->port;
	dln2->trigger_chan = -1;
	mutex_init(&dln2->mutex);

	platform_set_drvdata(pdev, indio_dev);

	ret = dln2_adc_set_port_resolution(dln2);
	if (ret < 0) {
		dev_err(dev, "failed to set ADC resolution to 10 bits\n");
		return ret;
	}

	chans = dln2_adc_get_chan_count(dln2);
	if (chans < 0) {
		dev_err(dev, "failed to get channel count: %d\n", chans);
		return chans;
	}
	if (chans > DLN2_ADC_MAX_CHANNELS) {
		chans = DLN2_ADC_MAX_CHANNELS;
		dev_warn(dev, "clamping channels to %d\n",
			 DLN2_ADC_MAX_CHANNELS);
	}

	for (i = 0; i < chans; ++i)
		DLN2_ADC_CHAN(dln2->iio_channels[i], i)
	IIO_CHAN_SOFT_TIMESTAMP_ASSIGN(dln2->iio_channels[i], i);

	indio_dev->name = DLN2_ADC_MOD_NAME;
	indio_dev->info = &dln2_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = dln2->iio_channels;
	indio_dev->num_channels = chans + 1;
	indio_dev->setup_ops = &dln2_adc_buffer_setup_ops;

	dln2->trig = devm_iio_trigger_alloc(dev, "%s-dev%d",
					    indio_dev->name,
					    iio_device_id(indio_dev));
	if (!dln2->trig)
		return -ENOMEM;

	iio_trigger_set_drvdata(dln2->trig, dln2);
	ret = devm_iio_trigger_register(dev, dln2->trig);
	if (ret) {
		dev_err(dev, "failed to register trigger: %d\n", ret);
		return ret;
	}
	iio_trigger_set_immutable(indio_dev, dln2->trig);

	ret = devm_iio_triggered_buffer_setup(dev, indio_dev, NULL,
					      dln2_adc_trigger_h,
					      &dln2_adc_buffer_setup_ops);
	if (ret) {
		dev_err(dev, "failed to allocate triggered buffer: %d\n", ret);
		return ret;
	}

	ret = dln2_register_event_cb(pdev, DLN2_ADC_CONDITION_MET_EV,
				     dln2_adc_event);
	if (ret) {
		dev_err(dev, "failed to setup DLN2 periodic event: %d\n", ret);
		return ret;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(dev, "failed to register iio device: %d\n", ret);
		goto unregister_event;
	}

	return ret;

unregister_event:
	dln2_unregister_event_cb(pdev, DLN2_ADC_CONDITION_MET_EV);

	return ret;
}
```

```c
static const struct mfd_cell dln2_devs[] = {
	{
		.name = "dln2-gpio",
		.acpi_match = &dln2_acpi_match_gpio,
		.platform_data = &dln2_pdata_gpio,
		.pdata_size = sizeof(struct dln2_platform_data),
	},
	{
		.name = "dln2-i2c",
		.acpi_match = &dln2_acpi_match_i2c,
		.platform_data = &dln2_pdata_i2c,
		.pdata_size = sizeof(struct dln2_platform_data),
	},
	{
		.name = "dln2-spi",
		.acpi_match = &dln2_acpi_match_spi,
		.platform_data = &dln2_pdata_spi,
		.pdata_size = sizeof(struct dln2_platform_data),
	},
	{
		.name = "dln2-adc",
		.acpi_match = &dln2_acpi_match_adc,
		.platform_data = &dln2_pdata_adc,
		.pdata_size = sizeof(struct dln2_platform_data),
	},
};

	ret = mfd_add_hotplug_devices(dev, dln2_devs, ARRAY_SIZE(dln2_devs));
	if (ret != 0) {
		dev_err(dev, "failed to add mfd devices to core\n");
		goto out_stop_rx;
	}
```