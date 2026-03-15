```rust
let s = String::from("hello");
println!("{}", s.len()); // 自动解引用，相当于 (*s).len()
```