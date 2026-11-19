# Linux 设备模型与对象生命周期

> 设备模型把设备、驱动和总线的关系交给驱动核心管理；匹配、探测、解绑和对象最终释放是不同阶段。

## TL;DR

- 总线负责设备和驱动匹配；class 按功能归类，不能代替总线拓扑。
- sysfs 节点存在不等于 probe 成功；探测失败还可能返回 `-EPROBE_DEFER`。
- 解绑先停止硬件和异步回调，再释放私有数据；`devm_*` 回收不代替停机。

适用范围：Linux 6.18 LTS。

## 对象关系

| 对象 | 职责 |
| --- | --- |
| `struct device` | 表示内核中的设备实例、父子关系及资源归属 |
| `struct device_driver` | 提供匹配后探测和解绑等操作；具体回调由相应总线组织 |
| bus | 按总线规则匹配设备和驱动，并协调 probe / remove |
| class | 按用户可见功能归类设备，不等同于实际连接总线 |
| kobject / sysfs / uevent | 支撑对象引用、用户空间可见关系和设备事件；sysfs 是内核对象视图 |

一般流程是注册设备与驱动、匹配、执行 probe，解绑时执行 remove，再在最后引用释放时回收设备对象。设备与驱动的实际注册顺序不固定。

驱动私有数据、IRQ、DMA 和工作项有各自生命周期。阻止新请求、停设备并同步异步回调的细节见 [[Kernel API - devres]]、[[Kernel API - kref]]。

Platform 总线的匹配顺序见 [[platform_match]]；设备树如何产生对应总线设备见 [[platform|设备树与 Platform 设备]]。

## 从匹配到解绑

```text
设备注册 ──┐
           ├→ 总线 match → probe → 设备与驱动绑定
驱动注册 ──┘                 │
                             ├→ 成功：开放设备接口
                             └→ 失败 / -EPROBE_DEFER：尚未绑定

驱动解绑：停止新请求 → remove / 关闭硬件 → 回收本次绑定的 devres
设备注销：从模型移除设备 → 归还注册引用 → 最后一个引用触发 release
```

- **match** 只回答驱动是否适合该设备；**probe** 还要完成供电、资源获取和硬件初始化，可能失败。
- `-EPROBE_DEFER` 常用于供应者尚未就绪；内核会在合适的时机再次尝试，驱动不应把它转换为成功。
- remove 完成不等于 `struct device` 立即释放。其他代码可能仍持有引用；release 回调负责对象最终释放。
- `dev_set_drvdata()` 保存驱动私有指针，不自动延长该指针的生命周期，也不让并发读取自动安全。

### 注册与引用怎样配对

| 操作 | 能保证什么 |
| --- | --- |
| `device_initialize()` → `device_add()` | 建立初始引用，再把设备发布到模型；`device_add()` 失败也要 `put_device()`，不能直接 `kfree()` |
| 驱动解绑 | 设备可以仍在总线上并重新绑定；之前的 `dev_get_drvdata()` 结果不能跨解绑复用 |
| `device_unregister()` | 调 `device_del()` 移除设备，再 `put_device()` 归还初始引用；有其他引用时 `release` 延后 |
| `get_device()` / `put_device()` | 只延长设备对象寿命，不延长当前驱动绑定、时钟或 `devm_*` 私有数据的寿命 |

使用总线已有的创建设备接口时，按它的成对接口注销；不要再对同一设备额外执行 `device_del()` 或归还并不属于调用者的引用。

## sysfs、class 与引用

`/sys/devices/` 体现设备层次；`/sys/bus/` 和 `/sys/class/` 提供按总线或功能查找的入口，许多条目是指向设备对象的链接。class 不表示硬件一定直接挂在同一物理总线，也不等于驱动绑定状态。

uevent 向用户空间通知对象变化，udev 等服务可以据此创建设备节点和属性；不能把收到事件等同于应用已经打开设备。对象引用仅保证内存生命周期，不能保证设备仍已绑定、上电或允许新 I/O。

## 定位绑定问题

1. 从 `/sys/bus/<bus>/devices/` 找到设备，检查 `driver` 链接和所属总线；没有链接时结合 probe 日志分析。
2. 设备存在但无驱动时，核对匹配表、设备树 `compatible`、模块加载和驱动 `probe()` 返回值。
3. 对延迟探测，查看内核日志及可用时的 `/sys/kernel/debug/devices_deferred`；继续追踪缺少的时钟、regulator、中断或其他供应者。
4. 解绑卡住时检查工作项、IRQ、DMA、引用和锁的退出顺序；不要仅凭设备节点消失判断后台活动已经停止。

## Linux 6.18 源码入口

| 入口 | 查看什么 |
| --- | --- |
| `drivers/base/core.c` 的 `device_add()` | 设备对象加入模型并进入总线探测路径 |
| `drivers/base/bus.c` 的 `bus_probe_device()` | 为已登记设备发起驱动匹配和探测 |
| `drivers/base/dd.c` 的 `really_probe()` | 供应者检查、资源准备与驱动 probe 的协调 |
| `drivers/base/dd.c` 的 `call_driver_probe()` | 进入总线或驱动提供的具体 probe 回调 |

异步探测、设备链接、总线实现和错误路径会增加分支；这些入口用于定位，而不是所有设备都会严格按同一条同步调用链运行。

## 参考

- 蜗窝科技：[设备与驱动](https://www.wowotech.net/device_model/device_and_driver.html)、[Bus](https://www.wowotech.net/device_model/bus.html)、[Class](https://www.wowotech.net/device_model/class.html)、[sysfs](https://www.wowotech.net/device_model/dm_sysfs.html)。
- Linux 6.18.62：[设备模型](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/overview.rst)、[设备资源管理](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/driver-model/devres.rst)。
- Linux 6.18.62：[`drivers/base/core.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/base/core.c)、[`drivers/base/dd.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/base/dd.c)。
