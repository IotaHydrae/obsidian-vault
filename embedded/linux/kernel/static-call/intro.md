# Linux Static Call

> Static call 提供可更新的调用目标；支持的架构和配置可通过代码修补减少间接调用开销。

```c
static void default_func(void) { pr_info("default\n"); }
static void new_func(void) { pr_info("new\n"); }

DEFINE_STATIC_CALL(my_call, default_func);

static_call(my_call)();
static_call_update(my_call, new_func);
```

这里仅展示 API 用法，函数声明、头文件、并发和模块生命周期需按实际上下文补齐。不同架构和配置可能使用 trampoline 或其他回退实现，因此不能假设每个调用点都被修补为直接调用。更新与调用的约束、宏形式及空目标行为以目标内核 `include/linux/static_call.h` 为准；是否有性能收益应在目标系统测量。
