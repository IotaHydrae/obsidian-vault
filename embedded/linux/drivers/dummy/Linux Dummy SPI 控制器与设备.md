# Linux Dummy SPI 控制器与设备

> 用同步完成的虚拟 SPI controller 和字节回环设备，学习 controller、device、driver 与 transfer 的协作。

## TL;DR

- `spi_controller` 提供传输能力，`spi_device` 描述片选和协议参数，`spi_driver` 与设备匹配并发起传输。
- 本例只提供 mode 0、8 bit、片选 0；发送和接收缓冲同时存在时，接收字节等于发送字节。
- 自注册版本显式创建设备；设备树版本先匹配 platform 控制器，再由 SPI core 根据子节点创建 `spi_device`。
- `transfer_one()` 返回 `0` 表示已同步完成；只有异步进行中才返回正值并在完成时通知 SPI core。
- 源码：[自注册 controller](examples/spi/dummy_spi_controller.c)、[设备树 controller](examples/spi/dummy_spi_of.c)、[回环回调](examples/spi/dummy_spi_loopback.h)、[设备驱动](examples/spi/dummy_spi_client.c)。

范围：Linux 6.18 LTS，host 侧 SPI 软件回环；它不模拟真实设备协议或电气波形。入口见 [[embedded/linux/drivers/dummy/README|虚拟驱动学习目录]]。

## 对象和调用链

```text
写入：value_store → dummy_spi_loopback → spi_sync_transfer
     → spi_sync 提交 message；core 可直走或排队
     → spi_transfer_one_message 遍历 transfer
     → transfer_one 将 TX 复制到 RX → 返回 0
     → spi_finalize_current_message → 客户端比较 TX / RX
读取：value_show → 返回最近一次接收值；本例读取属性本身不发起传输
```

| 对象 | 本例配置 | 硬件驱动中应关注 |
| --- | --- | --- |
| `spi_controller` | 动态总线号、一个片选、8 bit | 资源、DMA、时钟、片选和控制器能力 |
| `spi_device` | `CS0`、mode 0、上限 1 MHz | 固件给出的片选、频率、mode 和时序 |
| `spi_driver` | 按 `kb_spi_demo` 匹配、probe 时自检 | 具体芯片的命令、寄存器和设备资源 |
| `spi_transfer` | 一字节全双工 | 发送 / 接收缓冲、长度、频率与消息边界 |

自注册版本依次调用 `spi_alloc_host()`、`spi_register_controller()`、`spi_new_device()`；设备的 modalias 与设备驱动的 `spi_device_id` 匹配。设备树版本则使用：

```text
控制器 DT 节点 → platform_device → platform_driver.probe
→ controller.dev.of_node = pdev->dev.of_node → devm_spi_register_controller
→ SPI core 枚举可用子节点 → spi_device → of_match_table → spi_driver.probe
```

注册 controller 时就可能枚举设备并执行外设 `probe()`，因此 `transfer_one`、位宽和片选能力要在注册之前初始化。真实板级设备通常由设备树中的 SPI 外设子节点实例化，而不是由控制器驱动写死创建。

## 最小接口及作用

“必需”指本例选择的**普通 SPI 同步传输**；SPI core 还支持其他控制器实现方式，不应把本表当成所有控制器唯一的回调组合。

