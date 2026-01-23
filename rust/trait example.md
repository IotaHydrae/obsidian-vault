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