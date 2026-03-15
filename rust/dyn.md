# `&dyn Animal` 详解
## 语法组成

`\text{&dyn Animal}`

| 部分       | 含义                                        |
| -------- | ----------------------------------------- |
| `&`      | **引用符号** - 表示这是一个借用的引用，不转移所有权             |
| `dyn`    | **动态** - 表示这是动态分发，类型在运行时确定                |
| `Animal` | **Trait 名称** - 指向实现了 `Animal` trait 的任何类型 |
## 具体含义

**`&dyn Animal` 是一个 Trait 对象引用**，它表示：

- ✅ 指向**任何实现了 `Animal` trait 的类型**
- ✅ 可以是 `Dog`、`Cat` 或其他实现 `Animal` 的类型
- ✅ 具体是哪个类型在**运行时**才能确定
- ✅ 通过虚函数表（vtable）进行动态分发

## 关键区别

```rust
fn call_static_dog(animal: &Dog)
fn call_dynamic(animal: &dyn Animal)
```

| 特性        | `&Dog` | `&dyn Animal` |
| --------- | ------ | ------------- |
| 类型确定      | 编译时    | 运行时           |
| 分发方式      | 静态分发   | 动态分发          |
| 能存储多个类型吗？ | ❌ 不能   | ✅ 能           |
| 性能        | 更快     | 略慢（虚函数调用）     |
| 灵活性       | 低      | 高             |

## Example

```rust
trait Animal {
    fn speak(&self);
}

struct Dog;
struct Cat;

impl Animal for Dog {
    fn speak(&self) {
        println!("汪汪！");
    }
}

impl Animal for Cat {
    fn speak(&self) {
        println!("喵喵！");
    }
}

fn main() {
    let dog = Dog;
    let cat = Cat;

    // 方式1: 静态分发 - 编译时确定类型
    // 编译器会为每个类型生成一份函数副本
    fn call_static_dog(animal: &Dog) {
        animal.speak();
    }
    call_static_dog(&dog);

    // 方式2: 动态分发 - 运行时确定类型
    // 使用 &dyn Animal，可以接收任何实现 Animal 的类型
    fn call_dynamic(animal: &dyn Animal) {
        animal.speak();
    }

    call_dynamic(&dog);  // 传入 Dog
    call_dynamic(&cat);  // 传入 Cat - 同一个函数！

    // 方式3: 使用 trait 对象存储不同类型
    let animals: Vec<&dyn Animal> = vec![&dog, &cat];
    
    for animal in animals {
        animal.speak();
    }
}
```