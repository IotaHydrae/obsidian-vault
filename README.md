# 知识库

按主题存放技术笔记、开发记录和生活资料。

## 目录

```text
ai/                  AI 工具和应用
assets/              笔记引用的本地附件
cookbook/            菜谱
cs/                  计算机基础
dev-note/
  boards/            开发板实践和调试记录
  linux/             Linux 系统配置与故障排查
  tools/             开发工具、服务和使用记录
embedded/
  arch/              处理器架构
  hardware/          硬件、开发板和 PCB
  linux/             内核、驱动、设备树和用户空间
  pico/              Raspberry Pi Pico
  rootfs/            根文件系统
  uboot/             U-Boot
projects/
  rk3588-ubuntu/     RK3588 Ubuntu 镜像工程知识库
rust/
  embedded/          嵌入式 Rust
  macro/             宏与示例
  trait/             Trait 和动态分发
  泛型/              泛型和约束
  语法糖/            常用语法
```

## 笔记入口

- [RK3588 Ubuntu 镜像知识库](projects/rk3588-ubuntu/README.md)
- [Linux 内核地址](embedded/linux/kernel/memory/Kernel%20Address%20-%20Virtual%20and%20Physical.md)
- [USB 设备的枚举过程](embedded/linux/drivers/usb/USB%20设备的枚举过程.md)
- [MIPI-DSI 协议](embedded/hardware/MIPI-DSI%20协议.md)
- [Rust 泛型](rust/泛型/0.%20泛型的定义.md)
- [Rust 宏](rust/macro/0.%20Rust%20主要的几种宏.md)
- [树莓派 5 MIPI-DSI 实践](dev-note/boards/在树莓派5上驱动一块%20mipi-dsi%20屏幕的记录.md)
- [Linux 系统实时性优化](dev-note/linux/Linux%20系统实时性优化.md)
- [Gitea 安装与使用](dev-note/tools/gitea%20安装与使用.md)
- [Ollama](ai/ollama.md)
- [可乐鸡翅](cookbook/荤菜/可乐鸡翅.md)

## 整理约定

- 通用技术知识放在对应主题下；具体开发板的实践过程放在 `dev-note/boards/`。
- 本地附件集中放在 `assets/`。如需让新附件自动进入该目录，可在 Obsidian 的“文件与链接”设置中指定默认附件位置。
- `.obsidian/` 当前未纳入 Git，Obsidian 的本地设置需要在各设备分别配置。
