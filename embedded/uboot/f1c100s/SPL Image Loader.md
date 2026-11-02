# F1C100s：SPI-NAND SPL Loader 的启动路径

> SPI-NAND 启动需要同时满足 BootROM 的 SPL 布局、启动源识别和 SPL loader 的读盘/镜像解析逻辑；只打包镜像不足以完成启动。

## 环境与边界

| 项目 | 版本或型号 |
| --- | --- |
| SoC | Allwinner Suniv F1C100s |
| U-Boot | 基于 2023.07 的 ninjar-lite 实验分支 |
| Linux / Buildroot | 6.4.0-rc4 / 2023.02 |
| SPI-NAND | Winbond W25N01G |

这是此硬件与分支的移植参考。后续 2025.01 打包实验见 [[spi-nand firmware]]，不能将两个版本的偏移和接口直接混用。

## BootROM 到 U-Boot

1. BootROM 从 NAND 读取并执行 SRAM 中的 SPL；此实验按每个物理页的前 1024 字节组织 SPL，需核对目标 BootROM 与 NAND 页大小。
2. SPL 通过 `board_boot_order()`、`spl_boot_device()`、`sunxi_get_boot_device()` 取得启动源。
3. `boot_from_devices()` 根据启动设备选择 `SPL_LOAD_IMAGE_METHOD` 注册的 loader。
4. loader 初始化 SPI/NAND、读取镜像头、解析装载地址和大小，再将镜像装入 RAM。
5. `board_init_r()` 完成后续准备，并通过 `jump_to_image_no_args()` 跳转到镜像入口。

装载地址与入口地址是两个概念；此 legacy 镜像中二者恰好都为 `0x81700000`。legacy header 为 64 字节，含大端编码的长度、装载地址、入口及校验字段，应通过该版本的解析接口处理，不能仅强转结构体后按主机字节序使用。

## 启动源与 Loader 注册

此 Suniv 实现从 `fel_stash.sp - 4` 读取 BootROM 留下的值，再转换成 U-Boot 启动源。实验分支使用的值：

| 标识 | 值 |
| --- | --- |
| `SUNIV_BOOTED_FROM_MMC0` | `0xffff40f8` |
| `SUNIV_BOOTED_FROM_NAND` | `0xffff4114` |
| `SUNIV_BOOTED_FROM_SPI` | `0xffff4130` |
| `SUNIV_BOOTED_FROM_MMC1` | `0xffff4150` |
| FEL 实验识别值 | `0xe1a00000` |

在 `arch/arm/include/asm/arch-sunxi/spl.h` 和 `arch/arm/mach-sunxi/board.c` 中识别 SPI-NAND，并将其映射为 `BOOT_DEVICE_NAND`；对应 loader 注册为：

```c
SPL_LOAD_IMAGE_METHOD("sunxi SPI-NAND", 0, BOOT_DEVICE_NAND,
                      spl_spi_nand_load_image);
```

用 FEL 加载测试 SPL 的命令：

```bash
sudo sunxi-fel uboot u-boot-sunxi-with-spl.bin
```

为了在 RAM 中测试 NAND loader，可以临时重定向 FEL 路径；完成测试后必须恢复 FEL 的正常返回路径，否则会改变恢复和调试行为。`Trying to boot from ...` 只说明选择了哪一个 loader，不能证明后续镜像已正确装载。

## NAND 读取与实现参考

W25N01G 的普通读取路径先将页加载到芯片内部缓存，再按列地址读取缓存。跨页时必须加载下一页；控制器 FIFO 有限，需要分段传输并按实际完成长度推进。

完整实验实现保存在 [spl_spi_nand_sunxi.c](reference/spl_spi_nand_sunxi.c)，包含 Sun4i/Sun6i 寄存器、时钟及引脚配置、NAND 命令和镜像装载流程。它依赖此分支，不是通用或已完成可靠性验证的驱动。移植前至少处理：

- 页大小与地址计算中的固定 2048 字节假设。
- FIFO 计数掩码、发送回读字节、传输完成条件和超时返回；等待失败后不能继续当作成功读取。
- `malloc()` 失败、逐层读写返回值与短读；缓冲区和地址只能按实际传输字节推进。
- 页加载完成、器件状态、ECC 和坏块策略，以及头部、长度和装载范围校验。
- 未支持的 FIT / SPL OS boot 应明确失败，不能报告成功后跳转。

## 时钟与测量

一次实验在 AHB 200 MHz 下记录了以下结果：

| 分频 | 标称 SPI 速率 | 记录值 |
| --- | --- | --- |
| DIV_64 | 3 MHz | 1918 tick / 79 ms |
| DIV_32 | 6 MHz | 1918 tick / 79 ms |
| DIV_4 | 50 MHz | 1918 tick / 79 ms |

这些相同读数不足以证明 SPI 速率不影响装载时间。应先核对时钟配置是否实际生效、计时范围和单位，再记录重复测量。尤其不能将 `get_timer()` 的毫秒结果再次按 24 MHz 原始 tick 换算；按目标版本接口可使用以下形式测量：

```c
unsigned long started = get_timer(0);
/* 被测读取流程 */
unsigned long elapsed_ms = get_timer(started);
```

提高 SPI 时钟、减少固定延时或改变压缩格式都需要重新验证器件时序、数据正确性和完整启动时间。

## 来源

- [启动路径支持提交](https://github.com/ninjar-bsp/u-boot/commit/a8f3fe09b3363fec72d73b6a78063b154f39cabd)
- [SPI-NAND 装载实现提交](https://github.com/ninjar-bsp/u-boot/commit/6278f6cc78b0662d0b1b75ea794c728ecefb21ae)
- [ninjar-lite 分支](https://github.com/ninjar-bsp/u-boot/tree/ninjar-lite)
- [U-Boot v2023.07 时间接口](https://github.com/u-boot/u-boot/blob/v2023.07/lib/time.c)
- [镜像布局脚本](https://github.com/bamkrs/openwrt/blob/dolphinpi-spinand/target/linux/sunxi/image/gen_sunxi_spinand_onlyboot_img.sh)
- [F1C100S BootROM 与 SPL](https://www.cnblogs.com/yanye0xff/p/16341719.html)
- [V3s SPI-NAND 讨论](https://whycan.com/t_3123.html)
- [FEL 启动与烧写讨论](https://whycan.com/t_2449.html#p19694)
- [SPI 控制寄存器图](https://img2023.cnblogs.com/blog/2605173/202308/2605173-20230810151058237-67062592.png)
