# Luckfox Lyra Plus 驱动 ST7701SN MIPI-DSI 屏幕

> 在 RK3506G2、Linux 6.1 SDK 上，通过设备树配置 480×800 双 lane 屏幕的初始化序列与时序。

## TL;DR

- 核对屏幕和板卡 FPC 线序，并确认背光供电；时序和初始化序列应与实际屏幕匹配。
- 在 SDK 的 `rk3506-luckfox-lyra.dtsi` 中修改 DSI panel 配置，重新编译内核。
- 写入镜像前确认启动介质和 `boot` 分区，备份数据并准备恢复手段；下文分区号须按实际布局核对。

## 环境

| 器件 |  |  |
| --- | --- | --- |
| Luckfox Lyra Plus | RK3506G2 | 3 x Cortex A7 + 1 x Cortex-M0 & 片内集成 128MB DDR3 & 256MB SPI-NAND |
| GoldenMorning T397B5-C24-02 | ST7701SN | 480x800 2-lane mipi-dsi |
| WSL | Ubuntu 24.04.4 LTS |

![IMG_1182_compressed](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260629223855139-15801877.jpg)

## SDK 与硬件准备

先部署一下官方提供的 SDK。官方提供的是一个[网盘链接](https://pan.baidu.com/s/1KyPieuwh63ynd-O96ChlDA?pwd=mzqk)，你需要下载下来，解压到你的开发环境中。这是一个例子：

```bash
mkdir -p ~/luckfox/lyra
cd ~/luckfox
cp ~/Downloads/Luckfox_Lyra_SDK_250815.tar.gz .
tar xvf Luckfox_Lyra_SDK_250815.tar.gz -C lyra
```

按 [SDK 编译文档](https://wiki.luckfox.com/zh/Luckfox-Lyra/SDK-Image-Compilation)安装依赖。该 SDK 使用 Ubuntu 22.04 构建环境；换用其他发行版时，按实际包管理器核对依赖名称。

该 SDK 使用 Python 2。以下通过 [pyenv](https://github.com/pyenv/pyenv) 在项目范围选择 Python 2.7.18，shell 为 zsh：

```bash
curl -fsSL https://pyenv.run | bash

# 把下面这段内容添加进 ~/.zshrc
export PYENV_ROOT="$HOME/.pyenv"
[[ -d $PYENV_ROOT/bin ]] && export PATH="$PYENV_ROOT/bin:$PATH"
eval "$(pyenv init - zsh)"

source ~/.zshrc

pyenv install 2.7.18
```

安装完这个 python2 的版本之后，来到 SDK 根目录展开源码

```bash
cd ~/luckfox/lyra

pyenv local 2.7.18

.repo/repo/repo sync -l
```

先来谈一点硬件相关的内容，因为开发板 DSI 接口的线序跟屏幕引脚定义不同，而且体积较小的开发板上一般都没有屏幕背光驱动电路，所以我们需要画一个转接板，当然你也可以直接买一个带屏幕背光驱动转接板的屏幕。这是一个带背光驱动的转接板示例：

![图片](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260628144957774-909007525.png)

屏幕的接口为24pin FPC排线，定义如下：

![Snipaste_2026-06-28_14-55-21](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260628145711917-455671571.png)

开发板的 DSI 接口引脚定义如下：

![Snipaste_2026-06-29_22-42-15](https://img2024.cnblogs.com/blog/2605173/202606/2605173-20260629224305314-1380843971.png)

## 设备树配置

在 `kernel-6.1/arch/arm/boot/dts/rk3506-luckfox-lyra.dtsi` 的 `&dsi` 下找到 `dsi_panel: panel@0` 节点，按实际屏幕修改如下参数：

> 通常你可以在屏幕的规格书中找到时序参数，但是初始化序列需要厂商另外提供。

```dts
	width-mm = <52>;
	height-mm = <86>;

	panel-init-sequence = [
			39 00 06 FF 77 01 00 00 13
			15 00 02 EF 08
			39 00 06 FF 77 01 00 00 10
			39 00 03 C0 63 00
			39 00 03 C1 09 02
			39 00 03 C2 20 02
			15 00 02 CC 18
			39 00 11 B0 40 0E 51 0F 11 07 00 09 06 1E 04 12 11 64 29 DF
			39 00 11 B1 40 07 4C 0A 0E 04 00 08 09 1D 01 0E 0C 6A 34 DF
			39 00 06 FF 77 01 00 00 11
			15 00 02 B0 30
			15 00 02 B1 48
			15 00 02 B2 80
			15 00 02 B3 80
			15 00 02 B5 4F
			15 00 02 B7 85
			15 00 02 B8 23
			39 00 03 B9 22 13
			15 00 02 BB 03
			15 00 02 BC 10
			15 00 02 C0 89
			15 00 02 C1 78
			15 00 02 C2 78
			39 00 07 EF 08 08 08 4C 3F 54
			15 00 02 D0 88
			39 00 04 E0 00 00 02
			39 00 0C E1 04 00 00 00 05 00 00 00 00 10 10
			39 00 0F E2 00 00 00 00 00 00 00 00 00 00 00 00 00 00
			39 00 05 E3 00 00 33 00
			39 00 03 E4 22 00
			39 00 11 E5 03 34 AF B3 05 34 AF B3 00 00 00 00 00 00 00 00
			39 00 05 E6 00 00 33 00
			39 00 03 E7 22 00
			39 00 11 E8 04 34 AF B3 06 34 AF B3 00 00 00 00 00 00 00 00
			39 00 08 EB 02 00 40 40 00 00 00
			39 00 03 EC 00 00
			39 00 11 ED FA 45 0B FF FF FF FF FF FF FF FF FF FF B0 54 AF
			39 00 07 EF 08 08 08 45 3F 54
			39 00 06 FF 77 01 00 00 13
			39 00 03 E6 16 7C
			39 00 03 E8 00 0E
			39 00 06 FF 77 01 00 00 00
			05 78 01 11
			39 00 06 FF 77 01 00 00 13
			39 00 03 E8 00 0C
			39 00 03 E8 00 00
			39 00 06 FF 77 01 00 00 00
			15 00 02 35 00
			05 14 01 29
		];
		
		disp_timings0: display-timings {
			native-mode = <&dsi_timing0>;

			// 10inch1
			dsi_timing0: timing0 {
				clock-frequency = <31000000>;//60fps

				hactive = <480>;
				vactive = <800>;

				vsync-len = <5>;
				vback-porch = <12>;
				vfront-porch = <7>;

				hsync-len = <8>;
				hback-porch = <60>;
				hfront-porch = <80>;

				vsync-active = <1>;
				hsync-active = <1>;
				de-active = <1>;
				pixelclk-active = <0>;
			};
```

## 编译烧录

保存之后，来到 SDK 根目录，执行如下命令重新编译内核

```bash
./build.sh lunch

# 我的选择是 1 1 0，即
# RK3506G_Luckfox_Lyra_Plus
# SPI_NAND
# Buildroot

./build.sh kernel

❯ ls -lh output/firmware/boot.img
lrwxrwxrwx 1 developer developer 26 Jun 29 20:20 output/firmware/boot.img -> ../../kernel-6.1/zboot.img
```

**错误的目标分区会破坏数据或导致设备无法启动**；先检查 `/proc/mtd` 或分区表，确认对应 `boot` 分区，再备份并准备恢复手段。

### 方法一：linux 设备端烧录

```bash
scp output/firmware/boot.img root@BOARD_IP:~/

# 然后在板子一侧执行，重启生效
dd if=/root/boot.img of=/dev/mtdblock1 bs=1M && sync
```

将 `BOARD_IP` 替换为实际地址。`/dev/mtdblock1` 是示例目标，写入前必须核对当前设备的分区布局。

### 方法二：linux 主机端烧录

https://wiki.luckfox.com/zh/Luckfox-Lyra/Image-flashing#42-linuxx86_64-%E5%B9%B3%E5%8F%B0

### 方法三：Windows 烧录

```
cp output/firmware/boot.img /mnt/d/
```
然后参考这篇文章：[单独烧录内核和-rootfs](https://wiki.luckfox.com/zh/Luckfox-Lyra/Kernel-Configuration#5-%E5%8D%95%E7%8B%AC%E7%83%A7%E5%BD%95%E5%86%85%E6%A0%B8%E5%92%8C-rootfs)，进行烧录。需要用到 RKDevTool，在官方文档的[资料下载](https://wiki.luckfox.com/zh/Luckfox-Lyra/Download)章节中下载。

## 一些有用的命令

关闭控制台光标闪烁

```bash
echo 0 > /sys/class/graphics/fbcon/cursor_blink
```

SD 卡启动时的写入示例；先核对 `/dev/mmcblk0p2` 是否为目标分区：

```bash
dd if=/root/boot.img of=/dev/mmcblk0p2 bs=1M && sync
```

## 相关链接

- [Luckfox Lyra Plus 官方文档](https://wiki.luckfox.com/zh/Luckfox-Lyra/Introduction)
- [3.97寸TFT显示屏 全彩 480*800 驱动ST7701 MIPI接口 液晶屏](https://item.taobao.com/item.htm?id=837727544469&mi_id=0000UXe6-xnBIHqeLS928YqLeCL24Bt3Uoyqq8OIE-M3bps&skuId=5765638899362)
- https://wiki.luckfox.com/zh/Luckfox-Lyra/Download
