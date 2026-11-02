# Purple Pi R1：RGB 屏幕配置与启动阶段引脚复用

> 记录 SSD201 开发板连接 480×480 RGB 屏时的 SDK 配置和引脚复用问题；`bootlogo` 与复用状态的因果关系仍需验证。

## TL;DR

- 硬件为 Purple Pi R1、SSD201 和 D395C930UV0 RGB 屏；此 SDK 的默认屏幕配置为 1024×600 MIPI 屏。
- 屏幕配置涉及 `disp_init` 的 panel 参数及设备树中的 RGB565 pinmux；背光电路和屏幕时序需按实际硬件确认。
- 应用 RGB 配置后，`LCD_DATA6` 至 `LCD_DATA15` 仍处于 MIPI 复用状态；根因待验证。
- U-Boot `bootlogo` 会初始化 MIPI；需通过对照测试确定它是否影响最终 pinmux。

## 环境

| 项目 | 配置 |
| --- | --- |
| 板卡 | Purple Pi R1 IDO-SBC2D06，SSD201，双核 Cortex-A7 1.2 GHz |
| 内存 / 存储 | 64 MB DDR2，板载 128 MB SPI NAND |
| 显示屏 | D395C930UV0，RGB，480×480 |

## 配置显示屏

1. 核对 40-pin RGB LCD 引脚、屏幕规格书和背光供电；此板的背光需要外部驱动电路。
2. 检查 SDK 中 `sdk/verify/application/disp_init/src/sstardisp.c` 及相邻 panel 配置头文件，按屏幕设置接口、时序、极性和通道映射。
3. 检查 `kernel/arch/arm/boot/dts/infinity2m-ssc011a-s01a-padmux-rgb565-rmii-doublenet.dtsi` 中的 RGB565 pinmux，包括 `PAD_TTL0` 至 `PAD_TTL15`，以及 `PAD_TTL24` 至 `PAD_TTL27` 的时钟、同步和 DE 信号。
4. 若未更新文件系统，检查设备上的 `/config/fbdev.ini`，设置 `FB_WIDTH = 480`、`FB_HEIGHT = 480`；其余参数需与实际显示配置相符。

## RGB565 Pinmux

在 pinmux 列表中启用以下条目，并移除同一 PAD 的 MIPI 配置。宏来自此 SDK：

```dts
<PAD_TTL0  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT00>,
<PAD_TTL1  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT01>,
<PAD_TTL2  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT02>,
<PAD_TTL3  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT03>,
<PAD_TTL4  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT04>,
<PAD_TTL5  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT05>,
<PAD_TTL6  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT06>,
<PAD_TTL7  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT07>,
<PAD_TTL8  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT08>,
<PAD_TTL9  PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT09>,
<PAD_TTL10 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT10>,
<PAD_TTL11 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT11>,
<PAD_TTL12 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT12>,
<PAD_TTL13 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT13>,
<PAD_TTL14 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT14>,
<PAD_TTL15 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DOUT15>,
<PAD_TTL24 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_CLK>,
<PAD_TTL25 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_HSYNC>,
<PAD_TTL26 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_VSYNC>,
<PAD_TTL27 PINMUX_FOR_TTL_MODE_3 MDRV_PUSE_TTL_DE>,
```

## fbdev 配置

`/config/fbdev.ini` 的实验参数如下。`FB_TIMMING_*` 是 SDK 特定字段，移植时核对驱动实现，不宜仅凭名称修改。

```ini
[FB_DEVICE]
FB_HWLAYER_ID = 1
FB_HWWIN_ID = 0
FB_HWLAYER_DST = 3
FB_HWWIN_FORMAT = 5
FB_HWLAYER_OUTPUTCOLOR = 1
FB_WIDTH = 480
FB_HEIGHT = 480
FB_TIMMING_WIDTH = 1920
FB_TIMMING_HEIGHT = 1080
FB_MMAP_NAME = E_MMAP_ID_FB
FB_BUFFER_LEN = 4096
```

`FB_BUFFER_LEN` 的单位是 KiB，4096 对应 4 MiB。

## SDK 构建参数

