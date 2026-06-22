
Linux 的“实时性优化（Real-Time Optimization）”通常是指尽可能降低任务响应延迟（Latency）和抖动（Jitter），保证关键任务在确定时间内执行。对于嵌入式开发、工业控制、音视频处理、机器人、软件定义无线电（SDR）等领域非常重要。

---

# 1. 选择实时内核（PREEMPT_RT）

Linux 默认并不是严格实时系统。

查看当前内核：

```bash
uname -a
```

查看抢占模型：

```bash
cat /sys/kernel/realtime
```

或者：

```bash
cat /boot/config-$(uname -r) | grep PREEMPT
```

可能看到：

```text
CONFIG_PREEMPT_NONE=y
```

或者：

```text
CONFIG_PREEMPT=y
```

实时内核：

```text
CONFIG_PREEMPT_RT=y
```

RT内核特点：

- 几乎所有中断线程化
    
- 自旋锁变成可睡眠锁
    
- 更低中断延迟
    
- 更稳定调度
    

例如：

- Ubuntu RT Kernel
    
- Debian PREEMPT_RT
    
- Linux Mint 可自行编译RT内核
    

---

# 2. 使用实时调度策略

Linux默认：

```text
SCHED_OTHER
```

实时任务使用：

```text
SCHED_FIFO
SCHED_RR
```

查看：

```bash
ps -eo pid,cls,rtprio,comm
```

设置：

```bash
sudo chrt -f 99 ./app
```

含义：

```text
FIFO
优先级99（最高）
```

代码：

```c
struct sched_param param = {
    .sched_priority = 80
};

sched_setscheduler(
    0,
    SCHED_FIFO,
    &param
);
```

---

# 3. CPU隔离（CPU Isolation）

避免实时任务被普通进程干扰。

例如：

```text
CPU0 -> 系统
CPU1 -> 实时任务
```

启动参数：

```text
isolcpus=1
```

或者：

```text
isolcpus=1 nohz_full=1 rcu_nocbs=1
```

查看：

```bash
cat /proc/cmdline
```

绑定进程：

```bash
taskset -c 1 ./app
```

---

# 4. CPU频率固定

动态调频会导致延迟抖动。

查看：

```bash
cpupower frequency-info
```

设置性能模式：

```bash
sudo cpupower frequency-set -g performance
```

检查：

```bash
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```

应显示：

```text
performance
```

---

# 5. 禁用深度省电状态（C-State）

CPU从休眠状态唤醒可能耗时数十微秒甚至毫秒。

查看：

```bash
cpupower idle-info
```

内核参数：

```text
intel_idle.max_cstate=0
processor.max_cstate=1
```

或者：

```text
idle=poll
```

实时系统常见配置：

```text
isolcpus=1
nohz_full=1
intel_idle.max_cstate=0
```

---

# 6. 中断绑定（IRQ Affinity）

让中断远离实时CPU。

查看：

```bash
cat /proc/interrupts
```

例如：

```text
eth0
usb
nvme
```

绑定到CPU0：

```bash
echo 1 > /proc/irq/24/smp_affinity
```

其中：

```text
1 = CPU0
2 = CPU1
4 = CPU2
8 = CPU3
```

实时线程运行在CPU1时：

```text
CPU0 -> IRQ
CPU1 -> Real-time Task
```

---

# 7. 锁定内存

避免Page Fault。

实时任务最怕：

```text
缺页异常(Page Fault)
```

锁定内存：

```c
mlockall(
    MCL_CURRENT |
    MCL_FUTURE
);
```

效果：

```text
禁止换页
```

---

# 8. 禁止Swap

查看：

```bash
swapon --show
```

关闭：

```bash
sudo swapoff -a
```

实时系统一般：

```text
不使用swap
```

---

# 9. 高精度定时器

启用：

```bash
CONFIG_HIGH_RES_TIMERS
```

查看：

```bash
cat /proc/timer_list
```

使用：

```c
clock_nanosleep()
```

不要使用：

```c
usleep()
```

---

# 10. 使用CLOCK_MONOTONIC

实时循环：

```c
clock_gettime(
    CLOCK_MONOTONIC,
    &ts
);
```

周期执行：

```c
clock_nanosleep(
    CLOCK_MONOTONIC,
    TIMER_ABSTIME,
    &next,
    NULL
);
```

这种方式比：

```c
sleep()
usleep()
```

精度高很多。

---

# 11. 减少内核日志

大量 printk 会导致抖动。

查看：

```bash
dmesg -w
```

调低日志级别：

```bash
echo 1 > /proc/sys/kernel/printk
```

---

# 12. 关闭不必要服务

查看：

```bash
systemctl list-units
```

关闭：

```bash
bluetooth
cups
avahi
```

减少：

- 中断
    
- 调度
    
- 内存压力
    

---

# 13. 测试实时性能

Linux RT领域最经典工具：

### cyclictest

安装：

```bash
sudo apt install rt-tests
```

测试：

```bash
sudo cyclictest -p99 -m -n -i1000
```

结果：

```text
Min: 3 us
Avg: 7 us
Max: 18 us
```

含义：

```text
最大调度延迟18微秒
```

这是最重要的指标。

---

# 14. 分析延迟来源

### ftrace

启用：

```bash
echo function_graph > \
/sys/kernel/debug/tracing/current_tracer
```

查看：

```bash
cat trace
```

---

### trace-cmd

```bash
trace-cmd record -e sched
```

分析：

```bash
kernelshark
```

可看到：

- 调度延迟
    
- 中断延迟
    
- Wakeup Latency
    

---

# 15. Linux驱动开发中特别重要

对于你正在研究的 DRM 驱动和 GPIO 驱动：

### 避免

```c
mdelay()
```

改用：

```c
usleep_range()
```

---

避免长时间持锁：

```c
spin_lock()
...
大量工作
...
spin_unlock()
```

应改为：

```c
spin_lock()
快速处理
spin_unlock()

后续工作放workqueue
```

---

中断处理：

```c
irq_handler_t
```

只做：

```c
- 清中断
- 记录状态
```

复杂逻辑：

```c
threaded irq
workqueue
tasklet
```

---

# 实际效果参考

普通 Linux：

```text
最大延迟:
200~1000 us
```

开启 PREEMPT：

```text
50~200 us
```

PREEMPT_RT：

```text
10~50 us
```

优化CPU隔离后：

```text
5~20 us
```

高性能 x86 工业控制机：

```text
< 5 us
```

如果目标是**验证驱动实时性或做工业控制实验**，推荐的组合是：

```text
PREEMPT_RT
+
SCHED_FIFO
+
CPU Isolation
+
IRQ Affinity
+
mlockall()
+
performance governor
+
cyclictest
```

这是目前 Linux 社区最常见、效果也最明显的一套实时优化方案。
