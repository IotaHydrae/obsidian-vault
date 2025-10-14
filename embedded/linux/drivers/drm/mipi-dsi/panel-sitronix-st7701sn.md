| Platform       | Raspberry Pi B              |
| -------------- | --------------------------- |
| Kernel Version | 6.12.47+rpt-rpi-v6          |
| Distro         |                             |
| Panel          | BOE ZV039WVQ-N80            |
| Product        | GoldenMorning T397B5-C24-02 |
| Driver IC      | ST7701SN                    |

Differences between ST7701 and ST7701SN:
- Low power consumption

mipi-dsi 2-lane mode

```
i2c0: i2c@7e205000 {
	compatible = "brcm,bcm2835-i2c";
	reg = <0x7e205000 0x200>;
	interrupts = <2 21>;
	clocks = <&clocks BCM2835_CLOCK_VPU>;
	#address-cells = <1>;
	#size-cells = <0>;
	status = "disabled";
};

dsi1: dsi@7e700000 {
	compatible = "brcm,bcm2835-dsi1";
	reg = <0x7e700000 0x8c>;
	interrupts = <2 12>;
	#address-cells = <1>;
	#size-cells = <0>;
	#clock-cells = <1>;

	clocks = <&clocks BCM2835_PLLD_DSI1>,
		 <&clocks BCM2835_CLOCK_DSI1E>,
		 <&clocks BCM2835_CLOCK_DSI1P>;
	clock-names = "phy", "escape", "pixel";

	clock-output-names = "dsi1_byte",
				 "dsi1_ddr2",
				 "dsi1_ddr";

	status = "disabled";
};
```

### Run time kernel device tree modifying

```shell
sudo cp vc4-kms-dsi-boe-zv039wvq-n80.dtbo /boot/firmware/overlays/
sudo dtoverlay vc4-kms-dsi-boe-zv039wvq-n80
```

and you will see someting like this

```
[  320.126427] OF: overlay: WARNING: memory leak will occur if overlay removed, property: /soc/dsi@7e700000/status     

[  320.127453] /soc/dsi@7e700000: Fixed dependency cycle(s) with /panel_dsi@0                                          

[  320.127504] /panel_dsi@0: Fixed dependency cycle(s) with /soc/dsi@7e700000
```