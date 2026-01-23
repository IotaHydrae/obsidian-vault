```rust
use rusb::{Context, DeviceHandle, UsbContext, Result as RusbResult};

// ────────────────────────────────────────────────
// 我们的泛型 GPIO 封装（来自你之前的代码）
pub struct UsbGpio<T: UsbContext> {
    handle: DeviceHandle<T>,
}

impl<T: UsbContext> UsbGpio<T> {
    pub fn new(handle: DeviceHandle<T>) -> Self {
        UsbGpio { handle }
    }

    // 模拟的控制方法（实际项目中会调用 control_transfer）
    pub fn set_led(&self, on: bool) -> RusbResult<()> {
        let value = if on { 1u16 } else { 0u16 };
        println!("模拟：向设备发送 LED = {}", value);
        // 实际写法示例（注释掉）：
        // self.handle.write_control(
        //     rusb::request_type(Direction::Out, RequestType::Vendor, Recipient::Device),
        //     0xA0,           // bRequest (自定义请求)
        //     value,          // wValue
        //     0,              // wIndex
        //     &[],            // 数据（这里无数据）
        //     100,            // timeout ms
        // )?;
        Ok(())
    }

    pub fn read_button(&self) -> RusbResult<bool> {
        println!("模拟：读取按钮状态");
        // 实际中可能是：
        // let mut buf = [0u8; 1];
        // self.handle.read_control(...)?
        Ok(true) // 模拟按下
    }
}

// ────────────────────────────────────────────────
// 主函数示例：使用默认的 rusb::Context
fn main() -> RusbResult<()> {
    // 1. 创建 USB 上下文（实现了 UsbContext trait）
    let ctx = Context::new()?;

    // 2. 这里假设我们已经找到并打开了目标设备
    //    （实际项目中需要枚举设备、匹配 VID:PID 等）
    let devices = ctx.devices()?;
    
    // 为了演示，我们不真的打开设备，只展示类型使用方式
    // let device = devices.iter().find(|d| /* 匹配 VID PID */).unwrap();
    // let handle = device.open()?;

    // 模拟：假装我们有 handle 了
    // let gpio = UsbGpio::new(handle);

    println!("泛型 UsbGpio<T: UsbContext> 的典型使用方式：");
    println!("T 的具体类型通常就是 rusb::Context\n");

    // 说明：下面代码无法直接运行（因为没有真实设备），但类型上是正确的
    // let gpio: UsbGpio<Context> = UsbGpio::new(handle);

    // 更常见的写法（让编译器推导 T）
    // let gpio = UsbGpio::new(handle);

    // 使用示例调用
    // gpio.set_led(true)?;
    // let pressed = gpio.read_button()?;

    Ok(())
}

// ────────────────────────────────────────────────
// 进阶例子：如果将来有人实现了自己的 MockUsbContext 用于测试

#[cfg(test)]
mod tests {
    use super::*;
    use rusb::UsbContext;

    // 假的上下文，用于单元测试（不依赖真实 USB 硬件）
    struct MockUsbContext;
    
    impl UsbContext for MockUsbContext {
        // 必须实现 trait 的方法（这里简化，只为通过编译）
        fn devices(&self) -> RusbResult<rusb::DeviceList<Self>> {
            unimplemented!()
        }
        // ... 其他方法也需要实现或用 default
    }

    // 假的 handle（实际测试中可以用 mock 对象库更好）
    struct MockHandle;
    
    impl<T: UsbContext> From<MockHandle> for DeviceHandle<T> {
        fn from(_: MockHandle) -> Self {
            unimplemented!()
        }
    }

    #[test]
    fn can_use_with_mock_context() {
        // 假设我们有 mock handle
        // let handle: DeviceHandle<MockUsbContext> = MockHandle.into();
        // let gpio = UsbGpio::new(handle);
        
        // gpio.set_led(true).unwrap();
        println!("编译通过即代表 trait bound 生效");
    }
}
```

### 关键点总结

|写法|含义 / 作用|
|---|---|
|`struct UsbGpio<T: UsbContext>`|T 必须是实现了 `UsbContext` 的类型|
|`impl<T: UsbContext> UsbGpio<T>`|为所有满足约束的 T 实现方法|
|`UsbGpio<Context>`|最常见的具体类型（T = rusb::Context）|
|`let gpio = UsbGpio::new(handle);`|编译器自动推导 T（通常不需要显式写 `<Context>`）|
|测试时代可以用自定义类型|只要实现 `UsbContext` trait，就能用同样的 `UsbGpio`|
这样写的好处是：

- **生产代码** 用 `rusb::Context`
- **单元测试** 可以换成 mock 上下文（不插 USB 线也能测逻辑）
- **将来** 如果出现别的 USB 库，也有可能复用这套接口（只要它实现了 `UsbContext`）