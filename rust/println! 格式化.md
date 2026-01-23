
Rust 中最常用的输出方式就是 println!（以及相关的 print!、format!、eprintln! 等），它们都使用相同的格式化字符串语法（来自 std::fmt 模块）。

### 基本用法

```rust
println!("Hello");                    // 普通字符串
println!("x = {}", 42);               // 默认格式（Display）
println!("调试: {:?}", vec![1,2,3]);  // Debug 格式（最常用调试）
println!("名字参数: {name}", name = "木头");  // Rust 1.58+ 支持（推荐！）
```

