**schedule_timeout** 是 Linux 内核中用于让当前任务（线程/进程）睡眠一段时间的**底层核心函数**，定义在 `<linux/sched.h>` 中（实现主要在 `kernel/time/timer.c` 和 `kernel/sched/core.c`）。

它不像 `msleep()`、`ssleep()` 那样封装好状态，直接操作当前任务的状态 + 调度。

### 函数原型

```c
signed long schedule_timeout(signed long timeout);
```

- **参数**：`timeout` —— 以 **jiffies** 为单位（不是毫秒！）
    - 正值：睡眠最多这么多 jiffies
    - `MAX_SCHEDULE_TIMEOUT`：无限期睡眠（相当于 schedule()）
    - 负值：不推荐（行为未定义）
- **返回值**：
    - 剩余的 jiffies（正常超时返回 0 或很小的值）
    - 如果被信号打断，返回剩余的 jiffies（>0）

### 关键点：必须配合设置任务状态使用！

**schedule_timeout() 本身不设置 TASK_INTERRUPTIBLE 或 TASK_UNINTERRUPTIBLE**，它只在当前状态已经是睡眠状态时才起作用。

正确用法必须先设置状态：

```c
// 可被信号打断的睡眠（最常见）
set_current_state(TASK_INTERRUPTIBLE);
schedule_timeout(timeout_jiffies);

// 不可被信号打断的睡眠
set_current_state(TASK_UNINTERRUPTIBLE);
schedule_timeout(timeout_jiffies);
```

### 推荐的现代封装（优先使用这些）

| 函数                                          | 内部实现等价于                                                          | 可被信号打断？ | 推荐场景        | 参数单位    |
| ------------------------------------------- | ---------------------------------------------------------------- | ------- | ----------- | ------- |
| `msleep(unsigned int msecs)`                | `schedule_timeout(msecs_to_jiffies(msecs))` 可打断                  | 是       | 普通延迟，允许信号   | 毫秒      |
| `ssleep(unsigned int secs)`                 | `schedule_timeout(secs * HZ)` 不可打断                               | 否       | 必须完整睡眠，不理信号 | 秒       |
| `msleep_interruptible(msecs)`               | 同 msleep，但显式可打断                                                  | 是       | 需要检查信号的场景   | 毫秒      |
| `schedule_timeout_uninterruptible(timeout)` | `set_current_state(TASK_UNINTERRUPTIBLE)` + `schedule_timeout()` | 否       | 经典不可打断写法    | jiffies |

### 典型正确用法示例

```c
#include <linux/sched.h>       // schedule_timeout
#include <linux/delay.h>       // msleep 等
#include <linux/jiffies.h>     // msecs_to_jiffies

// ======== 方式1：推荐封装（99% 情况用这个） ========
msleep(100);                    // 睡 100ms，可被信号打断
ssleep(3);                      // 睡 3秒，不可打断
msleep_interruptible(500);      // 睡 500ms，可打断

// ======== 方式2：手动 schedule_timeout（更灵活） ========
set_current_state(TASK_INTERRUPTIBLE);
schedule_timeout(msecs_to_jiffies(250));   // 睡 250ms，可被信号打断

// 不可打断版本
set_current_state(TASK_UNINTERRUPTIBLE);
schedule_timeout(msecs_to_jiffies(1000));

// 带提前唤醒判断（经典 wait_event_timeout 风格）
set_current_state(TASK_UNINTERRUPTIBLE);
if (schedule_timeout(msecs_to_jiffies(5000)) == 0) {
    // 真的超时了
} else {
    // 被提前唤醒（剩余时间 > 0）
}

// ======== 方式3：无限期睡眠（等价于 schedule()） ========
set_current_state(TASK_UNINTERRUPTIBLE);
schedule_timeout(MAX_SCHEDULE_TIMEOUT);    // 一直睡，直到被 wake_up
```

### 常见错误写法（请避免）

```c
// 错误：没设置状态 → 不睡觉！
schedule_timeout(HZ);               // ← 几乎不起作用

// 错误：用了负数
schedule_timeout(-100);             // 未定义行为

// 错误：直接在中断上下文用（死锁/崩溃）
schedule_timeout(HZ);               // 在 IRQ 或 softirq 中禁止

// 不推荐：直接用 jiffies 硬编码（不移植）
schedule_timeout(100 * HZ);         // 应该用 msecs_to_jiffies
```

### 快速对照表

|需求|推荐写法|是否可被信号打断|参数单位|
|---|---|---|---|
|普通短延迟（<1s）|`msleep(msecs)`|是|毫秒|
|必须完整睡眠（不理信号）|`ssleep(secs)` 或 `schedule_timeout_uninterruptible()`|否|秒 / jiffies|
|需要检查是否被信号打断|`schedule_timeout()` + `TASK_INTERRUPTIBLE`|是|jiffies|
|无限期等待别人唤醒|`schedule_timeout(MAX_SCHEDULE_TIMEOUT)`|取决于状态|—|
|高精度纳秒级（现代内核）|`schedule_hrtimeout()` / `usleep_range()`|看具体封装|ns / us|
### 小结建议优先级

1. 用 `msleep()` / `ssleep()` / `msleep_interruptible()` → 最安全、最清晰
2. 需要精确控制“是否可打断”或“检查剩余时间” → 用 `schedule_timeout()` + 手动 `set_current_state`
3. 极少数需要纳秒级 → 考虑 `schedule_hrtimeout()` 或 `usleep_range()`