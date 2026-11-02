# F1C100s：从 SPI-NAND 读取内核与设备树

> 在已运行的 U-Boot 中，用 MTD 命令将 DTB 和 zImage 读入互不重叠的 RAM 区域，再用 `bootz` 启动。

## 读取与启动

以下地址对应此镜像布局，不是芯片固定地址。运行前确认 Flash 偏移、文件大小、DRAM 范围以及解压和搬移后的内存范围。

```text
mtd read spi-nand0 0x80C00000 0x100000 0x4000
mtd read spi-nand0 0x80008000 0x120000 0x500000
bootz 0x80008000 - 0x80C00000
```

| 内容 | Flash 偏移 | 读取长度 | RAM 地址 |
| --- | --- | --- | --- |
| DTB | `0x100000` | `0x4000`，16 KiB | `0x80C00000` |
| zImage | `0x120000` | `0x500000`，5 MiB | `0x80008000` |

`bootz` 的 `-` 表示没有提供 initrd。读取失败时不要继续启动；设置自动命令前先分别验证这些步骤。构建时的默认命令可配置 `CONFIG_BOOTCOMMAND`，已保存的环境变量是否覆盖默认值取决于该版本的环境机制。

## 启动证据

一次实验使用 Linux `6.13.2-tasks-todo+`、ARM926EJ-S，出现以下输出：

```text
Reading 16384 byte(s) (8 page(s)) at offset 0x00100000
Reading 5242880 byte(s) (2560 page(s)) at offset 0x00120000
Kernel image @ 0x80008000
Starting kernel ...
```

日志只能证明相应阶段已执行，不能单独证明根文件系统、应用和显示均正常。进一步核对 `bootargs`、MTD/UBI 附加结果及最终启动状态，见 [[bootargs]]。
