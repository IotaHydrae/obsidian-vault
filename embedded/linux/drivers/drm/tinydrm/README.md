# TinyDRM 与 Simple Display Pipe

> 阅读采用 simple display pipe 的小型 DRM 驱动时，从显示管线回调、framebuffer 更新和总线传输三个部分追踪。

TinyDRM 相关驱动中常见 `drm_simple_display_pipe` 辅助接口，但具体抽象随内核版本和驱动变化。查看目标版本的驱动及 helper 实现，避免只凭目录名推断所有驱动具有相同结构。

MIPI DBI 面板入口为 [[panel-mipi-dbi]]。
