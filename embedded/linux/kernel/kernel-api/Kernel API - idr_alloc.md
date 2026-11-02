# Linux 内核 `idr_alloc()`

> `idr_alloc()` 在指定范围内分配空闲 ID，并将其关联到给定指针；并发修改需要调用方同步。

## 接口

```c
int idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp);
```

- 正的 `end` 表示不包含在范围内的上界；`end <= 0` 时通常允许分配至 `INT_MAX`。
- 成功返回 ID；失败包括 `-ENOMEM`、`-ENOSPC`，负的 `start` 不合法。
- 调用方必须防止对同一个 IDR 的并发修改；读取时需遵循目标内核对应的 RCU 或锁规则。

使用前核对目标版本的接口、分配上下文和生命周期要求。
