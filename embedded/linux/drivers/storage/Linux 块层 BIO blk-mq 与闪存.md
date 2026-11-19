# Linux 块层 BIO、blk-mq 与闪存

> 块 I/O 的 `bio` 描述数据传输，blk-mq 将请求调度到设备队列；eMMC 和裸 NAND/NOR 使用的上层管理方式不同。

## TL;DR

- 文件系统、bio、request、硬件队列和完成回调属于不同层次；一个用户请求不保证对应一个硬件请求。
- blk-mq 通过软件提交上下文和硬件队列协调并发；是否启用 I/O scheduler 及队列数量由设备和配置决定。
- eMMC / SD 通常呈现块设备；裸 NAND/NOR 常经 MTD，NAND 还涉及坏块、ECC 和擦写管理。
- flush、FUA、discard 及写缓存语义应结合设备与文件系统核对；discard / 格式化会改变数据。

范围：Linux 6.18 LTS 的典型块设备和 MTD 路径；具体硬件、文件系统与总线可能有不同分支。

## 从文件 I/O 到设备

```text
文件系统 / 其他块 I/O 用户
→ bio（方向、扇区、内存段、完成状态）
→ 块层处理与可能的合并 / 拆分
→ blk-mq request / 硬件队列 → 块设备驱动 → 设备完成
```

实际路径可能包含 dm、分区、虚拟设备、I/O scheduler 或不同文件系统实现。bio 和 request 的数量可能变化；完成和错误必须传回对应用户，不能以提交成功当作介质写入成功。

| 对象 | 作用 |
| --- | --- |
| `bio` | 表达块设备 I/O 的位置、方向、内存片段及完成信息 |
| request | 块层和驱动之间调度 / 执行的请求对象 |
| 软件上下文与硬件上下文 | blk-mq 将 CPU 侧提交与设备硬件队列关联 |
| 完成路径 | 驱动报告成功或错误，块层完成请求并通知上层 |

## 看 trace 时区分请求阶段

| 事件 | 可观察的阶段 |
| --- | --- |
| `block_rq_insert` | 请求进入队列；直发路径可能没有这一事件 |
| `block_rq_issue` | 请求从块层交给设备驱动处理；不等于已经写到介质 |
| `block_rq_complete` | 驱动报告一部分或全部请求已完成；核对完成长度和错误码 |
| `block_rq_requeue` | 请求重新排队；不要把随后的重发当成独立用户 I/O |

`insert → issue` 只覆盖被观察到的块层排队，`issue → complete` 还包括驱动和设备服务时间。对同一设备有并发、合并、拆分或重发时，单凭相同 sector 不能把两条事件当成同一个请求；缺少可靠关联时，不应报告精确的单请求耗时。追踪基础见 [[ftrace]]。

### 写缓存的两个边界

- `REQ_PREFLUSH`：要求之前已完成的写先到非易失介质，再启动本次 I/O；它不保证本次带数据写完成时已经持久化。
- `REQ_FUA`：要求本次写的数据到非易失介质后才报告完成；它不代替此前写入的 preflush。blk-mq 对声明有易失写缓存、但不支持原生 FUA 的驱动可用后置 flush 实现。

设备和驱动需要如实报告写缓存及 FUA 能力。读到一次块请求完成，不能外推出整个应用事务已持久化。

## eMMC、MTD 与文件系统

| 介质 / 层次 | 常见路径 | 关键边界 |
| --- | --- | --- |
| eMMC / SD | MMC host → block device → 分区 / 块文件系统 | 设备内部存在介质管理；总线时序和供电仍重要 |
| SPI NOR / 裸 NAND | MTD 控制器 / 芯片驱动 → MTD 设备 | 擦除粒度、坏块 / ECC 能力取决于介质和驱动 |
| UBI / UBIFS | 适用的裸闪存经 MTD → UBI → UBIFS | UBI 处理适用的卷与擦写块管理，不等同于普通分区表 |
| ext4 / F2FS | 针对块设备的文件系统 | 一致性、写回和持久化依文件系统、块层与硬件共同决定 |

不能因为底层都是 Flash，就把 eMMC 当作裸 NAND 使用，也不能假定在任意 MTD 上放通用块文件系统是安全可靠的。

## 诊断

1. `lsblk -f`、`findmnt` 和 `/proc/mtd` 分别观察块设备、挂载和 MTD 拓扑；设备名随发现顺序可能变化。
2. 从 `dmesg` 的 controller、MMC/MTD、block 和 filesystem 日志定位第一个错误，区分超时、介质/ECC 和元数据损坏。
3. 用适合系统的块 I/O tracepoint、统计或基准工具比较请求排队和完成时间；测量会受 cache、合并及负载影响。
4. 测试写入、擦除、discard 或修复前，确认目标设备和备份，避免误操作生产数据。

缓存与 `fsync()` 见 [[Linux VFS Page Cache 与写回]]；电源和控制器恢复见 [[Linux 系统休眠与中断唤醒]]。

## 参考

- Linux 6.18.62：[blk-mq](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/block/blk-mq.rst)、[写缓存控制](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/block/writeback_cache_control.rst)、[MTD](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/mtd/index.rst)、[UBIFS](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/filesystems/ubifs.rst)。
- [块层 tracepoint 定义](https://github.com/gregkh/linux/blob/v6.18.62/include/trace/events/block.h)。
- 蜗窝科技：[eMMC 概述](https://www.wowotech.net/basic_tech/emmc_intro.html)、[MMC 架构](https://www.wowotech.net/comm/mmc_framework_arch.html)。
