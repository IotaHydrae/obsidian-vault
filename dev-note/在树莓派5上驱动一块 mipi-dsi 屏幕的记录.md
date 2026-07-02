
| 器件 |  |  |
| --- | --- | --- |
| Raspberry Pi 5 Model B | BCM2712 | 4 x Cortex-A76@2.4GHz + RP1 |
| GoldenMorning T397B5-C24-02 | ST7701SN | 480x800 2-lane mipi-dsi (panel: boe-zv039wvq-n80) |
| raspberrypi-os | Debian GNU/Linux 13 (trixie) |

![IMG_1183_compressed](https://img2024.cnblogs.com/blog/2605173/202607/2605173-20260702021304758-1620897413.jpg)


先来谈一点硬件相关的内容，因为开发板 DSI 接口的线序跟屏幕引脚定义不同，而且体积较小的开发板上一般都没有屏幕背光驱动电路，所以我们需要画一个转接板，当然你也可以直接买一个带屏幕背光驱动转接板的屏幕。这是一个带背光驱动的转接板示例：

![图片](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260628144957774-909007525.png)

屏幕的接口为24pin FPC排线，定义如下：

![Snipaste_2026-06-28_14-55-21](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260628145711917-455671571.png)

开发板的 DSI 接口引脚定义如下：

![Snipaste_2026-06-29_22-42-15](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260629224305314-1380843971.png)

回到软件部分，相关的驱动代码位于仓库
```bash
git clone https://github.com/IotaHydrae/panel-boe-zv039wvq-n80.git
```

> 通常你可以在屏幕的规格书中找到时序参数，但是初始化序列需要厂商另外提供。

这里贴出相关代码
```c
static const struct drm_display_mode t397b5_c24_02_mode = {
	.hdisplay = 480,
	.hsync_start = 480 + 80,
	.hsync_end = 480 + 80 + 8,
	.htotal = 480 + 80 + 8 + 60,

	.vdisplay = 800,
	.vsync_start = 800 + 7,
	.vsync_end = 800 + 7 + 5,
	.vtotal = 800 + 7 + 5 + 12,

	.width_mm = 52,
	.height_mm = 86,

	.clock = 31000,
};

static int t397b5_c24_02_init_seq(struct st7701sn *ctx)
{
	ST7701SN_WRITE(ctx, 0x11);
	msleep(200);
	ST7701SN_WRITE(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	ST7701SN_WRITE(ctx, 0xC0, 0x63, 0x00);
	ST7701SN_WRITE(ctx, 0xC1, 0x0A, 0x02);
	ST7701SN_WRITE(ctx, 0xC2, 0x31, 0x08);
	ST7701SN_WRITE(ctx, 0xB0, 0x00, 0x11, 0x19, 0x0C, 0x10, 0x06, 0x07,
		       0x0A, 0x09, 0x22, 0x04, 0x10, 0x0E, 0x28, 0x30, 0x1C);
	ST7701SN_WRITE(ctx, 0xB1, 0x00, 0x12, 0x19, 0x0D, 0x10, 0x04, 0x06,
		       0x07, 0x08, 0x23, 0x04, 0x12, 0x11, 0x28, 0x30, 0x1C);
	ST7701SN_WRITE(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);
	ST7701SN_WRITE(ctx, 0xB0, 0x4D);
	ST7701SN_WRITE(ctx, 0xB1, 0x3E);
	ST7701SN_WRITE(ctx, 0xB2, 0x07);
	ST7701SN_WRITE(ctx, 0xB3, 0x80);
	ST7701SN_WRITE(ctx, 0xB5, 0x47);
	ST7701SN_WRITE(ctx, 0xB7, 0x8A);
	ST7701SN_WRITE(ctx, 0xB8, 0x21);
	ST7701SN_WRITE(ctx, 0xC1, 0x78);
	ST7701SN_WRITE(ctx, 0xC2, 0x78);
	ST7701SN_WRITE(ctx, 0xD0, 0x88);
	msleep(100);
	ST7701SN_WRITE(ctx, 0xE0, 0x00, 0x00, 0x02);
	ST7701SN_WRITE(ctx, 0xE1, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
		       0x00, 0x00, 0x20, 0x20);
	ST7701SN_WRITE(ctx, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		       0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ST7701SN_WRITE(ctx, 0xE3, 0x00, 0x00, 0x33, 0x00);
	ST7701SN_WRITE(ctx, 0xE4, 0x22, 0x00);
	ST7701SN_WRITE(ctx, 0xE5, 0x04, 0x34, 0xAA, 0xAA, 0x06, 0x34, 0xAA,
		       0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ST7701SN_WRITE(ctx, 0xE6, 0x00, 0x00, 0x33, 0x00);
	ST7701SN_WRITE(ctx, 0xE7, 0x22, 0x00);
	ST7701SN_WRITE(ctx, 0xE8, 0x05, 0x34, 0xAA, 0xAA, 0x07, 0x34, 0xAA,
		       0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ST7701SN_WRITE(ctx, 0xEB, 0x02, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00);
	ST7701SN_WRITE(ctx, 0xED, 0xFA, 0x45, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF,
		       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x54, 0xAF);
	ST7701SN_WRITE(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);
	msleep(10);
	ST7701SN_WRITE(ctx, 0x29);

	return 0;
}

static const struct st7701sn_panel_desc t397b5_c24_02_desc = {
	.mode = &t397b5_c24_02_mode,
	.lanes = 2,

	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO |
		      MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS,

	.format = MIPI_DSI_FMT_RGB888,

	.init_seq = t397b5_c24_02_init_seq,
};

static const struct of_device_id st7701sn_of_match[] = {
	{ .compatible = "goldenmorning,t397b5", .data = &t397b5_c24_02_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, st7701sn_of_match);

static struct mipi_dsi_driver st7701sn_driver = {
	.probe = st7701sn_probe,
	.remove = st7701sn_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = st7701sn_of_match,
	},
};
module_mipi_dsi_driver(st7701sn_driver);
```

```c
 /*
 * Device Tree overlay for BOE ZV039WVQ-N80 3.97" MIPI DSI panel
 *
 */

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

			dsi_panel: dsi_panel@0 {
				compatible = "goldenmorning,t397b5";
				reg = <0>;
				// backlight = <&backlight>;
				reset-gpio = <&gpio 4 1 0>;  // CD1_SCL/GPIO41 ACTIVE_HIGH
				status = "okay";

				port {
					panel_in: endpoint {
						remote-endpoint = <&dsi_out>;
					};
				};
			};
		};
	};
};
```

## 编译、加载驱动

```c
make

sudo dtoverlay vc4-kms-dsi-boe-zv039wvq-n80-overlay.dtbo
sudo insmod panel-boe-zv039wvq-n80.ko
```

开机自动加载

```bash
# 开机自动加载设备树 overlay
sudo cp vc4-kms-dsi-boe-zv039wvq-n80-overlay.dtbo /boot/firmware/overlays/
sudo echo "dtoverlay=vc4-kms-dsi-boe-zv039wvq-n80-overlay" >> /boot/firmware/config.txt

# 开机自动加载驱动

/etc/modules-load.d/modules.conf
```

## 一些有用的命令

关闭控制台光标闪烁

```bash
echo 0 > /sys/class/graphics/fbcon/cursor_blink
```


## 相关链接

- https://github.com/IotaHydrae/panel-boe-zv039wvq-n80
- https://github.com/raspberrypi/linux/blob/rpi-6.18.y/drivers/gpu/drm/panel/panel-waveshare-dsi-v2.c
- https://github.com/raspberrypi/linux/blob/rpi-6.18.y/arch/arm/boot/dts/overlays/vc4-kms-dsi-waveshare-panel-overlay.dts
- https://github.com/raspberrypi/linux/blob/rpi-6.18.y/arch/arm/boot/dts/overlays/vc4-kms-dsi-waveshare-800x480-overlay.dts
- https://github.com/raspberrypi/linux/blob/rpi-6.18.y/Documentation/devicetree/bindings/display/panel/waveshare%2Cdsi-touch.yaml
- [3.97寸TFT显示屏 全彩 480*800 驱动ST7701 MIPI接口 液晶屏](https://item.taobao.com/item.htm?id=837727544469&mi_id=0000UXe6-xnBIHqeLS928YqLeCL24Bt3Uoyqq8OIE-M3bps&skuId=5765638899362)
