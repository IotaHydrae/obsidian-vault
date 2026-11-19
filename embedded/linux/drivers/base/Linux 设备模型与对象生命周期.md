# Linux 设备模型与对象生命周期

> 设备模型把设备、驱动和总线的关系交给驱动核心管理；匹配、探测、解绑和对象最终释放是不同阶段。

## 对象关系

| 对象 | 职责 |
| --- | --- |
| `struct device` | 表示内核中的设备实例、父子关系及资源归属 |
| `struct device_driver` | 提供匹配后探测和解绑等操作；具体回调由相应总线组织 |
| bus | 按总线规则匹配设备和驱动，并协调 probe / remove |
| class | 按用户可见功能归类设备，不等同于实际连接总线 |
| kobject / sysfs / uevent | 支撑对象引用、用户空间可见关系和设备事件；sysfs 是内核对象视图 |

一般流程是注册设备与驱动、匹配、执行 probe，解绑时执行 remove，再在最后引用释放时回收设备对象。实际注册顺序不固定，probe 也可能失败或返回 `-EPROBE_DEFER`；一个 sysfs 节点存在不等于驱动已绑定或硬件可用。

驱动私有数据、IRQ、DMA 和工作项有各自生命周期。解绑时先阻止新请求、停设备并同步异步回调，再释放数据；`devm_*` 自动回收注册资源，不证明硬件或线程已经停止。详见 [[Kernel API - devres]]、[[Kernel API - kref]]。

Platform 总线的匹配顺序见 [[platform_match]]；设备树如何产生对应总线设备见 [[platform|设备树与 Platform 设备]]。

## 参考

- 蜗窝科技：[设备与驱动](https://www.wowotech.net/device_model/device_and_driver.html)、[Bus](https://www.wowotech.net/device_model/bus.html)、[Class](https://www.wowotech.net/device_model/class.html)、[sysfs](https://www.wowotech.net/device_model/dm_sysfs.html)。
- Linux 6.18.62：[设备模型](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/overview.rst)、[设备资源管理](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/devres.rst)。
