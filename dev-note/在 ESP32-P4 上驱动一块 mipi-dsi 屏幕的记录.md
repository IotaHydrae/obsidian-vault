
| 器件 |  |  |
| --- | --- | --- |
| [Waveshare ESP32-P4-WIFI6](https://docs.waveshare.net/ESP32-P4-WIFI6) | ESP32-P4 | 双核高性能 RISC-V，最高400MHz + 单核低功耗 RISC-V 40MHz & 最大32MB Octal SPI PSRAM & 32MB SPI-NOR Flash |
| [GoldenMorning T397B5-C24-02](https://item.taobao.com/item.htm?id=837727544469&mi_id=0000UXe6-xnBIHqeLS928YqLeCL24Bt3Uoyqq8OIE-M3bps&skuId=5765638899362) | ST7701SN | 480x800 2-lane mipi-dsi |
| [ESP-IDF](https://github.com/espressif/esp-idf) | V5.5.2  |

![IMG_1152_compressed](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260628153207049-475757738.jpg)

屏幕的接口为24pin FPC排线，定义如下：

![Snipaste_2026-06-28_14-55-21](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260628145711917-455671571.png)

开发板的 DSI 接口引脚定义如下：

![Snipaste_2026-06-28_15-03-56](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260628150409203-1112527655.png)


因为开发板 DSI 接口的线序跟屏幕引脚定义不同，而且体积较小的开发板上一般都没有屏幕背光驱动电路，所以我们需要画一个转接板，当然你也可以直接买一个带屏幕背光驱动转接板的屏幕。这是一个带背光驱动的转接板示例：

![图片](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260628144957774-909007525.png)

接下来聊聊软件部分。你需要先拉取官方提供的示例工程：

```bash
git clone https://github.com/waveshareteam/ESP32-P4-Platform.git
```

你需要先配置好 esp-idf 环境，这里不讨论，有需要的话看[这个文档](https://github.com/espressif/esp-idf/blob/master/README_CN.md)。

配置好后，在 esp-idf 目录下执行如下命令：

```bash
. ./export.sh
```

然后来到 `ESP32-P4-Platform/examples/esp-idf/14_lvgl_demo_v9` 这个目录，执行如下命令进行工程参数配置

```bash
idf.py set-target esp32p4
idf.py menuconfig
```

来到最底部找到 `Board Support Package(ESP32-P4)` 回车进入 再找到 `Display`，选中 `Select LCD type` 回车，找到 `Waveshare 4-DSI-TOUCH-A Display` 选中后回车返回上一级，然后将 `Select LCD color format` 设置为 `RGB888` 方法同上。

> 如果用的是 LVGL 示例，需要把 LVGL 的颜色格式设置为 24: RGB888，默认是 16: RGB565。

> 我选的这块屏幕刚好和官方示例里的这个型号兼容，如果你选的屏幕跟官方的这几个型号不兼容，就需要自己修改初始化参数，见下文。

配置完成后，按键盘 `S` 保存，`ESC` 退出。

屏幕初始化相关的代码位于 `managed_components/waveshare__esp32_p4_platform/esp32_p4_platform.c`，屏幕的初始化序列为

```c
#elif CONFIG_BSP_LCD_TYPE_480_800_4_INCH_A
static const st7701_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0C, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x01, 0x07}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0xCD, 0x18, 0x1F, 0x0F, 0x13, 0x08, 0x09, 0x08, 0x08, 0x24, 0x03, 0x10, 0x0E, 0x21, 0x24, 0x0B}, 16, 0},
    {0xB1, (uint8_t[]){0xC3, 0x0F, 0x18, 0x0B, 0x0F, 0x05, 0x09, 0x09, 0x08, 0x24, 0x06, 0x13, 0x13, 0x28, 0x2D, 0x15}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x3F}, 1, 0},
    {0xB2, (uint8_t[]){0x82}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x45}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x3E}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 100},
    {0xE0, (uint8_t[]){0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20}, 11, 0},
    {0xE2, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x00}, 4, 0},
    {0xE4, (uint8_t[]){0x22, 0x00}, 2, 0},
    {0xE5, (uint8_t[]){0x04, 0x34, 0x9A, 0xA0, 0x06, 0x34, 0x9A, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x00}, 4, 0},
    {0xE7, (uint8_t[]){0x22, 0x00}, 2, 0},
    {0xE8, (uint8_t[]){0x05, 0x34, 0x9A, 0xA0, 0x07, 0x34, 0x9A, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 16, 0},
    {0xEB, (uint8_t[]){0x02, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xED, (uint8_t[]){0xFA, 0x45, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x54, 0xAF}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 200},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};
#endif
```

与初始化相关的函数如下，位于 520 行附近。
```c
esp_err_t bsp_display_new_with_handles(const bsp_display_config_t *config, bsp_lcd_handles_t *ret_handles)
```

相关的初始化代码

```c
esp_lcd_dpi_panel_config_t dpi_config = {
	.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
	.dpi_clock_freq_mhz = 30,
	.virtual_channel = 0,
	#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
	#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
	.in_color_format = LCD_COLOR_FMT_RGB888,
	#else
	.in_color_format = LCD_COLOR_FMT_RGB565,
	#endif
	#elif CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
	.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB888,
	#else
	.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
	#endif
	.num_fbs = 1,
	.video_timing =
	{
		.h_size = 480,
		.v_size = 800,
		.hsync_back_porch = 42,
		.hsync_pulse_width = 12,
		.hsync_front_porch = 42,
		.vsync_back_porch = 2,
		.vsync_pulse_width = 8,
		.vsync_front_porch = 60,
	},
	#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
	.flags.use_dma2d = true,
	#endif
};

st7701_vendor_config_t vendor_config = {
	.init_cmds = vendor_specific_init_default,
	.init_cmds_size =
		sizeof(vendor_specific_init_default) / sizeof(st7701_lcd_init_cmd_t),
	.flags =
	{
		.use_mipi_interface = 1,
	},
	.mipi_config =
	{
		.dsi_bus = mipi_dsi_bus,
		.dpi_config = &dpi_config,
	},
};

esp_lcd_panel_dev_config_t lcd_dev_config = {
	#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
	.bits_per_pixel = 24,
	#else
	.bits_per_pixel = 16,
	#endif
	.rgb_ele_order = BSP_LCD_COLOR_SPACE,
	.reset_gpio_num = BSP_LCD_RST,
	.vendor_config = &vendor_config,
};

ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7701(io, &lcd_dev_config, &disp_panel),
				  err, TAG, "New LCD panel ILI9881C failed");
ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(disp_panel), err, TAG,
				  "LCD panel reset failed");
ESP_GOTO_ON_ERROR(esp_lcd_panel_init(disp_panel), err, TAG,
				  "LCD panel init failed");
ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(disp_panel, true), err, TAG,
				  "LCD panel ON failed");
```

## 编译烧录

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## 相关链接

- https://docs.waveshare.net/ESP32-P4-WIFI6
- [3.97寸TFT显示屏 全彩 480*800 驱动ST7701 MIPI接口 液晶屏](https://item.taobao.com/item.htm?id=837727544469&mi_id=0000UXe6-xnBIHqeLS928YqLeCL24Bt3Uoyqq8OIE-M3bps&skuId=5765638899362)
