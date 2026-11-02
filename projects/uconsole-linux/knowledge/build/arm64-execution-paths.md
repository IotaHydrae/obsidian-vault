# ARM64 rootfs：原生、qemu-user 与完整虚拟机

> 选择执行路径要按实际安装工作负载测量；可用核心数不等于 APT 能获得的并行收益。

## TL;DR

- ARM64 原生执行、x86 的 qemu-user/binfmt 和 ARM64 qemu-system TCG 是不同路径。
- qemu-user 可以运行多个宿主线程；“只用单核”不是换完整虚拟机的可靠理由。
- x86 KVM 不能直接加速 ARM64 指令，完整 ARM64 VM 仍需 TCG。
- 本仓库 Ubuntu 路径支持原生 chroot 或已注册的 binfmt；没有内置完整 VM 构建后端。

## 执行模型

| 路径 | 执行什么 | 额外依赖 |
| --- | --- | --- |
| ARM64 原生 chroot | 目标用户程序直接使用宿主 ARM64 内核 | 同架构、权限、隔离挂载 |
| qemu-user/binfmt | 翻译用户态指令并转交宿主系统调用 | 注册匹配解释器、目标动态链接器与库 |
| qemu-system TCG | 目标 CPU、内核、设备与用户态 | 来宾内核、虚拟磁盘、网络与启动流程 |

user-mode 的多线程可对应多个宿主线程；安装器仍可能在包配置、脚本或 triggers 上串行。
system-mode 的多个 vCPU 不保证比 user-mode 更快，模拟设备与来宾内核也增加工作。
这些是执行机制说明，不包含对 CM5 或当前主机的性能承诺。

## chroot 预检

先确认目标是 aarch64 ELF，binfmt 已注册且带 F 标志；解释器路径还必须在当前挂载命名空间可用。
`Exec format error`、动态加载器缺失和用户 shell 缺库是不同错误，不用重装任意包掩盖。
构建器在隔离 chroot 执行 `/bin/true` 只验证基本执行链，不能证明全部 postinst 正常。
不要绑定整个宿主 `/run` 或未经约束的 `/dev` 以图方便；设备节点与挂载由执行环境拥有。

## 有效的速度比较

固定同一 Base、离线包集及摘要、APT 安装计划、CPU affinity、内存和存储条件。
分别记录解包、安装/配置、locale、收尾；不要将 VM 启动与热缓存安装混为一项。
每轮使用新 rootfs 或声明可复用状态，排除在线下载波动。

成功样本至少满足退出成功、dpkg 无未配置包、实际包名/版本/架构一致，
并验证代表性的目标运行路径。短 CPU 循环可说明计算吞吐，不等于完整 rootfs 提速。
跨平台的历史倍率不迁入为本仓库事实；实际结果在相同输入下重新测量。

## 依据与后续

Ubuntu 主机要求（工程源码：`image/ubuntu-base/README.md`）给出本仓库依赖。
机制参考 [QEMU user-mode](https://www.qemu.org/docs/master/user/main.html)与
[MTTCG](https://www.qemu.org/docs/master/devel/multi-thread-tcg.html)。
选择新执行后端前，先比较安装关键路径；不要为减少几条包装命令引入整套 VM 管理。
