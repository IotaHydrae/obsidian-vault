# BeagleBone Black Sysbench 测量

> 单次测量得到 CPU `39.61 events/s`、内存写测试 `147.61 MiB/s`；结果只适用于对应测试条件。

```bash
sysbench cpu --threads=$(nproc) run
sysbench memory run
```

工具版本为 sysbench 1.0.20、LuaJIT 2.1.1700206165。

| 测试 | 条件 | 结果 |
| --- | --- | --- |
| CPU | 1 线程，prime limit 10000，10.0108 s | 39.61 events/s，397 events |
| CPU 延迟 | 同上 | 平均 25.20 ms，最大 28.90 ms，P95 25.74 ms |
| 内存 | 1 线程，1 KiB block，write，global，10.0003 s | 147.61 MiB/s，累计 1477.83 MiB |

尚缺 CPU 频率、系统镜像、温度和后台负载。内存测试包含缓存及软件路径影响，不能直接解释为 DDR 硬件峰值带宽。
