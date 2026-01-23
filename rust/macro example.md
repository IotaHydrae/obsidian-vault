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

```rust
macro_rules! square {
    ($x:expr) => {
        $x * $x
    };
}

fn main() {
    let a = square!(8);          // → 8 * 8
    let b = square!(2 + 3);      // → (2 + 3) * (2 + 3) = 25
    println!("{} {}", a, b);     // 64 25
}
```

### 3. Macro with multiple arms (like match)

```rust
macro_rules! log {
    (info: $msg:expr) => {
        println!("[INFO] {}", $msg);
    };
    (warn: $msg:expr) => {
        println!("[WARN] {}", $msg);
    };
    (error: $msg:expr $(, $extra:expr)?) => {{
        eprintln!("[ERROR] {}", $msg);
        $($ crate::log!(warn: $extra); )?   // optional extra warning
    }};
}

fn main() {
    log!(info: "Starting server");
    log!(warn: "Low memory");
    log!(error: "Failed to connect", "Check network");
}
```

### 4. Popular pattern: vec![] like macro (repeating elements)

```rust
macro_rules! my_vec {
    // Empty vec
    () => {
        Vec::new()
    };
    
    // Single element
    ($elem:expr) => {
        vec![$elem]
    };
    
    // Multiple elements with comma separator
    ($($elem:expr),+ $(,)?) => {{
        let mut v = Vec::new();
        $( v.push($elem); )*
        v
    }};
}

fn main() {
    let v1: Vec<i32> = my_vec!();
    let v2 = my_vec![42];
    let v3 = my_vec![1, 2, 3, 4];
    let v4 = my_vec![true, false, 1 > 0,];   // trailing comma ok

    println!("{:?} {:?} {:?}", v1, v2, v3);   // [] [42] [1, 2, 3, 4]
}
```

### 5. Macro that creates a struct + impl (mini derive-like)

```rust
macro_rules! make_counter {
    ($name:ident) => {
        struct $name {
            count: u64,
        }

        impl $name {
            fn new() -> Self {
                Self { count: 0 }
            }

            fn increment(&mut self) {
                self.count += 1;
            }

            fn get(&self) -> u64 {
                self.count
            }
        }
    };
}

make_counter!(VisitorCounter);

fn main() {
    let mut counter = VisitorCounter::new();
    counter.increment();
    counter.increment();
    println!("Visitors: {}", counter.get());  // Visitors: 2
}
```

### 6. Very minimal procedural macro example (function-like)

Create a new crate with:

```shell
cargo new --lib my_macros
cd my_macros
```

Edit `Cargo.toml`:

```toml
[lib]
proc-macro = true

[dependencies]
quote = "1.0"
syn = "2.0"
proc-macro2 = "1.0"
```

`src/lib.rs`

```rust
use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, LitStr};

#[proc_macro]
pub fn shout(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as LitStr);
    let text = input.value().to_uppercase();

    quote! {
        println!("!!! {} !!!", #text);
    }.into()
}
```

In another crate (or same workspace):

```rust
use my_macros::shout;

fn main() {
    shout!("hello everyone");
    // expands roughly to:
    // println!("!!! HELLO EVERYONE !!!");
}
```

### Quick comparison table (2025–2026 perspective)

|Kind|Syntax|Power / Flexibility|Compile speed|Common use cases|Learning curve|
|---|---|---|---|---|---|
|Declarative|`macro_rules!`|Medium|Very fast|vec!, println!-like, small DSLs, repetition|★★☆☆☆|
|Derive macro|`#[derive(MyTrait)]`|High|Medium|Clone, Debug, Serialize, Builder patterns|★★★★☆|
|Attribute macro|`#[my_attr]`|Very high|Slow|tracing::instrument, sqlx::query!|★★★★★|
|Function-like proc|`my_macro!(…)`|Very high|Slow|custom syntax, mini-languages|★★★★★|