# Linux I2C 设备实例化方式

> I2C 设备通常不能自动枚举，应由固件描述、板级代码、驱动或受控的用户空间操作创建 client。

## 设备树

```dts
&i2c1 {
    status = "okay";
    eeprom@50 {
        compatible = "atmel,24c256";
        reg = <0x50>;
    };
};
```

`reg` 是目标总线上的设备地址；具体属性和 compatible 必须匹配设备 binding。不要为同一地址重复创建 client。

## 其他方式

- 板级代码可在适当的初始化阶段调用 `i2c_register_board_info()`。
- 设备驱动可使用 `i2c_new_client_device()`；未知地址时某些场景可使用 `i2c_new_scanned_device()`，但探测事务可能影响不兼容设备。检查返回的错误指针，并在清理时注销 client、释放 adapter 引用。
- 用户空间可在确认总线和地址后写入 `new_device`，例如 `echo eeprom 0x50 > /sys/bus/i2c/devices/i2c-3/new_device`；这会修改内核设备状态，类型必须与驱动支持的 ID 匹配。移除时使用对应的 `delete_device`。

参考：[Linux I2C 设备实例化文档](https://www.kernel.org/doc/html/latest/i2c/instantiating-devices.html)。
