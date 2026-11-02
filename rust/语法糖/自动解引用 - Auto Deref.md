# Rust 方法调用的自动解引用

> 方法调用可自动解引用接收者，例如通过 `Box<String>` 调用 `String` 的方法。

## 示例

```rust
let boxed = Box::new(String::from("hello"));
assert_eq!(boxed.len(), 5);
```

此例通过 `Box` 的解引用访问内部 `String`；直接调用 `String::len()` 不需要借助这层自动解引用。
