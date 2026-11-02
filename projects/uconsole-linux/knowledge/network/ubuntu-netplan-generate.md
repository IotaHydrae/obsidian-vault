# Ubuntu：保留 Netplan 功能，缩短 NetworkManager 生成等待

> 通过 NM 专用 generate 入口减少 Python 导入成本，同时保留 Netplan YAML 语义。

## TL;DR

- 只向 NetworkManager 服务提供专用 PATH，不替换全局 netplan CLI。
- 固定并校验已审查源码；参数、环境或源码改变时委托原命令。
- 验证生成、保存、修改、删除及真实网络状态；收益不能与关闭全部集成混算。

## 使用与验证方法

1. 保留发行版 NetworkManager 与 Netplan 软件包，由 Ubuntu 后端安装专用入口和服务 drop-in。
2. 检查 NM 进程的实际 PATH 与子进程入口，确认源码保护有效；其他调用继续走原 CLI。
3. 用不自动连接、绑定不存在接口的临时连接验证保存、修改、生成、重载与删除生命周期。
4. 对照试验使用相同内核、网络配置和采样工具；调用耗时、服务区间、进程启动与物理首帧分别报告。

## 同步生成的原因与调用约定

审查对象是 Ubuntu `network-manager 1.46.0-1ubuntu2.8` 与
Netplan `1.1.2-8ubuntu1~24.04.3`。NM 的
`src/core/settings/plugins/keyfile/nms-keyfile-utils.c::generate_netplan()`
每次通过 `g_spawn_sync(..., envp=NULL, G_SPAWN_SEARCH_PATH, ...)` 同步执行
`netplan generate`，没有缓存绝对路径。初次加载、显式连接重载、保存和删除均会调用。

快速入口（工程源码：`image/ubuntu-base/nm-netplan-generate.py`）只接管精确的单参数
`generate`，完整执行原 CLI 的三步：

1. `/usr/libexec/netplan/generate` 生成后端配置。
2. `udevadm control --reload` 重载规则。
3. `systemctl daemon-reload --no-ask-password` 重载生成的 systemd 单元。

即使 generator 正常退出为非零，后两步仍执行；返回码仍是 generator 的结果。
udev 重载的普通非零退出被忽略，systemd 重载的普通非零退出打印警告。
执行异常如文件不存在会中止后续步骤。这些行为来自发行版
`netplan_cli/cli/commands/generate.py::command_generate()`，不能因为想省时间而删去。

入口校验七个已审查 CLI 路径的 SHA-256，包括真实执行入口；源码缺失或变化时，
直接委托绝对路径 `/usr/sbin/netplan`。其他参数以及存在 `SNAP`、
`NETPLAN_GENERATE_PATH`、`PYTHONPATH` 或 `PYTHONHOME` 时也委托原 CLI，
包括环境变量值为空的情况。它保持原 CLI 的 locale/PATH 默认值，并在分派前清除
自身的 PATH 前缀，避免后代调用递归进入包装器。保护范围是已审查的 CLI 源码，
并不意味着任何 Netplan 子包升级都会触发回退。

服务 drop-in（工程源码：`image/ubuntu-base/90-uconsole-netplan.conf`）只在 NM 的 PATH
前面加入 `/usr/libexec/uconsole/nm-netplan`；该目录只放 root 所有的 `netplan`。
默认 shell 和独立的 NetworkManager dispatcher 服务继续使用系统路径。
没有覆盖 dpkg 所有的文件，没有 `dpkg-divert`，也不需要定制 NM 二进制或新的预编译包。
Ubuntu 构建器快照这两个文本输入，镜像检查校验安装字节及入口可执行权限。
该策略属于 Ubuntu rootfs，与 CM4/CM5 板名或 GPU 无关；其他 rootfs 后端不自动继承。

## 已测量的调用开销

2026-10-24，CM4 原生 Ubuntu 上交替执行完整 stock/fast 调用三次：

| 入口 | 耗时 |
| --- | --- |
| 原版 CLI | 0.626–0.629 s |
| 快速入口 | 0.436–0.454 s |

