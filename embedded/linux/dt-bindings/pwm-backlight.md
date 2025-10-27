```c
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