以下使用 `SDK_ROOT` 指向 `PurPle-Pi-R1-main-250121` 根目录，保留单独构建 `disp_init` 时的源码和链接依赖。优先使用 SDK 构建入口；需单独重编时再核对此命令：

```bash
SDK_ROOT=/path/to/PurPle-Pi-R1-main-250121
DISP_SRC="$SDK_ROOT/sdk/verify/application/disp_init/src"
MI_LIBS="$SDK_ROOT/project/release/nvr/i2m/common/glibc/8.2.1/mi_libs"
export PATH="$SDK_ROOT/toolchain/gcc-arm-8.2-2018.08-x86_64-arm-linux-gnueabihf/bin:$PATH"

arm-linux-gnueabihf-gcc -D_GNU_SOURCE -o "$DISP_SRC/disp_init" \
  "$DISP_SRC/disp_init.c" "$DISP_SRC/sstardisp_xingfan.c" \
  "$DISP_SRC/bmp.c" "$DISP_SRC/jpeg.c" -rdynamic -funwind-tables \
  -I. -I"$SDK_ROOT/project/release/include" \
  -I"$SDK_ROOT/sdk/verify/application/disp_init/inc" \
  -I"$SDK_ROOT/project/kbuild/4.9.84/i2m/include/uapi/mstar" \
  -L"$MI_LIBS/dynamic" -L"$MI_LIBS/static" \
  -L"$SDK_ROOT/sdk/verify/application/disp_init/lib" -L/usr/local/lib \
  -Wl,-Bdynamic -ljpeg -lz -lmi_disp -lmi_panel -lmi_gfx \
  -lmi_sys -lmi_common -lstdc++ -ldl -lpthread -lm
```

目标板运行应用前可补齐动态库搜索目录：

```bash
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}/usr/local/lib:/customer/lib:/lib:/config/wifi"
```

生成 customer 镜像的 SDK 命令如下；参数对应 2048 字节最小 I/O、126976 字节 LEB，`-c` 由脚本计算：

```bash
cd "$SDK_ROOT/project"
CUSTOMER_LEBS=$(./build/calc_nand_mfs.sh customer 0x800 0x20000 0 0x500000)
./build/mkfs.ubifs -F -r image/output/customer -o image/output/images/customer.ubifs \
  -m 0x800 -e 126976 -c "$CUSTOMER_LEBS"
```

## 排查复用状态

- 观察：`LCD_DATA0` 至 `LCD_DATA5` 以及 HS、VS、DE、CLK 已切换为 RGB；`LCD_DATA6` 至 `LCD_DATA15` 仍为 MIPI。
- 线索：U-Boot 中 `bootlogo` 命令位于 `boot/common/cmd_bootlogo.c`，启动日志显示 `PnlLink:11`；是否造成最终引脚状态尚待对照验证。
- 对照测试应只控制 `bootlogo` 的执行，并保留正常启动路径；直接清空 `bootcmd` 可能导致设备无法启动。修改前备份环境变量，并准备恢复方法。

## 网络与固件更新

在 `eth0` 上请求地址：

```bash
udhcpc -i eth0
```

可通过 `scp` 传输 kernel 镜像；示例 kernel 分区为 `/dev/mtdblock10`，设备号依固件布局而定。写入前核对当前分区、备份镜像并准备恢复手段。

```bash
scp images/kernel root@BOARD_IP:/root/
```

NAND 写入使用此 SDK 支持的更新流程，并核对启动日志；普通块设备的 `dd` 写入方式不能视为适用于所有 NAND 的通用方法。

## 参考

- [Purple Pi R1 烧录流程](https://industio.yuque.com/mdtih8/lgnqq1/yezo0g3ragnuar79?singleDoc#ePYxu)
- [Purple Pi R1 接口调试教程](https://industio.yuque.com/mdtih8/lgnqq1/nlecxxl09gxt9wdz?singleDoc#jVrW8)
- [Panel 开发参考 - SigmaStarDocs](https://wx.comake.online/doc/syg27dk2rkls-SSD20X/customer/development/software/Px/zh/display/P2/panel.html)
