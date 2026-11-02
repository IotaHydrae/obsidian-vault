# 怎么查开机 I/O 争抢

> 稳态冷缓存启动比开机快，只能证明启动环境不同。要归因 I/O，需同时测设备队列、进程活动，并做干预对照；不能把逐 PID 累计值相加当作应用实读量。

## TL;DR

- `/proc/diskstats` 测设备总量，`iostat` 看队列/等待，`iotop` 看活跃进程；三者回答不同问题。
- `/proc/<pid>/maps` 文件大小是映射上界，不是读盘字节数。
- `/proc/<pid>/io` 会包含回收子进程累加的统计；父子进程分别取最大值再求和会重复计算。
- 先检查实际 cgroup 挂载。开发板是 hybrid，存在 v1 blkio；不能因 v2 没有 `io.stat` 就说只能用 proc。
- 启动事件与首帧口径统一见 [boot-time.md](boot-time.md)。

## 1. 测什么

| 工具 | 观测 | 限制 |
| --- | --- | --- |
| `systemd-analyze` / journal | 单元和应用事件 | 不给出进程内部 I/O 归因；Started 不是页面首帧 |
| `/proc/diskstats` | 指定设备窗口内读扇区差 | 扇区按 512 B 换算；整盘和分区不要重复累计 |
| `iostat -x 1` | 吞吐、队列和 await | 要与慢启动窗口同时采样，不能只看峰值 |
| `iotop -b -o -d 1 -n 14` | 进程活动趋势 | 峰值不一定同时出现，累计值也须核对继承口径 |
| cgroup I/O 统计 | 服务及子进程按组统计 | 先查控制器版本、字段及单位，不能假定 v2 路径 |
| `perf` / `strace` | read、缺页、futex、ioctl 等等待 | 有测量开销，perf 需与 BSP 内核匹配 |

采样需 root 的部分用 sudo。分钟级或固定窗口测试放后台，打印开始/结束和日志位置。
开机探针可以用临时 `Type=simple` 服务后台记录；不要把等 14 秒的 oneshot 排到启动关键路径，
否则测到的是探针人为增加的时间。测完移除探针并恢复配置。

## 2. 比较与归因

1. 定义同一个终点，例如浏览器 FCP，或 Cage 启动到浏览器 GET；别拿 Started 和首帧相减比较。
2. 比较开机与稳态冷缓存启动，记录设备吞吐、await、队列和同时活动的进程。
3. 慢窗口与高 I/O 同时发生只算相关；只改变一个候选服务的启动安排，做 A/B 对照。
4. 同时记录 CPU、缺页和其它等待，避免把所有开机差异都归因磁盘。

`drop_caches` 能制造冷页缓存，不能复现开机的全部并发工作，且会影响其它程序；
不能仅凭稳态 1.4 s、开机 6 s 的差异确认 I/O 根因。

## 3. 两类失真的计数

**映射上界**：历史“Chromium 要读 613 MB”来自 maps 中的文件大小；mmap 按需取页，
共享库还共用页缓存，所以这个数不代表启动实读。预热方案的实际收益见
[chromium-cold-start.md](chromium-cold-start.md)，不再用映射大小解释收益。

**父子重复统计**：历史按每个 PID 的 `read_bytes` 最大值求和得到 74.7 MB 或 1179.8 MB，
其中 PID1 接近 977 MB，而设备窗口仅约 84.4 MB。它们不能证明 readahead 被记错了主人，
也不能据此确认“Chromium 真读 75 MB”。

源码依据：`kernel-6.1/kernel/exit.c` 的 `wait_task_zombie()` 把被回收子进程的 `ioac`
累加给父进程；`kernel-6.1/fs/proc/base.c` 的 proc I/O 展示又包含 `task->signal->ioac`。
子 PID 和之后的父 PID 同时求和会重复统计；窗口最大值还会混入窗口之前的累计量。

设备总量只能校验总账，不能直接分配给 Chromium。服务级归因优先使用可用的 cgroup 控制器，
并以设备总量和受控 A/B 交叉核对。字段不存在时报告观测限制，不伪造精确归因。

## 4. 页面探针

HTTP 探针要区分文档导航与 fetch 探活；测后端响应时须避免 HTTP 缓存吞掉请求，使用 cache-buster 或明确的 no-store 策略。
当前着陆页用 HEAD、`no-cors` 探测：opaque 响应无法读取状态码，GET-only 主页可返回 405，
因此探测完成只说明响应头已到达，不证明 HTTP 200。HTTP 200、浏览器导航、FCP 与 HDMI 首帧仍是不同事件。

printk ARM counter、journal 接收 monotonic 和墙钟不能混用；`fake-hwclock` 不会令 monotonic 跳变。
复位前缀用串口日志完整量得，不能仅加 U-Boot 自报 `Total`。工具与原始记录格式见
[serial-exec.md](../hardware/serial-exec.md)。
