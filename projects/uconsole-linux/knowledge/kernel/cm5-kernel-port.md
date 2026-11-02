# CM5 适配 Raspberry Pi Linux 6.18

> CM5 适配由面板、背光和板级 overlay 分担，沿用基线内核的标准 PMIC 驱动。

## TL;DR

- 对照固定 rpi-6.18.y 与参考 6.12 源码，避免把版本名当作二进制源码证明。
- AXP223 的额外寄存器写入须证明必要性后再引入。
- 保留 CM5/RP1 独立配置；CM4 或参考内核运行结果不能代替工程 CM5 完整验证。

## 适配方法

1. 固定基线提交，对照参考驱动和实际 DTBO 接线；二进制版本字符串只能作为线索。
2. 将面板、背光和 CM5 overlay 分成聚焦补丁，按板型选取，不修改上游子模块。
3. 已由标准 AXP22x 驱动提供的功能保持共用；额外 PMIC 写入须有目标电芯和实测依据。
4. 分别检查补丁应用、编译、包内启动文件、overlay 组合和实机功能；每层保留独立结论。

## 源码来源

- 基线：`raspberrypi/linux` 的提交 `234188315037848e5b07d65b85d38edc64095caa`（`rpi-6.18.y`）。
- 对比源码：[`ak-rex/rpi-linux`](https://github.com/ak-rex/rpi-linux/tree/874351b52621220e99c9a2d3849219fcce39c37f)，提交 `874351b52621220e99c9a2d3849219fcce39c37f`（`rpi-6.12.y`）。
- 设备已安装的 `akrex-kernel` 标识版本为 `6.12.94-v8-16k+`。这不能证明对比分支当前提交就是该二进制的确切源码。

## 文件依据与适配取舍

| 范围 | 依据 | 适配方式 |
| --- | --- | --- |
| 面板 | 对比源码中有 `drivers/gpu/drm/panel/panel-cwu50.c`，基线面板 Kconfig/Makefile 中没有对应项 | 保留两套初始化表和识别逻辑；使用 DRM 背光处理，并保证 regulator 清理时启用与释放相匹配。 |
| 背光 | 对比源码中有 `drivers/video/backlight/ocp8178_bl.c`，基线背光 Kconfig/Makefile 中没有对应项 | 保留时序及亮度映射；拒绝访问操作可能睡眠的 GPIO；在熄屏、移除和关机时保持 GPIO 为低。 |
| 板级设备树 | `arch/arm/boot/dts/overlays/clockworkpi-uconsole-cm5-overlay.dts`；已安装 DTBO 的主要接线相同 | 使用独立的 GPIO I2C 总线，为 PMIC 中断保留 GPIO2，并保留 RP1 DSI 时钟和电源接线。 |
| ADC | `drivers/iio/adc/axp20x_adc.c` 已包含 AXP22x 通道和映射 | 无需补丁。参考分支的差异主要是旧 API 和格式。 |
| 电池 | `drivers/power/supply/axp20x_battery.c` 已匹配 `x-powers,axp221-battery-power-supply` | 使用基线的电源遥测；省略直接复制的电芯容量、电流限制，以及未经验证的额外寄存器编程。 |
| 外部电源输入（AC） | `drivers/power/supply/axp20x_ac_power.c` 已匹配 AXP221 | 暂不移植参考分支在 IRQ 处理函数中切换 VBUS 寄存器的操作。 |
| 输入与音频 | 设备输入项使用标准 USB HID；已安装服务通过 `pinctrl` 操作 GPIO10/11 | 输入继续使用标准内核路径；音频路由保留在镜像层。 |

旧电池实现在释放 battery-info 对象后仍读取其中字段，并分别缓存电流和电压用于功率计算。它还会写入全局 PMIC 寄存器，未将操作限定在 uConsole 上。这些改动不是基本电量上报的前提。

## 固件文件选择

板级配置使用 `[cm5]` 和 `os_prefix=uconsole/` 选择工程内核、基础 DTB、overlay 以及显式指定的 initramfs。软件包包含 `overlays/README`：固件通过这一标记判断 overlay 是否属于该前缀目录。包内也包含用于 BCM2712 音频选择的 `overlay_map.dtb`。

配置指定 `cmdline=/cmdline.txt`，镜像检查工具会将该共享文件与 pi-gen 在导出时更新的根分区 PARTUUID 核对。路径开头的斜杠本身不能证明绕过了前缀：后续 CM4 固件日志显示，固件先尝试 `uconsole//cmdline.txt`，然后回退到共享的 `/cmdline.txt`。这是 [CM4 的观察结果](../boot/cm4-boot-time.md)，不构成独立的 CM5 路径解析验证。应避免前缀目录中残留旧命令行，以免继续引用旧根分区。

这些选择依据 Raspberry Pi 官方的[启动选项](https://www.raspberrypi.com/documentation/computers/config_txt.html#os_prefix)和[型号过滤器](https://www.raspberrypi.com/documentation/computers/config_txt.html#model-filters)。CM5 也匹配 `[pi5]`；`[cm5]` 能更准确地表达该镜像的目标范围。

## 历史验证证据与边界

已记录的离线验证中，三个补丁可按顺序应用到完整的固定 Linux 源码。`6.18.57-v8-16k-uconsole` 的 Image、模块、设备树编译及软件包生成已通过；两个新增驱动的模块 vermagic 均与内核匹配。编译后的板级、显示、音频和 USB overlay 可以应用到两种 CM5 eMMC 基础 DTB（CM5IO 和 CM4IO）。在 BCM2712 上，固件 overlay 映射将 `audremap` 重定向到 `audremap-pi5`；软件包包含该映射。

历史软件包哈希和镜像结果记录在[开发状态](../../project/status.md)。当前输入若已改变，
应运行严格产物校验或重新构建；该历史编译结果不证明现存包与当前源码一致。
编译成功和 overlay 应用成功也不能证明实机面板、背光、电池、音频或关机行为正常。
参考 CM5 系统或单模块试验的观察不能替代完整工程 CM5 内核和镜像的上板验证。
