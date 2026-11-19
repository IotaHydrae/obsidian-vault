# Linux 虚拟驱动学习入口

> 通过可构建的虚拟控制器或设备驱动观察 Linux 子系统的数据路径，再按硬件手册补充真实资源和时序。

## TL;DR

- [[通过 dummy I2C adapter 驱动了解 i2c adapter 驱动框架]]：虚拟 I²C adapter、寄存器设备和 client 驱动。
- [[Linux Dummy SPI 控制器与设备]]：虚拟 SPI controller、片选设备和同步回环传输。
- [[Linux Dummy UART 驱动]]：基于 serial core 的单端口 TTY 软件回环。
- 每篇笔记的 `examples/` 源码相互独立；模块编译依赖目标内核构建树，加载前应在可恢复的测试环境确认内核和权限。

这些示例不驱动物理引脚，不能替代电气、时序、中断、DMA 或真实设备协议测试。内核模块运行在内核权限下；建议先使用虚拟机，尤其不要直接在生产环境运行未经验证的模块。
