# 为 PT32L007x 生成 Rust PAC

> 从 CMSIS-Pack 取得 SVD，再用 chiptool 生成寄存器访问代码；生成成功后仍需核对寄存器描述、链接脚本和目标构建。

## 芯片与输入

PT32L007F8P7K 使用 Cortex-M0，最高 64 MHz，配备 64 KiB Flash 和 16 KiB SRAM。SRAM 地址示例为 `0x20000000`；Flash 基址及启动映射应按数据手册和链接脚本核对，不由 PAC 生成过程决定。

SVD 可从 [PAI-IC.PT32x007x_DFP.1.6.0_1.pack](https://img.pai-ic.com/files/PAI-IC.PT32x007x_DFP.1.6.0_1.pack) 提取，示例文件名为 `PT32L007x.svd`。PAC 提供底层寄存器接口，并不保证任意寄存器访问顺序或硬件操作都安全。

## 工具与生成

```bash
rustup target add thumbv6m-none-eabi
cargo install --git https://github.com/embassy-rs/chiptool --locked
cargo install form

chiptool generate --svd PT32L007x.svd
form -i lib.rs -o src/
find src -name '*.rs' -exec rustfmt {} +
```

在独立工作目录执行，避免覆盖已有源码。需要可复现生成时，固定 chiptool commit、form 版本和 SVD 校验值；验证拆分输出完整后再决定是否删除中间的 `lib.rs`。

## Crate 配置示例

以下依赖来自此生成方案，须与实际生成代码和锁文件匹配：

```toml
[dependencies]
cortex-m = "0.7"
cortex-m-rt = { version = "0.7.5", optional = true }
vcell = "0.1"
defmt = { version = "1.0.1", optional = true }

[features]
rt = ["cortex-m-rt/device"]
defmt = ["dep:defmt"]
```

构建检查可从 `cargo check --target thumbv6m-none-eabi` 开始，再核对向量表、memory.x、寄存器偏移及读写语义；编译通过不代表 SVD 或硬件访问正确。

## 参考

- [pt32l007x-blink 使用示例](https://github.com/IotaHydrae/pt32l007x-blink)
- [PT32L007x 数据手册](https://img.pai-ic.com/files/076f326e480bb2690753cb613b47ee8e.pdf)
- [PT32x007x 参考手册](https://img.pai-ic.com/files/9a634c19943beab82df2438615b6ad42.pdf)
