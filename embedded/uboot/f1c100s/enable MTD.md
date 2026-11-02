# U-Boot 启用 SPI-NAND MTD 命令

> SPI-NAND 的设备树节点、驱动和 MTD 命令都就绪后，才能在 U-Boot 中识别并读取 Flash；这与 SPL 的启动能力分开配置。

## 配置

以下选项对应此实验分支，实际名称和依赖以目标版本 Kconfig 为准：

```text
CONFIG_CMD_MTD=y
CONFIG_DM_MTD=y
CONFIG_MTD_SPI_NAND=y
```

分支中的 SPL 另使用 `CONFIG_SPL_SPI_NAND_SUNXI=y` 和 `CONFIG_SYS_SPI_U_BOOT_OFFS=0x20000`。开启 U-Boot 完整阶段的 MTD 驱动不会自动补齐 SPL loader。

```dts
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

## 检查

在 U-Boot 运行 `dm tree`，确认 SPI 控制器下存在已探测的 `spi_nand` / `flash@0`，再执行读取：

```text
mtd read spi-nand0 0x80C00000 0x100000 0x4000
```

该命令读取 16 KiB，而 `0x400` 只表示 1024 字节；验证日志时必须核对命令与字节数。目标 RAM 地址、Flash 布局和设备名需根据当前板卡确认，后续启动见 [[spi-nand bootcmd]]。
