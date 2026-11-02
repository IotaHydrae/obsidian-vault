# 用 Trait Bound 隔离设备操作

> 要在不连接硬件时测试业务逻辑，应让业务层依赖自己定义的操作接口，并提供真实实现与测试替身。

```rust
trait Gpio {
    type Error;
    fn set_led(&mut self, on: bool) -> Result<(), Self::Error>;
}

struct Controller<T: Gpio> { gpio: T }

impl<T: Gpio> Controller<T> {
    fn enable(&mut self) -> Result<(), T::Error> {
        self.gpio.set_led(true)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::convert::Infallible;

    #[derive(Default)]
    struct MockGpio { led_on: bool }
    impl Gpio for MockGpio {
        type Error = Infallible;
        fn set_led(&mut self, on: bool) -> Result<(), Self::Error> {
            self.led_on = on;
            Ok(())
        }
    }

    #[test]
    fn enable_turns_on_led() {
        let mut controller = Controller { gpio: MockGpio::default() };
        controller.enable().unwrap();
        assert!(controller.gpio.led_on);
    }
}
```

真实实现可以包装 `rusb::DeviceHandle<T>` 并调用相应 USB 控制传输；设备查找、协议字段和错误处理需要根据实际项目实现。`T: UsbContext` 只约束 USB 上下文类型；要替换设备操作，应在自己的接口上实现 mock。
