# Linux Runtime PM：访问期间保持设备可用

> Runtime PM 的 usage count 表示设备仍被使用；同步访问前 resume-and-get，最后一个访问者结束后再 put，不能在 DMA 或 work 尚未结束时提前交还。

## TL;DR

- 普通同步访问使用 `pm_runtime_resume_and_get()`，成功为 0，失败负错误且不会留下本次增加的 usage count。
- 成功 get 才配对一次 put；PM 引用不替代对象引用，也不串行化多个业务访问。
- 6.18.62 的 `pm_runtime_put_autosuspend()` 已更新 last_busy，再减 usage count 并安排 autosuspend。
- PM 回调、初始硬件状态和 autosuspend 策略须正确配置；不能只加 get/put 就认为完成电源管理。

源码基准：Linux 6.18 LTS / 6.18.62。

## 接口选择

| 接口 | 边界 |
| --- | --- |
| `pm_runtime_resume_and_get()` | 同步恢复并取得 usage；失败自动回滚本次计数 |
| `pm_runtime_get_sync()` | 失败仍可能保留新增 usage，新代码通常优先前者 |
| `pm_runtime_put_autosuspend()` | 本版本内置 mark_last_busy，释放 usage 并异步评估 autosuspend |
| `pm_runtime_put_noidle()` | 只减计数，不触发 idle/suspend；不是通用正常完成替代品 |
| `pm_runtime_set_autosuspend_delay()` / `pm_runtime_use_autosuspend()` | 配置延迟并启用相应策略 |
| `pm_runtime_enable()` / `pm_runtime_disable()` | 启用/禁用 runtime PM，不等同于设备物理上电/断电 |

Runtime PM 核心结合 usage count、子设备状态和策略协调 idle、suspend、resume 回调。设备驱动回调实际执行时钟、regulator、寄存器保存和恢复等操作；总线或电源域也可能参与回调选择。

## 示例：一次同步寄存器访问

```c
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

static int sensor_read_register(struct device *dev, struct regmap *map,
                                unsigned int reg, unsigned int *value)
{
    int ret;
    int pm_ret;

    ret = pm_runtime_resume_and_get(dev);
    if (ret < 0)
        return ret;

    ret = regmap_read(map, reg, value);

    pm_ret = pm_runtime_put_autosuspend(dev);
    if (pm_ret < 0)
        dev_dbg(dev, "autosuspend request returned %d\n", pm_ret);

    return ret;
}
```

前提：设备的 runtime PM 回调、初始状态及 autosuspend 已正确设置，调用者持有设备和 regmap 的有效生命周期并通过离线检查。读取成功才使用 `*value`。put 后的负返回不意味着可以重复 put；计数释放与挂起请求结果是不同问题，此处保留寄存器访问结果。

## 状态与异步生命周期

```text
取得对象/访问资格 → resume_and_get → 同步 I/O 或提交异步活动
→ 最后一次硬件访问结束 → put_autosuspend → 释放对象/访问资格
```

- DMA、IRQ 线程或 work 还需要设备时，usage 必须保持到它们结束；提交函数返回不代表异步硬件访问完成。
- PM usage 防止 runtime suspend，不会保护对象免于释放，也不会防止业务路径彼此交错；仍需锁和引用。
- 普通 resume-and-get 可睡眠，不能在 hardirq、关中断或持自旋锁时调用。`pm_runtime_irq_safe()` 有专门约束，不是通用豁免。
- `pm_runtime_set_active()` 只修改核心记录，调用前硬件必须确实已处于 active，且满足父设备等约束；不能用它代替上电。
- remove 时先关闭新访问并排空异步活动，再按真实硬件状态协调 PM 禁用、必要的恢复/停机和资源回收。`pm_runtime_disable()` 本身不停止 DMA 或所有业务 work。
- 移植到旧内核时核对 `pm_runtime_put_autosuspend()` 是否内置 mark_last_busy；6.18.62 无需为同一释放点再重复调用一次。

## 源码与参考

- [Runtime PM 文档](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/power/runtime_pm.rst)：usage、回调、初始状态和驱动退出。
- [pm_runtime.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/pm_runtime.h)、[runtime.c](https://github.com/gregkh/linux/blob/v6.18.62/drivers/base/power/runtime.c)：失败回滚、last_busy 与 autosuspend。
