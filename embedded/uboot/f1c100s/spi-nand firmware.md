# F1C200s：打包 SPI-NAND 启动镜像

> 镜像打包布局必须与 BootROM 读取方式和 SPL loader 的偏移配置一致；打包成功不等于能够从 NAND 启动。

## 构建与打包

示例平台为 `lctech_pi_f1c200s`，构建环境为 Ubuntu 24.04；除 U-Boot 常规依赖外，还需要 `swig` 和 `libpython3-dev`。`bear` 用于生成编译数据库，可按需使用。

```bash
make lctech_pi_f1c200s_defconfig
bear -- make -j$(nproc) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
./gen_sunxi_spinand_onlyboot_img.sh \
  u-boot-sunxi-with-spl-spinand.bin u-boot-sunxi-with-spl.bin 2048 128
```

脚本参数依次为输出镜像、输入镜像、NAND 页大小 2048 字节、擦除块大小 128 KiB。脚本来源及读取约束见 [[SPL Image Loader]]，移植时应固定具体版本。

## 核对布局

`.imgmeta` 示例中的关键数据：

```text
SPL-size 24576
u-boot-size 368316
Page-size 2 KiB
PEB-size 128 KiB
SPL-count 1
u-boot-count 1
spl-0 0x0
u-boot-0 0x20000
```

SPL 的 `CONFIG_SYS_SPI_U_BOOT_OFFS` 应与当前输出中的 U-Boot 偏移一致，此处为 `0x20000`。同时检查输出镜像：

```bash
hexdump -C u-boot-sunxi-with-spl-spinand.bin | less
```

在 `0x20000` 处可见 legacy image magic `27 05 19 56`；对应的一个镜像头标识为 U-Boot 2025.01。镜像头、实际载入地址和 loader 的格式支持还需一起核对。若 SPL 尚未实现 NAND loader，可先通过 FEL 启动到 U-Boot，再用 [[enable MTD|MTD 命令]]检查 Flash。
