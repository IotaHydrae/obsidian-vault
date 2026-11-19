# Linux panel-simple DSI 面板

> `panel-simple` 的 DSI 路径适用于驱动表支持、初始化要求足够简单的面板；匹配型号、供电和显示时序需要同时成立。

## TL;DR

- 驱动根据 DSI `compatible` 找到面板描述，设置 lane 数、像素格式和 DSI mode flags，再 attach 到 host。
- 对驱动已支持的简单面板，在 DSI host 的 DTS 子节点配置型号、虚拟通道、电源、使能 GPIO 和背光；`remote-endpoint` 不创建设备。
- panel prepare / enable、disable / unprepare 与 host / bridge 的显示管线协作；背光和像素数据是不同路径。
- 需要 DCS 初始化时，主线通常由专用面板驱动发送命令；支持 DTS 命令序列的厂商 BSP 可按其扩展格式配置。
- 黑屏先分辨 probe / attach、供电复位、mode、链路和背光，不能只看 DRM 设备是否出现。

范围：主线部分以 Linux 6.18.62 `panel-simple.c` 的 DSI 分支为准；DTS 命令序列另以 Rockchip BSP 6.1 为例。具体面板和 host 能力以 binding 与硬件为准。

## 从设备树到 panel

```text
DSI host 创建设备 → compatible 匹配 dsi_of_match
→ panel_simple_dsi_probe() / panel_simple_probe()
→ 设置 lanes、format、mode_flags → mipi_dsi_attach()
→ 上游 DRM 组件建立管线 → panel prepare / enable
```

**设备枚举和显示管线连接是两件事。** `mipi_dsi_host_register()` 枚举 host 下可用且带 `reg` 的直接子节点，`reg` 表示 DSI virtual channel。OF graph 的 `remote-endpoint` 表达组件之间的连接，不能代替这一步；若拓扑不使用 host 子节点，必须由相应驱动显式注册 `mipi_dsi_device`。实际面板的 `compatible` 要核对 binding 和驱动匹配表，不能只写一个泛化名称。

面板描述中的固定 timing、尺寸及 DSI 参数由对应型号定义；`panel-timing` 是否支持覆盖取决于驱动和 binding。已有的面板不应为了凑分辨率而借用另一型号的 `compatible`。

## DTS 配置示例

下面以 Linux 6.18.62 主线中的 **Tegra210 Jetson TX1 Developer Kit + AUO B080UAN01 面板**为例。`auo,b080uan01` 同时存在于 `panel-simple-dsi.yaml` 和 `panel-simple.c` 的 `dsi_of_match`，因此该型号可以通过设备树选择现有面板描述，而不需要为它再写一个 panel 驱动。

示例依赖板级文件 `arch/arm64/boot/dts/nvidia/tegra210-p2371-2180.dts` 及其包含的 `.dtsi`：`dsia`、GPIO 控制器和电源标签已经在那里定义，`backlight` 是该板级文件中的 I²C 背光设备。以下是放在**已有板级 DTS 中**的片段，不是可以单独编译的完整 DTS；若文件中已经有 `panel@0`，应修改该节点，而不是重复添加。

```dts
#include <dt-bindings/gpio/tegra-gpio.h>

&dsia {
    status = "okay";

    /* Tegra DSI 控制器自身的模拟电源 */
    avdd-dsi-csi-supply = <&vdd_dsi_csi>;

    panel@0 {
        compatible = "auo,b080uan01";
        reg = <0>;

        power-supply = <&vdd_5v0_io>;
        enable-gpios = <&gpio TEGRA_GPIO(V, 2) GPIO_ACTIVE_HIGH>;
        backlight = <&backlight>;
    };
};
```

### 每个属性的作用

