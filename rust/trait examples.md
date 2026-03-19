
## Trait example 1

### 1. Basic Trait + Implementation (like an interface)

```rust
// Define a trait (shared behavior)
trait Describable {
    fn describe(&self) -> String;           // required method
    fn default_greet(&self) -> String {     // provided default implementation
        "Hello from something describable!".to_string()
    }
}

// Struct that will implement the trait
struct Person {
    name: String,
    age: u32,
}

// Implement the trait for Person
impl Describable for Person {
    fn describe(&self) -> String {
        format!("{} is {} years old", self.name, self.age)
    }

    // We can override the default if we want
    // fn default_greet(&self) -> String { ... }
}

fn main() {
    let p = Person {
        name: "Wooden".to_string(),
        age: 30,
    };

    println!("{}", p.describe());           // "Woodan is 30 years old"
    println!("{}", p.default_greet());      // uses default impl
}
```

### 2. Trait Bounds on Generics (most common & performant pattern)

```rust
trait Speak {
    fn speak(&self) -> &'static str;
}

// Generic function that requires T to implement Speak
fn make_sound<T: Speak>(animal: &T) {
    println!("It says: {}", animal.speak());
}

// Two types implementing the same trait
struct Dog;
impl Speak for Dog {
    fn speak(&self) -> &'static str { "Woof!" }
}

struct Cat;
impl Speak for Cat {
    fn speak(&self) -> &'static str { "Meow!" }
}

fn main() {
    let dog = Dog;
    let cat = Cat;

    make_sound(&dog);   // Woof!
    make_sound(&cat);   // Meow!

    // This would **not** compile:
    // make_sound(&42);   // error: 42 does not implement Speak
}
```

### 3. Trait + Generic Struct

```rust
use std::fmt::Debug;

// Our "context" trait (like UsbContext in rusb)
trait Connection: Debug {
    fn send(&self, msg: &str);
    fn receive(&self) -> String;
}

// Generic struct that works with **any** type that implements Connection
#[derive(Debug)]
struct Device<T: Connection> {
    conn: T,
    name: String,
}

impl<T: Connection> Device<T> {
    fn new(name: &str, conn: T) -> Self {
        Self {
            conn,
            name: name.to_string(),
        }
    }

    fn ping(&self) {
        self.conn.send(&format!("PING from {}", self.name));
        let reply = self.conn.receive();
        println!("Reply: {}", reply);
    }
}
 
// Concrete implementation
#[derive(Debug)]
struct TcpConnection;

impl Connection for TcpConnection {
    fn send(&self, msg: &str) {
        println!("TCP send: {}", msg);
    }
    fn receive(&self) -> String {
        "PONG!".to_string()
    }
}

fn main() {
    let tcp = TcpConnection;
    let dev = Device::new("Sensor-1", tcp);
    dev.ping();
    // Output:
    // TCP send: PING from Sensor-1
    // Reply: PONG!
}
```

### 4. Trait Objects (dynamic dispatch – when you need heterogeneous collections)

```rust
trait Drawable {
    fn draw(&self);
}

// Note: object-safe trait (no generics, no Self returns in methods, etc.)
struct Circle;
impl Drawable for Circle {
    fn draw(&self) { println!("○"); }
}

struct Square;
impl Drawable for Square {
    fn draw(&self) { println!("■"); }
}

fn main() {
    // Vec of different concrete types → need trait object
    let shapes: Vec<Box<dyn Drawable>> = vec![
        Box::new(Circle),
        Box::new(Square),
    ];

    for shape in shapes {
        shape.draw();   // dynamic dispatch (vtable lookup at runtime)
    }
    // ○
    // ■
}
```

### Quick Comparison Table: Generics vs Trait Objects

| Feature                   | Generics (`<T: Trait>`)                  | Trait Objects (`dyn Trait`)                   |
| ------------------------- | ---------------------------------------- | --------------------------------------------- |
| Dispatch                  | Static (monomorphization)                | Dynamic (vtable)                              |
| Performance               | Usually faster (inlining possible)       | Small overhead (extra pointer + vtable)       |
| Binary size               | Larger (code duplicated per type)        | Smaller                                       |
| Heterogeneous collections | No (all elements same concrete type)     | Yes (Vec<Box<dyn Trait>> etc.)                |
| Use case                  | Most library APIs, zero-cost abstraction | GUI widgets, plugins, trait-object heavy code |
| Object safety required?   | No                                       | Yes (many restrictions)                       |

