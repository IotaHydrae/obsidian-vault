# 构建详解：源码同步、编译选项、缓存与压缩、ATF 覆盖

> 最短路径：`./build.sh -b lubancat-5-v2`（x86 交叉）或板上原生跑同一条命令。
> 这一篇是**构建期全部选项与坑**：源码从哪来、怎么同步到板子、`build.sh` 与主脚本的每个开关、
> apt 缓存与镜像压缩的取舍、ATF 覆盖逻辑、品牌化素材与体检。
> 原理与阶段内部实现见 [build-internals.md](build-internals.md)；两台机器耗时实测见
> [build-speed-comparison.md](build-speed-comparison.md)；缺 ATF 的排查见
> [../hardware/atf-missing-troubleshooting.md](../hardware/atf-missing-troubleshooting.md)。

## TL;DR

- **两条路**：x86_64 交叉并用 QEMU 执行 rootfs 用户态，或在 ARM64 上原生执行。
  历史 `rootfs` 阶段的端到端倍率与适用范围见[构建测量](build-speed-comparison.md)。
- `build.sh` 只做编排（补齐缺失的 kernel/U-Boot deb → 转调 `scripts/build-lsp-image.sh` 跑 `apps → base → rootfs → oem-host → board → image`）；
  **分阶段 / 强制重建 / 换桌面**要直接调主脚本。
- 产物：裸 `.img` 在 `images/`；要分发再跑 `tools/compress-image.sh`（压缩已从构建里摘掉）。
- ⛔ 内核/U-Boot 有改动时**两台机器分工**：服务器编 deb（24 核交叉）、板子跑 rootfs 与镜像（原生）。
- **失败重跑直接重跑同一条命令**（`rootfs` 阶段复用已解压 rootfs + apt 缓存）；出正式镜像才加 `--force`。

## 1. 获取源码与同步到板子

```bash
git clone -b 24.04-panfrost git@code.example.com:team/image-project.git
cd ubuntu-lsp-rk3588 && git lfs fetch && git lfs checkout    # packages/ 里的 .deb 是 LFS 对象
```

rootfs 在 ARM64 构建机上可原生执行，常见流程是“开发机改代码 → 同步到板子 → 板上构建”：

> ⚠️ **不要用一条普通 rsync 推整个仓库**：`build/` 里既有可重新生成的东西（rootfs、u-boot 源码副本、
> apt 缓存），也有**在板子上辛苦编出来的产物**（`build/u-boot-*.deb`、`linux-image-*.deb`）。
> 开发机 `build/` 里的旧 deb 会把板子上的覆盖掉 —— 典型后果：刚编出带 ATF 的 u-boot，
> 一同步又变回旧包，上电卡在 SPL→U-Boot（真实踩过）。

```bash
./tools/sync-to-board.sh -H board@<HOST_07>              # 默认：只同步源码
./tools/sync-to-board.sh -H board@<HOST_07> -n           # 先干跑，看会传/会删什么
./tools/sync-to-board.sh -H board@<HOST_07> --artifacts  # 确实要传 build/*.deb（先自检 ATF + 二次确认）
./tools/sync-to-board.sh -H board@<HOST_07> --delete     # 连"板子上多余的源文件"一起删
```

| 项 | 默认行为 |
| --- | --- |
| 源码（`README`/`notes`/`scripts`/`tools`/`config`/`overlay`/`packages`/`kernel-6.1`/`u-boot`…） | **同步** |
| `build/`（含 `*.deb`、u-boot 源码副本、apt 缓存） | **不同步**（`--artifacts` 放行 `build/*.deb` 与 `build/apps/`，含应用状态；`build/apt-cache` 始终保护） |
| `images/` | 不同步（`--images` 显式放行） |
| `.git/`、`.test-tmp/`、`*.orig`、`*.rej`、`*~` | 不同步 |
| `overlay/opt/data_collection/.install.lock` | 不同步；安装锁由更新器创建，`--delete` 也保留目标已有锁 |
| `--delete` | 默认**关** |

同步完先在板子上看一眼 ATF 链路再动手编译：`./tools/check-atf.sh`。
机器地址与操作边界见 AGENTS.md（镜像源码：`AGENTS.md`） §8，连接前核对实际启动状态。
上位机无需手工拷贝：构建会从 GitLab 拉取 `main` 最新提交，构建机账户须已有 SSH 读取权限。
`sudo` 调用时 SSH 使用调用账户身份；分支覆盖、缓存与排除规则见 [上位机源码同步](../system/oem-host-upstream-sync.md)。