| 配置 | 作用与替换边界 |
| --- | --- |
| `&dsia` / `status` | 启用实际连接屏幕的 DSI host；`dsia` 是 Tegra210 的标签，其他 SoC 应使用自己的 host 节点 |
| `avdd-dsi-csi-supply` | **DSI 控制器**的 Tegra 专有电源属性，不是通用面板属性；其他 host 按自己的 binding 配置 |
| `panel@0` / `reg = <0>` | 把面板作为 DSI host 子设备，使用虚拟通道 0；这不是 lane 数量 |
| `compatible` | 选择 `panel-simple.c` 中该型号的 `panel_desc_dsi`；必须与实际面板一致 |
| `power-supply` | 面板供电 regulator；`vdd_5v0_io` 是本板的电源标签和连线，不可直接照搬其他板 |
| `enable-gpios` | 面板使能信号，GPIO 控制器、引脚和有效电平取决于板级电路；它不是背光使能 |
| `backlight` | 指向已经注册的背光设备；本板的 `backlight` 节点由 I²C 背光驱动提供 |

`#address-cells = <1>` 和 `#size-cells = <0>` 已由本例的 SoC `.dtsi` 为 `dsia` 设置。上游 Tegra 显示控制器使用其 `nvidia,outputs` 关联 DSI host，本例的 panel 直接作为 host 子节点，不需要为了这个例子额外添加 `port` / `remote-endpoint`。其他 SoC 可能需要 OF graph，应按该 **host binding** 配置，而不是照搬 Tegra 的拓扑。

### 时序、lane 和格式在哪里配置

`compatible = "auo,b080uan01"` 命中驱动中的 `auo_b080uan01`。以下参数由 `panel-simple.c` 的对应描述符提供，`panel_simple_dsi_probe()` 将其中的 `lanes`、`format` 和 `flags` 传给 DSI host：

| 参数 | 本例驱动描述符的值 |
| --- | --- |
| 分辨率 / pixel clock | `1200 × 1920`，`154500 kHz` |
| 水平前沿 / 同步宽度 / 后沿 | `62 / 4 / 62` 个像素，水平总数 `1328` |
| 垂直前沿 / 同步宽度 / 后沿 | `9 / 2 / 8` 行，垂直总数 `1939` |
| 刷新率 | 约 `60 Hz`，由 pixel clock 和总像素数计算 |
| DSI 数据 lane / 像素格式 | `4` lane，`MIPI_DSI_FMT_RGB888` |
| DSI flags | `MIPI_DSI_MODE_VIDEO`、`MIPI_DSI_CLOCK_NON_CONTINUOUS` |

不要为了改 lane 数或格式而在这个面板节点中添加任意属性，或者把 `reg` 改成 lane 数；这些值在本例由驱动描述符给出，host 和实际连线也必须支持它们。

### `panel-timing` 和 reset 的边界

`panel-simple.c` 的共用解析路径会尝试读取 `panel-timing`，但只允许覆盖驱动描述符中允许范围内的 `display_timing`。**本例 AUO 面板使用固定 `drm_display_mode`（`num_modes = 1`）**，驱动会拒绝覆盖；6.18.62 的 `panel-simple-dsi.yaml` 也没有把 `panel-timing` 列为允许属性，不应添加到这个节点。

Linux 6.18.62 的 `panel-simple.c` 获取并控制的是 `enable-gpios`；虽然通用 panel binding 中存在 `reset-gpios`，本驱动不会仅因节点添加它就执行 reset 脉冲。如果屏幕需要特定 reset 序列、多路电源或 DCS 初始化命令，应使用适配该屏的专用驱动或按实际硬件实现所需支持。

## 需要 DCS 初始化时

很多 DSI 面板在上电、复位后，还需要 DCS 命令或控制器厂商定义的寄存器写入。**Linux 6.18.62 主线 `panel-simple-dsi` 不解析 `panel-init-sequence`，也不提供通用 `simple-panel-dsi` 匹配项。** 这类面板通常由具体型号的面板驱动在 `prepare()` / `enable()` 等阶段发送命令；例如主线 ST7701 驱动的 `st7701_prepare()` 在供电和复位后执行初始化，`st7701_enable()` 发送 Display On。