## Trait example 2

```rust
// 定义一个 trait：可描述的对象
trait Describable {
    // 必须实现的方法
    fn describe(&self) -> String;

    // 带默认实现的方法
    fn summary(&self) -> String {
        format!("[Summary] {}", self.describe())
    }
}

// 定义一个 trait：可比较大小
trait Comparable {
    fn compare(&self, other: &Self) -> std::cmp::Ordering;
}

// 结构体：动物
struct Animal {
    name: String,
    sound: String,
}

// 结构体：植物
struct Plant {
    name: String,
    height_cm: f64,
}

// 为 Animal 实现 Describable
impl Describable for Animal {
    fn describe(&self) -> String {
        format!("Animal '{}' says '{}'", self.name, self.sound)
    }
}

// 为 Plant 实现 Describable
impl Describable for Plant {
    fn describe(&self) -> String {
        format!("Plant '{}' grows to {}cm", self.name, self.height_cm)
    }
}

// 为 Plant 实现 Comparable
impl Comparable for Plant {
    fn compare(&self, other: &Self) -> std::cmp::Ordering {
        self.height_cm.partial_cmp(&other.height_cm).unwrap()
    }
}

// 使用 trait bound 的泛型函数
fn print_description<T: Describable>(item: &T) {
    println!("{}", item.describe());
}

// 使用 dyn 的动态分发（trait object）
fn print_all(items: &[&dyn Describable]) {
    for item in items {
        println!("  - {}", item.summary());
    }
}

// 多重 trait bound
fn print_and_compare_plants<T>(a: &T, b: &T)
where
    T: Describable + Comparable,
{
    println!("Comparing: {} vs {}", a.describe(), b.describe());
    let ordering = a.compare(b);
    match ordering {
        std::cmp::Ordering::Less    => println!("  -> first is smaller"),
        std::cmp::Ordering::Greater => println!("  -> first is larger"),
        std::cmp::Ordering::Equal   => println!("  -> they are equal"),
    }
}

fn main() {
    let dog = Animal {
        name: "Buddy".into(),
        sound: "Woof!".into(),
    };

    let tree = Plant {
        name: "Oak".into(),
        height_cm: 300.0,
    };

    let flower = Plant {
        name: "Daisy".into(),
        height_cm: 30.0,
    };

    // 1. 基本 trait 调用
    println!("=== 基本 describe ===");
    print_description(&dog);
    print_description(&tree);

    // 2. 默认方法 summary
    println!("\n=== 默认方法 summary ===");
    println!("{}", dog.summary());

    // 3. 动态分发（trait object）
    println!("\n=== 动态分发 print_all ===");
    let items: Vec<&dyn Describable> = vec![&dog, &tree, &flower];
    print_all(&items);

    // 4. 多重 trait bound + 比较
    println!("\n=== 多重 trait bound ===");
    print_and_compare_plants(&tree, &flower);
}
```

### 运行输出

```text
=== 基本 describe ===
Animal 'Buddy' says 'Woof!'
Plant 'Oak' grows to 300cm

=== 默认方法 summary ===
[Summary] Animal 'Buddy' says 'Woof!'

=== 动态分发 print_all ===
  - [Summary] Animal 'Buddy' says 'Woof!'
  - [Summary] Plant 'Oak' grows to 300cm
  - [Summary] Plant 'Daisy' grows to 30cm

=== 多重 trait bound ===
Comparing: Plant 'Oak' grows to 300cm vs Plant 'Daisy' grows to 30cm
  -> first is larger
```

### 要点总结

| 概念                   | 说明                                        |
| -------------------- | ----------------------------------------- |
| **trait 定义**         | 类似接口，定义共享行为的契约                            |
| **默认方法**             | trait 中可提供默认实现，实现者可选择覆盖                   |
| **泛型 + trait bound** | `fn foo<T: Trait>(x: &T)` — 编译期单态化，零运行时开销 |
| **dyn Trait**        | `&dyn Trait` — 运行时动态分发，通过 vtable 调用       |
| **多重 bound**         | `where T: A + B` — 约束类型同时满足多个 trait       |