## 2. 构建镜像

`./build.sh lunch` 用编号菜单保存根目录 `.build-config`，随后无参数 `./build.sh` 执行；
首次默认 5-v2、ubuntu-desktop、完整镜像、eMMC，`0` / Enter 接受默认、`q` 取消。
`lunch --dry-run` 只预览，不保存、不构建；显式参数入口不读写配置，详见[构建菜单](build-lunch.md)。

```bash
sudo ./build.sh --board=lubancat-5-v2     # 可选：lubancat-5io（列表 = config/boards/*.conf）
# 常用参数：-c 清理（保留 apt-cache）、-C 全清、-d Docker、-k 仅内核、-u 仅 U-Boot、-a [名字] 仅准备 app、-v 详细
# 产物位于 images/
```

- `build.sh` 只做编排：先把缺失的内核/U-Boot deb 补编出来，再把完整镜像交给
  `scripts/build-lsp-image.sh`（apps → base → rootfs → oem-host → board → image）。`-do` 是默认行为（只出桌面版），`-so`（server 版）已移除。
  要断点续跑 / 换桌面 / 单跑某阶段，直接用下面那个脚本。
- **`-u` 只编当前板卡**：`debian/targets.mk` 里登记了 5 个平台，默认 `dpkg-buildpackage` 会把 5 块板全编一遍；
  `build-u-boot.sh` 会按板卡配置传 `UBOOT_PLATFORM`（`lubancat-5-v2` → `lubancat-5-v2-rk3588`），
  只编这一块；要全编加 `UBOOT_PLATFORMS=all`。该变量不影响 deb 包名（仍由 `debian/control` 决定）。

```bash
# 等价于 build.sh 的镜像流水线；跳过的阶段自动复用已有产物
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=apps   # 校验 Rust deb、编译 C app，不操作 rootfs
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=rootfs      # 只构建 rootfs，要求应用产物有效
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=oem-host  # 已有基础 rootfs：更新、安装、预编译上位机
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=rootfs       # apps → base 依赖检查 → rootfs → oem-host → board → image
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=oem-host  # 更新上位机，再续 board、image
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=board       # 从某阶段续跑

# 换桌面（不同桌面用不同 display-manager，配置文件按 DM 预写）
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --desktop=xfce4   # XFCE + LightDM
sudo ./scripts/build-lsp-image.sh --board=lubancat-5io --desktop=kde      # KDE Plasma + SDDM
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --desktop=ubuntu-desktop --dry-run  # 预览
```

支持的桌面：`ubuntu-desktop`（默认，GDM3）/ `xfce4`（LightDM）/ `kde`（SDDM）。
LightDM/SDDM 的定制配置可放进 `overlay/etc/lightdm/`、`overlay/etc/sddm.conf.d/` 后自动生效（当前用默认）。

默认与 `--from=base` / `--from=apps` 均按 `apps → base → rootfs → oem-host → board → image` 执行。
`--stage` / `--from` 只接受这六个职责名称；数字仅用于日志进度，不再接受旧数字参数。
`--from=rootfs` 先准备 apps、检查 base 依赖，再续 rootfs → oem-host → board → image；`--from=oem-host` 从上位机阶段续跑。
apps 默认校验三个 Rust 仓库 deb、编译四个 C app；无需 Rust 工具链，仍需 C 编译器与 Make。
全量准备成功记录 `build/apps/.build-state` v2，纳入 manifest、包与源码摘要；单项准备不发布全量状态。
`rootfs` 阶段只安装已有应用产物，状态无效时提前拒绝；上位机另在 `oem-host` 阶段同步、安装依赖与预编译，
该阶段要求 `rootfs` 阶段已完成。更新上位机无需重做 `rootfs` 阶段；
C 用 `install-only` 安装，Rust 用 apt 安装固定本地 deb；缺包或源码不匹配直接失败。
显式更新 Rust 包用 `./scripts/build-app-debs.sh [APP...]`，须在 noble arm64 板执行，见 [Rust deb](rust-app-debs.md)。

> 镜像里**没有 snap、没有 LibreOffice**；浏览器是**带 RKMPP 硬解的 Chromium 151**
> （`packages/Chromium-rkmpp/` 的本地 deb，见 [deb-packages.md](deb-packages.md) §1.2、
> [../system/chromium-rkmpp.md](../system/chromium-rkmpp.md)），所有安装都用 `--no-install-recommends`。
> 上位机（工厂模式）的依赖覆盖方式、`device.yaml` 权限要求、Quest USB 访问规则见
> [../system/app-and-device-config.md](../system/app-and-device-config.md)。