一个简化的 DCS 初始化片段如下，**只展示 API 和错误传播，不是适用于任意屏幕的完整初始化序列**。调用者需保证 DSI host 已 attach、处于允许发送命令的状态，面板电源与复位满足要求，并负责失败时的电源回退：

```c
#include <linux/delay.h>
#include <drm/drm_mipi_dsi.h>

static int panel_send_dcs_example(struct mipi_dsi_device *dsi)
{
    struct mipi_dsi_multi_context ctx = { .dsi = dsi };

    /* 按具体面板手册，在此之前发送需要的厂商初始化命令。 */
    mipi_dsi_dcs_exit_sleep_mode_multi(&ctx);
    mipi_dsi_msleep(&ctx, 120);
    mipi_dsi_dcs_set_display_on_multi(&ctx);

    return ctx.accum_err;
}
```

这里的 `120 ms` 只是示例，实际命令顺序、延时、LP/HS 传输模式及背光开启时机必须按目标面板和 host 确定。厂商寄存器可能要求 DCS write，也可能要求 generic write；应根据控制器协议选择 `mipi_dsi_dcs_write*()` 或 `mipi_dsi_generic_write*()`，不能只凭命令字节判断。

`mipi_dsi_multi_context` 在首次命令失败后记录 `accum_err`，后续带该 context 的命令和 `mipi_dsi_msleep()` 会跳过；调用者检查返回值并回退已经打开的资源。面板的 `compatible`、电源、reset 与背光仍由其**专用 binding 和 DTS**描述；不同面板的厂商命令参数不能通用。

### Rockchip BSP 的 DTS 命令序列

部分 Rockchip BSP 在 `panel-simple.c` 中扩展了 `compatible = "simple-panel-dsi"`，可从 DTS 读取 `dsi,lanes`、`dsi,format`、`display-timings` 和 `panel-init-sequence`。下面以一份 **Rockchip 6.1 BSP 的 800 × 1280、4 lane 面板配置**为例；它不是 Linux 6.18 主线 binding。GPIO、供电、背光、host 和命令必须改为实际硬件参数。

```dts
#include <dt-bindings/gpio/gpio.h>
#include <dt-bindings/pinctrl/rockchip.h>
#include <dt-bindings/display/drm_mipi_dsi.h>

&dsi {
    status = "okay";

    panel@0 {
        compatible = "aoly,sl008pa21y1285-b00", "simple-panel-dsi";
        reg = <0>;
        backlight = <&backlight>;
        enable-gpios = <&gpio0 RK_PB7 GPIO_ACTIVE_LOW>;
        reset-gpios = <&gpio3 RK_PB7 GPIO_ACTIVE_LOW>;
        prepare-delay-ms = <20>;
        reset-delay-ms = <20>;
        init-delay-ms = <20>;
        enable-delay-ms = <120>;
        disable-delay-ms = <20>;
        unprepare-delay-ms = <20>;
        width-mm = <108>;
        height-mm = <172>;

        dsi,flags = <(MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
                      MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET)>;
        dsi,format = <MIPI_DSI_FMT_RGB888>;
        dsi,lanes = <4>;

        panel-init-sequence = [
            05 78 01 11
            05 14 01 29
        ];
        panel-exit-sequence = [
            05 00 01 28
            05 00 01 10
        ];

        display-timings {
            native-mode = <&panel_timing>;

            panel_timing: timing0 {
                clock-frequency = <66000000>;
                hactive = <800>;
                vactive = <1280>;
                hfront-porch = <2>;
                hsync-len = <18>;
                hback-porch = <18>;
                vfront-porch = <4>;
                vsync-len = <4>;
                vback-porch = <16>;
                hsync-active = <0>;
                vsync-active = <0>;
                de-active = <0>;
                pixelclk-active = <0>;
            };
        };
    };
};
```

