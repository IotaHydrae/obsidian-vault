# 设备树：PWM 背光

> `pwm-backlight` 使用 PWM 和亮度表控制背光；控制器、周期与亮度档位需按硬件配置。

## 示例

```dts
&pwm10 {
    status = "okay";
    pinctrl-0 = <&pwm10m1_pins>;
};

backlight: backlight {
    compatible = "pwm-backlight";
    pwms = <&pwm10 0 50000 0>;
    brightness-levels = <0 4 8 16 32 64 128 255>;
    default-brightness-level = <7>;
    status = "okay";
};
```

`default-brightness-level` 指向亮度表中的下标；本例下标 7 对应 255。PWM 参数与 pinctrl 要结合实际控制器绑定及背光电路确认。