| 接口 / 字段 | 必需性 | 作用 |
| --- | --- | --- |
| `spi_alloc_host()` | 自注册版本 | 分配 host controller；注册失败前用 `spi_controller_put()` 释放 |
| `devm_spi_alloc_host()` | 设备树版本 | 按 platform 设备生命周期分配 controller；解绑时由 devres 释放 |
| `num_chipselect` | 本例必需 | 声明支持的片选数；本例为 1，因此可使用 CS0 |
| `bus_num` | 本例使用 | `-1` 表示由 core 分配编号；固件 alias 也可能影响编号 |
| `mode_bits` / `bits_per_word_mask` | 本例使用 | 声明支持的 mode 标志和字长；本例支持 mode 0、8 bit |
| `transfer_one()` | 本例必需 | 执行一条 `spi_transfer`；同步完成返回 `0`，异步时遵循完成通知契约 |
| `spi_finalize_current_transfer()` | 异步传输需要 | 通知 core 当前异步 transfer 已结束；本例同步返回，不调用此函数 |
| `transfer_one_message()` | 替代方案 | 驱动自行处理整条 message；与 `transfer_one()` 的通用 core 调度路径不能混用 |
| `set_cs()` | 取决于硬件 | 驱动片选电平；虚拟回环没有物理片选，故未实现 |
| `root_device_register()` / `root_device_unregister()` | 自注册版本 | 创建 / 释放虚拟父设备；设备树版本使用 platform 设备作为父设备 |
| `spi_register_controller()` / `spi_unregister_controller()` | 自注册版本 | 注册 / 注销 controller；成功注销也释放其 controller 引用 |
| `devm_spi_register_controller()` | 设备树版本 | 将 controller 注册绑定到 platform 设备，解绑时自动撤销注册 |
| `spi_new_device()` / `spi_unregister_device()` | 自注册版本 | 创建 / 删除片选上的设备；DT 子节点模式不手动重复创建 |
| `platform_driver.probe` / `.of_match_table` | 设备树版本 | 匹配控制器节点后初始化 controller；真实硬件通常在这里获取资源 |
| `module_platform_driver()` | 设备树版本 | 注册 / 注销 platform driver；本例不需要额外模块初始化步骤 |
| `MODULE_DEVICE_TABLE(of, ...)` | 用于模块自动加载 | 导出设备树匹配别名；实际设备匹配仍由 `.of_match_table` 完成 |
| `spi_driver.probe` | 本例使用 | 匹配后分配收发缓冲并用 `0x5a` 自检；SPI core 本身允许不提供该回调 |
| `spi_device_id` / `of_match_table` | 按枚举方式选择 | 分别匹配自注册设备名和 DT `compatible`；本例同时提供两种表 |
| `module_spi_driver()` | 本例使用 | 注册 / 注销 `spi_driver` |
| `spi_sync_transfer()` | 设备驱动使用 | 提交一个或多个 transfer 并等待结果；成功返回 `0` |
| `value_show()` / `value_store()` | 可选 | 提供测试属性，不是 SPI 控制器或外设驱动的强制回调 |
| `spi_driver.remove` | 按资源需要 | 解绑时停止异步工作和释放非托管资源；本例仅有 devm 分配的缓冲，未实现 |

## DTS 节点与匹配

以下是 `examples/spi/dummy-spi.dtso` 的节点内容；它描述实验用虚拟设备，`example,...` 不是正式产品 binding：

```dts
spi-dummy {
    compatible = "example,kb-dummy-spi";
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";

    device@0 {
        compatible = "example,kb-spi-demo";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

| 属性 | 作用 |
| --- | --- |
| 控制器 `compatible` | 匹配 `dummy_spi_of.c` 中的 `platform_driver` |
| `#address-cells` / `#size-cells` | 定义子设备 `reg` 为一个片选编号 cell，不含长度 |
| 子设备 `compatible` | 匹配 `dummy_spi_client.c` 的 `of_match_table` |
| 子设备 `reg = <0>` | 选择 CS0，不是寄存器物理地址；必须小于本例的 `num_chipselect` |
| `spi-max-frequency` | 设备允许的最高 SPI 时钟；本例只保存参数，不产生时钟 |

子节点没有 `spi-cpol` 和 `spi-cpha`，对应 mode 0，与虚拟 controller 能力一致。`spi-tx-bus-width` / `spi-rx-bus-width` 描述数据线数量，不是每字的位数；本例的 8 bit 由 controller 的 `bits_per_word_mask` 限定。这里没有真实 GPIO 片选、MMIO、时钟或中断；实板属性需遵循具体 controller 和外设 binding。

`dummy_spi_of_probe()` 在注册之前关联控制器节点并填写能力；完整 platform 匹配和资源分配见对应源码：

