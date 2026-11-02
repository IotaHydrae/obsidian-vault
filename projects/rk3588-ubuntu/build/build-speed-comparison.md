# 构建速度对比：RK3588 原生 rootfs 与 x86 上 QEMU

> 2026-09-12 的两轮记录中，RK3588 的 `rootfs` 阶段用时 6m04s / 6m17s，
> x86 上 QEMU 为 46m57s / 46m43s，端到端倍率约 7.7× / 7.4×。
> 这支持将 rootfs 放到 ARM64 主机执行；数字包含网络、存储和包安装，不能当成当前镜像、
> 应用编译或 QEMU 本身的固定加速比。内核 / U-Boot 的交叉编译另行比较。

## 1. 历史测量条件

记录来自当时构建日志的 `[info] 阶段 N 总耗时`；内核 / U-Boot 还依据构建日志与产物时间戳。
两端使用 Ubuntu 24.04.4、华为云 apt 源及当时的包清单，rootfs 从底包重建、apt 下载缓存为空。
两端脚本有少量配置差异，网络和存储也不同；这是工程路径对比，不是只改变 QEMU 一个变量的实验。

| 条件 | RK3588 开发板 | x86 编译服务器 |
| --- | --- | --- |
| 硬件 | LubanCat-5 V2，4×A76 + 4×A55，7.7 GiB RAM | Core Ultra 9 285K，24 核，62 GiB RAM |
| 存储 / 网络 | eMMC / WiFi；`rootfs` 阶段下载约 811 MB | NVMe / 有线 |
| `rootfs` 阶段 | 原生 ARM64 apt/dpkg | `qemu-aarch64-static` 执行 ARM64 apt/dpkg |
| 内核 / U-Boot | 原生 GCC | 宿主原生运行交叉编译器，不通过 QEMU 编译 |

当前已更换底包、裁剪包清单，并新增独立 `apps` / `oem-host` 阶段；旧记录不包含这两项独立阶段的计时。
下文按当前职责标注历史操作，历史 ATF 检查计时归入 `image` 的子步骤，不表示当前阶段划分。
重新评估整轮构建时，应分别记录 apps、rootfs、板卡配置和制镜像的时间。

## 2. 记录结果

| 操作 | x86 服务器 | RK3588 | 观察 |
| --- | --- | --- | --- |
| 内核 `bindeb-pkg`（首轮） | 3m43s | 35m01s | 服务器约快 9.4× |
| U-Boot（首轮） | 8s | 49s | 服务器约快 6.1× |
| `rootfs` 阶段（首轮） | 46m57s | 6m04s | RK3588 约快 7.7× |
| `rootfs` 阶段（复测） | 46m43s | 6m17s | RK3588 约快 7.4× |
| `board` 阶段安装内核 / U-Boot（复测） | 1m19s | 20s | RK3588 约快 4.0× |
| `image` 阶段制镜像（复测） | 11s | 1m36s | 服务器约快 8.7× |
| `image` 阶段 ATF 检查（复测） | 0s | 0s | 已含 ATF，均跳过覆盖 |
| `rootfs` 至 `image` 合计（复测） | 48m13s | 8m13s | RK3588 约快 5.9× |

两端内核均使用当时的 `rockchip_linux_defconfig`，生成 11324 个 `.o`；U-Boot 为全量编译。
这些数字支持当时“服务器编 deb、ARM64 做 rootfs”的分工；`board` 阶段是安装 deb，不能套用内核编译倍率。
`image` 阶段涉及文件系统创建和 rootfs 拷贝，NVMe / eMMC 的差异会影响结果。

2026-10-03 另有一轮 RK3588 热 apt 缓存记录：`rootfs` 阶段为 2m14s、`board` 阶段为 1m00s、
制镜像为 1m21s、ATF 检查为 0s，合计 4m35s，生成 5.64 GiB 镜像。
这轮复用已有构建状态，不能与上面的冷下载、全新 rootfs 记录作单变量对照。

