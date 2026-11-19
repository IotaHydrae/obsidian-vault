# Linux DRM KMS 显示管线

> KMS 将显示缓冲区、合成、输出接口和显示终端连接成管线；画面异常应沿这条路径逐段确认。

## 主要对象

```text
Framebuffer → Plane → CRTC → Encoder / Bridge → Connector / Panel
```

| 对象 | 作用 |
| --- | --- |
| framebuffer / buffer | 描述显示内容及格式、尺寸、stride 等；底层缓冲区可由不同机制管理 |
| plane | 向 CRTC 提供图层；硬件支持的格式、缩放和组合能力不同 |
| CRTC | 管理扫描输出和显示时序 |
| encoder / bridge | 将显示数据转换或传递给具体输出链路 |
| connector / panel | 表达外部连接、屏幕和可用模式 |

Atomic KMS 用状态描述一次显示更新；驱动在检查阶段验证组合是否合法，提交后由硬件按实现生效。模式、像素格式、buffer 生命周期和同步都可能影响最终画面。

## 排查顺序

1. 确认 connector、mode 和管线是否被识别，检查内核日志、设备树连接及 DRM 驱动绑定。
2. 黑屏时核对面板供电、复位、背光、时钟、桥接器以及链路状态；背光亮不等于像素流正确。
3. 花屏或颜色错误时检查分辨率、时序、格式、stride、总线位宽和 buffer 内容。
4. 间歇问题再看 atomic 提交、buffer / fence 生命周期及 DMA 同步。

工具和 debugfs 节点依内核配置、权限与驱动而异；具体 MIPI DSI / DBI 面板配置见 [[simple-panel-dsi]]、[[panel-mipi-dbi]]。

## 参考

- 蜗窝科技：[Linux 图形子系统](https://www.wowotech.net/graphic_subsystem/graphic_subsystem_overview.html)、[DRI](https://www.wowotech.net/graphic_subsystem/dri_overview.html)。
- Linux 6.18.62：[DRM KMS](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/gpu/drm-kms.rst)、[DRM 用户接口](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/gpu/drm-uapi.rst)。
