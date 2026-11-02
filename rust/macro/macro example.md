# Rust 宏示例：避免重复求值

> 声明式宏直接展开输入表达式；当它在模板中出现多次，副作用也可能执行多次。

```rust
macro_rules! square {
    ($x:expr) => {{
        let value = $x;
        value * value
    }};
}

macro_rules! log {
    (info: $msg:expr) => { println!("[INFO] {}", $msg) };
    (warn: $msg:expr) => { eprintln!("[WARN] {}", $msg) };
}

fn main() {
    let mut calls = 0;
    let result = square!({ calls += 1; 2 + 3 });
    assert_eq!(result, 25);
    assert_eq!(calls, 1);
    log!(info: "finished");
}
```

示例中的乘法使用 `value` 两次，适用于支持该用法的类型，例如整数。若展开为 `$x * $x`，输入表达式会执行两次；复杂参数或副作用会导致意外结果。

重复匹配和生成列表见 [[1. 声明式宏 (Declarative Macros)]]；过程宏例子分别见 [[2. 过程宏 - 派生宏]] 和 [[4. 过程宏 - 函数式宏 (Function-like Macros)]]。
