# Linux CPUFreq、OPP、Thermal 与 CPUIdle

> CPU 性能和功耗由频率策略、允许的工作点、温度约束及空闲状态共同决定；单次频率读数不能解释端到端延迟。

## TL;DR

- CPUFreq policy 管理一个或多个共享性能控制的 CPU，governor 根据策略选择目标；具体频率由驱动和硬件执行。
- OPP 描述设备可使用的性能工作点及相关约束，频率与电压组合必须来自平台数据。
- thermal zone、trip、governor 和 cooling device 共同处理温度约束；降频原因需结合状态与时间序列确认。
- CPUIdle 选择空闲状态；退出延迟与目标驻留时间影响功耗和唤醒，不等同于 CPUFreq 的频率调整。

范围：Linux 6.18 LTS；不同 CPUFreq 驱动和平台可能采用不同控制策略。

## 性能控制关系

```text
任务运行与利用率 → CPUFreq governor / 驱动 → policy 目标及硬件频率
                                    ↑
                         OPP、频率 QoS、thermal 等约束

CPU 空闲 → CPUIdle governor / 驱动 → 选择支持的 idle state → 事件唤醒
```

`schedutil` 可以使用调度器利用率信号，见 [[Linux EEVDF PELT 与 schedutil]]。先将线程实际运行的 CPU 对应到 policy，不应假定每个逻辑 CPU 都有完全独立的时钟和电压。

## 各框架的职责

| 框架 | 关注点 |
| --- | --- |
| CPUFreq | 驱动、policy、可用频率、目标和 governor；实际硬件频率可能受其他约束 |
| OPP | 受支持工作点的频率及电压等信息；部分设备可存在多电源或额外依赖 |
| Thermal | 温度传感器、thermal zone、trip、governor 和 cooling device |
| CPUIdle | idle state 的进入条件、目标驻留时间和退出延迟 |

调高 policy 上限不保证负载一定获得最高频率；温度、功耗、硬件、共享 policy 和任务是否可运行都会影响结果。禁用 thermal 保护也不能作为性能问题的通用修复。

### 读对 policy 的频率字段

从 `/sys/devices/system/cpu/cpufreq/policy*/` 中查 `related_cpus`，其中包含该 policy 的在线和离线 CPU；`affected_cpus` 列出当前在线成员。若负载会迁移，应把采样时刻、实际运行 CPU 与 policy 一起记录。

| sysfs 字段 | 判断边界 |
| --- | --- |
| `scaling_max_freq` | 当前允许的上限（kHz），不是实际达到的频率 |
| `scaling_cur_freq` | 多数驱动报告最近请求的 P-state 频率（kHz）；部分架构会提供更接近实际的读数 |
| `cpuinfo_cur_freq` | 硬件取得的当前频率（kHz）；无法获得时该属性可不存在，而不是用 `scaling_cur_freq` 当实测值 |

判别降频先看上限是否随 thermal/QoS 改变，再看请求值与硬件反馈是否不同，最后检查任务是否在这些 CPU 上持续运行。只看某个瞬间的 `scaling_cur_freq`，不能区分三者。

## 分析延迟和降频

1. 在相同负载下同时记录任务调度状态、CPU 频率、温度、thermal 限制和供电条件；优先比较时间序列。
2. 确认任务 affinity、cgroup 配额和 policy 中涉及的 CPU；排队或 I/O 等待不能靠提高频率消除。
3. 对唤醒延迟，区分硬件事件、IRQ、idle state 退出、线程唤醒和调度；用 tracepoint 验证，而不是只看 idle state 名称。
4. 查看 `/sys/devices/system/cpu/cpufreq/`、各 CPU 的 `cpuidle/`、`/sys/class/thermal/` 中实际可用属性；字段和权限由系统配置决定。

## 参考

- Linux 6.18.62：[CPUFreq](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/admin-guide/pm/cpufreq.rst)、[CPUIdle](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/admin-guide/pm/cpuidle.rst)、[OPP](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/power/opp.rst)、[thermal sysfs](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/thermal/sysfs-api.rst)。
- 蜗窝科技：[OPP](https://www.wowotech.net/pm_subsystem/pm_opp.html)、[thermal 概述](https://www.wowotech.net/pm_subsystem/519.html)、[schedutil](https://www.wowotech.net/process_management/schedutil_governor.html)。
