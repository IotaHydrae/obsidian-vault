# Rust 方法调用的自动引用

> 方法需要借用接收者时，调用语法可自动插入合适的引用。

## 示例

```rust
let s = String::from("hello");
s.chars(); // 自动引用，相当于 (&s).chars()
```
