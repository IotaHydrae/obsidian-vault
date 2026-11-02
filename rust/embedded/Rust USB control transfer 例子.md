# Rust `rusb` USB 控制传输示例

> 使用 `rusb::DeviceHandle` 发送 vendor control request；必须传播传输错误并检查读取长度。

示例协议使用 VID:PID `1209:c303`、请求 `0x00` 和索引 `0x106` / `0x107` 控制 GPIO。这些字段、引脚编号和数据编码属于特定固件；使用前核对设备协议及 `rusb` 版本。

依赖示例为 `rusb = "0.9.4"`，接口见 [rusb 0.9.4 UsbContext](https://docs.rs/rusb/0.9.4/rusb/trait.UsbContext.html)。

```rust
use rusb::{DeviceHandle, Direction, Recipient, RequestType, UsbContext};
use std::time::Duration;

const TIMEOUT: Duration = Duration::from_millis(100);

struct UsbGpio<T: UsbContext> {
    handle: DeviceHandle<T>,
}

impl<T: UsbContext> UsbGpio<T> {
    fn open(context: &T, vid: u16, pid: u16) -> rusb::Result<Self> {
        let handle = context.open_device_with_vid_pid(vid, pid)
            .ok_or(rusb::Error::NoDevice)?;
        match handle.kernel_driver_active(0) {
            Ok(true) => handle.detach_kernel_driver(0)?,
            Ok(false) | Err(rusb::Error::NotSupported) => {},
            Err(error) => return Err(error),
        }
        Ok(Self { handle })
    }

    fn set_gpio(&self, pin: u8, state: u8) -> rusb::Result<()> {
        let request_type = rusb::request_type(Direction::Out, RequestType::Vendor, Recipient::Device);
        self.handle.write_control(request_type, 0x00,
            u16::from_le_bytes([state, pin]), 0x106, &[], TIMEOUT)?;
        Ok(())
    }

    fn get_gpio(&self, pin: u8) -> rusb::Result<u8> {
        let request_type = rusb::request_type(Direction::In, RequestType::Vendor, Recipient::Device);
        let mut data = [0u8; 1];
        let received = self.handle.read_control(request_type, 0x00,
            u16::from_le_bytes([0, pin]), 0x107, &mut data, TIMEOUT)?;
        if received != data.len() { return Err(rusb::Error::Io); }
        Ok(data[0])
    }
}

fn main() -> rusb::Result<()> {
    let context = rusb::Context::new()?;
    let gpio = UsbGpio::open(&context, 0x1209, 0xc303)?;
    const PC0: u8 = 32;
    gpio.set_gpio(PC0, 1)?;
    println!("GPIO {PC0}: {}", gpio.get_gpio(PC0)?);
    Ok(())
}
```

分离内核驱动会影响其他程序或设备功能，仅在确认接口和平台要求时使用；实际项目可能还需要声明接口、恢复驱动及配置设备访问权限。`open_device_with_vid_pid` 返回 `None` 时不能区分未找到和打开失败；精确诊断需枚举设备并分别处理打开错误。
