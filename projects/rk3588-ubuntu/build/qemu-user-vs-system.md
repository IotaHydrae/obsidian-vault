# ARM64 rootfs：qemu-user 与 qemu-system TCG 对照

> 2026-10-15 实测：处理同一批 99 个离线 deb，八核预算下 qemu-user 中位 399.11 s，
> qemu-system TCG 中位 563.69 s，后者慢 41.2%。两者都能使用多核；
> 本轮结果不支持为 rootfs 提速改用完整 ARM64 VM，继续采用 ARM64 原生构建分工。

## 比较的问题

qemu-user 并不限于宿主单核。QEMU 8.2.2 的
[user-mode 文档](https://github.com/qemu/qemu/blob/v8.2.2/docs/user/main.rst)
说明每个模拟线程可以对应一个宿主线程；
[MTTCG 文档](https://github.com/qemu/qemu/blob/v8.2.2/docs/devel/multi-thread-tcg.rst)
说明 system 模式可为每个 vCPU 使用独立宿主线程。
能使用多核不等于 apt/dpkg 的实际工作负载能充分并行。

本实验比较同一台 x86 主机上的两条 ARM64 执行路径，不比较 ARM64 原生构建。
user 路径把系统调用转换给宿主内核；system 路径还执行模拟的 ARM64 内核及设备。
x86 上的 ARM64 VM 使用 TCG，不能使用 x86 KVM 加速 ARM64 指令。

## 固定条件

2026-10-15，Core Ultra 9 285K，24 核、62 GiB RAM，Ubuntu 24.04；
宿主内核 `6.17.0-1033-oem`、governor `powersave`，未修改宿主电源策略。
两条路径均使用 QEMU `8.2.2`，Ubuntu 包版本 `1:8.2.2+ds-0ubuntu1.18`。
user 为 `qemu-aarch64-static` + binfmt，设置 `QEMU_CPU=max`；
system 为 `virt,gic-version=3`、`-cpu max -accel tcg,thread=multi`，
8 GiB RAM、virtio qcow2，来宾内核 `6.8.0-146-generic`。

| 组 | 执行路径 | 宿主 CPU 预算 |
| --- | --- | --- |
| U1 | qemu-user，整个工作进程树 | CPU0 |
| U8 | qemu-user，整个工作进程树 | CPU0–7 |
| S1 | 1 vCPU ARM64 VM，整个 QEMU 进程树 | CPU0 |
| S8 | 8 vCPU ARM64 VM，整个 QEMU 进程树 | CPU0–7 |

每组安装 3 轮，先完成八核组，再完成单核组；每种预算内按 S/U、U/S、S/U 交替。
U 运行时暂停 VM；S 运行前停止并读回来宾 apt、unattended-upgrades、snapd 单元状态。
相同 vCPU 配置内复用已启动的 VM，切换配置时重启 VM；每轮安装重新解压 chroot，
不清除宿主页缓存或 VM 的 TCG 缓存。

每轮输入为同一份官方 Ubuntu Base `24.04.5` ARM64，rootfs 位于新建的 3 GiB tmpfs。
只启用冻结的 `file:` apt 仓库，99 个 `.deb`，约 41.6 MB；计时前复制到 tmpfs 内的
`/var/cache/apt/archives/`，属于全新 rootfs、热包缓存、无在线下载。
每轮实际为 94 包新装、5 包升级、0 移除，最终共 185 个已安装包。
13 个显式请求包为 `ca-certificates curl gnupg locales fontconfig fonts-dejavu-core
python3 python3-numpy python3-gi python3-yaml python3-requests libglib2.0-bin
gsettings-desktop-schemas`，覆盖证书、字体、Python 扩展、GLib schemas 与 locale。
使用 `--no-download --no-install-recommends`、非交互环境、`policy-rc.d=101`、
`Dpkg::Use-Pty=0` 与 dpkg `force-unsafe-io`；两边都没有 eatmydata 预加载。

## 计时与验收

两边使用相同 ARM64 驱动及 `CLOCK_MONOTONIC_RAW`，只报告 wall time：

- 安装：fork 启动 apt 至子进程退出，包含解包、配置及该次 apt 处理的 triggers。
- locale：另计 `locale-gen en_US.UTF-8 zh_CN.UTF-8`。
- exec：串行执行 500 次 `fork → execve(/bin/true) → waitpid`。
- CPU：1、8 个 pthread，每线程固定整数计算 10 亿次，计时含线程创建及 join。

每种 CPU 预算完成安装后，先测 U、再测 S；每组共用一个新解压的 CPU chroot，
按 `1/8、8/1、1/8` 线程顺序连续执行六次。

安装计时不包括 VM 启动、解压、输入复制、apt 索引与模拟计划、收尾检查或结果传输。
原每线程 5000 万次的短 CPU 记录只作诊断，不进入正式 CPU 性能结论。

有效样本要求 apt 与驱动退出成功、`dpkg --audit` 为空、triggers 无未归并项；
最终包名、版本、架构、已安装状态与 apt 模拟计划跨样本完全一致。
`dpkg --verify` 只过滤底包实际 `path-exclude` 规则解释的 missing，
保留 copyright/changelog.* 例外；其他缺失或修改均失败，原始输出留存。
另外检查 Python ssl/gi/NumPy/YAML/requests、CA 加载、GLib、字体匹配、GNOME schema
及两个 locale 的实际存在；CPU 核对独立小参考向量、每线程工作量和跨组 checksum。

## 结果

四组各 3 轮安装、24 次正式长 CPU 测量、4 份能力记录全部验收通过，记录缺失和验收错误均为 0。
12 轮的安装计划及最终包清单完全一致，清单 SHA256 为
`ae901739b3ca2fd3ce631c1c5a65aeceb34cfbf77f1648bfd79dd95188243079`。
包状态、triggers、文件核验和上述功能检查均通过。

以下均为秒；方括号为三轮最小值、最大值。exec 为 500 次串行启动的总耗时，
locale 为安装后单独执行的两种 locale 生成耗时，均不叠加进安装列。

| 组 | 安装三轮 | 安装中位 [min, max] | exec 中位 | locale 中位 |
| --- | --- | --- | --- | --- |
| U1 | 401.544, 401.550, 401.659 | 401.550 [401.544, 401.659] | 3.882 | 5.574 |
| U8 | 399.259, 399.107, 398.568 | 399.107 [398.568, 399.259] | 4.492 | 5.589 |
| S1 | 629.343, 630.016, 629.832 | 629.832 [629.343, 630.016] | 12.415 | 13.821 |
| S8 | 565.364, 563.527, 563.693 | 563.693 [563.527, 565.364] | 11.397 | 14.301 |

从单核到八核，user 安装耗时减少 0.61%，system 减少 10.5%；同为八核时 system 慢 41.2%。
增加可用核心未使这批安装工作充分并行，完整 VM 的多核能力也未抵消其额外开销。
exec 和 locale 是另外的工作负载，不能用它们给 apt 安装耗时分摊占比。

正式 CPU 测量每线程 10 亿次，八线程总工作量是单线程的八倍。
吞吐扩展比为 `8 × 单线程中位 / 八线程中位`，每项各 3 次：

| 组 | 单线程中位 [min, max]，s | 八线程中位 [min, max]，s | 吞吐扩展比 |
| --- | --- | --- | --- |
| U1 | 1.7650 [1.7647, 1.7662] | 14.1329 [14.1168, 14.1354] | 1.00× |
| U8 | 1.8291 [1.8290, 1.8293] | 1.8676 [1.8656, 1.8677] | 7.84× |
| S1 | 1.9564 [1.9548, 1.9611] | 16.4771 [16.4729, 16.4779] | 0.95× |
| S8 | 1.9703 [1.9695, 1.9782] | 2.1407 [2.0831, 2.1644] | 7.36× |

所有正式 CPU 样本的每线程 checksum 均为 `0xca889120e7859781`。
两条路径的八核组都明显扩展，反驳了“qemu-user 只能调度到宿主单核”的前提；
这只证明固定计算负载的并行能力，不代表 apt 具有相同收益。

运行时能力实际探测如下，SVE VL 均为 64 B：

| 组 | HWCAP | HWCAP2 | online CPU 数 | 实际 affinity |
| --- | --- | --- | --- | --- |
| U1 | `0x00000000effffffb` | `0x000018007fc77fff` | 24 | 0 |
| U8 | `0x00000000effffffb` | `0x000018007fc77fff` | 24 | 0–7 |
| S1 | `0x00000000ffffffff` | `0x000018000003ffff` | 1 | 0 |
| S8 | `0x00000000ffffffff` | `0x000018000003ffff` | 8 | 0–7 |

online CPU 数不等于进程允许使用的 CPU 数；四组实际 affinity 均符合预算。

## 适用范围与证据

这是离线安装子集，不是完整 `rootfs` 阶段；未覆盖桌面全量依赖、overlay 和应用安装。
tmpfs 去除了 rootfs 主体持久化 I/O，但日志仍分别写宿主文件系统和来宾 qcow2；
宿主/来宾内核及运行时 CPU 能力也属于执行路径差异，不能称为纯指令转译开销。
`max` 不保证两边相同 HWCAP/SVE；上表来自 `getauxval` 和 `PR_SVE_GET_VL`，
两边 HWCAP/HWCAP2 确有差异，不能宣称运行时能力完全相同。
未采集可直接横比的 user/sys CPU time、RSS 或全程 CPU 利用率。
各组 n=3 只表示本条件下重复观察，不宣称统计显著性，不把局部倍率外推为整镜像收益。

脚本、输入 SHA256 清单、环境、原始日志与每轮 JSON 保存在编译服务器工程的
`build/qemu-rootfs-ab/`；最终 `summary.json` 由 `python3 analyze.py --final` 验收生成。
预跑因验收器未识别底包主动排除文件而中止，单独保存在 `pilot-U8-1/`，不纳入统计。
切换单核 VM 时的启动探测超时已恢复；单核安装当时尚未开始，正式计时驱动和输入未变。
测试使用私有 mount/PID namespace，独立 `/dev`，不绑定宿主整个 `/dev` 或 `/run`。

证据归档为该目录下 `qemu-rootfs-ab-evidence-20261015.tar.gz`，含脚本、C 源码、
测量二进制、环境与验收记录、输入 SHA256 清单及原始结果，不含底包、deb 或 VM 磁盘。
归档 SHA256：`f80b0b1ce2fadb68cf537d5a3381300cb1e93aa56ae988e17ae6673194c26824`。
完整输入仍保留在服务器实验目录；归档本身不是自包含的重跑输入包。
用 ARM64 GCC `13.3.0-6ubuntu2~24.04.1`、`-std=c11 -O2 -Wall -Wextra -Werror -Wpedantic`
（bench 加 `-pthread`）重建，两个二进制均与测量版本逐字节一致。
结束检查未发现匹配的 VM / 实验进程、22442 端口监听或实验挂载；
当时宿主 systemd 为 running，无 failed 单元。

ARM64 原生与 x86 qemu-user 的历史端到端测量及当前构建验证见
[构建速度对比](build-speed-comparison.md)。
