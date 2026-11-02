# Rust 返回位置的 `impl Trait`

> 返回位置的 `impl Trait` 隐藏具体类型，但返回值必须实现指定 trait。

## 示例

```rust
fn returns_closure() -> impl Fn(i32) -> i32 {
    |x| x * 2
}
```

同一函数的各个返回路径需要返回同一种具体类型；`impl Trait` 不等同于可以混用任意不同类型。
