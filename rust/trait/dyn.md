# Rust `dyn Trait` 与 trait 对象

> `&dyn Trait` 借用实现了该 trait 的值，通过动态分发调用方法；`Box<dyn Trait>` 则持有值的所有权。

```rust
trait Animal {
    fn speak(&self) -> &'static str;
}

struct Dog;
struct Cat;
impl Animal for Dog { fn speak(&self) -> &'static str { "woof" } }
impl Animal for Cat { fn speak(&self) -> &'static str { "meow" } }

fn main() {
    let animals: Vec<Box<dyn Animal>> = vec![Box::new(Dog), Box::new(Cat)];
    assert_eq!(animals.iter().map(|a| a.speak()).collect::<Vec<_>>(),
               ["woof", "meow"]);
}
```

## 选择与限制

- `dyn Trait` 是动态大小类型，不能直接作为 `Vec` 元素；通过 `&dyn Trait`、`Box<dyn Trait>` 等指针存放。trait 本身还必须满足 dyn 兼容性要求。
- trait 对象指针带有数据指针及用于动态分发的元数据；不能假定 `Box<dyn Trait>` 是单个 8 字节指针，也不要依赖未承诺的具体布局。
- `Vec<&dyn Trait>` 借用已有值；`Vec<Box<dyn Trait>>` 拥有各值；`Rc` / `Arc` 可用于共享所有权。`Arc` 的引用计数是原子的，但跨线程使用还取决于内部类型及对象边界是否满足 `Send` / `Sync`，必要时写成 `Arc<dyn Trait + Send + Sync>`。
- 泛型 `T: Trait` 通常静态分发；`dyn Trait` 通常经 vtable 动态分发。优化、性能和代码体积需按实际程序测量。

泛型及 trait 的综合示例见 [[trait examples]]。
