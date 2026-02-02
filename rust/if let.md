Rust 中的 if let 用法 是非常常用且实用的语法糖，主要用来只关心某一种模式匹配成功的情况，而不需要写完整的 match。

基本形式

```rust
if let 模式 = 表达式 {
    // 匹配成功时执行这里的代码
    // 模式中绑定的变量可以在 {} 里使用
}
```

最经典的例子（处理 Option）：

```rust
let maybe_number = Some(7);

// 传统 match 写法（啰嗦）
match maybe_number {
    Some(n) => println!("得到了数字：{}", n),
    None    => {}   // 什么都不做
}

// 用 if let 简化（推荐写法）
if let Some(n) = maybe_number {
    println!("得到了数字：{}", n);
}
```

带 else 的版本（Rust 1.65+ 推荐）

```rust
let maybe_number = Some(42);

if let Some(n) = maybe_number {
    println!("有值：{}", n);
} else {
    println!("是 None");
}
```

更现代的写法（2024–2025 年后非常流行）是用 let else：

```rust
let Some(n) = maybe_number else {
    println!("是 None，提前返回或处理");
    return;
};
// 到这里一定有值，n 已解构出来
println!("值是：{}", n);
```

### 常见使用场景举例

1. **Option 解包**（最常见）

```rust
fn print_length(s: Option<&str>) {
    if let Some(text) = s {
        println!("长度：{}", text.len());
    } else {
        println!("没有字符串");
    }
}
```

2. Result 简单处理

```rust
let result: Result<i32, String> = "123".parse();

if let Ok(number) = result {
    println!("解析成功：{}", number);
} else {
    println!("解析失败");
}
```

3. 枚举单变体匹配

```rust
enum Command {
    Quit,
    Move { x: i32, y: i32 },
    Write(String),
}

let cmd = Command::Write(String::from("hello"));

// 只关心 Write 情况
if let Command::Write(text) = cmd {
    println!("要写入：{}", text);
}
```

4. 多条件 + else if let（可以链式使用）

```rust
let value = Some(3);

if let Some(1) = value {
    println!("是 1");
} else if let Some(3) = value {
    println!("是 3！");
} else {
    println!("其他值或 None");
}
```

### if let vs match 对比表

|场景|推荐写法|理由|
|---|---|---|
|只关心 1 种模式|`if let`|简洁、不用写 `_ => {}`|
|关心 2 种及以上模式|`match`|可读性更好，强制穷尽检查|
|一定要处理失败情况且提前返回|`let else`|代码更平、不用嵌套（2022年后主流）|
|只要判断是否存在（不关心值）|`.is_some()`|更直观|
|复杂模式 + 守卫|`match`|`if let` 也能写守卫但较少用|
### 小技巧总结（2025–2026 常用写法）

```rust
// 1. 最推荐的现代写法（提前返回风格）
let Some(age) = get_user_age() else {
    println!("用户年龄不存在");
    return;
};
println!("用户今年 {} 岁", age);

// 2. 快速过滤
if let Ok(user) = db.find_user(id) {
    // 使用 user
}

// 3. 配合 while let（循环解包）
while let Some(line) = lines.next() {
    println!("{}", line);
}
```

一句话总结：

**只想处理某一种成功情况 → 用 if let** **需要处理多种情况或强制穷尽 → 用 match** **想写平的代码且提前返回 → 用 let else**