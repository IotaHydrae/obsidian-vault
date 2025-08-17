为了方便开发，我在 `sunxi` video 驱动的基础上拷贝了一份代码进行修改，进而完成了一个专用于 I8080 屏幕的 video 驱动。 你可以在 `drivers/video/suniv` 目录下找到相关的文件。

## TCON

如下是有关接口类型设置的内容

![输入图片说明](https://foruda.gitee.com/images/1749278619617339526/a1af512b_5412693.png "屏幕截图")

`arch/arm/include/asm/arch-sunxi/lcdc.h`
```c
#define SUNXI_LCDC_TCON0_CTRL_I80_SEL		(1 << 24)
```

`drivers/video/suniv/lcdc.c`
```c
void lcdc_tcon0_mode_set(struct sunxi_lcdc_reg *const lcdc,
			 const struct display_timing *mode,
			 int clk_div, bool for_ext_vga_dac,
			 int depth, int dclk_phase)
{
    ...
	writel(SUNXI_LCDC_TCON0_CTRL_ENABLE | SUNXI_LCDC_TCON0_CTRL_I80_SEL |
		SUNXI_LCDC_TCON0_CTRL_CLK_DELAY(clk_delay), &lcdc->tcon0_ctrl);
    ...

        writel(0, &lcdc->tcon0_hv_intf);
        	writel((0x1 << 29) | (1 << 26), &lcdc->tcon0_cpu_intf);
    ...
}
```

下面这部分是时钟、DEBE、FE的初始化，我简化了这部分的代码，感兴趣的可以自己跳转看一下，`drivers/video/suniv/suniv_display.c`
```c
	suniv_de_clk_init();

	suniv_lcdc_init();
	suniv_composer_init();
	suniv_composer_mode_set(&panel_timing);
	suniv_lcdc_tcon0_mode_set(suniv_display, &panel_timing, false);
	suniv_lcdc_enable(suniv_display);
	suniv_composer_addr_set(suniv_display->fb_addr);
```

framebuffer 的地址以及大小获取
```c
	suniv_display->fb_addr = plat->base;    // 起始地址
	suniv_display->fb_size = plat->size;
```

## DE BE

![输入图片说明](https://foruda.gitee.com/images/1754983198121304270/fe6e3bfd_5412693.png "屏幕截图")

![输入图片说明](https://foruda.gitee.com/images/1754983318451262985/531e43e8_5412693.png "屏幕截图")

如果想让 BE 工作与 RGB565 模式，需要如下设置：
```c
#define SUNXI_DE_BE_LAYER_STRIDE_RGB565(x)	((x) << 4)
#define SUNXI_DE_BE_LAYER_ATTR1_FMT_RGB565	(0x05 << 8)

	/* DEBE Layer 0 Frame Buffer Line Width Register */
	writel(SUNXI_DE_BE_LAYER_STRIDE_RGB565(mode->xres), &de_be->layer0_stride);
	writel(SUNXI_DE_BE_LAYER_ATTR1_FMT_RGB565, &de_be->layer0_attr1_ctrl);

#define LCD_MAX_LOG2_BPP    VIDEO_BPP16

        plat->size = LCD_MAX_WIDTH * LCD_MAX_HEIGHT * VNBYTES(LCD_MAX_LOG2_BPP);
        uc_priv->bpix = LCD_MAX_LOG2_BPP;
```

如果是 XRGB8888 模式，需要设置：
```c
#define SUNXI_DE_BE_LAYER_STRIDE(x)		((x) << 5)
#define SUNXI_DE_BE_LAYER_ATTR1_FMT_XRGB8888	(0x09 << 8)

	writel(SUNXI_DE_BE_LAYER_STRIDE(mode->xres), &de_be->layer0_stride);
	writel(SUNXI_DE_BE_LAYER_ATTR1_FMT_XRGB8888, &de_be->layer0_attr1_ctrl);

#define LCD_MAX_LOG2_BPP    VIDEO_BPP32

        plat->size = LCD_MAX_WIDTH * LCD_MAX_HEIGHT * VNBYTES(LCD_MAX_LOG2_BPP);
        uc_priv->bpix = LCD_MAX_LOG2_BPP;
```

如果你想要了解更多有关信息，请参考 `drivers/video/suniv/suniv_display.c` 文件

![输入图片说明](https://foruda.gitee.com/images/1755140113099361798/257ae3bb_5412693.jpeg "IMG_09091.JPG")