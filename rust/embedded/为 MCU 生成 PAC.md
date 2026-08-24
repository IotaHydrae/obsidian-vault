# PAC（Peripheral Access Crate）for PT32L007x MCUs

> What is PAC?
>
> PAC stands for Peripheral Access Crate. A safe, low-level register access library automatically generated for a specific MCU. It is the cornerstone of the entire embedded Rust ecosystem, with almost all higher-level HALs, drivers, and frameworks (Embassy, RTIC, etc.) built on top of a certain PAC.. It is generated from the microcontroller's CMSIS-SVD format XML file usually called `PT32L007x.svd` for example.

The PT32L007x series utilizes a high-performance, low-power Cortex™-M0 32-bit core operating at 64MHz. It features integrated high-speed memory (up to 64KB of flash memory and up to 16KB of SRAM), versatile multiplexed I/O ports, and a rich set of peripherals connected to the APB bus. All models include a 12-bit ADC, an advanced timer, two basic 16-bit timers, and a low-power timer. Standard communication interfaces are also included: two UART interfaces, one I2C interface, and one SPI interface.

| Hardware Info |                   |
| ------------- | ----------------- |
| MCU           | PT32L007F8P7K     |
| CPU           | Cortex-M0 @ 64MHz |
| SRAM          | 16KB @ 0x20000000 |
| Flash         | 64KB @ 0x00000000 |

![img](./assets/system.png)

## A step by step guide how to generate PAC for PT32L007x

Install dependencies and tools:

```bash
rustup target add thumbv6m-none-eabi
cargo install --git https://github.com/embassy-rs/chiptool --locked
cargo install form
```

Generate PAC `lib.rs` through a svd file and split it into multiple files via `form`:

```bash
chiptool generate --svd PT32L007x.svd
form -i lib.rs -o src/
rm lib.rs
find . -name "*.rs" | xargs rustfmt
```

> A svd file could be found inside [PAI-IC.PT32x007x_DFP.1.6.0_1.pack](https://img.pai-ic.com/files/PAI-IC.PT32x007x_DFP.1.6.0_1.pack) A CMSIS-Pack.

Edit `Cargo.toml` add dependencies:

```rust
[dependencies]
cortex-m = "0.7"
cortex-m-rt = { version = "0.7.5", optional = true }
vcell = "0.1"
defmt = { version = "1.0.1", optional = true }

[features]
rt = ["cortex-m-rt/device"]
defmt = ["dep:defmt"]
```

Here is an example [pt32l007x-blink](https://github.com/IotaHydrae/pt32l007x-blink) showing how to use this PAC.

# Links

[PAI-IC.PT32x007x_DFP.1.6.0_1.pack](https://img.pai-ic.com/files/PAI-IC.PT32x007x_DFP.1.6.0_1.pack)

[PT32L007x 数据手册.pdf](https://img.pai-ic.com/files/076f326e480bb2690753cb613b47ee8e.pdf)

[PT32x007x 参考手册.pdf](https://img.pai-ic.com/files/9a634c19943beab82df2438615b6ad42.pdf)