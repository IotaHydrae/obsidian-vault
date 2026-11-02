# Rust Trait 定义、约束和动态分发

> Trait 定义共享行为；泛型约束适合编译期确定类型，`dyn Trait` 适合通过统一接口处理不同类型。

```rust
trait Describable {
    fn describe(&self) -> String;
    fn summary(&self) -> String {
        format!("[Summary] {}", self.describe())
    }
}

struct Animal { name: String }
struct Plant { name: String }

impl Describable for Animal {
    fn describe(&self) -> String { format!("Animal {}", self.name) }
}
impl Describable for Plant {
    fn describe(&self) -> String { format!("Plant {}", self.name) }
}

fn print_one<T: Describable>(item: &T) {
    println!("{}", item.describe());
}

fn print_all(items: &[&dyn Describable]) {
    for item in items { println!("{}", item.summary()); }
}

fn main() {
    let animal = Animal { name: "Buddy".into() };
    let plant = Plant { name: "Oak".into() };
    print_one(&animal);
    print_all(&[&animal, &plant]);
}
```

- 实现者必须提供无默认实现的方法；有默认实现的方法可以覆盖。
- `T: A + B` 或 `where T: A + B` 可组合多个约束。每次泛型实例化的 `T` 是一个具体类型。
- `&dyn Describable` 可以借用不同的实现类型，但 trait 必须满足 dyn 兼容性要求。所有权和线程安全的选择见 [[dyn]]。
- 不要把静态分发一定更快、动态分发一定更小当作保证；按具体程序测量。