三次均成功，单次减少约 0.17–0.19 s。独立分阶段观察为：C generator
0.015–0.017 s、udev reload 约 0.019–0.020 s、systemd reload 约 0.312–0.315 s。
这是当前运行环境下的调用测量，不是物理开机至可见画面的时间，也没有取回
禁用整个 Netplan 功能所省的约 0.7 s。

同一内核、固件、网络配置与采样工具下，按原版→快速→原版→快速重启：

| 样本 | boot ID | NM Starting→Started | labwc 进程启动 | Noctalia 进程启动 |
| --- | --- | --- | --- | --- |
| 原版 1 | `00000000-0000-4000-8000-000000000006` | 1.416 s | 5.91 s | 7.59 s |
| 快速 1 | `00000000-0000-4000-8000-000000000030` | 1.129 s | 5.65 s | 7.31 s |
| 原版 2 | `00000000-0000-4000-8000-000000000033` | 1.419 s | 5.92 s | 7.61 s |
| 快速 2 | `00000000-0000-4000-8000-000000000004` | 1.148 s | 5.67 s | 7.30 s |

桌面时间相对内核入口；NM 区间取 systemd 的单调日志时间。四组 daemon 均为原包
SHA-256 `a520dffa7da482f92feaceebaa6e3f352781b268ded13311354796da1e5cb7aa`，
网络配置哈希相同，供电为充电状态，Wi-Fi、网关、DNS、合成截图检查正常，
无失败 unit，固件保护标志为零。两次候选的进程 PATH、入口摘要和源码保护均已验证。
启动区间减少约 0.27–0.29 s，与运行中调用的 0.17–0.19 s 是不同条件下的观察，
不能互换，也不构成物理首帧或断电冷启动测量。

## 验证依据与边界

主机测试（工程源码：`tests/test_nm_netplan_generate.py`）使用保持原样的发行版方法作为
操作/异常行为 oracle，对照命令顺序、返回码、信号及执行异常，并检查所有回退条件。
构建/检查夹具分别验证专用服务路径、原系统 CLI 保留、输入字节和执行权限。

实机使用绑定不存在接口 `uctest0`、禁止自动连接的临时 Ethernet 连接；现有 Wi-Fi
不参与修改。检查新建、改名、静态地址/MTU 修改、重载、重新生成、删除后不复活。
原 Netplan 库会在保存连接时重排已有 YAML 的字段，因此生命周期验证比较 YAML 语义；
结束后恢复原文件字节。直接要求操作期间 YAML 哈希不变，会把合法重新序列化误判为故障。

同一候选下，另一个禁止自动连接的临时 profile 在 NM 停止期间通过 YAML 修改名称
与 MTU。服务启动读回新值；重启 `00000000-0000-4000-8000-000000000035`
后属性仍相同。随后删除、生成及重载，连接没有复活，原网络配置字节全部恢复。
通过 `/proc` 观察也确认 NM 的实际子进程执行专用入口，而非仅检查服务配置。
原 daemon 和九个插件的磁盘摘要均与试验前相同；临时连接、离线修改服务、回退
timer/unit 与根权限暂存目录均已清理，保留正式入口与服务 drop-in。

该轮 346 项主机测试完成，其中 1 项可选跳过，其余通过；补丁检查通过。构建器已接入该策略。
当时设备保留它，SD 镜像尚未重新生成；后续镜像状态见[开发状态](../../project/status.md)。
本轮没有覆盖 CM5 或不同网络场景。

不能只跳过第一次 `reload_connections()`：NM 服务重启也会走第一次加载，
那样会漏掉服务停止期间的 YAML 修改。`/run/netplan` 标记或已存在生成文件，也不能
证明输入未变化、生成成功或输出完整。本方案每次都保留生成与重载。

原始测量、安装源码及生命周期证据保存在开发机
`~/.cache/uconsole-desktop-20261024/boot-resume/` 的 `netplan-*` 文件中。
开发机测试不替代不同 DHCP、VPN、IPv6、CM5 或其他核心板的硬件验证。