这是节点片段：`dsi`、`gpio0`、`gpio3` 和 `backlight` 必须已在板级 DTS/DTSI 中定义，显示控制器到 DSI host 的连接也需按该 BSP 配置。该实例的板级供电由实际硬件配置决定；若面板由独立 regulator 供电，还应按 BSP binding 配置 `power-supply`。

在这份 BSP 的 `panel_simple_parse_cmd_seq()` 中，每条记录按 **`数据类型 延时(ms) 载荷长度 载荷...`** 编码；方括号内都是十六进制字节。例如：

| 字节 | 解释 |
| --- | --- |
| `05 78 01 11` | DCS short write（`05`），载荷 1 字节：`0x11` Exit Sleep；发送后等待 `0x78` = 120 ms |
| `05 14 01 29` | DCS short write，载荷 `0x29` Display On；发送后等待 `0x14` = 20 ms |
| `05 00 01 28` | DCS short write，载荷 `0x28` Display Off；本条延时为 0 |
| `05 00 01 10` | DCS short write，载荷 `0x10` Enter Sleep；本条延时为 0 |

`05` 是 **DSI packet data type**，不是 DCS 命令；载荷的首字节才是这里的 DCS 命令。`15`、`39` 等数据类型与实际载荷及发送函数的关系应以所用 BSP 的解析和发送代码为准。上述退出延时是该板配置，其他面板可能要求命令后等待；不要把它当成通用安全值。

另一个仅用于理解编码的例子是 `15 00 02 3a 77`：`15` 表示带一个参数的 DCS short write，载荷长度为 2，`3a` 是 Set Pixel Format 命令，`77` 是参数。面板是否接受该值、它与视频格式如何对应，必须查该屏的控制器资料；不能直接加入任意面板的初始化序列。

这份 Rockchip 实现中，`panel_simple_of_get_desc_data()` 读取时序和命令序列，`panel_simple_xfer_dsi_cmd_seq()` 用记录中的 data type 选择 DCS 或 generic 发送函数，并在每条命令后按记录延时。具体 DCS short / long packet 类型还会由 `mipi_dsi_dcs_write_buffer()` 按实际载荷长度选择，记录类型和长度必须匹配。发送函数对单条传输错误会记录日志并继续处理，所以 **probe/prepare 成功也要检查 DSI 错误日志**。不同厂商、分支和配置的属性名称、记录格式及错误处理可能不同，必须核对正在使用的 BSP 源码。

## 在主线适配自己的面板

1. 在 `panel-simple.c` 的 `dsi_of_match` 和 `panel-simple-dsi.yaml` 中确认**实际型号**已支持，并核对其 mode、lane、格式和时序。
2. 将 host、regulator、GPIO、背光标签替换成目标板实际节点，检查 DSI host/PHY 的电源、时钟、reset 和显示控制器连接。
3. 如果型号不在匹配表，但面板确实只需要 simple-panel 能力，应为该型号添加匹配项、面板描述符及 binding；仅在 DTS 里编一个 `compatible` 不会自动生成描述符。
4. 如果需要控制器命令或复杂时序，选择对应专用 panel driver；不要复用仅分辨率相同的型号。

## 构建与验证

确认内核启用了 DRM、目标 DSI host 驱动和 `CONFIG_DRM_PANEL_SIMPLE`。`CONFIG_DRM_MIPI_DSI` 是内核内部布尔配置，通常由相关 DRM host 驱动选择；背光、regulator 和 GPIO 对应的驱动也要可用。

在已配置 ARM64 的 Linux 内核源码树中，使用已安装的交叉编译工具链编译示例板的 DTB：

```sh
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
    nvidia/tegra210-p2371-2180.dtb
```

安装了 `dtschema` 及相关依赖后，可进一步检查面板 binding：

