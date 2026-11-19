# Linux Dummy SPI 控制器与设备

> 用同步完成的虚拟 SPI controller 和字节回环设备，学习 controller、device、driver 与 transfer 的协作。

## TL;DR

- `spi_controller` 提供传输能力，`spi_device` 描述片选和协议参数，`spi_driver` 与设备匹配并发起传输。
- 本例只提供 mode 0、8 bit、片选 0；发送和接收缓冲同时存在时，接收字节等于发送字节。
- `transfer_one()` 返回 `0` 表示已同步完成；只有异步进行中才返回正值并在完成时通知 SPI core。
- 完整源码：[虚拟 controller](examples/spi/dummy_spi_controller.c)、[设备驱动](examples/spi/dummy_spi_client.c)；构建文件为 `examples/spi/Makefile`。

范围：Linux 6.18 LTS，host 侧 SPI 软件回环；它不模拟真实设备协议或电气波形。入口见 [[embedded/linux/drivers/dummy/README|虚拟驱动学习目录]]。

## 对象和调用链

```text
sysfs value 写入
  → dummy_spi_client：spi_sync_transfer()
  → SPI core：组织 spi_message / spi_transfer，调度控制器
  → dummy_spi_controller：transfer_one()
  → RX 缓冲区得到 TX 字节 → 同步返回 → 客户端校验
```

| 对象 | 本例配置 | 硬件驱动中应关注 |
| --- | --- | --- |
| `spi_controller` | 动态总线号、一个片选、8 bit | 资源、DMA、时钟、片选和控制器能力 |
| `spi_device` | `CS0`、mode 0、上限 1 MHz | 固件给出的片选、频率、mode 和时序 |
| `spi_driver` | 按 `kb_spi_demo` 匹配、probe 时自检 | 具体芯片的命令、寄存器和设备资源 |
| `spi_transfer` | 一字节全双工 | 发送 / 接收缓冲、长度、频率与消息边界 |

控制器模块依次调用 `spi_alloc_host()`、`spi_register_controller()`、`spi_new_device()`；`spi_new_device()` 创建的 modalias 与设备驱动的 `spi_device_id` 匹配。真实板级设备通常由设备树中的 SPI 外设子节点实例化，而不是由控制器驱动写死创建。

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

## 构建和验证

目标内核需要 `CONFIG_MODULES`、`CONFIG_SPI`，以及与运行内核匹配的构建产物和 `Module.symvers`：

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

- Linux 6.18.62：[SPI 框架和控制器](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/spi/spi-summary.rst)、[`spi.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/spi/spi.h)、[SPI core](https://github.com/gregkh/linux/blob/v6.18.62/drivers/spi/spi.c)。
