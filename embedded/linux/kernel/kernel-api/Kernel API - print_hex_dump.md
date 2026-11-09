# Linux `print_hex_dump()`

> `print_hex_dump()` 把缓冲区分行写入内核日志；`hex_dump_to_buffer()` 只格式化一行，不负责打印，也不会自动遍历整个输入。

## TL;DR

- 分析协议字节优先使用 `DUMP_PREFIX_OFFSET`、`rowsize = 16`、`groupsize = 1`。
- 常驻调试使用 `print_hex_dump_debug()` / `print_hex_dump_bytes()`，并限制输入长度与触发频率。
- 缓冲区须可直接由内核安全读取，并在整个打印过程中保持有效；日志函数不会替你保护并发修改。

源码基准：Linux 6.18 LTS / 6.18.62。

## 参数与输出

```c
void print_hex_dump(const char *level, const char *prefix_str, int prefix_type,
                    int rowsize, int groupsize,
                    const void *buf, size_t len, bool ascii);
```

| 参数 | 建议与含义 |
| --- | --- |
| `level` | `KERN_DEBUG`、`KERN_INFO` 等，是否出现在控制台还取决于日志配置 |
| `prefix_str` | 每行前缀，必要时自己加尾部空格 |
| `prefix_type` | `DUMP_PREFIX_NONE` / `OFFSET` / `ADDRESS`；偏移更适合对照协议 |
| `rowsize` | 每行 16 或 32 字节；其他值在实现中回退为 16 |
| `groupsize` | 1、2、4 或 8 字节；不合法或该行长度不能整除时回退为 1 |
| `buf` / `len` | 输入缓冲区与实际可访问长度，不是字符串长度 |
| `ascii` | 是否附加可打印 ASCII 列 |

多字节组按目标机器的数值解释呈现，可能与协议的线上字节顺序不同。排查端序时先看逐字节 dump，再按协议解析，不要从一组十六进制数字直接推断字段端序。

## 示例：限制打印长度

```c
#include <linux/minmax.h>
#include <linux/printk.h>

static void dump_packet_prefix(const void *buf, size_t len)
{
    size_t shown = min_t(size_t, len, 64);

    print_hex_dump_debug("packet: ", DUMP_PREFIX_OFFSET,
                        16, 1, buf, shown, true);
}
```

调用者保证前 `shown` 字节在打印期间可访问且内容稳定。`debug` 形式受 dynamic debug、`DEBUG` 等构建配置控制；它不等于自动限流。若每秒大量触发，应在调用方限流、按条件启用或改用 trace/采样，避免日志改变设备时序。

## 只需要一行字符串时

```c
#include <linux/printk.h>

static int format_packet_line(const void *buf, size_t len,
                              char *line, size_t capacity)
{
    return hex_dump_to_buffer(buf, len, 16, 1,
                              line, capacity, true);
}
```

该例最多处理 16 个输入字节；要格式化后续行须由调用方推进输入指针。返回值不包含结尾 NUL，截断时表示缓冲区足够大时本应生成的长度；对有效 `capacity > 0`，可用 `ret >= capacity` 判断输出截断。若 `capacity = 0` 不会写字符串，不能据此读取 `line`。空输入在有输出空间时生成空字符串。

## 使用边界与验证

- 不直接传用户指针、MMIO 地址或已释放内存；先使用对应访问接口取得受控快照。
- 长度必须来自可信边界检查，不能让设备报告的任意长度越过实际缓冲区。
- 打印密钥、令牌和无关个人数据会把内容复制到日志；选择需要的字段或脱敏后的快照。
- 硬中断内大量打印会拉长处理时间；需要观察并发数据时，短临界区取快照，锁外打印。
- 验证时检查日志级别和 debug 开关、dump 长度、偏移连续性及最后一行，区分“未启用打印”和“数据为空”。

## 源码与参考

- [6.18.62 lib/hexdump.c](https://github.com/gregkh/linux/blob/v6.18.62/lib/hexdump.c)：行格式、分组回退与返回值。
- [include/linux/printk.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/printk.h)：debug 形式及日志配置分支。
