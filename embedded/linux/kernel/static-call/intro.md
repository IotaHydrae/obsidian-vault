`static call` 是 Linux 内核里一个**高性能函数调用优化机制**，主要用于**替代函数指针调用（indirect call）**，减少分支预测失败和间接跳转带来的性能损耗。

它在较新的内核（5.x 之后）被广泛用于热点路径，比如调度器、tracepoint、LSM 等。

# 一、为什么需要 static call？

传统做法：

```c
void (*func)(void);  
  
func();  // 间接调用
```

问题：

- CPU 无法很好预测目标地址（BTB 命中率低）
- 影响 pipeline
- 在高频路径（hot path）性能很差

# 二、static call 的核心思想

👉 把“函数指针调用”变成“可动态修改的直接调用”

也就是说：

```c
func();   // 间接调用 ❌
```

变成：

```c
call some_function  // 直接调用 ✅
```

但关键是：

> 这个 `some_function` 是可以在运行时被修改的！

# 三、基本使用方式

## 1. 定义 static call

```c
DEFINE_STATIC_CALL(my_call, default_func);
```

或者带返回值：

```c
DEFINE_STATIC_CALL_RET0(my_call, default_func);
```

---

## 2. 调用

```c
static_call(my_call)();
```

编译后：

👉 这里不是函数指针调用，而是一个 **patched call 指令**

