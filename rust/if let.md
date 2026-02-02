Rust 中的 if let 用法 是非常常用且实用的语法糖，主要用来只关心某一种模式匹配成功的情况，而不需要写完整的 match。

基本形式

```rust
if let 模式 = 表达式 {
    // 匹配成功时执行这里的代码
    // 模式中绑定的变量可以在 {} 里使用
}
```

最经典的例子（处理 Option）：

```rust
let maybe_number = Some(7);

// 传统 match 写法（啰嗦）
match maybe_number {
    Some(n) => println!("得到了数字：{}", n),
    None    => {}   // 什么都不做
}

// 用 if let 简化（推荐写法）
if let Some(n) = maybe_number {
    println!("得到了数字：{}", n);
}
```

带 else 的版本（Rust 1.65+ 推荐）

```rust
let maybe_number = Some(42);

if let Some(n) = maybe_number {
    println!("有值：{}", n);
} else {
    println!("是 None");
}
```

更现代的写法（2024–2025 年后非常流行）是用 let else：

```rust
let Some(n) = maybe_number else {
    println!("是 None，提前返回或处理");
    return;
};
// 到这里一定有值，n 已解构出来
println!("值是：{}", n);
```

### 常见使用场景举例

1. **Option 解包**（最常见）

```rust
fn print_length(s: Option<&str>) {
    if let Some(text) = s {
        println!("长度：{}", text.len());
    } else {
        println!("没有字符串");
    }
}
```