```sh
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
    DT_SCHEMA_FILES=display/panel/panel-simple-dsi.yaml dtbs_check
```

若使用 `O=` 构建目录或不同工具链，应为两个命令加入相同的实际构建选项。`dtbs_check` 检查构建到的 DTB；语法或 schema 通过不代表屏幕已经上电、连线正确或实际出图。

启动目标板后，可先确认设备树创建的 DSI 设备和驱动绑定，再查看 DRM 输出：

```sh
dmesg | grep -Ei 'drm|dsi|panel|backlight'
ls -l /sys/bus/mipi-dsi/devices/
ls -l /sys/bus/mipi-dsi/drivers/panel-simple-dsi/
ls -l /sys/class/drm/
```

设备名称、connector 编号和日志因平台而异；`panel-simple-dsi` 驱动目录中的设备链接、connector 的可用 mode、背光和实际图像都应分别核对。设备未绑定时先查 `compatible`、模块和依赖；出现 `-EPROBE_DEFER` 时继续检查 regulator、backlight 或其他供应者。

## 生命周期

| 阶段 | 典型责任 | 观察点 |
| --- | --- | --- |
| probe / attach | 获取电源、GPIO、backlight，注册 panel 并关联 DSI host | 是否有 `-EPROBE_DEFER`、attach 错误或缺少供应者 |
| prepare | 按面板要求准备电源、使能和必要延时 | 电源轨、reset 与实际时间 |
| enable | 让 panel 可显示，并由框架协调 backlight | 面板状态、背光使能 |
| disable / unprepare | 停止显示并按顺序关背光与电源 | 避免仍有活动传输或访问已断电硬件 |

面板回调与 host / bridge 的实际次序以 DRM 驱动实现和器件约束为准；不能把上述表当成任意面板的固定电气波形。

## 调试

1. 验证 host、面板节点、`compatible`、设备树 graph 和 power/backlight 依赖；记录 probe 和 attach 日志。
2. 检查 lane 数、格式、mode flags、像素时钟和 mode timing；host 与 panel 的能力必须匹配。
3. 测量供电、reset 和背光，再结合 DRM connector/CRTC/plane 状态确认像素流是否送出。
4. 画面颜色、偏移或抖动异常时核对总线格式、时序和线缆；在对应硬件上验证，不能只靠软件状态推断信号完整性。

显示管线见 [[Linux DRM KMS 显示管线]]；DBI SPI 屏幕见 [[panel-mipi-dbi]]。

## 参考

- Linux 6.18.62：[`panel-simple.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/panel/panel-simple.c)、[`drm_mipi_dsi.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/drm_mipi_dsi.c)、[panel-simple-dsi binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/display/panel/panel-simple-dsi.yaml)、[DRM panel](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/gpu/drm-kms-helpers.rst)。
- 本例板级配置：[`tegra210-p2371-2180.dts`](https://github.com/gregkh/linux/blob/v6.18.62/arch/arm64/boot/dts/nvidia/tegra210-p2371-2180.dts)、[Tegra DSI host binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/display/tegra/nvidia,tegra20-dsi.yaml)。
- 主线 DCS 实现：[`panel-sitronix-st7701.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/panel/panel-sitronix-st7701.c)、[`drm_mipi_dsi.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/drm/drm_mipi_dsi.h)。
- Rockchip BSP 示例：[驱动 `panel-simple.c`](https://github.com/rockchip-linux/kernel/blob/develop-6.1/drivers/gpu/drm/panel/panel-simple.c)、[RK3326 面板节点](https://github.com/rockchip-linux/kernel/blob/develop-6.1/arch/arm64/boot/dts/rockchip/rk3326-863-lp3-v10.dtsi)、[BSP panel binding](https://github.com/rockchip-linux/kernel/blob/develop-6.1/Documentation/devicetree/bindings/display/panel/panel-simple.yaml)。具体产品仍以所用 BSP 的源码为准。
