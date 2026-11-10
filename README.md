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
  network/           网络分流、节点选择和路径诊断
  mobile/            Android、iOS 诊断与手机远程使用
  tools/             开发工具、服务和使用记录
embedded/
  arch/              处理器架构
  hardware/          硬件、开发板和 PCB
  linux/             内核、驱动、设备树和用户空间
  pico/              Raspberry Pi Pico
  rootfs/            根文件系统
  uboot/             U-Boot
projects/
  kindle-linux/      Kindle KT4、Alpine chroot 与 KOReader 应用
  rk3588-ubuntu/     RK3588 Ubuntu 镜像工程知识库
  uconsole-linux/    uConsole Linux、CM4/CM5 与硬件资料
rust/
  embedded/          嵌入式 Rust
  macro/             宏与示例
  trait/             Trait 和动态分发
  泛型/              泛型和约束
  语法糖/            常用语法
```

## 笔记入口

- [通用排障方法：网络、手机、接口与系统](dev-note/通用排障方法.md)
- [手机诊断与远程使用](dev-note/mobile/README.md)
- [Kindle Linux 工程知识库](projects/kindle-linux/README.md)
- [uConsole Linux 工程知识库](projects/uconsole-linux/README.md)
- [RK3588 Ubuntu 镜像知识库](projects/rk3588-ubuntu/README.md)
- [Linux 内核地址](embedded/linux/kernel/memory/Kernel%20Address%20-%20Virtual%20and%20Physical.md)
- [Linux 线程化中断：原理、触发路径与接口示例](embedded/linux/kernel/irq/threaded-irq.md)
- [Linux Kernel API 笔记索引](embedded/linux/kernel/kernel-api/README.md)
- [USB 2.0 设备的枚举过程](embedded/linux/drivers/usb/USB%202.0%20设备的枚举过程.md)
- [USB 2.0 设备的传输过程](embedded/linux/drivers/usb/USB%202.0%20设备的传输过程.md)
- [USB 3.0 设备的枚举过程](embedded/linux/drivers/usb/USB%203.0%20设备的枚举过程.md)
- [MIPI-DSI 协议](embedded/hardware/MIPI-DSI%20协议.md)
- [Rust 泛型](rust/泛型/0.%20泛型的定义.md)
- [Rust 宏](rust/macro/0.%20Rust%20主要的几种宏.md)
- [树莓派 5 MIPI-DSI 实践](dev-note/boards/在树莓派5上驱动一块%20mipi-dsi%20屏幕的记录.md)
- [Linux 系统实时性优化](dev-note/linux/Linux%20系统实时性优化.md)
- [Gitea 安装与使用](dev-note/tools/gitea%20安装与使用.md)
- [常用 Skills](ai/常用skills.md)
- [Ollama](ai/ollama.md)
- [可乐鸡翅](cookbook/荤菜/可乐鸡翅.md)

## 整理约定

- 通用知识提炼为机制、参数化示例、验证判据和适用范围，具体部署值按实际环境确定。
- 来源材料中，仅 RK3588 Ubuntu 镜像工程的对外副本要求脱敏；其他来源保留与知识有关的实际信息。
- 通用技术知识放在对应主题下；具体开发板的实践过程放在 `dev-note/boards/`。
- 本地附件集中放在 `assets/`。如需让新附件自动进入该目录，可在 Obsidian 的“文件与链接”设置中指定默认附件位置。
- `.obsidian/` 当前未纳入 Git，Obsidian 的本地设置需要在各设备分别配置。
