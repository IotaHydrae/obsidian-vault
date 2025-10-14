| Platform       | Raspberry Pi B              |
| -------------- | --------------------------- |
| Kernel Version | 6.12.47+rpt-rpi-v6          |
| Distro         |                             |
| Panel          | BOE ZV039WVQ-N80            |
| Product        | GoldenMorning T397B5-C24-02 |
| Driver IC      | ST7701SN                    |

Differences between ST7701 and ST7701SN:
- Low power consumption

First, you need to design a PCB to convert the panel interface (usually an FPC cable) to the DSI interface on the Raspberry Pi. Additionally, the PCB must also provide power to the panel driver and backlight.

Here is an example interface adapter board

![[3D_T397B5-C24-02-V1-CM5-IO-Adapter-Board_2025-10-14.png]]


### Example MIPI DSI panel dts overlay

```c
/dts-v1/;
/plugin/;

/ {
	compatible = "brcm,bcm2835";

	fragment@0 {
		target = <&dsi1>;
		__overlay__ {
			#address-cells = <1>;
			#size-cells = <0>;
			status = "okay";
			port {
				dsi_out: endpoint {
					remote-endpoint = <&panel_in>;
				};
			};


		};
	};

	fragment@1 {
		target-path = "/";
		__overlay__ {
			panel: panel_dsi@0 {
				reg = <0>;
				compatible = "goldenmorning,t397b5-c24-02";
				// backlight = <&backlight>;
				reset-gpios = <&gpio 1 0>;  // SCL0 - I2C0_CSI_DSI

				port {
					panel_in: endpoint {
						data-lanes = <2>;
						remote-endpoint = <&dsi_out>;
					};
				};
			};
		};
	};
};
```

### Runtime kernel device tree modification

```shell
sudo cp vc4-kms-dsi-boe-zv039wvq-n80.dtbo /boot/firmware/overlays/
sudo dtoverlay vc4-kms-dsi-boe-zv039wvq-n80
```

and you will see something like this in `dmesg`

```
[  320.126427] OF: overlay: WARNING: memory leak will occur if overlay removed, property: /soc/dsi@7e700000/status     

[  320.127453] /soc/dsi@7e700000: Fixed dependency cycle(s) with /panel_dsi@0                                          

[  320.127504] /panel_dsi@0: Fixed dependency cycle(s) with /soc/dsi@7e700000
```

also if you want the change to be saved, add this to your `config.txt`

```
dtoverlay=vc4-kms-dsi-boe-zv039wvq-n80
```
### Some details that may be helpful

Related DTS nodes from `bcm283x.dtsi`

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