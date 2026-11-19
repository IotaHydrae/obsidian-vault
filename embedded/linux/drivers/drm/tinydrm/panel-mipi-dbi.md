# Linux panel-mipi-dbi SPI 显示驱动

> `panel-mipi-dbi` 使用 SPI 和 MIPI DBI/DCS 控制带帧存的显示控制器；设备树描述硬件，固件文件提供控制器初始化命令。

## TL;DR

- DBI 是控制器接口，DSI 是另一类显示链路；不要把 DBI 面板当作 DSI 面板配置。
- 驱动通过第一个 `compatible` 加 `.bin` 加载初始化固件；文件名、格式和设备控制器必须匹配。
- `panel-timing` 的分辨率及偏移、SPI、供电、reset、D/C、背光和像素格式都需要按实际硬件设置。
- 6.18.62 的驱动实际只接受 `r5g6b5` 和 `b6x2g6x2r6x2`；不能只看 binding 的枚举选 `format`。
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

### 初始化固件怎么读

| 位置 | 6.18.62 驱动的解释 |
| --- | --- |
| 前 15 字节 | ASCII `MIPI DBI` 后补七个 `00` |
| 第 16 字节 | 文件格式版本 `01` |
| 此后每条记录 | `命令字节 参数数量 参数…`；参数数量不含命令自身 |
| `00 01 78` | 特殊记录，等待 `0x78` = 120 ms，不向面板发送这三个字节 |

例如 `11 00` 是无参数的 Sleep Out 命令。`od -Ax -tx1 -N 24 /实际路径/sainsmart18.bin` 可查看头部和首条记录；驱动用 `panel_mipi_dbi_check_commands()` 检查头部、长度和版本，但不能验证命令是否适用于实际屏幕。初始化值来自面板控制器和模块资料。这个 `.bin` 格式不是 DSI BSP 的 `panel-init-sequence`。

## 硬件和设备树

| 项目 | 要核对什么 |
| --- | --- |
| `compatible` / `reg` | binding 允许的具体面板标识、通用 `panel-mipi-dbi-spi` 及片选 |
| SPI 与 D/C | 有 `dc-gpios` 走 Type C Option 3；无此 GPIO 走 Option 1，D/C 编入第 9 位。控制器不支持 9 bit 时，驱动可打包成 8 bit SPI 传输 |
| `reset-gpios`、供电与背光 | 有效极性、上电顺序、时序与电平匹配 |
| `write-only` | 控制器读取线路未连接时按 binding 标记；不能用它掩盖其他通信故障 |
| `panel-timing` | `hactive` / `vactive`，及 binding 中定义的可选偏移；不能当作普通视频面板 porch 模板 |
| `format` | 目标线上的像素布局；与初始化命令一致。不写默认 `r5g6b5`，其他值须查 `panel_mipi_dbi_formats[]` |

### DTS 示例：有 D/C 引脚的 160 × 128 SPI 屏

以下是基于 6.18.62 binding 的 `sainsmart18` 示例组织的板级片段。`spi0`、`gpio`、`backlight` 必须是目标 DTS 中的实际标签；引脚、频率、供电和屏幕型号必须对应电路。不要同时声明同片选的另一个设备。

```dts
#include <dt-bindings/gpio/gpio.h>

&spi0 {
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";

    display@0 {
        compatible = "sainsmart18", "panel-mipi-dbi-spi";
        reg = <0>;
        spi-max-frequency = <40000000>;
        dc-gpios = <&gpio 24 GPIO_ACTIVE_HIGH>;
        reset-gpios = <&gpio 25 GPIO_ACTIVE_HIGH>;
        write-only;
        backlight = <&backlight>;
        width-mm = <35>;
        height-mm = <28>;
        format = "r5g6b5";

        panel-timing {
            clock-frequency = <0>;
            hactive = <160>;
            vactive = <128>;
            hfront-porch = <0>;
            hback-porch = <0>;
            hsync-len = <0>;
            vfront-porch = <0>;
            vback-porch = <0>;
            vsync-len = <0>;
        };
    };
};
```

`reg = <0>` 选 CS0，第二个兼容串绑定通用驱动，第一个兼容串决定请求 `sainsmart18.bin`。`hback-porch` / `vback-porch` 在这里是显存窗口的 x/y 偏移，不是视频信号的消隐长度。若电路有独立逻辑电源，按 binding 补 `power-supply` / `io-supply`。核对 reset 时看实际电平：本驱动的 `mipi_dbi_hw_reset()` 输出 GPIO 逻辑 `0` 再 `1`；按示例的 `GPIO_ACTIVE_HIGH` 且没有外部反相时，得到先拉低、再拉高的波形。

## 失败定位

1. 看 SPI 设备、驱动绑定与 `dmesg`：固件找不到、格式错误、供电/背光依赖未就绪是不同阶段。
2. 检查 reset、D/C、片选、时钟和数据波形，以及初始化命令的实际次序。
3. DRM 注册后检查 mode、格式和 framebuffer；背光亮只能证明背光路径工作。
4. 图像偏移、镜像或颜色异常时核对 offset、控制器地址窗口、旋转方向和 RGB/BGR，结合实际测试图案验证。

DRM 管线见 [[Linux DRM KMS 显示管线]]，SPI 总线见 [[Linux I2C 与 SPI 驱动数据路径]]。

## 参考

- Linux 6.18.62：[`panel-mipi-dbi.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/tiny/panel-mipi-dbi.c)、[panel-mipi-dbi-spi binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/display/panel/panel-mipi-dbi-spi.yaml)、[`drm_mipi_dbi.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/drm_mipi_dbi.c)。
