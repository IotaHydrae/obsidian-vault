# Linux DRM KMS 显示管线

> KMS 将显示缓冲区、合成、输出接口和显示终端连接成管线；画面异常应沿这条路径逐段确认。

## TL;DR

- 黑屏先分离模式设置、像素数据、链路、面板电源和背光。
- framebuffer 描述缓冲区布局，plane / CRTC / connector 的状态组合要通过驱动检查。
- atomic commit 与 buffer / fence 生命周期需协调；提交成功不等于面板已显示预期画面。

适用范围：Linux 6.18 LTS。

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

## 从缓冲区到显示

framebuffer 记录图像的尺寸、像素格式、stride、偏移和底层存储引用，不等同于显示控制器或面板。plane 将其中的数据按硬件支持的格式、裁剪、位置和缩放要求送给 CRTC；CRTC 对应扫描输出及 mode 时序。实际设备可能有多个 plane、多个 CRTC 或多级 bridge，图中只表达常见逻辑关系。

显示 buffer 的分配与共享可使用 GEM、dma-buf 等机制，具体取决于驱动和用户空间。共享一个 dma-buf 并不自动证明各设备支持相同格式、modifier、寻址方式和同步协议。

## Atomic 检查与提交

1. 用户空间构造 plane、CRTC 和 connector 的新状态，包括 framebuffer 和 mode。
2. 驱动 atomic check 验证路由、格式、缩放、时序和硬件资源是否允许该组合；检查成功不代表已经改变画面。
3. commit 后由驱动按同步和硬件要求更新状态；framebuffer、fence 和相关对象必须保持有效到允许释放的时机。
4. 输出仍需经过 bridge / panel、供电和背光；显示控制器状态正常不等于物理链路已就绪。

| 提交标志或属性 | 排查时的用途 |
| --- | --- |
| `DRM_MODE_ATOMIC_TEST_ONLY` | 检查这一组属性是否可用，不改变当前显示状态；不能据此证明画面已切换 |
| `DRM_MODE_ATOMIC_ALLOW_MODESET` | 允许需要 modeset 的状态变化；对同一组属性带和不带它做 `TEST_ONLY` 可区分是否需要 modeset |
| `DRM_MODE_ATOMIC_NONBLOCK` | 不等待完整的硬件提交完成；用户态根据请求的 page-flip 事件或 CRTC out-fence 判断完成，不用 ioctl 返回时间推断上屏 |
| plane `IN_FENCE_FD` / CRTC `OUT_FENCE_PTR` | 支持时分别表达输入缓冲何时可用、这次输出提交何时完成；检查对象是否公开对应属性 |

KMS 对象 ID、`/dev/dri/cardN` 编号和驱动提供的 debugfs 信息会因硬件与启动顺序变化；调试时结合设备拓扑，不硬编码数字。

## Linux 6.18 源码入口

- `drivers/gpu/drm/drm_atomic_uapi.c` 的 `drm_mode_atomic_ioctl()` 处理用户空间 atomic 请求。
- 对采用 atomic helper 的驱动，`drm_atomic_helper_check()` 和 `drm_atomic_helper_commit()` 提供常见检查及提交框架；具体驱动可以有自己的回调和硬件处理。
- 面板及桥接器的上电、复位、链路和背光步骤由相关驱动及硬件约束决定，不能把 framebuffer 创建成功当作显示成功。

## 黑屏和花屏的区分

| 现象 | 优先检查 |
| --- | --- |
| connector 不存在 | 设备树端点连接、面板 / bridge probe、驱动依赖 |
| connector 存在但无可用 mode | 面板时序、显示链路、可能使用的 EDID 及对应驱动配置 |
| atomic check 失败 | plane 能力、格式 / modifier、缩放、路由和带宽限制 |
| 提交成功但屏幕全黑 | plane 是否可见、buffer 内容、CRTC 时序、面板供电与背光 |
| 颜色错误、倾斜或撕裂 | 格式、stride、同步、buffer 生命周期和链路时序 |

EDID 只适用于提供该信息的连接方式；固定时序面板不一定有 EDID。检查 KMS 对象时记录同一次测试的 connector、CRTC、plane、mode 和 framebuffer，避免混用多次提交的状态。

## 排查顺序

1. 确认 connector、mode 和管线是否被识别，检查内核日志、设备树连接及 DRM 驱动绑定。
2. 黑屏时核对面板供电、复位、背光、时钟、桥接器以及链路状态；背光亮不等于像素流正确。
3. 花屏或颜色错误时检查分辨率、时序、格式、stride、总线位宽和 buffer 内容。
4. 间歇问题再看 atomic 提交、buffer / fence 生命周期及 DMA 同步。

有限制地读取 `dri/<DRM minor>/state`（若启用 debugfs 和 atomic 状态输出，通常在 `/sys/kernel/debug/` 下）；先确认对应的 DRM 设备，再对照 connector 的 `crtc`、CRTC 的 `active` / `mode` 和主 plane 的 `fb` / `crtc`。`status=connected` 但没有活动 CRTC 是路由或提交未成立；活动 CRTC 上没有有效 framebuffer 时，应查 plane，不能先归因于背光。这里读取的是软件状态，不触发 modeset。具体 MIPI DSI / DBI 配置见 [[simple-panel-dsi]]、[[panel-mipi-dbi]]。

## 参考

- 蜗窝科技：[Linux 图形子系统](https://www.wowotech.net/graphic_subsystem/graphic_subsystem_overview.html)、[DRI](https://www.wowotech.net/graphic_subsystem/dri_overview.html)。
- Linux 6.18.62：[DRM KMS](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/gpu/drm-kms.rst)、[DRM 用户接口](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/gpu/drm-uapi.rst)。
- Linux 6.18.62：[`drm_atomic_uapi.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/drm_atomic_uapi.c)、[`drm_atomic_helper.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/gpu/drm/drm_atomic_helper.c)。
