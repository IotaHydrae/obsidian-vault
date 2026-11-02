# Android 内存少、zram 满：怎样判断是否真的有压力

> `MemAvailable`、换出量、zram 物理开销和任务停顿共同决定判断；“空闲很少”或“swap 满了”都不是单独的故障判据。

## TL;DR

- 先连续观察可用内存、PSI、应用重载和 LMKD 事件，再谈扩容或限制后台。
- zram 的逻辑数据量与压缩后的实际 RAM 开销是两种口径。
- 缓存冻结减少 CPU 活动，进程及其内存仍会保留。

## 读取同一时间窗口

在已授权 ADB 的设备上读取，部分节点可能因权限或内核配置不可用：

```sh
adb shell cat /proc/meminfo
adb shell cat /proc/swaps
adb shell cat /proc/pressure/memory
adb shell cat /sys/block/zram0/mm_stat
adb shell dumpsys meminfo
```

记录时间、正在运行的任务及充电状态，比较故障前、中、后。没有 PSI 节点代表缺少该观测能力；PSI 一时为零也不能排除几分钟前的卡顿。

| 指标 | 含义与限制 |
| --- | --- |
| `MemFree` | 完全空闲页；不包含可回收缓存 |
| `MemAvailable` | 内核对无需换出即可供新任务使用的内存估计；不是保证值 |
| swap 已用 | 换出页的逻辑规模，不等于 zram 实际占用同样多的 RAM |
| zram `orig_data_size` | 设备内已存数据的未压缩字节数 |
| zram `compr_data_size` | 压缩数据字节数 |
| zram `mem_used_total` | 分配器实际分配量，包含碎片和元数据开销 |
| memory PSI `some`／`full` | 至少部分任务／所有非空闲任务同时因内存资源停顿的时间比例 |

`mm_stat` 为位置字段；按设备内核对应的 zram 文档解释，不把新内核新增字段的位置照搬到旧版本。

## 内存账目常见误读

- 标称 6 GB、内核可管理 GiB、进程统计属于不同口径；单位换算和硬件保留都影响差额。
- PSS 按比例分摊驻留共享页；`SwapPss` 单独统计按共享比例分摊后的换出量。`dumpsys` 不同列可能分别显示或汇总它们，先读表头再比较，不能把含换出的总数当成纯驻留 RAM。
- 不把进程 RSS、PSS、文件缓存和内核总量直接相加；共享、映射、换出和统计时间不同会造成重叠或遗漏。
- 界面没打开不等于没有后台服务；输入法、播放器、推送和 VPN 都可能保留进程。
- 缓存冻结暂停执行，主要减少 CPU 活动；不等于清空内存。

例如，约 1.06 GiB 的 zram 逻辑空间接近使用完，但 `mem_used_total` 约 0.39 GiB、系统仍有约 1.4 GiB 可用内存，可以同时成立。冷页不会因为 RAM 暂时宽裕就立即全部换回。

## 优化以业务为准

先检查持续运行而不再需要的服务，区分“结束本次播放”与“禁用应用”。结束服务后立即释放的内存只是瞬时效果；重新使用应用会重新分配，还应验收消息、输入法、VPN 和后台播放。

只有反复观察到停顿、频繁冷启动／回收或分配失败，才有依据评估缓存策略、zram 和工作负载。不存在适用于所有手机的剩余内存或 PSI 阈值。

`swappiness=0` 不等于绝不换出；在运行中 `swapoff` 会要求搬回换出页，可能增加压力。扩容脚本先按 [启动模块审查](android-tuning-module-audit.md) 核查，不凭 zram 使用率单独触发重建。

## 参考

- [Linux：zram 字段与配置](https://docs.kernel.org/admin-guide/blockdev/zram.html)
- [Linux：procfs PSS 与 SwapPss](https://docs.kernel.org/filesystems/proc.html)
- [Linux：PSI](https://docs.kernel.org/accounting/psi.html)
- [Android：LMKD](https://source.android.com/docs/core/perf/lmkd)
- [Android：缓存应用冻结](https://source.android.com/docs/core/perf/cached-apps-freezer)
- [Android：dumpsys 内存统计](https://developer.android.com/tools/dumpsys#meminfo)
