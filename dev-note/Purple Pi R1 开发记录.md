
| 部件  | 规格                            |
| --- | ----------------------------- |
| 开发板 | Purple Pi R1 IDO-SBC2D06）     |
| CPU | SSD201 2 x Cortex-A7 @ 1.2GHz |
| RAM | SIP 64MB DDR2                 |
| ROM | On board 128MB SPI NAND Flash |

## 固件烧录

首先给板子配网，将网线连接至靠近USB-A口的网口(eth0)，然后执行

```bash
udhcpc -i eth0
```

如果网络连接正常，可以观察到如下日志

```bash
udhcpc: started, v1.31.1
udhcpc: sending discover
udhcpc: sending select for 192.168.50.243
udhcpc: lease of 192.168.50.243 obtained, lease time 86400
deleting routers
adding dns 192.168.50.1
```

### kernel

单独更新 kernel 分区，可以参考如下步骤，首先在PC端将编译好的 kernel 镜像拷贝至板子中

```
scp images/kernel root@192.168.50.243:~/
```

回到板子一侧，更新分区并重启

```bash
dd if=/root/kernel of=/dev/mtdblock10 bs=1M && reboot
```

## 屏幕配置

先说一下硬件部分，开发板提供了一个 40 Pin 的RGB LCD接口，引脚定义与市面上大部分RGB屏幕相同，但是板子没有提供屏幕背光驱动电路，所以你需要自己画一个带背光驱动的转接板，或者你可以直接买一个带背光驱动的屏幕模组。

在编写本文时，我使用的屏幕是一块 4.0 寸的 RGB接口的屏幕，就是86盒上用的那种，分辨率 480x480，型号为 D395C930UV0。

板子提供的SDK中默认的屏幕配置是一块分辨率为 1024x600 的 MIPI 屏。

我们使用的是一块RGB接口的LCD，这类屏幕通常会在规格书中提供一组时序参数

再来聊聊软件部分，你需要修改 panel 配置，在sdk中有一个示例

```
vim sdk/verify/application/disp_init/src/sstardisp.c
```

其中包含了很多屏幕配置头文件，里面bao'han'le
### 修改引脚复用模式

```bash
vim kernel/arch/arm/boot/dts/infinity2m-ssc011a-s01a-padmux-rgb565-rmii-doublenet.dtsi
```

SDK将默认接口模式改成了MIPI，我们这里需要改成RGB，你可以在设备树里找到如下被注释的内容，将如下内容取消注释，同时将MIPI相关的注释掉（PAD_TTL6 ~ PAD_TTL15）。

```c
//for RGB565
//<PAD_TTL0            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT00 >,
//<PAD_TTL1            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT01 >,
//<PAD_TTL2            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT02 >,
//<PAD_TTL3            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT03 >,
//<PAD_TTL4            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT04 >,
//<PAD_TTL5            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT05 >,
//<PAD_TTL6            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT06 >,
//<PAD_TTL7            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT07 >,
//<PAD_TTL8            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT08 >,
//<PAD_TTL9            PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT09 >,
//<PAD_TTL10           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT10 >,
//<PAD_TTL11           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT11 >,
//<PAD_TTL12           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT12 >,
//<PAD_TTL13           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT13 >,
//<PAD_TTL14           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT14 >,
//<PAD_TTL15           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DOUT15 >,

//<PAD_TTL24           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_CLK >,
//<PAD_TTL25           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_HSYNC >,
//<PAD_TTL26           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_VSYNC >,
//<PAD_TTL27           PINMUX_FOR_TTL_MODE_3      MDRV_PUSE_TTL_DE >,
```

```bash
./build/mkfs.ubifs -F -r /home/developer/industio/PurPle-Pi-R1-main-250121/project/image/output/customer -o /home/developer/industio/PurPle-Pi-R1-main-250121/project/image/output/images/customer.ubifs -m 0x800 -e 126976 -c `./build/calc_nand_mfs.sh customer 0x800 0x20000 0 0x500000`
```

```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/customer/lib:/lib:/config/wifi/

arm-linux-gnueabihf-gcc -D_GNU_SOURCE -o  /home/developer/industio/PurPle-Pi-R1-main-250121/project/../sdk/verify/application/disp_init/src/disp_init  /home/developer/industio/PurPle-Pi-R1-main-250121/project/../sdk/verify/application/disp_init/src/disp_init.c /home/developer/industio/PurPle-Pi-R1-main-250121/project/../sdk/verify/application/disp_init/src/sstardisp_xingfan.c /home/developer/industio/PurPle-Pi-R1-main-250121/project/../sdk/verify/application/disp_init/src/bmp.c /home/developer/industio/PurPle-Pi-R1-main-250121/project/../sdk/verify/application/disp_init/src/jpeg.c -rdynamic -funwind-tables -I. -I/home/developer/industio/PurPle-Pi-R1-main-250121/project/release/include -I/home/developer/industio/PurPle-Pi-R1-main-250121/project/../sdk/verify/application/disp_init/inc -I/home/developer/industio/PurPle-Pi-R1-main-250121/project/kbuild/4.9.84/i2m/include/uapi/mstar -L/home/developer/industio/PurPle-Pi-R1-main-250121/project/release/nvr/i2m/common/glibc/8.2.1/mi_libs/dynamic  -L/home/developer/industio/PurPle-Pi-R1-main-250121/project/release/nvr/i2m/common/glibc/8.2.1/mi_libs/static -L/home/developer/industio/PurPle-Pi-R1-main-250121/project/../sdk/verify/application/disp_init/lib -L/usr/local/lib -Wl,-Bdynamic -ljpeg -lz  -lmi_disp -lmi_panel  -lmi_gfx -lmi_sys -lmi_common -ldl  -Wl,-Bdynamic -lstdc++ -ldl  -lpthread -lm
```

## 开发过程中遇到的问题

### 屏幕接口切换异常

切换屏幕接口后为 RGB 后，LCD_DATA6 ~ LCD_DATA15 依然是 MIPI 复用的功能，剩余其他引脚已经切换为 RGB 功能，例如 HS, VS, DE, CLK, LCD_DATA0 ~ LCD_DATA5(Panel_R整个通道和Panel_G2)，目前还不知道是什么原因导致的。

## 参考文档

[Purple Pi R1 烧录流程]([Purple Pi R1 烧录流程](https://industio.yuque.com/mdtih8/lgnqq1/yezo0g3ragnuar79?singleDoc#ePYxu))
[PurPle-Pi-R1接口调试教程手册]([PurPle-Pi-R1接口调试教程手册](https://industio.yuque.com/mdtih8/lgnqq1/nlecxxl09gxt9wdz?singleDoc#jVrW8))

[Panel开发参考 - SigmaStarDocs](https://wx.comake.online/doc/syg27dk2rkls-SSD20X/customer/development/software/Px/zh/display/P2/panel.html)