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