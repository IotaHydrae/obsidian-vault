# Rust 应用的 arm64 deb 发布与镜像复用

> 三个 Rust app 以 `packages/apps/manifest.tsv` 固定发布包，日常镜像直接安装，不运行 Rust 编译器。
> 修改源码后，在 Ubuntu noble arm64 板显式运行 `./scripts/build-app-debs.sh [APP...]` 重新编译、打包。

## TL;DR

- 范围为 `lsp-mcap-view`、`lsp-rk-monitor`、`lsp-usb-map`；其余四个 C app 继续在 apps 阶段编译。
- 默认校验源码摘要、包 SHA256、包身份与 arm64；缺包、LFS 指针或旧源码直接失败，不自动回落编译。
- 包版本为 `0.1.0+src.<12hash>-1`；deb 与 manifest 一起维护，已有 `*.deb` Git LFS 规则。
- 应用运行参数、二进制安装路径、优化设置和 MCAP 默认文件关联保留。

## 1. 重新编译与打包

发布包在 Ubuntu 24.04 / noble、arm64 开发板原生构建，使运行库基线与目标镜像一致。
宿主须有 Make、C 链接器、可用的 Cargo/rustc 和 deb 打包工具；这些是构建依赖。
Rust 工具链选择与下载源见 [apps.md](apps.md#5-构建机依赖)。

```bash
./scripts/build-app-debs.sh                 # 重新编译、打包三个 Rust app
./scripts/build-app-debs.sh lsp-usb-map     # 只更新一个包
sudo ./build.sh -a                         # 验证全部应用并刷新全量状态
```

打包先执行真实 build，Cargo 使用 `--locked`；不能拿 `build/apps/` 里遗留的二进制直接当新包。
沿用各 app 当前 release 配置，不为打包修改 `opt-level`、LTO、strip 或 panic 行为。
`Cargo.lock` 必须提交，源码与打包输入在构建期间变化时不能发布成功状态。

临时目录用于准备候选包，构建或校验失败返回非零，不能继续制镜像或把旧包当作本次新产物。
全部选中应用成功且源码未变后，才发布候选 deb 并原子替换 manifest；未选中应用的记录保留。
同版本包内容不一致时拒绝覆盖；工具不执行 Git commit 或 push。
包与清单的提交仍遵循仓库按主题提交的约定。

## 2. 固定清单与可追溯性

`packages/apps/manifest.tsv` 每个应用一行，字段以 Tab 分隔：

| 字段 | 含义 |
| --- | --- |
| app | `apps/` 中的应用目录名 |
| 包路径 | 相对工程根目录的固定 deb 路径；不通过目录中“最新文件”选包 |
| SHA256 | 实际 deb 文件内容的摘要，LFS 指针不能代替对象 |
| sourcehash | 该应用源码与打包输入的完整摘要，版本使用其前 12 位 |

三个 Cargo 包当前版本均为 `0.1.0`，deb 版本形如 `0.1.0+src.<12hash>-1`。
短摘要用于识别版本，完整源码摘要和包摘要用于校验；版本相同不能替代内容检查。
源码摘要纳入应用树（含 `debian/`、`Cargo.lock`、Makefile、`files/`）、共享 Rust 工具链与打包脚本，
包含可执行位，排除 `target/`、`.cache/`、`.git/`、mtime、属主和机器环境。
完整摘要写入包控制字段 `X-Lsp-Source-Sha256`；精确清单与范围见 packages/apps/README.md（镜像源码：`packages/apps/README.md`）。

`apps-state-v2` 在原有应用源码、脚本和参数状态之外纳入 manifest 与实际 deb 摘要。
Rust 项校验 deb，C 项校验 `build/apps/<app>/<app>`；Rust 不要求本机残留 release 二进制。
源码、清单或包变化使应用状态失效，已安装 rootfs 与后续阶段随之重新准备。
状态验证仍须逐包核对 sourcehash、Package 和 Architecture，不能只检查清单文件存在。

## 3. 日常镜像的准备与安装

`apps` 阶段先验证固定 Rust deb、编译 C app，全量成功才发布 `build/apps/.build-state`。
默认机器只需 C 构建工具与包检查工具，无需 Cargo、rustc 或访问 crates 源。
单项准备不发布全量状态；`--stage=rootfs` 需要已有有效的全量状态。

`rootfs` 将验证后的 Rust 本地 deb 交给 apt 安装，依赖由目标 rootfs 的包管理器处理；
C app 继续 `make install-only`。安装阶段不运行编译器，也不在包缺失时临时编译 Rust。
本地包安装失败必须阻止 rootfs 完成标记，不能用目标目录里原有的二进制宣称安装成功。

| app / 包名 | 保留的运行入口 |
| --- | --- |
| `lsp-mcap-view` | `/usr/local/bin/lsp-mcap` 与 `lsp-mcap-open` |
| `lsp-rk-monitor` | `/usr/local/bin/lsp-rk-monitor` |
| `lsp-usb-map` | `/usr/local/bin/lsp-usb-map` |

MCAP 包携带自己的 MIME 定义、desktop 文件和打开包装脚本，数据库由提供方的 dpkg 文件触发器更新。
镜像在装包后覆盖 `overlay/usr/share/applications/lsp-mcap.desktop`，以 `NoDisplay=true` 隐藏应用启动图标，保留文件关联。
postinst 在 `/etc/xdg/mimeapps.list` 没有 MCAP 关联时补入 `application/x-mcap=lsp-mcap.desktop;`，
已有设置保留；不把构建板的 MIME 数据库打进包。
应用本身的接口与功能边界见 [apps-examples.md](apps-examples.md)。

## 4. 跨机器与失败处理

Git clone 后取回 LFS 实体；源码同步默认包含 `packages/apps/`，不需要 `--artifacts`。
后者只用于额外传输 `build/` 中的 C 产物、应用状态或内核 / U-Boot 包，见 [apps.md](apps.md#4-跨机器复用产物)。

| 校验结果 | 处理 |
| --- | --- |
| deb 缺失或仍为 LFS 指针 | 取回该清单对应的真实 LFS 对象 |
| 源码摘要不匹配 | 在 noble arm64 板运行重打包命令，再同步包与清单 |
| 包摘要、身份或架构不符 | 检查清单与实际对象，不能换用目录里另一份旧包 |
| C 产物或全量状态缺失 | 运行默认 apps 准备阶段 |
| Rust 编译 / 打包失败 | 修正失败原因后重跑，不发布本次成功状态 |

包校验只能证明版本、来源输入与内容匹配，不能替代目标系统运行验证。
动态库、ISA 和实际功能仍须核对，glibc / 工具链边界见 [apps.md](apps.md#6-rust-应用的注意事项)。

## 5. 实现依据

首批包在 LubanCat-5 V2、Ubuntu noble arm64、内核 6.1.172 #13、Rust 1.98.1 上原生生成。
三个真实包均为 AArch64，最高 GLIBC 符号为 2.39；归档属主 root:root、目录 0755、程序 0755、数据 0644。
临时解包后的 monitor / USB map 帮助返回 0，MCAP 无参 usage 返回 2；板上 apt 只读模拟仅计划安装这三个包，
依赖已满足。隔离工程以不可用的 Cargo/rustc 路径执行真实 apps 阶段，四个 C app 编译成功，三个 deb 直接复用，
v2 状态有效。包发布 / 安装循环隔离回归 43 项通过。

2026-10-23 的 `lubancat-5-v2` / `ubuntu-desktop` / eMMC 镜像已完成构建及只读离线验收：
三个包均实际安装，版本、arm64 架构和源码指纹匹配；整镜像体检 744 通过、0 失败。
该结果覆盖包安装与镜像静态集成，尚未启动这份镜像验证应用实际运行。

- `scripts/build-app-debs.sh`：显式原生构建、候选打包与发布入口。
- `scripts/lib/app-debs.sh`：逐应用摘要、固定清单与 deb 校验契约。
- `scripts/build-apps.sh`、`scripts/lib/apps.sh`：混合准备、依赖预检与 v2 状态。
- `scripts/build-lsp-image.sh`：rootfs 的 Rust 本地包安装与 C `install-only`。
- `.gitattributes`：`*.deb` 使用 Git LFS；[包总览](deb-packages.md)与[阶段机制](build-internals.md)说明构建位置。