## 3. 失败重跑、缓存与阶段复用

`rootfs` 阶段中途失败后**直接重跑同一条命令**即可续跑，复用已装的包和已有 apt 下载；
新增或升级的包仍可能下载。完整构建与 `--from=rootfs` 发现当前底包缺失会先补下载，
保留已有 rootfs；单跑 `--stage=rootfs` 则要求底包预先就绪：

- 会话内操作都是幂等的（`useradd` 有守卫、`write_append` 处都有前置覆盖或 `grep` 守卫），
  会话开头会先 `dpkg --configure -a` 修复被中断的包数据库。
- 要丢弃现有 rootfs、从底包重新安装时加 `--force`（或 `LSP_CLEAN_ROOTFS=Y`）：
  ```bash
  sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=rootfs --force
  ```
  这会丢弃 `build/rootfs` 从底包重新解压；在线仓库的包版本仍可能变化，不保证镜像可复现。

切换 `--desktop` 时必须重建 rootfs，不能复用另一桌面的完成状态；
[保存配置的执行入口](build-lunch.md) 会检查当前桌面，必要时传入清理参数；仅保存不清理。
命令行须选择包含 `rootfs` 的流程并加 `--force`，缓存识别依据见[内部机制](build-internals.md#1-不再压缩完立刻解压阶段间直接复用-buildrootfs)。

**apt 缓存跨构建复用**（`LSP_APT_CACHE=Y`，默认）：`rootfs` 阶段收尾把下载好的 `.deb` 搬进
`build/apt-cache/archives/`，重建时只复制 `.deb` 回 rootfs 并保留缓存源。
`--force` 仍可复用匹配版本的包，新增或升级的包可能需要下载；保存 / 恢复失败会停止构建。

```bash
sudo LSP_APT_CACHE=N ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=rootfs --force  # 重建且不恢复缓存
sudo env APT_CACHE_DIR="$HOME/lsp-apt-cache" ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --force  # 多 checkout 共用

./tools/apt-cache.sh stat                            # 包数 / 大小 / 架构分布
./tools/apt-cache.sh export -o /tmp/cache.tar.zst    # 打包（附 MANIFEST；默认排除非 arm64/all）
sudo ./tools/apt-cache.sh import /tmp/cache.tar.zst  # 合并导入，同名包自动跳过
./tools/apt-cache.sh dir                             # 打印缓存目录
```

- `./build.sh -c` 只清 `build/` 其余内容、**保留 `build/apt-cache`**（结束时打印保留了多少）；
  连缓存一起删用 `-C`，或直接 `rm -rf build/apt-cache`。
- `LSP_APT_CACHE=N` 只禁用跨构建保存 / 恢复，现有 rootfs 的 `.deb` 仍可复用；
  配合 `--force` 才会重新解压 rootfs 且不恢复缓存。
- 跨机器分发缓存的原理（为什么只缓存 `.deb`、为什么不能走 git）见 [build-internals.md](build-internals.md)、
  [apt-cache.md](apt-cache.md)。

**阶段间复用**：`board` / `image` 阶段默认直接用 `build/rootfs`，不生成中间 tar；要归档或换机器跑后续阶段时：

```bash
sudo LSP_ROOTFS_TAR=Y ./scripts/build-lsp-image.sh --board=lubancat-5-v2
```

完成标记是 `build/.stage-rootfs.done`、`build/.stage-board.done`（不再依赖 tar 是否存在）；
`--stage=board` / `--stage=image` 单独执行时，优先使用 `build/rootfs`，缺失时尝试对应 tar 归档。为什么不再"压缩完立刻解压"、
省下多少时间与磁盘见 [build-internals.md](build-internals.md)。
`oem-host` 阶段每次参与流程都检查远端最新提交，获取失败即停止，不能用旧缓存兜底。
`build/.stage-oem-host.done` 记录已安装来源，仓库、分支或提交不变时可跳过安装。
来源变化只重做上位机并使 `board` / `image` 阶段失效；重跑 `rootfs` 阶段会撤回上位机完成标记，后续必须重新集成。
单跑 `rootfs` 阶段不同步或预编译上位机；单跑 `board` / `image` 阶段或 `--from=board` / `--from=image` 不联网，
但启用上位机时须有与 rootfs 来源一致的上位机完成状态，不能跳过集成直接打包。
`board` 阶段恢复 rootfs 后、`image` 阶段拷贝或解包到镜像根分区后校验，兼容只有 tar 归档的回退路径。
启用 `LSP_ROOTFS_TAR=Y` 时，`oem-host` 阶段会刷新 rootfs 归档，避免后续恢复旧归档丢失上位机。
修改其他包清单、overlay 或 rootfs 构建逻辑不会自动使完成标记失效，须显式重跑 `--stage=rootfs`；
仅 `--from=rootfs` 仍可能跳过，细节见[阶段标记边界](build-internals.md#4-相关阶段与标记速查)。

> **内核 / U-Boot deb 需要先编译**（很久，可先在别的机器编好）。脚本 `board` 阶段会 `cd build/` 后查找安装：
> ```bash
> cp <别的机器>/linux-image-*.deb <别的机器>/linux-headers-*.deb <别的机器>/u-boot-lubancat-rk3588_*.deb build/
> sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=board --force
> ```
> 驱动模块（`rtl8822ce.ko`、`rtl8852be.ko` 等）随 `linux-image` deb 装进 rootfs —— **这些驱动不在内核源码树里**，
> 要看装好的模块得等内核包进系统之后（`lsmod` / `find /lib/modules/<ver>/ -iname '*8822*'`）。

## 4. 镜像压缩：构建只出裸 `.img`

```bash
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2   # 构建（不压缩，产物 images/<name>.img）
./tools/compress-image.sh images/<name>.img               # 默认 zstd -3（推荐：小、解压快）
./tools/compress-image.sh images/<name>.img --zstd=1      # 最快的一档
./tools/compress-image.sh images/<name>.img --zip         # Windows 资源管理器 / RKDevTool 能直接解压
./tools/compress-image.sh images/<name>.img --zip=0       # 只打包、不压缩
./tools/compress-image.sh                                 # 不给镜像：自动挑 images/ 下最新的 .img
```

> 2026-09-15 起压缩与"做镜像"**解耦**（以前压缩代码在构建脚本与 `make-sd-image.sh` 里各一份）：
> 做镜像的工具只出裸 `.img`；旧的 `--compress` / `--no-compress` / `--keep-raw` / `LSP_IMG_COMPRESS`
> 现在会**直接报错**并指向新工具。

| 方法 | 产物后缀 | 速度 | 体积 |
| --- | --- | --- | --- |
| **zstd -3（默认）** | `.zst` | 16 s | 1.10 GiB |
| `--zstd=1` | `.zst` | 6.8 s | 1.21 GiB |
| `--zip`（级别 1） | `.zip` | 79 s | 1.29 GiB（Windows 能直接解压） |
| `--zip=0` | `.zip` | 32 s | 4.88 GiB（仅打包） |

> 这张表来自 2026-09-15 的 4.88 GiB 镜像，保留为历史参考。
> 当前镜像没有重测；压缩率和耗时还取决于内容、CPU 与存储，不能仅按镜像体积线性外推。

- 工具**始终**生成 `<包>.rawsize`（解压后字节数）；`<包>.sha256` **默认不算**（几 GB 要 5~15 s）——
  发布/分发时加 `--sha256`。`tools/write-sd.sh` / `tools/update-emmc.sh` 支持 `.img` / `.img.zst` / `.img.zip`
  （`.xz`/`.gz`/`.lz4` 只为历史产物保留读取能力），先按**修改时间**取最新；若它是压缩包而**同名裸 `.img` 还在**，
  优先用裸镜像（免解压）。
- 未安装对应工具会明确报错（`apt-get install -y zstd` / `zip`）；lz4/gzip/xz 已移除
  （实测都比 zstd 差，见 [../storage/boot-flash-card.md](../storage/boot-flash-card.md) §6）。
- 回归测试：`./tests/compress-image-tool.sh`（假仓库 + 16 MiB 假镜像，不动真 `images/`）。

## 5. 预置 WiFi（可选）

```bash
WIFI_SSID="EXAMPLE_ID_05" WIFI_PASSWD="<SECRET>" sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2
WIFI_SSID="" WIFI_PASSWD="" sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2   # 不预置
```

- 默认 `example-project` / `example-project1111`（脚本里的默认值）。
- SSID 含空格自动转义为 `_`（keyfile 文件名）；密码含特殊字符时建议构建后验证连接。
- 写入 `/etc/NetworkManager/system-connections/<ssid>.nmconnection`，权限 **0600**。

## 6. 桌面自动登录与会话（GNOME / GDM3）

```bash
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2    # 默认：board 自动登录 + ubuntu-wayland
AUTO_LOGIN_USER=robot AUTO_LOGIN_SESSION=ubuntu-xorg sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2
AUTO_LOGIN_USER="" sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2   # 禁用自动登录
```

> ⚠️ **开机默认不进 GDM** —— 默认会话是 kiosk（见 README「主要特性」）；这套自动登录只在
> **维护模式切回桌面**（`sudo lsp-maintenance on`）时生效。

- 会话名可选 `ubuntu-wayland` / `ubuntu-xorg`（由 `ubuntu-session` 包提供）。
- LightDM / SDDM 分支暂不生成自动登录与会话配置，切换桌面时需自行配置。
- 实现方式（`/etc/gdm3/custom.conf`、AccountsService、`.dmrc`）见 [../system/desktop-session.md](../system/desktop-session.md)。

## 7. ATF bootloader 覆盖（`image` 阶段）

仓库编译的 U-Boot 若缺少 ATF，启动会卡在 SPL→U-Boot。`image` 阶段会**自动检测**镜像内的 bootloader：

- **已含 ATF** → 保留你自己编译的 U-Boot（不覆盖），可正常通过 `build.sh -u` 改 U-Boot。
- **缺少 ATF** → 用 `prebuilt/idbloader.img`（@32 KB，`seek=64`）与 `prebuilt/u-boot.itb`（@8 MB，`seek=16384`）
  覆盖镜像头部。
- 强制覆盖加 `--atf`；重新封装并检查用 `--stage=image`，只读检查既有镜像用 `tools/check-atf.sh -i <镜像>`。
- ATF 检查或补写成功后，才发布 `build/.stage-image-<rootfs名>-<板型>.done`；不再有独立 ATF 阶段或 `.atf-ok` 标记。
- 这一步是**原地 `dd`** 覆盖头部，没有解压/重压开销（压缩已解耦到 `tools/compress-image.sh`）。

> 上电卡在 SPL→U-Boot、怀疑镜像里没有 ATF：`./tools/check-atf.sh` 会依次检查 ①`prebuilt`
> ②`build/*.deb` ③`build/rootfs` ④`images/` 最新镜像（`-i images/xxx.img` 指定镜像，
> `sudo ./tools/check-atf.sh -d /dev/mmcblk0` 再查设备上实际写入的 bootloader）。
> 检测原理、根因（`decode_bl31.py` 的 python2 shebang）与断点处理见
> [../hardware/atf-missing-troubleshooting.md](../hardware/atf-missing-troubleshooting.md)。

## 8. 品牌化素材

用 `tools/make-assets.sh`（镜像源码：`tools/make-assets.sh`） 把源素材（PNG/JPEG/PDF/SVG）一键转成镜像要求的
格式与尺寸、放进 `overlay/` 对应位置，再重编镜像生效：

```bash
./tools/make-assets.sh list                                      # 查看各部位规格与当前状态
./tools/make-assets.sh watermark logo.pdf wallpaper photo.png    # 一次多个
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --force  # 重建使其生效
```

各部位规格、格式要求、生效机制与排查顺序见 [../system/branding-customization.md](../system/branding-customization.md)；
素材工具本身的规则见 [../system/branding-assets-tool.md](../system/branding-assets-tool.md)。
主机名走构建变量 `LSP_HOSTNAME`（默认 `example-project`），motd/登录横幅的改法见
[../system/hostname-and-motd.md](../system/hostname-and-motd.md)。

## 9. 体检（确认定制项是否生效）

把"我们改了什么"逐条断言出来，避免刷完机器靠肉眼试：

```bash
./tools/verify-image.sh --root build/rootfs   # 1) 刷机前离线体检（推荐先跑，不用 root）
./tools/verify-image.sh                       # 2) 板子上体检（用桌面用户跑，gsettings 需要会话总线）
sudo ./tools/verify-image.sh                  #    额外校验 eMMC 上实际写入的 bootloader
./tools/verify-image.sh --quiet               # 只列失败/警告 + 汇总（适合反复跑）
```

- 退出码：有 FAIL 返回 1，可直接用于脚本/CI。
- 离线模式下 gsettings 用 `GSETTINGS_BACKEND=memory` 读**编译期默认值**（不碰宿主机的 dconf）；
  live 模式读的是**生效值**。
- 脚本里的期望值与单元清单同 `scripts/build-lsp-image.sh`，两处都带注释标明，**改一处要同步另一处**。
- 某项 FAIL：桌面类改动重跑 `--stage=rootfs`；bootloader 相关看 `--stage=image`。
- 全部检查项明细（分组输出 ✓ 通过 / ✗ 失败 / ! 警告 / - 跳过）见
  [../system/app-and-device-config.md](../system/app-and-device-config.md)。
