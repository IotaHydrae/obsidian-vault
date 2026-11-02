# 设备树：GPIO 背光

> 使用 `gpio-backlight` 通过 GPIO 控制背光开关；极性和引脚必须与硬件一致。

## 示例

```dts
backlight {
    compatible = "gpio-backlight";
    gpios = <&gpio3 4 GPIO_ACTIVE_HIGH>;
    default-on;
};
```

将 GPIO 控制器、引脚号和极性替换为实际硬件配置，并包含提供 `GPIO_ACTIVE_HIGH` 的头文件。
