# 通过虚拟 I²C Adapter 理解控制器框架

> `i2c_adapter` 表示一条 I²C 总线，`i2c_algorithm` 提供传输能力；虚拟 adapter 可以验证部分软件路径，但不能证明电气时序或真实设备行为。

## TL;DR

- adapter 负责提供总线操作，client 描述总线上的目标设备，`i2c_driver` 负责匹配并驱动 client。
- `i2c_transfer()` 的成功返回值是完成的 **message 数量**，不是传输字节数；短于请求数也不是完整成功。
- `i2c-stub` 是内核提供的内存模拟 **SMBus** adapter；它不等同于可模拟任意原始 I²C 事务的控制器。
- `i2c_new_dummy_device()` 创建的是占用目标地址的 **dummy client**，不是创建虚拟 adapter 或模拟外设寄存器。

范围：Linux 6.18 LTS；此文解释框架和可控软件验证，不替代硬件测试。

## 对象与调用路径

```text
client driver → i2c_transfer() / i2c_smbus_*()
→ I²C core：能力检查、总线锁、适用的重试与分派
→ i2c_adapter.algo → 控制器传输或虚拟实现
```

| 对象 | 职责 |
| --- | --- |
| `struct i2c_adapter` | 总线实例、设备模型关系、锁和能力入口 |
| `struct i2c_algorithm` | `xfer`、`smbus_xfer`、`functionality` 等操作；不支持的能力不能虚报 |
| `struct i2c_client` | 某个 adapter 上的设备地址、IRQ 和设备对象 |
| `struct i2c_driver` | client 匹配、probe / remove 和设备逻辑 |

Linux 6.18 的 `i2c_algorithm` 同时提供 `xfer` 和 `master_xfer` 命名；控制器实现需遵循当前头文件的签名和执行上下文。普通 I²C 传输可能睡眠，不能在硬中断或持有不可睡眠锁时调用。

## 组合消息和返回值

常见寄存器读取可能由“写寄存器地址”和“读数据”两个 `i2c_msg` 组成。两个消息放入一次 `i2c_transfer()` 允许控制器按 I²C 规则处理组合事务；设备对 repeated START、地址宽度和寄存器格式的要求必须来自设备手册。

当请求 `num=2` 时，返回 `2` 才表示两个消息都已完成；负值为错误，非负但小于 `2` 也需要按失败或部分传输处理。`i2c_check_functionality()` 用于确认 adapter 声明支持相应操作，不能证明目标器件一定存在。

## 使用 i2c-stub 的边界

`i2c-stub` 在内存中模拟指定地址的 SMBus 寄存器，适合验证驱动部分 SMBus 读写和错误处理。基本步骤是确认 `CONFIG_I2C_STUB`、加载并指定测试地址、预置寄存器、创建受测 client，再观察读取与 probe 结果；卸载前先停止测试并移除测试 client。

该测试不能验证实际 START/STOP 波形、总线上拉、时钟拉伸、真实中断或器件内部状态机。驱动要求原始 I²C 组合消息而 stub 只提供 SMBus 能力时，应选择支持该协议的模拟环境或真实硬件，不能凭 stub 加载成功宣布驱动可用。

在真实总线上用 `i2cdetect`、`i2cget` 或 `i2cset` 前必须确认器件协议及地址；探测和写操作可能改变设备状态，也可能与内核驱动争用。

## 生命周期与排查

adapter 注册由 `i2c_add_adapter()` 等接口完成，移除用 `i2c_del_adapter()`；驱动必须保证传输、client 和底层资源在退出过程中的生命周期。动态编号、固件 alias 和 adapter 注册方式共同影响总线编号，不能硬编码 `/dev/i2c-N`。

| 现象 | 检查 |
| --- | --- |
| 没有 adapter | 控制器驱动、设备树、时钟/复位和注册结果 |
| 有 adapter 无 client | 设备节点、地址、compatible 与实例化方式 |
| probe 失败 | 传输能力、返回的消息数、供电、地址和寄存器协议 |
| stub 可用而实板失败 | 电气连接、上拉、时序、设备状态和控制器能力 |

设备创建方式见 [[如何实例化I2C设备]]；client 及 SPI 数据路径见 [[Linux I2C 与 SPI 驱动数据路径]]。

## 参考

- Linux 6.18.62：[i2c-stub](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/i2c-stub.rst)、[`i2c.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/i2c.h)、[`i2c-stub.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/i2c/i2c-stub.c)、[`i2c-core-base.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/i2c/i2c-core-base.c)。
- 蜗窝科技：[I²C 软件架构](https://www.wowotech.net/comm/i2c_overview.html)、[I²C provider](https://www.wowotech.net/comm/i2c_provider.html)。
