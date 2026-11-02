# 内核与 U-Boot 的 icecc 接入

`tools/icecc-run.sh` 为内核 / U-Boot 编译选择本机或 icecc。`auto` 只在本机逻辑 CPU 数不超过 8
时尝试集群；这是项目的保守策略，不能仅凭核数判断哪种方式更快。未配置或探测失败时回退本机构建，
默认并行度为 `nproc`，不自动安装依赖。

icecc 只分发编译，不加速下载、apt / dpkg、rootfs 配置或镜像写盘。rootfs / 镜像仍在 ARM64
构建机原生运行，构建位置见 AGENTS.md（镜像源码：`AGENTS.md`） §5.0。

## 1. 模式与工具链

| 配置 | 行为 |
| --- | --- |
| `LSP_ICECC=auto`（默认） | 本机 `nproc` ≤ `LSP_ICECC_AUTO_MAX_NPROC` 时尝试集群，否则本机编译 |
| `LSP_ICECC=0` | 本机编译 |
| `LSP_ICECC=1` | 强制要求集群；配置或探测不可用时报错 |
| `LSP_JOBS=<正整数>` | 覆盖并行度，跳过槽位探测；仍校验工具链环境 |

工具链名称优先取 `-t` / `LSP_ICECC_PREFIX`，否则由 `CROSS_COMPILE` 去掉末尾 `-` 推导；
未指定交叉编译器时为 `native`。内核构建入口显式传入其编译器前缀；U-Boot 根据实际
`CROSS_COMPILE` 或 Debian build / host GNU type 选择交叉 profile 或 `native`，与包内构建规则一致。

启用前校验以下条件：

- `LSP_ICECC_ROOT/toolchains/<prefix>.conf` 可读，`icecc` 可用，本机 iceccd socket 存在。
- 配置的主环境和附加环境为非空可读普通文件；C 编译器可执行，masquerade 包含与 profile 匹配的编译器入口。
- 未设置有效 `LSP_JOBS` 时，取得正数集群槽位。

默认配置根为 `/opt/icecc`。此时优先使用 `icecc-cross`，查找顺序为显式
`LSP_ICECC_CROSS`、`PATH`、`SUDO_USER` 的 `~/.local/bin`；找不到时由本脚本导出
`ICECC_VERSION` 和 masquerade `PATH`。自定义 `LSP_ICECC_ROOT` 直接加载该目录的配置，
避免外部 wrapper 仍使用其固定的 `/opt/icecc`。

槽位先问 `icecc-cross --jobs`，失败后读取调度器监控口的 `listcs`，仅接受完整响应和正数总槽位。
每次探测限时 3 秒，另留 1 秒强制回收；监控口单行读取限时 1 秒。均失败时，`auto` 同步回退本机模式
和 `nproc`；强制模式报错。有效 `LSP_JOBS` 不探测调度器，因此不能据此确认集群在线。
这些回退仅发生在编译前；命令执行失败保留其退出状态，不自动切回本机重复执行。

其它入口变量：`LSP_ICECC_AUTO_MAX_NPROC`（默认 8）、`LSP_ICECC_VERBOSE=1`；调度器可通过
`ICECC_SCHEDULER_HOST` 指定，监控端口默认 8766，可用 `ICECC_MONITOR_PORT` 覆盖。
U-Boot 将选定并行度写入 `DEB_BUILD_OPTIONS=parallel=<N>`，替换旧 `parallel=` 并保留其它选项。

## 2. 检查与验证

```bash
# 只检查配置与模式，不执行编译
tools/icecc-run.sh -t aarch64-linux-gnu --check
tools/icecc-run.sh -t aarch64-linux-gnu --jobs
LSP_ICECC=0 tools/icecc-run.sh -t aarch64-linux-gnu --check
# 强制要求集群，失败即退出
LSP_ICECC=1 tools/icecc-run.sh -t aarch64-linux-gnu --check
```

`--check` 和槽位数只证明接入条件、模式及并行度选择，不证明作业已经派发到远端，也不验证环境包内容。
实际派发须在编译期间观察调度器作业及 worker 状态，并核对目标产物架构；某些作业由客户端本地完成
是正常行为，不能用一次本地作业断言集群失效。验证提速则须比较同一源码、配置、缓存状态与负载的墙钟时间。

本仓库的隔离回归入口是 `bash tests/icecc-run.sh [--keep]`，58 项通过，覆盖模式选择、失效配置、
有界探测、回退和 U-Boot profile / 并行度传递；它不替代真实集群编译。节点部署、环境包生成及预热维护在另一个工程
`~/iotahydrae/icecc-node-setup`，本篇仅说明本仓库的接入。

## 3. 历史测量与适用范围

2026-09-30，14 核 x86_64 开发机作为客户端；当时集群配置为本机 14 槽、24 核 x86_64
服务器 24 槽、ARM64 开发板 6 槽，共 44 槽。96 个 AArch64 目标 C 文件以 `-j44` 编译时，
调度器曾观测到服务器峰值 31 作业、开发板 8 作业、本机 2 作业，96 个产物均识别为 AArch64。
这证明该次实验发生过远端派发，不能替代当前内核或原生 U-Boot 构建的派发验证。

同日对预备并编译过的同一内核构建树，`touch` 同一批 `.c` 后重编，得到以下热缓存结果：

| 负载 | 本机 `-j14` | 集群 `-j44` | 仅服务器 `-j24` |
| --- | --- | --- | --- |
| `drivers/net/wireless/realtek/`，314 对象 | 21.6 s | 52.4 s | — |
| `drivers/net/ethernet/`，当时配置启用部分 | 15.6 s | 44.8 s | 46.6 s |
| `fs/btrfs/`，67 编译单元 | 7.8 s | 42.4 s | — |

这套机器、网络和负载下，集群分别慢约 2.4 / 2.9 / 5.4 倍。预处理、调度及传输是可能开销，
当时未做逐项耗时归因；不能推广为所有增量构建的固定损失，也不能据此保证低核数机器会更快。
默认阈值 8 是据此选择的项目策略，需要不同机器上的同负载 A/B 才能调整。

复测时固定编译器、内核配置及目标子目录，先完成 `prepare` 和该子目录首次构建，再对相同源文件
触发重编，分别以 `LSP_ICECC=0` 和 `LSP_ICECC=1` 计时，并记录实际并行度、缓存、网络和派发状态。
完整内核构建、增量子目录编译、链接与 deb 打包应分别报告，避免将不可分发工作计为集群编译能力。
