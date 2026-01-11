https://elixir.bootlin.com/linux/v6.18.4/source/drivers/gpio/gpio-dln2.c
https://elixir.bootlin.com/linux/latest/source/drivers/i2c/busses/i2c-dln2.c
https://elixir.bootlin.com/linux/v6.18.4/source/drivers/spi/spi-dln2.c
https://elixir.bootlin.com/linux/v6.18.4/source/drivers/iio/adc/dln2-adc.c

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
```