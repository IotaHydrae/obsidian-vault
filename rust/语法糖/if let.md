# Rust 的 `if let`、`let else` 和 `match`

> 只处理一个模式时使用 `if let`；需要穷尽各分支时使用 `match`；失败时提前退出可使用 `let else`。

## `if let`

```rust
let value = Some(7);
if let Some(n) = value {
    println!("{n}");
} else {
    println!("no value");
}
```

`if let` 可以解构 `Option`、`Result` 或自定义枚举，也可以通过 `else if let` 连续判断其他模式。只需判断是否存在而不使用值时，可考虑 `is_some()`。

## `let else`

失败分支必须发散，例如 `return`；成功绑定的变量可在后续作用域中使用。

```rust
fn print_value(value: Option<i32>) {
    let Some(n) = value else {
        return;
    };
    println!("{n}");
}
```

## 选择方式

- 需要处理多个变体或利用穷尽检查：用 `match`。
- 只对一个模式执行操作：用 `if let`。
- 反复匹配直到失败：用 `while let`。
- 匹配失败即提前返回：用 `let else`。

上述语法及具体条件组合应以项目使用的 Rust 版本为准。