## 3. 可以解释到什么程度

[CoreMark 与进程启动测量](../hardware/cpu-coremark.md)记录：动态小程序启动在 QEMU 下为
9.693 ms，RK3588 为 1.176 ms。它提示短进程启动是值得调查的开销，
但未统计 `rootfs` 阶段的 exec 次数，也未隔离解包、压缩、网络和 I/O，不能证明 7.7× 几乎都来自 exec。
QEMU 用户态可以运行多线程程序；实际并行度取决于 apt/dpkg、解压器和维护脚本，不能笼统说一个进程只能用一个核。

同一台 x86 主机上的 user / system TCG 对照见 [qemu-user 与 qemu-system](qemu-user-vs-system.md)：
它分别限制单核与八核，使用相同的 99 个离线包，不与本篇历史全阶段耗时混用。

同一 CoreMark 记录中，QEMU 执行 ARM64 约 14100、A76 约 17200；
x86 原生约 55820。前两者比较的是模拟执行路径，不能据此说 x86 原生单核与 A76 接近，
也不能用它解释原生运行的交叉编译器。内核构建受单核性能、核数、工具链和 I/O 共同影响。

测量须记录 `LSP_UNSAFE_IO`。dpkg / eatmydata 同步控制的覆盖边界和产物清理要求见
[内部机制](build-internals.md#22-构建期同步控制)；其独立收益需要对照，不能从端到端的原生 / QEMU 倍率推导。

## 4. 重新测量

- 固定源码、底包、包版本与源；记录宿主 CPU / RAM、存储、QEMU 版本、并行度和网络条件。
- 区分底包缓存、apt 下载缓存、已安装 rootfs、应用编译缓存与整阶段跳过。
  `--force` 重建 rootfs，但默认仍复用 apt 下载缓存，也不会清理 Cargo 缓存。
- `apps` 阶段与 `rootfs` 阶段分开计时；内核 / U-Boot 编译和 `board` 阶段安装也分开。
- 保留完整日志与退出码；先验证产物有效，再比较相同范围的耗时。

例如，底包已准备后，在两端各执行同一组命令，测量冷下载的 `rootfs` 阶段：

```bash
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=apps
set -o pipefail
sudo LSP_APT_CACHE=N ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=rootfs --force \
    2>&1 | tee /tmp/lsp-rootfs-cold.log
rg '总耗时' /tmp/lsp-rootfs-cold.log
```

这仍包含网络下载，不能用于单独量化 QEMU 开销。测热下载缓存时，先准备同批 `.deb`，
保持 `LSP_APT_CACHE=Y` 并重建 rootfs；测续跑或完成标记跳过时另列结果。
执行前遵守构建位置与会话规则（镜像源码：`AGENTS.md#50-构建位置`）；缓存机制见[内部机制](build-internals.md)。

## 5. 2026-10-15 流水线构建验证

2026-10-15 在 LubanCat-5 V2、aarch64、6.1.172 上以 `--force` 完整运行当时的 apps → base → rootfs → board → image 流程，
底包为 Ubuntu Base 24.04.5，启用默认同步控制，复用 apt / Cargo / wheel 缓存及现有内核、U-Boot deb。
总 wall time 为 511.6 s；按职责对应的计时分别为 apps 3 s、base 6 s、rootfs 367 s、
board 64 s、制镜像 72 s、ATF 检查 0 s。恢复 1703 个 deb，收尾保留 1904 个，wheel 缓存为 29 个。

产物为 5,515,509,760 B 裸镜像；只读挂载后静态验收 625 通过、0 失败，ATF 内部校验通过。
六个应用与预编译产物哈希一致，powerkey 保持静态链接，ping capability 保留；构建加速配置及
eatmydata 两包未进入镜像，无残留挂载，构建后 HDMI 会话恢复、上位机 HTTP 返回 200。
本轮未刷机或验证新镜像启动，也没有同条件旧版对照，不能由该耗时宣称本次改动的加速比。