```c
controller->dev.of_node = pdev->dev.of_node;
controller->bus_num = -1;
controller->num_chipselect = 1;
controller->mode_bits = 0;
controller->bits_per_word_mask = SPI_BPW_MASK(8);
controller->transfer_one = dummy_spi_transfer_one;
return devm_spi_register_controller(&pdev->dev, controller);
```

控制器的 `of_match_table` 负责匹配父节点；`controller->dev.of_node` 让 SPI core 枚举子节点；设备驱动自己的 `of_match_table` 再匹配 `example,kb-spi-demo`。三者分别作用于不同阶段。

从仓库根目录生成**新的测试 DTB**，用它启动虚拟机或测试板，并确认目标内核启用 `CONFIG_OF`：

```sh
dtc -@ -I dts -O dtb -o /tmp/dummy-spi.dtbo \
  embedded/linux/drivers/dummy/examples/spi/dummy-spi.dtso
fdtoverlay -i /path/to/base.dtb -o /tmp/dummy-spi-test.dtb /tmp/dummy-spi.dtbo
```

在目标机执行：

```sh
cd embedded/linux/drivers/dummy/examples/spi
make KDIR=/path/to/matching/linux-6.18-build W=1
sudo insmod ./dummy_spi_client.ko
sudo insmod ./dummy_spi_of.ko
ls -l /sys/bus/platform/drivers/kb_dummy_spi_of/
value=$(find /sys/devices/platform/spi-dummy -type f -name value -print -quit)
[ -n "$value" ] && cat "$value"                      # probe 回环结果：90
[ -n "$value" ] && printf '37\n' | sudo tee "$value"
[ -n "$value" ] && cat "$value"                      # 应为 37
sudo rmmod dummy_spi_of
sudo rmmod dummy_spi_client
```

驱动目录应出现 `spi-dummy` 设备链接；属性缺失或读回不符均不是通过。`dummy_spi_controller.ko` 是独立的自注册版本，加载它会另外创建一条虚拟总线，不能证明 DT 路径正确。

## 实现和完成语义

控制器中的核心回调可简化为：

```c
for (i = 0; i < transfer->len; i++) {
    if (rx)
        rx[i] = tx ? tx[i] : 0xff;
}
return 0;
```

| 缓冲区 | 本例行为 |
| --- | --- |
| 同时有 TX / RX | 逐字节回环 |
| 只有 TX | 消费数据，不保存内容 |
| 只有 RX | 填充 `0xff` |

设备驱动在 `probe()` 中发送 `0x5a`，确认接收值一致；`value` 写入一个 `0..255` 的数后发起新的同步 transfer，读取该属性得到最近一次接收值。SPI 同步接口成功返回 `0`，而不是传输字节数。

`transfer_one()` 同步完成返回 `0`，失败返回负错误码；需要等待 IRQ / DMA 等异步完成时返回 `1`，并在完成时调用 `spi_finalize_current_transfer()`，异步失败需先设置 `SPI_TRANS_FAIL_IO`。这和自行实现 `transfer_one_message()`、使用 `spi_finalize_current_message()` 是不同的回调契约，不应混用。本例没有异步操作，不能额外调用完成接口。

`spi_message` 可以包含多个 transfer，其片选保持、延时和设备协议由 API 配置与控制器能力决定。本例没有真正的 `set_cs`、时钟或片选信号；名义的 1 MHz 只是设备参数。驱动使用独立的动态 TX / RX 缓冲并在同步调用返回前保持有效；迁移到 DMA 控制器时还需遵循目标平台和 SPI API 的 DMA 缓冲要求。`spi_sync_transfer()` 可能睡眠，不应在硬中断或持有不可睡眠锁时调用。

### 一个 message 内先写后读

在已匹配的虚拟 `spi_device` 上，比较两种设计：单条全双工 transfer 的 RX 等于 TX；一条 TX-only 接一条 RX-only 的 RX 为 `ff ff`，因为本例不保存上一条 transfer 的内容。下面的片段以第二种设计为检查项；调用者从可睡眠上下文传入两个独立、至少两字节的 DMA 兼容缓冲区，并已填好 TX：

