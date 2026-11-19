# Linux panel-mipi-dbi SPI 显示驱动

> `panel-mipi-dbi` 使用 SPI 和 MIPI DBI/DCS 控制带帧存的显示控制器；设备树描述硬件，固件文件提供控制器初始化命令。

## TL;DR

- DBI 是控制器接口，DSI 是另一类显示链路；不要把 DBI 面板当作 DSI 面板配置。
- 驱动通过第一个 `compatible` 加 `.bin` 加载初始化固件；文件名、格式和设备控制器必须匹配。
- `panel-timing` 的分辨率及偏移、SPI、供电、reset、D/C、背光和像素格式都需要按实际硬件设置。
- DRM 节点出现不代表面板已显示正确图像；需分别验证固件、供电、像素传输和画面。

范围：Linux 6.18.62 的 `drivers/gpu/drm/tiny/panel-mipi-dbi.c` 和对应 DT binding。

## 驱动路径

```text
SPI 设备匹配 → panel_mipi_dbi_spi_probe()
→ 解析 panel-timing / 电源 / GPIO / SPI
→ request_firmware("<第一个 compatible>.bin")
→ 初始化 DRM simple display pipe 并注册 DRM 设备
→ 启用管线时复位 / 执行初始化命令 / 更新 framebuffer
```

Linux 6.18.62 的驱动在 `panel_mipi_dbi_commands_from_fw()` 读取 `compatible` 的第一个字符串并追加 `.bin`。固件由 `request_firmware()` 查找，失败时 probe 无法完成；固件路径与排查见 [[Kernel API - request_firmware]]。

固件包含受驱动检查的头部和命令序列。命令、参数数量及延时格式必须符合驱动定义；初始化值应来自面板控制器和模块资料，不能把另一款屏的固件直接复用。不要把任意 DTS `compatible` 写成示例值后就假定绑定和固件兼容。

## 硬件和设备树

| 项目 | 要核对什么 |
| --- | --- |
| `compatible` / `reg` | binding 允许的具体面板标识、通用 `panel-mipi-dbi-spi` 及片选 |
| SPI 与 D/C | 独立 `dc-gpios` 的 8 位方式或适用的 9 位方式；控制器能力和接线一致 |
| `reset-gpios`、供电与背光 | 有效极性、上电顺序、时序与电平匹配 |
| `write-only` | 控制器读取线路未连接时按 binding 标记；不能用它掩盖其他通信故障 |
| `panel-timing` | `hactive` / `vactive`，及 binding 中定义的可选偏移；不能当作普通视频面板 porch 模板 |
| `format` | 面板支持的 RGB/BGR 位布局；与初始化命令和 framebuffer 格式一致 |

`dc-gpios`、SPI 三线/四线、位宽和速率都由控制器与线路共同约束；binding 中的示例值不是目标板的通用参数。面板 `hback-porch` / `vback-porch` 在此 binding 中用于显示区域偏移，检查驱动计算后再设定。

## 失败定位

1. 看 SPI 设备、驱动绑定与 `dmesg`：固件找不到、格式错误、供电/背光依赖未就绪是不同阶段。
2. 检查 reset、D/C、片选、时钟和数据波形，以及初始化命令的实际次序。
3. DRM 注册后检查 mode、格式和 framebuffer；背光亮只能证明背光路径工作。
4. 图像偏移、镜像或颜色异常时核对 offset、控制器地址窗口、旋转方向和 RGB/BGR，结合实际测试图案验证。

DRM 管线见 [[Linux DRM KMS 显示管线]]，SPI 总线见 [[Linux I2C 与 SPI 驱动数据路径]]。

## 参考

- Linux 6.18.62：[`panel-mipi-dbi.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/tiny/panel-mipi-dbi.c)、[panel-mipi-dbi-spi binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/display/panel/panel-mipi-dbi-spi.yaml)、[`drm_mipi_dbi.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/drm_mipi_dbi.c)。
