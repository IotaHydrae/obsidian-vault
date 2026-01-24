`init_completion` 是 Linux 内核中用于**初始化 completion 结构**的函数/宏，主要用于线程/任务间“完成事件”同步（类似于一个轻量级的、单次使用的 semaphore 或 barrier）。

它定义在 `<linux/completion.h>` 中。

### 核心类型

```c
struct completion {
    unsigned int done;
    wait_queue_head_t wait;
};
```

### 主要初始化接口对比

|接口|作用|使用时机|是否清空等待队列|现代内核推荐度|备注|
|---|---|---|---|---|---|
|`init_completion(&done)`|完整初始化（done=0 + 初始化等待队列）|**第一次使用** 或 静态变量声明后|是（新建空队列）|★★★★★|最常用|
|`DECLARE_COMPLETION(done)`|声明 + 初始化（栈上或文件作用域）|局部变量或模块静态变量|是|★★★★☆|方便|
|`INIT_COMPLETION(done)`|只把 `done` 置 0（**不**动等待队列）|**复用同一个 completion** 时|否|★★☆☆☆|已 deprecated|
|`reinit_completion(&done)`|只把 `done` 置 0（**不**动等待队列）|**复用**（推荐替代 INIT_COMPLETION）|否|★★★★★|现代首选复用方式|