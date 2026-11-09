# Linux `call_usermodehelper()`：由内核启动用户程序

> `call_usermodehelper()` 将用户程序启动请求交给内核 helper 机制；等待 exec 成功、等待程序退出和仅提交请求是不同结果，必须按所选模式解释。

## TL;DR

- `path` 指定可执行文件，`argv` / `envp` 均以 `NULL` 结束；不会自动经过 shell，也不会自动展开参数。
- `UMH_WAIT_EXEC` 不等待业务完成，`UMH_WAIT_PROC` 才等待进程退出；`UMH_NO_WAIT` 无法反馈后续 exec 失败。
- 同步等待要求可睡眠上下文；异步参数和模块代码必须活到 helper 真正不再访问它们。
- 普通驱动与用户服务协作优先评估既有子系统、uevent 或 netlink 等接口，不应把内核 helper 当作通用业务进程管理器。

源码基准：Linux 6.18 LTS / 6.18.62。

## 接口与模式

```c
int call_usermodehelper(const char *path, char **argv, char **envp, int wait);
```

| 模式 | 返回时代表什么 | 参数与上下文要求 |
| --- | --- | --- |
| `UMH_NO_WAIT` | 已完成提交阶段，不代表 exec 或业务成功 | 6.18.62 使用原子分配路径，可用于中断提交；参数须异步持有 |
| `UMH_WAIT_EXEC` | 等到 exec 阶段结果，程序可能仍在运行 | 可睡眠，不能把成功当业务已完成 |
| `UMH_WAIT_PROC` | 等到 helper 进程退出 | 可睡眠，可能长时间等待 |
| 加 `UMH_KILLABLE` | 使相应等待可被致命信号打断 | 仅与对应等待模式组合；提前返回不代表 helper 已停止 |

负值通常表示内核阶段失败，例如无法分配、禁止 helper、文件不存在或 exec 失败。`UMH_WAIT_PROC` 的非负返回可能为编码后的进程退出/信号状态，**不是直接的应用 exit code**；需按目标内核 wait status 规则解码。返回 0 也不能在所有配置下证明程序执行过：`CONFIG_STATIC_USERMODEHELPER_PATH=""` 可使调用成为成功的空操作。

## 实现路径

```text
call_usermodehelper()
  → call_usermodehelper_setup() 建立 subprocess_info
  → call_usermodehelper_exec() 检查 helper 是否允许
  → 将执行请求加入 system_unbound_wq
  → helper 执行线程准备上下文并 kernel_execve()
  → 按 WAIT_EXEC / WAIT_PROC 完成通知或等待退出
  → 释放 subprocess_info；若设置则调用 cleanup
```

`setup()` 保存参数指针，不会替调用者深拷贝 argv、envp 和字符串。即使调用来源是特定用户任务，也不能假设 helper 继承调用者的交互终端、当前目录、环境、命名空间或权限语境；默认 helper 使用内核准备的高权限执行上下文，并受安全策略和内核配置约束。

## 示例：在任务上下文同步等待退出

```c
#include <linux/umh.h>

static int run_device_helper(void)
{
    char *argv[] = {
        "/usr/libexec/example-device-helper", "--probe", NULL
    };
    char *envp[] = {
        "HOME=/", "PATH=/usr/sbin:/usr/bin:/sbin:/bin", NULL
    };

    return call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
}
```

路径是需要由系统实际部署的示例，不是内核保证存在的程序。此例选择没有 killable 提前退出的同步等待，栈上参数在等待完成前仍有效；调用者需处理负错误和编码后的退出状态。这里只验证接口写法，不应为测试笔记而从内核执行此程序。

不能只把模式改为 `UMH_NO_WAIT` 就继续使用栈数组。异步场景应通过 `call_usermodehelper_setup()` 分配或持有参数，设置适当 `cleanup` 回收，再 `call_usermodehelper_exec()`；同时设计模块卸载时的跟踪与等待。cleanup 自身及其访问的数据也不能提前失效。

## 适用边界与排查

- 路径与参数来自受控逻辑，不把外部输入拼成 `/bin/sh -c` 字符串；需要传参时使用独立 argv 项。
- 不在持有 helper 完成所依赖的锁时使用同步等待，避免锁和资源依赖死锁。
- `UMH_NO_WAIT` 的 0 不是执行回执；需要业务结果就建立明确的状态反馈协议。
- 调用期间可能被挂起/恢复流程、helper 禁用状态、安全策略、文件系统挂载状态或静态 helper 配置影响。
- 排查先区分“提交失败、exec 失败、进程异常退出、业务返回失败”，再检查可执行路径、解释器、文件权限、环境和目标系统日志。
- 同步等待没有天然业务期限；超时控制和取消需另行设计，不能假设函数会自动杀掉卡住的程序。

## 源码与参考

- [6.18.62 umh.h](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/umh.h)：等待标志、setup/exec 接口和回调。
- [kernel/umh.c](https://github.com/gregkh/linux/blob/v6.18.62/kernel/umh.c)：参数持有、工作队列、exec、退出状态及禁用分支。
- 相关：[workqueue](Kernel%20API%20-%20workqueue.md)、[completion](Kernel%20API%20-%20init_completion.md)。
