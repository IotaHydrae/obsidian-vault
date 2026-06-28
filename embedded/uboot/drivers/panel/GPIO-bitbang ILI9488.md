要启用 panel 驱动，首先要开启一些前置 CONFIG：

```c
CONFIG_VIDEO=y
```

panel驱动用于在 I8080 video 驱动初始化——输出时序之前，正确的设置连接到总线的屏幕。以I8080接口的屏幕为例，需要做的有：初始化驱动IC，设置绘制窗口等。

你可以在源码目录 `drivers/video` 下找到一些示例，例如本文档实验使用的基于ILI9488驱动的TFT屏幕。

## 添加一个新的 panel 驱动

修改 `drivers/video/Makefile` 添加如下内容：
```c
obj-$(CONFIG_VIDEO_LCD_ILITEK_ILI9488) += ilitek-ili9488.o
```

修改 `drivers/video/Kconfig` 添加如下内容：
```c
config VIDEO_LCD_ILITEK_ILI9488
	bool "Ilitek ILI9488 DBI Type A/B LCD panel support"
	depends on PANEL && BACKLIGHT
	select GPIO
	help
	Say Y here if you want to enable support for Ilitek ILI9488
	MIPI DBI Type A/B 16-bit 8080 panel.
```

使用编辑器打开源文件 `drivers/video/ili9488.c`，来到文件底部，可以看到如下内容：

```c
struct panel_ops ili9488_ops = {
	.enable_backlight   = ili9488_panel_enable_backlight,
	.get_display_timing = ili9488_panel_get_display_timing,
};

static const struct udevice_id ili9488_ids[] = {
	{ .compatible = "ilitek,ili9488", },
	{ }
};

U_BOOT_DRIVER(ili9488) = {
	.name = "ili9488",
	.id = UCLASS_PANEL,
	.of_match = ili9488_ids,
	.probe = ili9488_probe,
	.ops = &ili9488_ops,
	.priv_auto = sizeof(struct ili9488_priv),
};

```
分别表示panel驱动实现的接口，dts标识，驱动的定义。

panel_ops 的这两个接口会被 video 驱动中的这两个函数调用

```c
/**
 * panel_enable_backlight() - Enable/disable the panel backlight
 *
 * @dev:	Panel device containing the backlight to enable
 * @enable:	true to enable the backlight, false to dis
 * Return: 0 if OK, -ve on error
 */
int panel_enable_backlight(struct udevice *dev);

/**
 * panel_get_display_timing() - Get display timings from panel.
 *
 * @dev:	Panel device containing the display timings
 * Return: 0 if OK, -ve on error
 */
int panel_get_display_timing(struct udevice *dev,
			     struct display_timing *timing);
```

再来看下 probe 函数的流程
```c
static int ili9488_probe(struct udevice *dev)
{
	struct ili9488_priv *priv = dev_get_priv(dev);

	printf("%s\n", __func__);

	priv->display = &default_ili9488_display;
	priv->tftops = &default_ili9488_ops;
	priv->buf = (u8 *)malloc(PAGE_SIZE);
	priv->dev = dev;

	ili9488_of_config(priv);    // 从设备数获得引脚定义等参数

	return ili9488_enable(dev);
}

static int ili9488_enable(struct udevice *dev)
{
	struct ili9488_priv *priv = dev_get_priv(dev);
	printf("%s\n", __func__);
	ili9488_hw_init(priv);    // 初始化屏幕
	ili9488_attach(dev);    // 最后的配置，准备连接到 video 接口
	return 0;
}

static int ili9488_attach(struct udevice *dev)
{
	struct ili9488_priv *priv = dev_get_priv(dev);

	printf("%s\n", __func__);

        /* 设置绘制窗口为整个屏幕 */
	priv->tftops->set_addr_win(priv, 0, 0,
				   priv->display->xres - 1,
				   priv->display->yres - 1);
	return 0;
}

static int ili9488_set_addr_win(struct ili9488_priv *priv, int xs, int ys, int xe,
				int ye)
{
	/* set column adddress */
	write_reg(priv, 0x2A, xs >> 8, xs & 0xFF, xe >> 8, xe & 0xFF);

	/* set row address */
	write_reg(priv, 0x2B, ys >> 8, ys & 0xFF, ye >> 8, ye & 0xFF);

	/* write start */
	write_reg(priv, 0x2C);
	return 0;
}
```

一些重要初始化参数的说明

![输入图片说明](https://foruda.gitee.com/images/1749280760952892392/a54a42d3_5412693.png "屏幕截图")

![输入图片说明](https://foruda.gitee.com/images/1749280744614158611/495f362f_5412693.png "屏幕截图")

![输入图片说明](https://foruda.gitee.com/images/1749280801937133174/edd51872_5412693.png "屏幕截图")

（图片来源于ILI9488 数据手册）

```c
static int ili9488_init_display(struct ili9488_priv *priv)
{
    write_reg(priv, 0x36, 0x8 | (1 << 5) | (1 << 6));    // 屏幕旋转设置
    write_reg(priv, 0x3a, 0x55);    // 屏幕像素格式设置：16bits/pixel

}
```

大致流程就是：
1. 从设备树获取引脚定义并记录
2. 使用GPIO模拟8080接口初始化屏幕
3. 设置绘制窗口为整个屏幕
4. 回到 video 驱动

video 驱动会 probe 索引到的第一个 panel 驱动，相关代码如下:
```c
#ifdef CONFIG_PANEL
	ret = uclass_get_device(UCLASS_PANEL, 0, &panel_dev);
	if (ret) {
		dev_err(dev, "No panel device found: %d\n", ret);
		dev_err(dev, "## Using default display timing instead.\n");
		video_ctfb_mode_to_display_timing(&default_mode, &panel_timing);
		goto skip_panel_setup;
	}

	ret = panel_get_display_timing(panel_dev, &panel_timing);
	if (ret)
		dev_err(dev, "Failed to get panel display timing!: %d\n", ret);

	ret = panel_enable_backlight(panel_dev);
	if (ret)
		dev_warn(dev, "Panel enable backlight failed: %d\n", ret);

	width = panel_timing.hactive.typ;
	height = panel_timing.vactive.typ;

skip_panel_setup:
#else
	#error "This driver requires the CONFIG_PANEL option to be enabled."
#endif
```

在 `uclass_get_device ` 调用后，panel驱动的probe函数就会被执行，如果 video 驱动并未找到 panel 驱动，则会尝试输出一组默认时序，不过这通常不会正确的驱动屏幕，除非屏幕被外部的设备事先初始化好了。