我们需要借助 u-boot 的 mtd 命令来加载 spinand 中的内核、设备树文件到内存中

要使用mtd命令，首先需要开启一些 CONFIG 支持

```c
CONFIG_CMD_MTD
CONFIG_DM_MTD
CONFIG_MTD_SPI_NAND
```

截止目前，新的 defconfig 与原始的 lctech_pi_f1c200s_defconfig 有如下差异

```c
❯ make savedefconfig
scripts/kconfig/conf  --savedefconfig=defconfig Kconfig
❯ diff defconfig configs/lctech_pi_f1c200s_defconfig
10,12d9
< CONFIG_SPL_SPI_NAND_SUNXI=y
< CONFIG_SYS_SPI_U_BOOT_OFFS=0x20000
< CONFIG_CMD_MTD=y
14,15d10
< CONFIG_DM_MTD=y
< CONFIG_MTD_SPI_NAND=y
```

设备树中添加 spi-nand 节点
```c
&spi0 {
	pinctrl-names = "default";
	pinctrl-0 = <&spi0_pc_pins>;
	status = "okay";

	flash@0 {
		compatible = "spi-nand";
		reg = <0>;
		spi-max-frequency = <50000000>;
	};
};
```

u-boot启动后，在控制台中可通过如下命令查看mtd设备:
```sh
=> dm tree 
 Class     Seq    Probed  Driver                Name
-----------------------------------------------------------
 root          0  [ + ]   root_driver           root_driver
 simple_bus    0  [ + ]   simple_bus            |-- soc
 spi           0  [ + ]   sun4i_spi             |   |-- spi@1c05000
 mtd           0  [ + ]   spi_nand              |   |   `-- flash@0
```

测试 mtd 读取命令
```sh
=> mtd read spi-nand0 0x80C00000 0x100000 0x400
Reading 16384 byte(s) (8 page(s)) at offset 0x00100000
```