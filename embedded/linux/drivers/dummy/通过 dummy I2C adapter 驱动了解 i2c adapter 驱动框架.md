# 通过虚拟 I²C Adapter 编写总线和设备驱动

> 用一个只存在于内存中的 I²C adapter 和寄存器设备，串起控制器注册、client 匹配、组合消息与资源释放。

## TL;DR

- 控制器通过 `i2c_adapter` 和 `i2c_algorithm.xfer` 接收消息；设备驱动通过 `i2c_client` 调用 `i2c_transfer()`。
- 本例在动态编号的虚拟总线上创建 `0x42` 设备，提供只读 ID 和可读写寄存器，不访问真实 I²C 硬件。
- 两条消息的组合读取必须返回 `2`；`i2c_master_send()` 返回的是字节数，不是消息数。
- 完整源码：[adapter 与虚拟设备](examples/i2c/dummy_i2c_adapter.c)、[client 驱动](examples/i2c/dummy_i2c_client.c)；构建文件为 `examples/i2c/Makefile`。

范围：Linux 6.18 LTS、7 位地址和同步传输。入口见 [[embedded/linux/drivers/dummy/README|虚拟驱动学习目录]]。

## 对象与数据路径

```text
用户读取或写入 sysfs value
  → dummy_i2c_client：i2c_transfer() / i2c_master_send()
  → I²C core：检查、总线锁、调用 adapter 算法
  → dummy_i2c_adapter：xfer() 解析消息并访问内存中的寄存器
```

| 对象 | 本例职责 | 真实驱动中对应的内容 |
| --- | --- | --- |
| `i2c_adapter` | 动态注册一条虚拟总线 | SoC 或外接控制器提供的总线实例 |
| `i2c_algorithm` | 处理两种约定好的消息格式 | 配置控制器，执行传输并返回结果 |
| `i2c_client` | 描述地址为 `0x42` 的虚拟设备 | 固件、板级信息等提供的外设实例 |
| `i2c_driver` | 匹配 client、检查 ID、提供 `value` | 面向芯片寄存器协议的外设驱动 |

控制器模块用 `i2c_add_adapter()` 分配总线号，再用 `i2c_new_client_device()` 创建 client。client 的名字 `kb_i2c_demo` 与设备驱动的 `i2c_device_id` 匹配，随后进入 `probe()`。这里是为了自包含实验而手动创建 client；实板上的设备通常由设备树等固件描述，见 [[如何实例化I2C设备]]。

## 虚拟设备协议

| 寄存器 | 属性 | 初始值 | 行为 |
| --- | --- | --- | --- |
| `0x00` | 只读 | `0xa5` | 设备 ID，client 的 `probe()` 校验它 |
| `0x01` | 读写 | `0x00` | 一个无符号字节，通过 `value` 属性访问 |

本例仅接受以下两种格式，格式不支持时返回 `-EOPNOTSUPP`，地址不为 `0x42` 时返回 `-ENXIO`：

```text
写寄存器： [W, 地址 0x42, 2 字节：reg, value]                  → 返回 1
读寄存器： [W, 地址 0x42, 1 字节：reg] [R, 地址 0x42, 1 字节] → 返回 2
```

第二种格式是一次调用内的两个 message，语义上对应写寄存器地址后 repeated START 再读取。它们不会在本例中产生电气信号。对 ID 写入返回 `-EACCES`，不存在的寄存器返回 `-EINVAL`；不支持多字节寄存器和其他协议格式。

设备驱动读取时要检查完成的消息数量：

```c
struct i2c_msg msgs[] = {
    { .addr = client->addr, .len = 1, .buf = &reg },
    { .addr = client->addr, .flags = I2C_M_RD, .len = 1, .buf = value },
};
int ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));

if (ret == ARRAY_SIZE(msgs))
    return 0;
return ret < 0 ? ret : -EIO;
```

