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