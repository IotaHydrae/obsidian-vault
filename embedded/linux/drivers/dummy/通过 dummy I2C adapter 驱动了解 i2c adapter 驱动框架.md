# 通过虚拟 I²C Adapter 编写总线和设备驱动

> 用一个只存在于内存中的 I²C adapter 和寄存器设备，串起控制器注册、client 匹配、组合消息与资源释放。

## TL;DR

- 控制器通过 `i2c_adapter` 和 `i2c_algorithm.xfer` 接收消息；设备驱动通过 `i2c_client` 调用 `i2c_transfer()`。
- 本例在动态编号的虚拟总线上创建 `0x42` 设备，提供只读 ID 和可读写寄存器，不访问真实 I²C 硬件。
- 自注册版本直接创建 client；设备树版本由 platform 驱动注册 adapter，再由 I²C core 根据子节点创建设备。
- 两条消息的组合读取必须返回 `2`；`i2c_master_send()` 返回的是字节数，不是消息数。
- 源码：[自注册 adapter](examples/i2c/dummy_i2c_adapter.c)、[设备树 adapter](examples/i2c/dummy_i2c_of.c)、[传输模拟](examples/i2c/dummy_i2c_emulator.h)、[client 驱动](examples/i2c/dummy_i2c_client.c)。

范围：Linux 6.18 LTS、7 位地址和同步传输。入口见 [[embedded/linux/drivers/dummy/README|虚拟驱动学习目录]]。

## 对象与数据路径

```text
读取：value_show → dummy_i2c_read_reg → i2c_transfer(2 个 msg)
     → I²C core 加锁 / 检查 → algorithm.xfer → 读取虚拟寄存器
     → 消息数 2 → 格式化为 sysfs 文本 → 用户

写入：value_store → kstrtou8 → i2c_master_send(reg + value)
     → i2c_transfer(1 个 msg) → algorithm.xfer → 更新虚拟寄存器
     → 消息数 1 转换为字节数 2 → sysfs 写入完成
```

| 对象 | 本例职责 | 真实驱动中对应的内容 |
| --- | --- | --- |
| `i2c_adapter` | 动态注册一条虚拟总线 | SoC 或外接控制器提供的总线实例 |
| `i2c_algorithm` | 处理两种约定好的消息格式 | 配置控制器，执行传输并返回结果 |
| `i2c_client` | 描述地址为 `0x42` 的虚拟设备 | 固件、板级信息等提供的外设实例 |
| `i2c_driver` | 匹配 client、检查 ID、提供 `value` | 面向芯片寄存器协议的外设驱动 |

自注册版本用 `i2c_add_adapter()` 注册总线，再用 `i2c_new_client_device()` 创建 client；名字 `kb_i2c_demo` 与 `i2c_device_id` 匹配。设备树版本的创建和匹配路径是：

```text
控制器 DT 节点 → platform_device → platform_driver.probe
→ adapter.dev.of_node = pdev->dev.of_node → devm_i2c_add_adapter
→ I²C core 枚举可用子节点 → i2c_client → of_match_table → client.probe
```

注册期间就可能同步调用 client 的 `probe()`；因此算法、状态和节点指针必须在注册 adapter **之前**准备好。client 的其他实例化方式见 [[如何实例化I2C设备]]。

## 最小接口及作用

这里的“必需”指本实验所选的**原始 I²C 同步传输路径**，不是所有 I²C 控制器的唯一实现方式。例如只提供 SMBus 能力的 adapter 可有不同算法回调。

| 接口 / 字段 | 必需性 | 作用 |
| --- | --- | --- |
| `i2c_adapter.name`、`.owner`、`.algo` | 本例必需 | 命名总线、标识模块所有者、连接控制器算法；设备树版本还设置 `.dev.parent` 和 `.dev.of_node` |
| `i2c_algorithm.xfer` | 本例必需 | 接收 `i2c_msg` 数组，执行普通 I²C 消息；成功返回完成的消息数，失败返回负错误码 |
| `i2c_algorithm.functionality` | 本例必需 | 向上层报告 `I2C_FUNC_*` 能力，供 `i2c_check_functionality()` 检查 |
| `i2c_set_adapdata()` / `i2c_get_adapdata()` | 本例使用 | 将每条虚拟总线的寄存器状态与 adapter 关联，传输回调由此取得状态；不要求所有驱动都使用 |
| `root_device_register()` / `root_device_unregister()` | 自注册版本 | 创建 / 释放没有真实硬件的父设备；设备树版本使用 platform 设备作为父设备 |
| `i2c_add_adapter()` / `i2c_del_adapter()` | 自注册版本 | 注册 / 注销 adapter；编号由 core 分配或结合固件 alias 决定 |
| `devm_i2c_add_adapter()` | 设备树版本 | 注册 adapter 并绑定 platform 设备生命周期；解绑时自动注销，不再手动 `i2c_del_adapter()` |
| `i2c_new_client_device()` / `i2c_unregister_device()` | 自注册版本 | 显式创建 / 删除虚拟 client；DT 子节点由 core 创建时不再调用，避免重复占用地址 |
| `platform_driver.probe` / `.of_match_table` | 设备树版本 | 匹配控制器节点后准备 adapter；真实硬件通常在这里获取资源 |
| `module_platform_driver()` | 设备树版本 | 注册 / 注销 platform driver；适用于本例无需额外模块初始化步骤的情况 |
| `MODULE_DEVICE_TABLE(of, ...)` | 用于模块自动加载 | 导出设备树匹配别名；设备匹配仍由驱动的 `.of_match_table` 完成 |
| `i2c_driver.probe` | 本例必需 | 在设备匹配后检查能力和设备 ID，成功后设备才完成绑定 |
| `i2c_device_id` / `of_match_table` | 按枚举方式选择 | 分别匹配自注册 client 名称与 DT `compatible`；本例同时提供两种表 |
| `module_i2c_driver()` | 本例使用 | 注册 / 注销 `i2c_driver`；展开后等价于配对的驱动注册和注销 |
| `i2c_transfer()` / `i2c_master_send()` | client 使用 | 前者提交一个或多个 message，后者提交单条写消息并以字节数返回结果 |
| `value_show()` / `value_store()` | 可选 | 通过 `dev_groups` 暴露测试属性，不是 I²C 框架的强制接口 |
| `i2c_driver.remove` | 按资源需要 | 解绑时清理非托管资源、停止异步工作；本例 client 没有此类资源，因此未实现 |

