Here are several practical Rust macro examples, starting from the most common and easiest (declarative macros with macro_rules!) up to a very simple procedural macro introduction.

### 1. Very simple declarative macro (macro_rules!)

```rust
macro_rules! say_hello {
    () => {
        println!("Hello, world! (from macro)");
    };
}

fn main() {
    say_hello!();           // expands to println!(...)
    say_hello!();           // you can call it many times
}
```

### 2. Macro with one argument (expression)

