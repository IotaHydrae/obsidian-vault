# 构建内部机制：阶段复用、apt 缓存与镜像压缩

> 本文收录从 README 移出的构建内部机制说明：阶段间为什么直接复用 `build/rootfs`、
> apt 缓存如何跨构建/跨机器复用、中间 tar 与阶段标记、以及「默认不压缩」的取舍。
> 命令用法在 README，这里是原理与设计取舍。

## TL;DR

- 阶段间**直接复用 `build/rootfs`**（不再"压缩完立刻解压"，§1）；apt 缓存默认跨构建复用（§2）。
- ⛔ **镜像压缩已从构建里摘掉**（2026-09-15 起，§3）；构建入口只有一条：
  `build.sh` → `scripts/build-lsp-image.sh`（§5）。
- ⚠️ 坑：**全新底包上会缺 `render` 组**，长期复用旧 rootfs 会把这个坑掩盖住（§6）。
- 阶段标记与速查见 §4。
- `apps` 阶段校验 Rust 仓库 deb、编译 C app；`rootfs` 只安装通过状态校验的产物（§4）。
- 板卡列表从 `.conf` 发现；启动自检在 rootfs 操作前检查 heredoc，`rootfs` 阶段使用七个独立会话（§7）。

## 1. 不再"压缩完立刻解压"：阶段间直接复用 `build/rootfs`

当前底包是 Ubuntu Base **24.04.5 / noble / arm64**；格式与源配置见
[APT 源说明](apt-sources-deb822.md)。更换底包后须从 `base` 阶段完整重建：
`sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --force`。
`base` 缓存同时要求当前版本底包和 rootfs 的 `etc/os-release`。完整构建或 `--from=rootfs`
缺底包时自动补下载，保留现有 rootfs 及下游状态；单跑 `--stage=rootfs` 缺底包则提前拒绝。
仅首次、rootfs 缺失、显式 `--stage=base` 或 `--force` 才从底包解压；下载失败不删除原 rootfs。
补下载不会把旧 rootfs 升级到新底包版本，更换 `UBUNTU_RELEASE` 后仍须完整重建。

阶段交接直接复用 rootfs，避免反复压缩、落盘和解包。省去的工作量取决于 rootfs 大小、
压缩方式与存储速度，不用固定分钟数承诺收益：

- **`board` 阶段直接复用 `build/rootfs`**（`rootfs` 阶段刚构建的就是它），不再解压；
  只有 `build/rootfs` 不在时才回退到从 tar 解压；`--force` / `LSP_CLEAN_ROOTFS=Y` 也仅在 tar 存在时重新解压。
- **`image` 阶段也直接复用 `build/rootfs`**，用 tar 管道保留文件属性并拷到镜像根分区
  （无中间文件）；只有 `build/rootfs` 不在时才解包 `board` 阶段的 tar。
- **两个中间 tar 默认都不生成**，减少压缩与临时磁盘 I/O。需要归档、或想拷到
  别的机器再跑后续阶段时，用一个开关都生成：

  ```bash
  sudo LSP_ROOTFS_TAR=Y ./scripts/build-lsp-image.sh --board=lubancat-5-v2
  ```

- `rootfs` / `board` 阶段的"完成"标记分别为 `build/.stage-rootfs.done`、`build/.stage-board.done`
  （不再依赖 tar 是否存在）。`rootfs` 阶段记录所安装的应用状态；`board` 阶段记录共享 rootfs 当前板卡 ID。
  应用状态变化会重跑 `rootfs` 阶段，切板或旧空标记会重跑 `board` 阶段。
  `--stage=image` 复用 rootfs 时必须确认板型一致，否则在写镜像前拒绝。

桌面也属于缓存条件。`rootfs` 阶段在 `/etc/lsp-image-release` 写入 `LSP_IMAGE_DESKTOP`；
旧 rootfs 没有该字段时，从 dpkg 已安装的 `gnome-shell`、`xfce4-session`、`plasma-workspace` 识别。
桌面不匹配或无法唯一识别时，命令行拒绝复用；须让流程包含 `rootfs` 阶段并显式重建。
[lunch](build-lunch.md) 只保存根目录 `.build-config`；无参 `build.sh` 执行时按需设置 `LSP_CLEAN_ROOTFS=Y`，保存不清理。
单跑上位机、板卡或镜像阶段不能切换桌面；仅编译应用或下载底包不受该限制。

