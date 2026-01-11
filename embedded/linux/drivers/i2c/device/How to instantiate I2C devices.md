## Method 1: Declare the I2C devices statically

### Declare the I2C devices via devicetree

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

## Method 2: Instantiate the devices explicitly


## Method 3: Probe an I2C bus for certain devices


## Method 4: Instantiate from user-space