## DTS 节点与匹配

以下是 `examples/i2c/dummy-i2c.dtso` 的节点内容；它是**实验用虚拟设备**，`example,...` 不是可直接用于产品的正式硬件 binding：

```dts
i2c-dummy {
    compatible = "example,kb-dummy-i2c";
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";

    device@42 {
        compatible = "example,kb-i2c-demo";
        reg = <0x42>;
    };
};
```

| 属性 | 作用 |
| --- | --- |
| 控制器 `compatible` | 匹配 `dummy_i2c_of.c` 的 `platform_driver` |
| `#address-cells` / `#size-cells` | 定义子设备 `reg` 为一个地址 cell、不含长度 cell |
| 子设备 `compatible` | 匹配 `dummy_i2c_client.c` 的 `of_match_table` |
| 子设备 `reg = <0x42>` | 7 位 I²C 地址，不是 MMIO 地址；必须与本例模拟的 `0x42` 一致 |
| `status` | `okay` 表示节点可用；没有此属性时通常也视为可用 |

该虚拟控制器没有 MMIO、时钟和中断，故不编造控制器 `reg`、`clocks` 或 `interrupts`。在真实板上通常启用 SoC DTSI 已定义的 I²C 控制器，并按实际器件 binding 配置其子节点。

`dummy_i2c_of_probe()` 的关键代码如下；完整的 platform 匹配表和内存分配见对应源码：

```c
adapter->owner = THIS_MODULE;
adapter->algo = &dummy_i2c_algorithm;
adapter->dev.parent = &pdev->dev;
adapter->dev.of_node = pdev->dev.of_node;
strscpy(adapter->name, "kb-dummy-i2c-of", sizeof(adapter->name));
i2c_set_adapdata(adapter, &bus->state);
return devm_i2c_add_adapter(&pdev->dev, adapter);
```

`adapter->dev.of_node` 让 I²C core 知道要从哪个控制器节点寻找子设备；只写 platform 驱动的 `of_match_table` 而不设置它，不能完成这里的子节点枚举。client 驱动还要有与 `example,kb-i2c-demo` 对应的 `of_match_table`。

下面从仓库根目录生成**新的测试 DTB**；`fdtoverlay` 在离线文件中合并，不会自动更改当前运行系统：

```sh
dtc -@ -I dts -O dtb -o /tmp/dummy-i2c.dtbo \
  embedded/linux/drivers/dummy/examples/i2c/dummy-i2c.dtso
fdtoverlay -i /path/to/base.dtb -o /tmp/dummy-i2c-test.dtb /tmp/dummy-i2c.dtbo
```

让虚拟机或测试板用新的 DTB 启动，确认目标内核启用 `CONFIG_OF`，然后在目标机执行：

```sh
cd embedded/linux/drivers/dummy/examples/i2c
make KDIR=/path/to/matching/linux-6.18-build W=1
sudo insmod ./dummy_i2c_client.ko
sudo insmod ./dummy_i2c_of.ko
ls -l /sys/bus/platform/drivers/kb_dummy_i2c_of/
value=$(find /sys/devices/platform/i2c-dummy -type f -name value -print -quit)
[ -n "$value" ] && printf '37\n' | sudo tee "$value"
[ -n "$value" ] && cat "$value"                      # 应为 37
sudo rmmod dummy_i2c_of
sudo rmmod dummy_i2c_client
```

驱动目录应出现 `i2c-dummy` 设备链接；找不到属性或读回不是 `37` 都不是通过。**不要同时把自注册模块当成同一条 DT 总线**：它会另建一条独立的虚拟总线，不能验证 DT 路径。

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

以下验证自注册版本。准备与**运行中的目标内核**完全匹配的构建树和 `Module.symvers`，并启用 `CONFIG_MODULES`、`CONFIG_I2C`。交叉编译时使用目标内核的 `ARCH`、`CROSS_COMPILE`，并将模块放到目标机；不要用不同内核的构建结果直接加载。

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

- Linux 6.18.62：[`i2c.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/i2c.h)、[I²C core 的 DT 枚举](https://github.com/gregkh/linux/blob/v6.18.62/drivers/i2c/i2c-core-of.c)、[I²C client 驱动](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/writing-clients.rst)、[DMA 注意事项](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/dma-considerations.rst)、[i2c-stub](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/i2c/i2c-stub.rst)。
- 蜗窝科技：[I²C 软件架构](https://www.wowotech.net/comm/i2c_overview.html)、[I²C provider](https://www.wowotech.net/comm/i2c_provider.html)。
