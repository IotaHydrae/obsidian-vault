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