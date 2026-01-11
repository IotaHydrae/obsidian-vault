## Method 1: Declare the I2C devices statically

### 1.1 Declare the I2C devices via devicetree

```c
i2c1: i2c@400a0000 {
        /* ... master properties skipped ... */
        clock-frequency = <100000>;

        flash@50 {
                compatible = "atmel,24c256";
                reg = <0x50>;
        };

        pca9532: gpio@60 {
                compatible = "nxp,pca9532";
                gpio-controller;
                #gpio-cells = <2>;
                reg = <0x60>;
        };
};
```

### 1.2 Declare the I2C devices in board files

```c
static struct i2c_board_info h4_i2c_board_info[] __initdata = {
      {
              I2C_BOARD_INFO("isp1301_omap", 0x2d),
              .irq            = OMAP_GPIO_IRQ(125),
      },
      {       /* EEPROM on mainboard */
              I2C_BOARD_INFO("24c01", 0x52),
              .platform_data  = &m24c01,
      },
      {       /* EEPROM on cpu card */
              I2C_BOARD_INFO("24c01", 0x57),
              .platform_data  = &m24c01,
      },
};

static void __init omap_h4_init(void)
{
      (...)
      i2c_register_board_info(1, h4_i2c_board_info,
                      ARRAY_SIZE(h4_i2c_board_info));
      (...)
}
```

## Method 2: Instantiate the devices explicitly

### 2.1 i2c_new_client_device

```c
static struct i2c_board_info sfe4001_hwmon_info = {
      I2C_BOARD_INFO("max6647", 0x4e),
};

int sfe4001_init(struct efx_nic *efx)
{
      (...)
      efx->board_info.hwmon_client =
              i2c_new_client_device(&efx->i2c_adap, &sfe4001_hwmon_info);

      (...)
}
```

### 2.2 i2c_new_scanned_device

```c
static const unsigned short normal_i2c[] = { 0x2c, 0x2d, I2C_CLIENT_END };

static int usb_hcd_nxp_probe(struct platform_device *pdev)
{
      (...)
      struct i2c_adapter *i2c_adap;
      struct i2c_board_info i2c_info;

      (...)
      i2c_adap = i2c_get_adapter(2);
      memset(&i2c_info, 0, sizeof(struct i2c_board_info));
      strscpy(i2c_info.type, "isp1301_nxp", sizeof(i2c_info.type));
      isp1301_i2c_client = i2c_new_scanned_device(i2c_adap, &i2c_info,
                                                  normal_i2c, NULL);
      i2c_put_adapter(i2c_adap);
      (...)
}
```

## Method 3: Probe an I2C bus for certain devices

Sometimes you do not have enough information about an I2C device, not even to call `i2c_new_scanned_device()`. The typical case is hardware monitoring chips on PC mainboards.

## Method 4: Instantiate from user-space

```shell
echo eeprom 0x50 > /sys/bus/i2c/devices/i2c-3/new_device
```

## Reference

[How to instantiate I2C devices — The Linux Kernel documentation](https://www.kernel.org/doc/html/latest/i2c/instantiating-devices.html)