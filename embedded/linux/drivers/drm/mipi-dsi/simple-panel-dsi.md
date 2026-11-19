# Linux panel-simple DSI 面板

> `panel-simple` 的 DSI 路径适用于驱动表支持、初始化要求足够简单的面板；匹配型号、供电和显示时序需要同时成立。

## TL;DR

- 驱动根据 DSI `compatible` 找到面板描述，设置 lane 数、像素格式和 DSI mode flags，再 attach 到 host。
- panel prepare / enable、disable / unprepare 与 host / bridge 的显示管线协作；背光和像素数据是不同路径。
- 涉及复杂 DCS 初始化、多个电源或特殊时序的面板，不应强行假定通用 simple-panel 能满足。
- 黑屏先分辨 probe / attach、供电复位、mode、链路和背光，不能只看 DRM 设备是否出现。

范围：Linux 6.18.62 `panel-simple.c` 的 DSI 分支，具体面板和 host 能力以 binding 与硬件为准。

## 从设备树到 panel

```text
DSI host 创建设备 → compatible 匹配 dsi_of_match
→ panel_simple_dsi_probe() / panel_simple_probe()
→ 设置 lanes、format、mode_flags → mipi_dsi_attach()
→ 上游 DRM 组件建立管线 → panel prepare / enable
```

设备树节点可以是 DSI host 的子节点，或按 binding 使用 OF graph 关联。`reg` 表示 DSI virtual channel；实际面板可用的 `compatible` 要同时查看 binding 与目标驱动匹配表，不能只写一个泛化名称。

面板描述中的固定 timing、尺寸及 DSI 参数由对应型号定义；`panel-timing` 是否支持覆盖取决于驱动和 binding。已有的面板不应为了凑分辨率而借用另一型号的 `compatible`。

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

- Linux 6.18.62：[`panel-simple.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/panel/panel-simple.c)、[panel-simple-dsi binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/display/panel/panel-simple-dsi.yaml)、[DRM panel](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/gpu/drm-kms-helpers.rst)。