示例声明 `I2C_FUNC_I2C` 供配套 client 识别原始 I²C 接口，但 `xfer()` 故意只实现上述协议，不能作为支持任意 I²C 事务的通用适配器。I²C core 序列化同一 adapter 上的传输，本例寄存器只在 `xfer()` 访问；如果将来有其他并发访问者，还需保护共享状态。普通同步 I²C 传输可能睡眠，不应在硬中断或持有不可睡眠锁时发起。

## 构建和验证

准备与**运行中的目标内核**完全匹配的构建树和 `Module.symvers`，并启用 `CONFIG_MODULES`、`CONFIG_I2C`。交叉编译时使用目标内核的 `ARCH`、`CROSS_COMPILE`，并将模块放到目标机；不要用不同内核的构建结果直接加载。

```sh
cd embedded/linux/drivers/dummy/examples/i2c
make KDIR=/path/to/matching/linux-6.18-build W=1
sudo insmod ./dummy_i2c_client.ko
sudo insmod ./dummy_i2c_adapter.ko
```

日志应包含虚拟总线号、地址 `0x42` 和 `virtual device ID 0xa5`。在自身的虚拟父设备下查找属性，避免操作真实总线：

```sh
value=$(find /sys/devices/kb-dummy-i2c-parent -type f -name value -print -quit)
printf 'value path: %s\n' "$value"
[ -n "$value" ] && printf '37\n' | sudo tee "$value"
[ -n "$value" ] && cat "$value"                 # 应为 37
[ -n "$value" ] && printf '0xff\n' | sudo tee "$value"
[ -n "$value" ] && cat "$value"                 # 应为 255
```

属性不存在、值不一致或写入失败都不是通过。写入 `256` 应失败，读取仍为 `255`。写属性使用 `tee` 是为了让打开 sysfs 文件的进程拥有所需权限；权限模型以目标系统为准。

```sh
sudo rmmod dummy_i2c_adapter
sudo rmmod dummy_i2c_client
```

先卸载 adapter 会先注销其 client、解除设备驱动绑定，再删除 adapter 和父设备。也可以先卸载 client 驱动；adapter 仍在，但不再有 `value` 属性。模块名、sysfs 路径和内核日志能区分“总线未注册”“client 未创建”“匹配 / probe 失败”。

## 走向真实硬件

1. 控制器驱动从 platform 资源取得 MMIO、时钟、复位、中断和可选 DMA；按控制器手册实现地址阶段、START / repeated START / STOP、ACK、超时及错误恢复。
2. 正确声明 `functionality`、消息限制与长度约束；区分设备 NACK、总线忙、超时和仲裁失败，并保证失败及卸载时没有仍在访问寄存器的中断或 DMA。
3. 外设驱动根据芯片手册处理地址、寄存器宽度、字节序、电源和复位；从设备树等获取设备资源，不要在控制器驱动中固定创建实际产品设备。
4. 组合读取和栈缓冲示例未设置 `I2C_M_DMA_SAFE`。DMA 安全条件由接口、内存和控制器共同决定，不能给栈地址随意加该标志。

`i2c-stub` 是内核中的 SMBus 内存模拟 adapter，不等于支持任意原始 I²C 事务；`i2c_new_dummy_device()` 创建的是占用地址的 dummy **client**，也不是创建 adapter。虚拟结果不能证明上拉、电平、时序、时钟拉伸或真实设备状态机正确；不要在未知的真实总线上随意扫描或写入。

相关：[[Linux I2C 与 SPI 驱动数据路径]]、[[Linux 设备模型与对象生命周期]]。

## 参考

- Linux 6.18.62：[`i2c.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/i2c.h)、[I²C client 驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/writing-clients.rst)、[DMA 注意事项](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/dma-considerations.rst)、[i2c-stub](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/i2c-stub.rst)。
- 蜗窝科技：[I²C 软件架构](https://www.wowotech.net/comm/i2c_overview.html)、[I²C provider](https://www.wowotech.net/comm/i2c_provider.html)。
