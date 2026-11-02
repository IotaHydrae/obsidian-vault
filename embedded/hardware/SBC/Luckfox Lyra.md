# Luckfox Lyra CoreMark 测量

> 一次 CoreMark 运行得到 `4096.122338 iterations/s`；该数值只对应下列构建与运行条件，不作为性能保证。

| 项目 | 测量条件或结果 |
| --- | --- |
| CoreMark | 1.0，performance run，数据大小 666 |
| 编译器 | GCC 10.3.1 20210621 |
| 编译参数 | `-O2 -DPERFORMANCE_RUN=1 -lrt` |
| 迭代次数 | 60000 |
| 总时间 | 14.648 s |
| 结果 | 4096.122338 iterations/s |
| 数据位置 | Heap |
| 校验 | 报告为 `Correct operation validated` |

校验字段：`seedcrc=0xe9f5`、`crclist=0xe714`、`crcmatrix=0x1fd7`、`crcstate=0x8e3a`、`crcfinal=0xbd59`。

最终结果行标为 Heap，但详细的 memory location 字段尚未填写；另外还缺板卡具体型号、CPU 频率、系统镜像、温度、负载及完整复现命令。比较不同设备前，应补齐这些条件并按 CoreMark 的运行与报告规则测量。STREAM、fio 和 iperf3 尚无测量数据。
