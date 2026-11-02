# Raspberry Pi 5 Sysbench 测量

> 单次测量得到 CPU `10598.98 events/s`、内存写测试 `3677.94 MiB/s`；尚未建立重复测量基线。

```bash
sysbench cpu --threads=$(nproc) run
sysbench memory run
```

工具版本为 sysbench 1.0.20、LuaJIT 2.1.1723681758。

| 测试 | 条件 | 结果 |
| --- | --- | --- |
| CPU | 4 线程，prime limit 10000，10.0003 s | 10598.98 events/s，106005 events |
| CPU 延迟 | 同上 | 平均 0.38 ms，最大 4.64 ms，P95 0.38 ms |
| 内存 | 1 线程，1 KiB block，write，global，10.0000 s | 3677.94 MiB/s，累计 36783.55 MiB |

尚缺 CPU 频率、系统镜像、温度、散热和后台负载。比较性能时应统一测试参数；内存结果不能直接当作 DDR 峰值带宽。
