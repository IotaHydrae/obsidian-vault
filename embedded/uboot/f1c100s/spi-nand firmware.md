这里以以lctech_pi_f1c200s为例

你需要先安装如下软件包才能够编译u-boot，以 Ubuntu 24.04 LTS 为例

```bash
sudo apt install swig libpython3-dev
```

```bash
make lctech_pi_f1c200s_defconfig
bear -- make -j$(nproc) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
./gen_sunxi_spinand_onlyboot_img.sh u-boot-sunxi-with-spl-spinand.bin u-boot-sunxi-with-spl.bin 2048 128
```
参数解释
```bash
./gen_sunxi_spinand_onlyboot_img.sh \   # 要执行的脚本
    u-boot-sunxi-with-spl-spinand.bin \ # 要生成的 spi-nand 固件名字
    u-boot-sunxi-with-spl.bin \         # 传入脚本的 spi-nor 固件
    2048 \     # spi-nand 的 page 大小
    128        # spi-nand 的 erase block 大小
```

使用 `gen_sunxi_spinand_onlyboot_img.sh` 时会同时生成一个记录文件，里面存放有生成的 spi-nand 文件的各种信息，这是一个例子：
```ini
❯ cat u-boot-sunxi-with-spl-spinand.bin.imgmeta
u-boot-sunxi-with-spl-spinand.bin u-boot-sunxi-with-spl.bin 2048 128
SPL-size 24576
u-boot-size 368316
block-size 1 KiB
Page-size 2 KiB
PEB-size 128 KiB
SPL-count 1
u-boot-count 1
SPL-entry-steps 0x10000
SPL-blocks 24
u-boot-block 384
first-u-boot 128
## Layout ##
spl-0 0x0
u-boot-0 0x20000
```
可以得知 u-boot 位于 `0x20000` 处的位置，将 `CONFIG_SYS_SPI_U_BOOT_OFFS` 设置为该值，这是SPL loader 获知 u-boot 固件位于 FLash 中偏移地址的方式。


此外还可以通过 hexdump 工具查看

```bash
hexdump -C ./u-boot-sunxi-with-spl-spinand.bin | less

00008bc0  01 00 00 00 2d 19 00 00  a0 44 00 00 02 00 00 00  |....-....D......|
00008bd0  2d 19 00 00 99 44 00 00  03 00 00 00 2d 19 00 00  |-....D......-...|
00008be0  49 42 00 00 04 00 00 00  d1 0c 00 00 ae 41 00 00  |IB...........A..|
00008bf0  08 00 00 00 a5 0a 00 00  00 00 00 00 00 00 00 00  |................|
00008c00  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00020000  27 05 19 56 f8 81 0b db  68 30 c4 a9 00 06 a7 30  |'..V....h0.....0|
00020010  81 70 00 00 81 70 00 00  c6 ac 40 0a 11 02 05 00  |.p...p....@.....|
00020020  55 2d 42 6f 6f 74 20 32  30 32 35 2e 30 31 2d 67  |U-Boot 2025.01-g|
00020030  64 37 62 30 38 61 64 33  32 63 38 37 2d 64 69 72  |d7b08ad32c87-dir|
00020040  b8 00 00 ea 14 f0 9f e5  14 f0 9f e5 14 f0 9f e5  |................|
00020050  14 f0 9f e5 14 f0 9f e5  14 f0 9f e5 14 f0 9f e5  |................|
00020060  60 00 70 81 c0 00 70 81  20 01 70 81 80 01 70 81  |`.p...p. .p...p.|
00020070  e0 01 70 81 40 02 70 81  a0 02 70 81 ef be ad de  |..p.@.p...p.....|
```

注意，目前为止 SPL 还没有实现 spi-nand 的 loader。虽然固件的格式正确，但暂时还无法用于启动。