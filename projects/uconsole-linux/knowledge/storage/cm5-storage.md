# CM5 eMMC 与 uConsole microSD 插槽

> 带 eMMC 的 CM5 不向连接器引出外部 SDIO，底板插卡无热插拔日志符合规格。

## TL;DR

- 先核对核心板是 CM5 还是 CM5Lite；只有 Lite 暴露该外部 SD 接口。
- 本板根存储为 HS400 eMMC，不能根据 dmesg 没反应推断插槽驱动坏。
- 可通过 USB 读卡器使用可移除 SD；该结论不能套用到无 eMMC 的 CM4。

## 判断方法

1. 核对核心板具体料号与 eMMC 容量，区分 CM5 和 CM5Lite；底板有插槽不证明接口可用。
2. 对照模块数据手册的连接器信号，再用 `lsblk`、`/sys/class/mmc_host` 与内核日志识别实际存储。
3. 将根 eMMC、无线 SDIO 和外部存储分开记录；不要通过试加 overlay 推断物理信号存在。
4. 需要可移除存储时选择 USB 读卡器等独立路径，并单独验证读卡器、卡和启动策略。

## 硬件原因与规格依据

[Raspberry Pi CM5 数据手册](https://datasheets.raspberrypi.com/cm5/cm5-datasheet.pdf)第 2.7 节 **SDIO (CM5Lite only)**，以及连接器引脚 61–72：外部 SDIO 接口由不带板载 eMMC 的 CM5Lite 引出。带 eMMC 的 CM5 不通过连接器引出这些 SD 数据和命令信号。

## 设备观察，2026-10-17

- 型号：Raspberry Pi Compute Module 5 Rev 1.0。
- `mmc0` 发现一个 29.1 GiB HS400 MMC 设备 `BJTD4R`，带有 eMMC 启动和 RPMB 硬件分区。`/dev/mmcblk0p2` 是 `/`，`mmcblk0p1` 是 `/boot/firmware`。
- `mmc1` 发现供 `brcmfmac43455-sdio` 使用的 SDIO Wi-Fi 设备。
- `/sys/class/mmc_host` 中只有 `mmc0` 和 `mmc1`。
- 用户插入 microSD 卡后，没有观察到新的 `dmesg` 消息。

## 适用边界与未知项

没有插卡日志，与该模块的硬件接口限制一致。启用 SD overlay 无法补出连接器上不存在的信号。尚未用外部读卡器检查卡本身的状态。检查期间没有修改设备存储或启动配置。