```c
static int dummy_spi_check_split(struct spi_device *spi, const u8 *tx, u8 *rx)
{
    struct spi_transfer xfers[] = {
        { .tx_buf = tx, .len = 2 },
        { .rx_buf = rx, .len = 2 },
    };
    struct spi_message msg;
    int ret;

    spi_message_init_with_transfers(&msg, xfers, ARRAY_SIZE(xfers));
    ret = spi_sync(spi, &msg);
    if (ret)
        return ret;
    if (msg.actual_length != 4 || rx[0] != 0xff || rx[1] != 0xff)
        return -EIO;
    return 0;
}
```

默认 `cs_change = 0` 的两条 transfer 在普通硬件 host 上通常保持同一次片选，分成两次 `spi_sync()` 则不能要求片选保持或阻止其他消息插入。`actual_length` 是各 transfer 完成长度之和；一条 2 字节全双工 transfer 仍只记 2 字节，不把 TX 和 RX 相加。虚拟测试不验证片选波形。

## 构建和验证

以下验证自注册版本。目标内核需要 `CONFIG_MODULES`、`CONFIG_SPI`，以及与运行内核匹配的构建产物和 `Module.symvers`：

```sh
cd embedded/linux/drivers/dummy/examples/spi
make KDIR=/path/to/matching/linux-6.18-build W=1
sudo insmod ./dummy_spi_client.ko
sudo insmod ./dummy_spi_controller.ko
```

日志应出现 `virtual loopback returned 0x5a` 和动态总线号。仅在虚拟父设备下寻找属性：

```sh
value=$(find /sys/devices/kb-dummy-spi-parent -type f -name value -print -quit)
printf 'value path: %s\n' "$value"
[ -n "$value" ] && cat "$value"                   # probe 结果：90
[ -n "$value" ] && printf '37\n' | sudo tee "$value"
[ -n "$value" ] && cat "$value"                   # 应为 37
[ -n "$value" ] && printf '0xff\n' | sudo tee "$value"
[ -n "$value" ] && cat "$value"                   # 应为 255
```

属性缺失、写入失败或数值不一致均为未通过；写入 `256` 应失败并保留上一次结果。

```sh
sudo rmmod dummy_spi_controller
sudo rmmod dummy_spi_client
```

控制器退出先注销虚拟 `spi_device`，再注销 controller 和父设备；注册失败时，尚未注册的 controller 使用 `spi_controller_put()` 释放。注册成功后 `spi_unregister_controller()` 会撤销注册并释放 controller 引用，不应再重复 put。

## 迁移到硬件

1. 在 controller 驱动的 `probe()` 获取寄存器、时钟、复位、中断和可选 DMA，按照硬件限制填写 mode、位宽、最大频率、片选和 DMA 能力。
2. 在 `transfer_one()` 配置传输、填充或读取 FIFO；同步或异步完成必须严格遵守返回值与完成通知契约。超时、错误、中断竞争和移除时的在途传输都需要处理。
3. 在设备驱动中根据芯片手册组织命令、地址、dummy cycle 和读写数据，确认片选是否跨 transfer 保持，并检查频率、CPOL / CPHA 和字节序。
4. 用可验证的设备响应和示波器 / 逻辑分析仪检查时序。内存回环只覆盖软件路径，不能验证物理片选、DMA 缓存一致性或硬件故障恢复。

相关：[[Linux I2C 与 SPI 驱动数据路径]]、[[Linux DMAEngine 数据传输]]、[[Linux 驱动异步退出与资源生命周期]]。

## 参考

- Linux 6.18.62：[SPI 框架和控制器](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/spi/spi-summary.rst)、[SPI controller binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/spi/spi-controller.yaml)、[`spi.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/spi/spi.h)、[SPI core](https://github.com/gregkh/linux/blob/v6.18.62/drivers/spi/spi.c)。
