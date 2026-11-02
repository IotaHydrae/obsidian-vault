# Raspberry Pi CM4 Sysbench 测量

> 两次 CPU 测量分别在 1.5 GHz 和 2.0 GHz 下得到 `5855.53` 与 `7803.38 events/s`；这些结果不证明长期稳定性。

```bash
sysbench cpu --threads=$(nproc) run
sysbench memory run
```

工具版本为 sysbench 1.0.20、LuaJIT 2.1.1723681758；CPU 使用 4 线程、prime limit 10000。

| CPU 频率 | 运行时间 | events/s | 平均 / 最大 / P95 延迟 |
| --- | --- | --- | --- |
| 1.5 GHz | 10.0006 s | 5855.53 | 0.68 / 1.42 / 0.68 ms |
| 2.0 GHz | 10.0005 s | 7803.38 | 0.51 / 12.56 / 0.51 ms |

内存写测试为 1 线程、1 KiB block、global scope，运行 10.0001 s，得到 `1967.24 MiB/s`，累计传输 `19680.54 MiB`；该次测试对应的 CPU 频率未注明。

尚缺系统镜像、温度、散热、供电及后台负载。比较频率设置时应重复测量，并同时检查稳定性与降频情况；内存结果不能直接当作 DDR 峰值带宽。