### 1.1 rootfs 复制必须保留 file capability

所有 rootfs tar 创建与解包都使用 `--xattrs --xattrs-include='*'`，包含底包解包、
`rootfs` / `board` 阶段可选归档和 `image` 阶段的直接管道/归档回退。GNU tar 默认不保存扩展属性，
解包时仅加 `--xattrs` 也不能保证恢复 `security.capability`，须显式包含该命名空间。
依据为 GNU tar 的 [Extended Attributes](https://www.gnu.org/software/tar/manual/html_node/Extended-Attributes.html)。

Ubuntu 24.04 的 `iputils-ping` postinst 为 `/usr/bin/ping` 设置 `cap_net_raw=ep` 并去掉 setuid；
能力丢失时，普通账户可能报 `SOCK_RAW: Operation not permitted`。
`iputils-ping` 已显式安装，`libcap2-bin` 是其 Depends；不以 sudo 或放开 `ping_group_range` 掩盖复制丢属性。

源 `build/rootfs/usr/bin/ping` 与挂载后镜像中的文件都应由 `getcap` 读出 `cap_net_raw=ep`，
`tools/verify-image.sh --root <rootfs>` 也检查该属性；不执行离线目标的 ARM64 程序。
板上另以普通账户测试 `ping -c 1 -W 1 127.0.0.1` 和 `ping -6 -c 1 -W 1 ::1`。
旧 tar 或源 rootfs 已丢失的属性不能靠新增解包选项补回；须先通过包重新配置/安装恢复，
或重新构建 rootfs，再生成新归档与镜像。

## 2. apt 缓存跨构建复用（`LSP_APT_CACHE=Y`，默认）

`rootfs` 阶段收尾前不提前清理已下载的包，失败续跑可以直接复用。
收尾时将 `.deb` 从 rootfs **搬进** `build/apt-cache/archives/`；重建 rootfs 后仅将其中的
`.deb` **复制**回 rootfs，保留缓存源。保存或恢复失败立即报错，不发布完成标记。
新增或升级的包仍可能下载；缓存命中不等于跳过解包、配置或整阶段。

```bash
# 重建前的缓存会自动保留/复用，日志里会看到（数字是 2026-10-03 开发板实测的规模）：
#   [cache] 已缓存 1703 个 .deb（808M）→ build/apt-cache/archives
#   [cache] 已恢复 1703 个已下载的 .deb（新增或升级的包仍可能需要下载）
sudo LSP_APT_CACHE=N ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=rootfs --force
# 重建 rootfs 且不从跨构建缓存恢复；已有 app 状态与底包须就绪
```

- 只缓存 `.deb`，**不缓存 apt 索引**（`/var/lib/apt/lists` 照旧删除、每次重新 `update`）：
  索引小，缓过去反而可能因过期引出怪问题。
- `LSP_APT_CACHE=N` 只关闭跨构建保存 / 恢复，不删除现有 rootfs 的 `.deb`；
  要重新解压并不恢复缓存，须同时使用 `--force`。
- 缓存目录可安全删除；镜像里不会带任何缓存（收尾时已清空 `archives` 与 `lists`）。
- `./build.sh -c` / `--clean` 只清 `build/` 的其余内容，**会保留 `build/apt-cache`**（并在结束时
  打印保留了多少）；要连缓存一起删用 `-C` / `--clean-all`，或者直接 `rm -rf build/apt-cache`。

### 2.1 跨机器共用 / 分发缓存
缓存里只有 `.deb`，而且是给 **arm64** rootfs 的（构建时 chroot 就是 arm64，apt 在 chroot 里下载），
所以 **x86 交叉构建机 ↔ ARM64 板子可以直接互相同步**，不存在架构不兼容。

默认每台机器一份 `build/apt-cache`。要放到共享位置（多台机器 / 多个 checkout 共用一份，不必互相拷），
用 `APT_CACHE_DIR` 指过去；保存 / 恢复时应串行使用，不将同一路径用于并发构建。
`~` 会展开，相对路径按仓库根目录解析：

```bash
# sudo 会重置 $HOME，所以让外层 shell 先把变量展开好
sudo env APT_CACHE_DIR="$HOME/lsp-apt-cache" ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --force
APT_CACHE_DIR=/srv/lsp-apt-cache ./scripts/build-lsp-image.sh ...   # 绝对路径同样可以
```

打包 / 合并 / 查看用 `tools/apt-cache.sh`（缓存目录常是 sudo 构建产生的，**导入要 sudo**）：

```bash
./tools/apt-cache.sh stat                            # 包数 / 大小 / 架构分布
./tools/apt-cache.sh export -o /tmp/cache.tar.zst    # 打包（附 MANIFEST；默认排除非 arm64/all 的包）
sudo ./tools/apt-cache.sh import /tmp/cache.tar.zst  # 合并导入，同名包自动跳过
sudo ./tools/apt-cache.sh import /mnt/usb/apt-cache  # 也可以直接从 rsync 过来的目录合并
./tools/apt-cache.sh dir                             # 打印缓存目录（cd "$(./tools/apt-cache.sh dir)"）
```

- 导入是**合并**语义：不删已有包，同名保留已有文件，不比较内容；apt 使用时按仓库索引校验。
  混合缓存中的多余或过期包只占空间，同名损坏文件需删除后重新导入或下载。
- 文件名结尾的 `_arm64.deb` / `_all.deb` 才是用得上的；`stat` 会提示混进来的其它架构，
  `export` 默认把它们排除（除非加 `--keep-foreign`）。
- 别用 git 分发：`/build/` 已被 `.gitignore` 忽略，而且这是大二进制、每台机器各不相同。

### 2.2 构建期同步控制

`LSP_UNSAFE_IO=Y` 默认减少构建 rootfs 的同步等待；`N` 在首次执行 guest 命令前清除
旧的预加载与 `01-unsafe-io` 设置，包含复用 rootfs 的续跑路径。
`force-unsafe-io` 只跳过 dpkg 解包阶段 rename 前的安全同步，不能等同于关闭所有 dpkg 同步。
`eatmydata` 通过预加载拦截动态链接进程的库调用；静态程序或直接 syscall 不保证覆盖。
两者的加速幅度需要独立对照，不能从端到端的 QEMU / 原生倍率推导。
启用 eatmydata 要核对动态链接器的实际加载列表；`/bin/true` 返回 0 不能证明库已加载，
加载失败须撤回预加载配置。收尾移除配置及 `eatmydata` / `libeatmydata1`，包清理失败不发布完成标记。
构建中断或掉电可能留下不完整 rootfs；重建前应先解除绑定挂载，不沿挂载点删除 rootfs。

RK3588 上以 arm64 `libeatmydata1=131-1ubuntu1` 做子进程预加载验证：对普通临时文件写入并调用
`os.fsync`，strace 观察正常执行 1 次 fsync、预加载后 0 次，两者内容相同，loader 加载列表也匹配库。
这只验证该调用路径的拦截，不代表断电持久性或整阶段提速；缓存回归 10 项与 I/O 控制回归 5 项
在本机、RK3588 均通过，入口见 tests/README.md（镜像源码：`tests/README.md`）。

### 2.3 上位机 Python 依赖缓存

独立 `oem-host` 阶段将 `build/pip-wheels` 复制到 rootfs 的临时缓存，先用 `pip download` 联网补齐，
再以 `--no-index --find-links` 安装到系统 Python。下载失败可尝试已有缓存，安装失败则停止。
成功安装后把缓存文件搬回 `build/pip-wheels`，保存成功才清理 rootfs 临时目录；
恢复或保存错误立即报错，保留原 stderr，保存失败保留 staging，不发布阶段完成标记。
缓存减少重复下载，不跳过依赖解析和安装，也不保证无网络时缓存足够。
隔离回归见 `python3 tests/build-download-acceleration.py [--keep]`；
它同时检查 Cargo 索引探测边界，不运行真实 pip / Cargo 或测量下载提速。

## 3. 镜像压缩：**已从构建里摘掉**（2026-09-15 起）

压缩几 GB 镜像要几十秒到几分钟，而测试期间镜像会反复重出，压缩纯属浪费；而且"压给谁看"
（归档 / Windows）跟"能不能启动"是两件事。所以现在**做镜像的工具只产出裸 `.img`**
（`scripts/build-lsp-image.sh` 与 `tools/make-sd-image.sh` 都是），压缩统一走
**`tools/compress-image.sh`**（命令见 README；实测数字见
[storage/boot-flash-card.md](../storage/boot-flash-card.md) §6）：

- 之前压缩代码在**两份**脚本里（构建脚本 + `make-sd-image.sh`），改级别/加方法要改两处 ——
  这正是解耦的原因。旧的 `--compress` / `--no-compress` / `--keep-raw` / `LSP_IMG_COMPRESS` /
  `LSP_IMG_KEEP_RAW` 现在**直接报错**并指向新工具（不静默忽略）。
- 校验和默认**不算**（`<包>.sha256` 要 `--sha256`；构建脚本同理，几 GB 要 5~15 s，日常重建不值得）；
  只有 `<包>.rawsize`（解压后字节数）始终生成 —— 它给写入工具做
  **容量校验与进度条**用（`zstd -l` 的字段会随单位浮动，解析不可靠，所以直接记录）。
- `tools/write-sd.sh` / `tools/update-emmc.sh` 支持 `.img` / `.img.zst` / `.img.zip`
  （`.xz` / `.gz` / `.lz4` 只为历史产物保留读取能力）：先按**修改时间**取最新，
  若它是压缩包而**同名裸 `.img` 还在**，则优先用裸镜像（免解压）。
- `image` 阶段检测已含 ATF 时跳过覆盖；需要覆盖时直接写裸镜像，不解压或重新压缩。
- 镜像完成标记为 `build/.stage-image-<rootfs名>-<板型>.done`，仅在制镜像、ATF 检查或补写、
  以及启用时的 SHA256 生成均成功后发布。`.img` 存在本身不能证明构建成功；缺镜像时标记不可复用。

## 4. 相关阶段与标记速查

- 默认顺序为 `apps → base → rootfs → oem-host → board → image`，阶段参数使用职责名称。`--stage=apps` 只准备应用；
  `--from=base` / `--from=apps` 跑全流程，`--from=rootfs` 先准备 apps、检查 base 依赖，再续 rootfs、oem-host、board、image。
- `oem-host` 在 `rootfs` 阶段后独立获取源码、安装依赖并预编译；每次检查远端分支，按
  `build/.stage-oem-host.done` 复用未变化的安装，新提交只使 `board` / `image` 阶段失效。`rootfs` 阶段重跑会撤回此标记，
  不能跳过上位机阶段直接打包；来源、独立重跑及归档契约见 [上位机同步](../system/oem-host-upstream-sync.md)。
- `apps` 全量准备成功才写 `build/apps/.build-state`。v2 状态覆盖源码、共享脚本、参数、manifest 与产物内容，
  不含编译缓存。三项 Rust deb 须匹配源码和包摘要，缺失或过期不回落编译；单项准备不发布全量状态。
  `--stage=rootfs` 提前要求有效状态；安装 Rust 本地 deb 用 apt，C 产物用 `make install-only`，均不调用编译器。
- `rootfs` 阶段完成标记记录 app 状态。新产物或源码变化使该状态不再匹配时，默认构建会重新安装，
  并使上位机、板卡和镜像阶段失效。应用契约见 [apps.md](apps.md)，Rust 发布见 [rust-app-debs.md](rust-app-debs.md)。
- 该状态不覆盖系统 apt 包列表、overlay 或 rootfs 构建逻辑；修改这些内容后须显式重跑 `--stage=rootfs`，
  需要从底包重建时再加 `--force`。默认构建或仅 `--from=rootfs` 仍会按完成标记跳过已完成阶段。
- `rootfs` 阶段默认**复用**已解压的 rootfs，直接重跑同一命令即可续跑；`--force` / `LSP_CLEAN_ROOTFS=Y` 才重建。
- `board` 阶段会 `cd build/` 后查找并安装内核 / U-Boot deb（编译产物放 `build/` 即可，见 README）。
  两块板的 U-Boot 包冲突，切板时先在构建 rootfs 内解除另一板包的 hold 并移除，再安装目标包；
  仅解除 hold 不足以解决虚拟包 `Conflicts`，不会修改构建宿主的包或 bootloader。
- `base` 重新解包或重跑 `rootfs` 会清除全部下游标记与过期中间 tar；base 仅补下载保留状态。`board` / `image` 清除当前板下游标记。
  已有 `.img` 和 apt-cache 保留。旧数字阶段标记不迁移、不复用，相关阶段首次使用名称标记时重新执行。
- `bash tests/build-lsp-stage-cache.sh` 隔离检查底包补下载与解包边界、板型拒绝、失效链、失败重试和 dry-run；
  撤销底包判据或保留守卫的反面对照可检出错误。`bash tests/apps-build-stage.sh` 验证应用状态与安装边界，
  不进行真实镜像构建；Rust 包校验与重打包的范围见 [rust-app-debs.md](rust-app-debs.md)。
- `image` 阶段中的 ATF 检查与补写见 [构建详解](build-guide.md#7-atf-bootloader-覆盖image-阶段)；
  完整启动链路与存储布局见 [rk3588-boot-flow.md](../hardware/rk3588-boot-flow.md)。
- `image` 阶段才写入最终 `/etc/fstab`。完整离线验收须只读挂载镜像的根分区与启动分区，
  再运行 `verify-image.sh --root <挂载根目录>`；中间 `build/rootfs` 缺少最终 Data1T 条目。

## 5. 构建入口只有一条：`build.sh` → `scripts/build-lsp-image.sh`

```
build.sh --board=<板卡>
  ├─ 缺 build/linux-image-*.deb / build/linux-headers-*.deb → scripts/build-kernel.sh
  ├─ 缺 build/u-boot-<板卡>_*.deb                          → scripts/build-u-boot.sh
  └─ scripts/build-lsp-image.sh --board=<板卡>             ← apps → base → rootfs → oem-host → board → image
```

- `build.sh` **只做编排**：补编缺失的内核 / U-Boot deb，其余全部交给合并脚本。
  `-k` / `-u` 分别只跑 `build-kernel.sh` / `build-u-boot.sh`；`-l`（LAUNCHPAD=Y）会跳过后两者，
  由合并脚本从 PPA 装内核/U-Boot；`-do` 是默认行为（本工程只出桌面版）；`-so` 已移除。
- 想把内核/U-Boot **从头编**，得先把 `build/*.deb` 挪走（`build.sh` 是"deb 在就跳过"）。
- 已删除的 `scripts/build-rootfs.sh`、`scripts/config-image.sh`、`scripts/build-image.sh` 不属于当前构建链；
  不恢复其安装清单来改本工程，见 AGENTS.md（镜像源码：`AGENTS.md`） §2 第 11 条。

---

## 6. 坑：**全新底包上会缺 `render` 组**（复用 rootfs 把它掩盖了）

**症状**（2026-09-23 用户实测，`build/` 里没有 rootfs、首次跑 `rootfs` 阶段）：

```
gpasswd: group 'render' does not exist in /etc/group
Error: in /bin/bash on line 54
Error: in ./scripts/build-lsp-image.sh on line 1045
```

- 报错的是 `rootfs` 阶段第一个 chroot 会话里的 `gpasswd -a board render`（会话里 `gpasswd -a board render` 那行）；该会话是 `set -eE`，
  整轮构建就断在这里。
- **根因**：`render` 组**不在 ubuntu-base 底包里**（实测 `build/ubuntu-base-24.04.4-base-arm64.tar.gz`
  的 `etc/group`：`video`(44) / `audio`(29) / `plugdev`(46) 都在，**唯独没有 `render`**）。
  它由 **`udev` 包**的 sysusers 配置创建：

  ```console
  $ grep -n render /usr/lib/sysusers.d/debian-udev.conf
  9:g render -
  $ dpkg -S /usr/lib/sysusers.d/debian-udev.conf
  udev: /usr/lib/sysusers.d/debian-udev.conf
  ```

  装/升级 `udev` 时由 `systemd-sysusers` 执行 ⇒ 而那段会话跑在**刚解压的底包**上，`udev` 还没过 postinst。
- **为什么以前没炸**：`rootfs` 阶段默认**复用 `build/rootfs`**（见 §1），那份 rootfs 里 `udev` 早装过、
  `render` 已被创建 ⇒ 只有"**全新解压的底包 + 首次跑**"才会走到这条路上。换机器 / 换底包版本都会复现。
- **修法**（已进 `scripts/build-lsp-image.sh`）：装组之前补一次幂等创建 ——

  ```bash
  getent group render > /dev/null 2>&1 || groupadd --system render
  gpasswd -a board render
  ```

  `groupadd` 来自 `passwd` 包（那时已在，`useradd`/`gpasswd` 都用它）；之后 `udev` 装好时
  systemd-sysusers 看到同名组已存在不会重复创建。判据：把该 chroot 会话正文单独抽出来 `bash -n`
  （AGENTS.md §5 的要求）；构建启动时也自动检查每段正文。

> 教训（与 §1 呼应）：**"一直复用旧 rootfs"会让"依赖某些包已安装"的步骤长期不暴露** ——
> 改动 `rootfs` 阶段时，凡是用到"某个包装完才会有的东西"（系统组、sysusers 建的账号、包自带的目录），
> 都要按"**全新底包**"这条路径想一遍，别只在复用路径上试。

## 7. 板卡发现与 chroot 会话边界

板卡列表来自 `config/boards/*.conf` 的普通文件，帮助与参数校验使用同一列表。
备份文件、目录和未匹配的 glob 不进入列表；空目录明确报错。只读取选中的配置，
其语法错误或缺失 `BOARD_NAME` 都会停止构建，宿主的同名变量不能掩盖缺项。
`--help` 在自检和构建目录创建之前退出。
`--dry-run` 的文件写入统一经过预览函数：镜像指纹、维护密码和 MOTD 不落盘；
`chgrp` 与其他变更命令一样使用替身，维护台哈希生成和写入校验也只预览。
安装后的 cage 版本校验在真实构建中执行，预览不把 chroot 替身的输出当版本号。
厂商 deb 的权限归一只打印包名，不用 `dpkg-deb --contents` 解压枚举载荷。

`scripts/lib/chroot-heredocs.sh` 是构建与回归共用的扫描器。构建先按宿主 Bash 的
结束符语义抽出每段会话，再检查正文的 heredoc 闭合与 `bash -n`；不调用测试套件，
不依赖 ShellCheck，也没有弱化的备用检查。拒绝发生在解析阶段函数和操作 rootfs 之前。
临时正文使用私有目录，检查结束即删除。

扫描器仅支持本工程的字面量标识符结束符与单行会话入口：
`cat << 'EOF' | run_chroot_session …` 或 `cat << 'EOF' | chroot …`。
新增其他 heredoc 写法时须同步扩展扫描器和反面对照。仅运行外层 `bash -n` 不够：
未闭合的正文有时只产生 warning，退出码仍为 0。

`rootfs` 阶段保持原有安装顺序与宿主侧 overlay 操作，只将首段按职责拆分：

| 会话 | 职责 | 依赖前段留下的状态 |
| --- | --- | --- |
| `accounts` | 修复 dpkg、账户/组、主机名、时区、dpkg 构建加速配置 | ubuntu-base |
| `bootstrap` | CA、curl、GPG、locales、eatmydata、生成 locale | dpkg 配置与 Ubuntu 源 |
| `ros2` | 密钥校验、源切换、安装 ROS2 | CA/GPG 已安装，keyring 已预置 |
| `base-packages` | Rockchip PPA、基础包、基础裁剪 | CA 与 ROS2 源 |
| `desktop` | 桌面与 Chromium、kiosk dconf、桌面裁剪 | 基础包与宿主侧暂存的 Chromium deb |
| `app-dependencies` | 应用 apt/pip 依赖、ROS2 shell 环境 | 桌面层已装好 |
| `runtime-packages` | 移除运行镜像不用的独立多媒体开发包 | 所有应用包已安装，按最终依赖清理 |

XFCE 分支另有一段专属设置会话，保留原来的位置与入口；默认 Ubuntu 桌面不执行它。

`run_chroot_session` 为每次 chroot 前置 `set -eE`、带主题名的 ERR trap、固定 locale
和同一份 `apt_update_retry` 定义，不把临时脚本装进 rootfs。宿主参数仍显式 `export`；
会话局部变量不跨进程传递，跨段依赖只通过 rootfs 文件保留。宿主统一 EXIT 清理覆盖正常退出、
错误和 INT / TERM / HUP；镜像 loop 与 dry-run 共用它。根自绑设 private、`/dev` 设 slave，避免
shared 传播造成重复 API 挂载。卸载用绝对路径递归完成并复核；busy 或残留时停止，不删 rootfs，
保留原失败码；成功路径清理失败也报错。SIGKILL / 掉电无法捕获，重跑或 `--clean` 先严格卸载。

验证：`tests/build-lsp-image-heredocs.sh` 检查正文与碰撞反例；`tests/build-lsp-image-preflight.sh`
检查入口、会话、APT 配置与预览。`sudo bash tests/build-lsp-mount-cleanup.sh` 在私有 mount namespace
验证真实挂载的退出 / 信号 / 传播 / busy 路径；不跑 APT 或构建，不能替代真实镜像构建。
