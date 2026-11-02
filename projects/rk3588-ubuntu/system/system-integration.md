# 系统集成清单：我们往镜像里加了什么、装在哪、谁触发、谁验证

> **这份文档是"总账"。** 项目往系统里塞的命令 / 服务 / udev 规则 / 配置越来越多，
> 日子久了最难的不是写，而是**回头找**——所以这里按"资产"组织，每一样都记清
> 「源文件 → 安装点 → 触发方式 → 验证项 → 细节在哪篇 note」。
>
> 与 AGENTS.md（镜像源码：`AGENTS.md`） 的分工：`AGENTS.md` 是**给 AI 的行动规则与已核实事实**
> （按主题写"注意什么"）；这里是**清单**（按"文件→位置"写"有什么、在哪"）。
> 两份都要维护，但改动的**入口**永远是下面第 0 节那三步。

---

## TL;DR

- **新增一样东西要同步改四处**（§0）：overlay 文件 → 构建脚本里 `install` → 触发（`enable` / udev /
  服务依赖）→ `tools/verify-image.sh` 的断言。少一处就会出现"手工打过补丁的板子没问题、新编的镜像里没有"。
- 本文按"资产"组织，先查表再翻代码：命令（§1）、systemd 单元（§2）、udev 规则（§3）、配置与资源（§4）、
  启动分区与设备树（§5）、对厂商东西打的补丁（§6）、因 `--no-install-recommends` 必须显式加的包（§7）。
- ⛔ §8 列的是**用户自己写的文件**（板卡 conf / uEnv / motd / `overlay/opt/` / 用户那 5 条 udev 规则）——
  **不要顺手改**。

## 0. 新增一样东西时，要同步改四处

少改一处就会留下隐患，所以列成硬规则：

| # | 改哪 | 干什么 |
| --- | --- | --- |
| 1 | `overlay/` | 放源文件（路径 = 镜像内路径，直接对应） |
| 2 | `scripts/build-lsp-image.sh` | 加安装块，**显式 `install -m <权限>`** |
| 3 | `tools/verify-image.sh` | 加检查项（它是本项目的回归测试，没加就不算改完） |
| 4 | 本文档 + `notes/README.md` 索引 | 登记 |

**例外：需要编译的东西（C / Rust / CMake）不放 `overlay/`，放 `apps/<name>/`**（见
[build/apps.md](../build/apps.md)）：源文件与 `files/`（原样进 rootfs 的树）都在应用目录里，
**装到哪由应用自己的 Makefile 声明**，所以第 2 步不用改脚本（`build-lsp-image.sh` 里只有一个
通用的 `apps/*/` 循环）；编译产物落 `build/apps/`、**不进 git**，第 3、4 步照旧。

三个反复踩到的点：

- **权限位必须显式写。** `git` 只记录可执行位，`chmod 600`/`0644` 的差别**不会跟着 clone 走**
  （克隆出来一律 0644）。踩过的：`adbkey`（必须 0600）、`lsp-usb`（必须 0755）。
  所以**单个文件一律 `install -m`**，不靠仓库里的权限。⚠️ `cp` 也别用：它的结果是
  "新建文件取 `源权限 & ~umask`、覆盖已有文件则保留目标文件的权限"，两条都不可控
  （2026-09-26 已把镜像构建里的单文件 `cp` 全部换成 `install -m`）。
  `cp -a` 更极端 —— 连 umask 都不看，直接照搬源权限（见下一条）。
  📌 构建脚本开头现在显式 `umask 022`（2026-09-26）：`mkdir -p`、shell 重定向建的配置文件、
  普通的 `cp` 全都吃 umask，实测宿主 umask 002 时同样一条命令会落出 `0664`/`0775`，
  而 `sudo` 默认 022 时是 `0644`/`0755` —— 一份源码两种结果。需要更宽/更严的
  （`device.yaml` 0660、sudoers 0440、token 0600）本来就在调用点用 `install -m` 写死了，不受影响。
- **`overlay/` 里同名文件会被包覆盖。** 安装顺序必须是「先装包、再拷 overlay」
  （脚印：`plymouth` 装包时把我们换过的 `watermark.png` / `ubuntu-logo.png` 覆盖成了 Ubuntu 原版）。
  构建脚本目前满足这个顺序；**新增文件时如果它和某个包同名，要确认拷贝发生在装包之后**。
- **整目录拷贝：`copy_overlay_dir` 拷完会归一权限/属主（2026-09-26 起）。**
  `cp -a` 是"原样保留"，会把**仓库 checkout** 的 `board:board 0664/0775` 直接带进镜像。
  工程机实测（修复前）：`/usr/share/keyrings/ros-archive-keyring.gpg`、
  `/etc/udev/rules.d/80-net-setup-link.rules`、`/etc/dconf/**`、
  `/usr/lib/NetworkManager/conf.d/**`、`/usr/share/glib-2.0/schemas/99-lsp-desktop.gschema.override`、
  `/opt/banner.txt` 全是 `-rw-rw-r-- board board`，`/opt/data_collection` 是 `board:board`；
  最糟的一处是 `copy_overlay_dir "etc/udev/rules.d"` **把前面显式 `install -m 0644` 装好的
  `99-lsp-typec-powerfix.rules` 又盖回 0664**（同一个文件装了两遍，后一遍说了算）。
  现在 `normalize_overlay_perms()` 在每个条目拷完后按 `install -m` 的语义归一：属主 `root:root`、
  去掉组/other 的写位（0664→0644、0775→0755）；**刻意收紧的文件（组/其他人不可读，如 0600 的 token）
  只改属主、不动权限位**；软链只改链接本身 ——
  ⛔ 所以**不能**把 `copy_overlay_dir` 的 `cp -a` 直接换成 `install`：`overlay/opt/data_collection/{current,previous}`
  是软链，解引用会把整棵 release 树复制一份。
  回归断言在 `tools/verify-image.sh` 的「overlay / app files 权限（cp -a 带进来的仓库属性）」组。
  一键审计（新增 overlay 文件后跑一次，确认"仓库里的组/other 写位没被带进镜像"）：
  ```bash
  find overlay -type f -perm /022 -printf '%m %p\n' | sort
  ```

  当前启动分区为 ext4，`*.dtbo` 同样需要正确属主与权限。其余目录合并包括
  往发行版目录里**合并**的 `/usr/share/{alsa,pulseaudio}`（保留包自己的权限）、
  以及 lightdm/sddm 的预留分支（目录不存在时不执行）。
  📌 `apps/*/Makefile` 的 `files/` 树同理（以前是 `cp -a files/.`，板上实测
  `/etc/systemd/system/lsp-powerkey.service`、`/etc/default/lsp-keyfilter` 是 664，
  `/usr/local/bin/lsp-mcap-open` 是 775）；2026-09-26 全部改成逐条 `install -D -m`，
  写法与理由见 [build/apps.md](../build/apps.md) §1。

---

## 1. 命令（`/usr/local/bin`、`/usr/local/sbin`）

| 命令 | 源（`overlay/`） | 安装点 | 权限 | 作用 | 细节 |
| --- | --- | --- | --- | --- | --- |
| `lsp-usb` | `usr/local/bin/lsp-usb` | `/usr/local/bin/` | 0755 | 固定 host/source 状态查询、`power source` 只读确认与外设 `reenumerate`；不提供 device 模式 | [usb-gadget.md](../usb/usb-gadget.md) |
| `lsp-usb-powerfix` | `usr/local/bin/lsp-usb-powerfix` | 同上 | 0755 | 旧角色配置的兼容纠正入口；当前固定 source 不依赖它供电 | [quest3-usb-power.md](../usb/quest3-usb-power.md) |
| `check-hdmi-plymouth.sh` | `usr/local/bin/check-hdmi-plymouth.sh` | 同上 | 0755 | 无显示输出时提前退出 plymouth，把控制台让给串口 | [branding-customization.md](branding-customization.md) §4 |
| `lsp-growroot.sh` | `usr/local/sbin/lsp-growroot.sh` | `/usr/local/sbin/` | 0755 | 首启把根分区扩到整盘 | system/tuning/（镜像源码：`notes/system/tuning`） |
| `lsp-ethernet-direct` | `usr/local/sbin/lsp-ethernet-direct` | 同上 | 0755 root:root | 根据当前 NM UUID / 固定地址启动 DHCP-only 进程，或协调接口对应服务；不桥接、不配置 NAT | [ethernet-direct.md](ethernet-direct.md) |
| `lsp-swapfile.sh` | `usr/local/sbin/lsp-swapfile.sh` | 同上 | 0755 | 首启用 `fallocate` 创建 `/swapfile` 并 `swapon -p`（镜像里**不预置** swapfile） | [swap.md](tuning/swap.md) |
| `lsp-journal-serial` | `apps/lsp-journal-serial/src/lsp-journal-serial.c` | `/usr/local/sbin/` | 0755 root:root | C 写入器：将 stdin 写入指定串口，登录挂断时重开并续写；由通用 apps 阶段构建、rootfs 阶段安装，源码另存 `/usr/local/src/lsp-journal-serial.c` | [journal-serial.md](journal-serial.md) |
| `lsp-config` | `usr/local/sbin/lsp-config` | 同上 | 0755 | 板级配置总入口（whiptail 菜单 + 子命令）：看状态 / USB 主机查询 / WiFi（看状态、只保留 example-project、改密码）/ 系统维护（交换空间 / 服务 / 主机名 / `device.yaml` / 操作日志）/ 重启关机。主菜单 5 项 + `0 退出`，**只放不改配置的入口**；改 board 登录密码只作 `passwd` 子命令、**不在菜单里**。**只做前端**，USB 一律转 `lsp-usb`；**每次调用与每处改动都记到 `/var/log/lsp-config.log`（0640 root:adm，密码不记）**；whiptail 菜单用 `NEWT_COLORS` 设成类 menuconfig 的**天蓝底**（默认是刺眼的紫底），可用环境变量覆盖 | [lsp-config.md](lsp-config.md) |
| `lsp-init-sdcard` | `usr/local/sbin/lsp-init-sdcard` | 同上 | 0755 | 数据 SD 卡初始化：新卡或显式 `--force` 时整盘重建分区、ext4 + `Data1T`，维护卷标设备触发的 fstab 条目，挂到 `/run/lsp_data_1T` 后发布 `/mnt/data_1T`，设置属主与自检。已有 ext4 不重格式化，布局未铺满时拒绝；拒绝系统盘，支持 `--dry-run`。标准数据卡的自动挂载不依赖运行初始化器 | [init-sdcard.md](../storage/init-sdcard.md) |
| `lsp-mpp-load.sh` | `usr/local/sbin/lsp-mpp-load.sh` | 同上 | 0755 | **打开 MPP/VPU 负载统计**：把 `load_interval`（默认 1000ms，可被 `/etc/default/lsp-mpp-load` 覆盖）写进 `/proc/mpp_service/load_interval` —— 不开的话驱动完全不统计，`cat /proc/mpp_service/load` 只回一行提示。读占用率不需要 root，实测开销 ~0.1%（噪声级，见我写的 note） | [chromium-rkmpp.md](chromium-rkmpp.md) §VPU |
| `lsp-usb-no-autosuspend.sh` | `usr/local/sbin/lsp-usb-no-autosuspend.sh` | 同上 | 0755 | **关闭 USB 自动挂起**：把每个 USB 设备的 `power/control` 写成 `on`。不带参数 = 整机扫一遍（开机 service 用），带 `/devices/...` = 只处理一个（udev `RUN+=` 用，接口设备会连父设备一起关）。配合启动参数 `usbcore.autosuspend=-1`（内核级真不挂起）与 udev 规则 | [usb-autosuspend-off.md](tuning/usb-autosuspend-off.md) |
| `lsp-powerkey` | `usr/local/sbin/lsp-powerkey`（**app 目录** `apps/lsp-powerkey/`：源码 `src/lsp-powerkey.c`，配置在 `files/`；镜像里另存一份源码到 `/usr/local/src/lsp-powerkey.c`） | 同上 | 0755 | **电源键长按处置（C 程序，交叉编译的静态二进制）**：自己读 `/dev/input/eventN`（**按设备名 `rk805 pwrkey` 找，编号会漂移**），按"**按下**"计时 —— 2s `systemctl --no-block poweroff`、7s 再发一次（兜底）、10s 写 PMIC `DEV_OFF`（`/sys/rk806single/debug` ← **`w 72 01`**）强制断电；不足 2s 松手什么都不做。`--setup` 子命令把 PMIC 长按兜底从 6s 放宽到 12s（写 `0x76=0xB6`，⛔ 不能碰 bit7）。必须 root（`eventN` 是 `root:input 0660`，且要写 sysfs）。日志带绝对时间写 `/var/log/lsp-powerkey.log`（每行 fsync）。`LSP_POWERKEY_DRYRUN=1` 只打日志。**二进制不进 git**：`./build.sh -a lsp-powerkey` 编到 `build/apps/lsp-powerkey/`，`rootfs` 阶段由 `make -C apps/lsp-powerkey install-only` 安装预编译产物 | [rk806-pwron.md](../hardware/rk806-pwron.md) §8、[build/apps.md](../build/apps.md) |
| `lsp-sync-data` | `usr/local/sbin/lsp-sync-data`（**app 目录** `apps/lsp-sync-data/`：源码 `src/lsp-sync-data.c`，配置在 `files/etc/default/lsp-sync-data`；镜像里另存一份源码到 `/usr/local/src/lsp-sync-data.c`） | 同上 | 0755 | **采集数据"封口即落盘"兜底（C 程序）**：递归 inotify 盯采集根（默认 `/mnt/data_1T/rosbags`），**只在 `metadata.json` 落盘时**（`CLOSE_WRITE` / `MOVED_TO`）对数据盘 `syncfs` 一次 —— 采集侧 `episode_recorder.py::_seal()` 写 metadata.json **没有 fsync**（只有关机前 `os.sync()`），这一条防"最后一集刚采完就被拔电"。细节：新 episode 目录收到 `IN_CREATE\|IN_ISDIR` 就补 watch；`IN_Q_OVERFLOW` 保守刷；触发按 `LSP_SYNC_DATA_COOLDOWN_MS`（默认 1000ms）**合并不丢**；启动刷一次；`--check` / `--once` 子命令。日志全 ASCII。**二进制不进 git**：`./build.sh -a lsp-sync-data` 编到 `build/apps/lsp-sync-data/` | [data-card-durability.md](../storage/data-card-durability.md) |
| `boot-data-collection-update` | `usr/local/sbin/boot-data-collection-update` | `/usr/local/sbin/` | 0755 root:root | 数据采集更新协调器：校验签名发布通道，管理下载、暂存、健康检查与 `current` 切换，启停两个业务服务。`--check-only` 只读检查；正常启动所需 `boot-enabled` 由现场创建，镜像不预置 | 本文 §4 |
| `download-data-collection-release` | `usr/local/sbin/download-data-collection-release` | 同上 | 0755 root:root | 按所选配置从 GitLab 或 BOS 下载并验证 release；GitLab 使用 deploy token，BOS 使用独立 SDK venv。BOS 下载命令只验证，拒绝 `--install` | 本文 §4 |
| `install-data-collection-release` | `usr/local/sbin/install-data-collection-release` | 同上 | 0755 root:root | 验证或暂存下载的 release；协调器使用 `--stage-only` 将内容放到 `/opt/data_collection/releases/<版本>`，后续激活由协调器管理 | 本文 §4 |
| `resize.sh` | `etc/profile.d/resize.sh` | `/etc/profile.d/` | 0644 | 登录时把终端尺寸同步给伪终端 | — |

构建脚本中的 `lsp-usb` 安装与 device 残留清理按同一主机策略维护；其它定位锚点：
`# ---- Type-C 电源角色自动纠正（host + sink → source）----`、`# ---- 关闭 USB 自动挂起（相机 2 秒空闲就被挂起，重新出流有几秒异常）----`、
`# ---- apps/：把每个应用的产物装进 rootfs ----`（通用循环：应用自己的 `apps/<name>/Makefile`
声明装到哪，详见 [build/apps.md](../build/apps.md)）、
`# swapfile：首启创建 + 启用（脚本 + 单元 + 配置 + swappiness）`、
`# ---- 板级配置工具 lsp-config（whiptail 菜单 + 可脚本化的子命令）----`、
`# ---- 数据采集系统（data-collection，2026-09-23 集成）----`、
`# 启动 logo`（plymouth 三件套）。

## 2. systemd 服务与单元覆盖

| 单元 | 源（`overlay/`） | 镜像内路径 | 开机自启 | 作用 / 触发 |
| --- | --- | --- | --- | --- |
| `lsp-journal-serial.service` | `etc/systemd/system/` | 同上 | **是**（`multi-user.target`） | journalctl 回放并跟随本次开机告警，经管道交给 C 写入器输出到 `/dev/ttyFIQ0`；登录挂断仅重开串口，不重启读取进程。见 [journal-serial.md](journal-serial.md) |
| `lsp-ethernet-direct@.service` | `etc/systemd/system/` | 同上 | **否** | NM dispatcher 在直连配置激活后按接口启动，绑定接口设备生命周期；租约仅存 `/run/`，拔线停止。见 [ethernet-direct.md](ethernet-direct.md) |
| `lsp-usb-powerfix.service` | `etc/systemd/system/` | 同上 | **是**（`multi-user.target`） | 旧配置兼容检查；Type-C change 非阻塞触发，20 秒限制 swap。当前固定 source 不需角色纠正，见 [quest3-usb-power.md](../usb/quest3-usb-power.md) |
| `lsp-usb-no-autosuspend.service` | `etc/systemd/system/` | 同上 | **是**（`multi-user.target`，`After=multi-user.target`） | 开机后把整机 USB 的 `power/control` 扫成 `on`（补上"设备在 udev 读到规则之前就枚举完"的那批）；热插拔由 udev 规则处理 |
| `lsp-powerkey.service` | `etc/systemd/system/` | 同上 | **是**（`multi-user.target`，`After=systemd-logind.service`） | 跑 `/usr/local/sbin/lsp-powerkey`，`User=root`，`Restart=always`；`ExecStartPre=... --setup` 先把 PMIC 长按兜底从 6s 放宽到 12s（易失寄存器，每次开机都要设）。**电源键的处置权在这里**：logind 已被 drop-in 设成 ignore（见下）。⚠️ 没有它长按就没有任何反应。logind 那条 drop-in 是**第二层保险**：正常环境（带桌面、只是不接显示器）GNOME 的 block 型 inhibitor 已经挡住 logind，但**一旦 GNOME 不在**（图形会话挂了、或将来做无桌面版本）logind 就会拿走电源键且不看时长 —— 板上对照实测：停掉 gdm 后同样 95ms 轻点，`ignore` 无反应、`poweroff` 立刻关机 | [rk806-pwron.md](../hardware/rk806-pwron.md) §8 |
| `run-lsp_data_1T.mount` | `image` 阶段写 `/etc/fstab`，由 `systemd-fstab-generator` 生成；`etc/systemd/system/run-lsp_data_1T.mount.d/10-lsp-data-path.conf` | `/run/systemd/generator/`、`/etc/systemd/system/…d/` | 卷标 `.device` 触发 | `LABEL=Data1T` 出现即挂到 `/run/lsp_data_1T`，移除即停止；只由设备拉起，不加入 `local-fs.target`，无卡不排挂载任务。挂载成功后拉起入口发布服务，见 [init-sdcard.md](../storage/init-sdcard.md) |
| `lsp-data-card-path.service` | `etc/systemd/system/`、`usr/local/sbin/lsp-data-card-path`、`etc/tmpfiles.d/lsp-data-card.conf` | 同上 | 真实挂载单元拉起 | 挂载成功后发布 `/mnt/data_1T -> /run/lsp_data_1T`，再启动同步；停止、失败时收回入口。镜像不预建公开入口；tmpfiles 清理开机残留并预建受限 `/run` 底层目录，见 [init-sdcard.md](../storage/init-sdcard.md) |
| `lsp-sync-data.service` | `apps/lsp-sync-data/files/etc/systemd/system/` | `/etc/systemd/system/` | **是**（`multi-user.target`，**构建脚本 enable**） | 跑 `/usr/local/sbin/lsp-sync-data`，`User=root`、`Restart=always`、`RestartSec=5`；**`ConditionPathIsMountPoint=/run/lsp_data_1T`**，跟随公开路径发布服务启动/停止（没插数据卡 ⇒ 整个单元不启动，不留 failed）、**`Nice=19` + `IOSchedulingClass=idle`**（`syncfs` 可能要等卡 flush，绝不跟采集抢 IO）。为什么默认开：它是"掉电丢最后一集"的兜底，不默认生效等于没有；⚠️ 但它**保不住卡自己的内部缓存** —— 物理拔电仍可能丢，零丢失要靠采集侧在封口点 fsync | [data-card-durability.md](../storage/data-card-durability.md) |
| `lsp-kiosk.service` | `etc/systemd/system/` | 同上 | **是**（`graphical.target`；构建期 disable gdm3） | kiosk 账户、PAM login、tty1，与 GDM/getty 互斥。Cage + Chromium 与后端并行启动；极简引导页以 HEAD 等待响应头后导航，连接失败按秒重试，不复制 HMI；按键过滤、浏览器策略、VT/CAD 兜底已启用。`RestartSec=0`、关闭启动限流、停止直接 SIGKILL，预算 20s。UID 与配置配套见 [session-design.md](kiosk/session-design.md) |
| `logind.conf.d/99-lsp-powerkey.conf` | `etc/systemd/logind.conf.d/` | 同上 | 单元覆盖（非服务） | logind drop-in：`HandlePowerKey=ignore` + `HandlePowerKeyLongPress=ignore`。把电源键交给 `lsp-powerkey`；实测 logind 会打 `Handling of handle-power-key (edge) is disabled, taking no action.`。**实测：systemd 255 支持 `logind.conf.d/`**（移走文件重启 logind → 属性回 `poweroff`，放回 → `ignore`） | [rk806-pwron.md](../hardware/rk806-pwron.md) §3.4 |
| `lsp-time-save.service` | `usr/lib/systemd/system/` | 同上 | **是**（`FAKE_HWCLOCK=Y` 时） | `Type=simple` 后台等待 chrony 首次同步收敛，原子保存 fake-hwclock 存档后退出；不拉 chrony-wait，不设置系统时钟 / RTC。N 时 disabled | [time-sync.md](tuning/time-sync.md) |
| `lsp-growroot.service` | `usr/lib/systemd/system/` | `/usr/lib/systemd/system/` | **是** | 首启扩容 |
| `lsp-mpp-load.service` | `usr/lib/systemd/system/` | `/usr/lib/systemd/system/` | **是**（`multi-user.target`） | 开机把 `/proc/mpp_service/load_interval` 设成 1000ms，让 `cat /proc/mpp_service/load` 能看 VPU 占用率（procfs 设置**重启恢复**，所以要每次开机设）；`ConditionPathExists=/proc/mpp_service/load_interval`，内核没开 MPP procfs 时直接算成功、不留 failed 单元 |
| `lsp-swapfile.service` | `usr/lib/systemd/system/` | 同上 | **是**（`multi-user.target`） | 首启创建并启用 `/swapfile`（`After=local-fs.target lsp-growroot.service`，**不参与 `swap.target`**） |
| `plymouth-quit-wait.service.d/check-hdmi.conf` | `etc/systemd/system/` | `/etc/systemd/system/…d/` | 覆盖 | 保留无屏检查；等待动作由字典序更后的 `zz-lsp-keep-splash.conf` 覆盖为 `/bin/true`，退出动画交给 kiosk。见 [session-design.md](kiosk/session-design.md) |
| `systemd-networkd-wait-online.service.d/override.conf` | `etc/systemd/system/` | 同上 | 覆盖 | 桌面用 NetworkManager，**装完会显式删掉** |
| `data-collection-boot.service` | `etc/systemd/system/` | `/etc/systemd/system/` | **否**（刻意 disabled） | 每次开机跑一次 `boot-data-collection-update`（`ExecStart=/usr/bin/python3 -I /usr/local/sbin/boot-data-collection-update`）。⛔ **镜像里不 enable**：由现场流程先写 `/etc/data-collection-updater/boot-enabled`、再用**部署时才装上**的 `enable-collection-autostart` 打开（⛔ 那个 wrapper 与配套 sudoers 都不进镜像，2026-09-23 用户明确）。`TimeoutStartSec=infinity` —— 网络/下载各自有超时，不许因为"总时长"在切版本时把进程 SIGKILL | 本文 §4 |
| `collect_system.service` | 同上 | 同上 | **否**（默认 disabled/stopped） | 业务：跑 `/opt/data_collection/current/share/data_collection_trigger/scripts/run_collect_system.sh`（`User=board`）。默认由 `boot-data-collection-update` start/stop；kiosk 可通过固定 sudo 命令显式 enable 或 start，enable 会绕过更新器的启动门控，见 [sudo-policy.md](kiosk/sudo-policy.md)。⚠️ 它的"所有设备断开"多半是 **DDS 共享内存段被 unlink**（不是设备掉线），判定/恢复见 [collection-dds-shm-fault.md](collection-dds-shm-fault.md) | 本文 §4 |
| `device_reporter.service` | 同上 | 同上 | **否**（必须 disabled） | 业务：同一 release 里的 `run_device_reporter_system.sh`（MQTT 上报），同样归协调器管 | 本文 §4 |

> 厂商包与 unit 本体保留，但 `usbdevice.service` 由 `/etc` 下的 mask 禁用（见第 6 节）；
> 本工程不提供启动该 device 栈的入口。

## 3. udev 规则

`overlay/etc/udev/rules.d/` 是**整目录拷贝**（`copy_overlay_dir`），所以**往目录里丢新规则不需要改脚本**。
唯一例外：`99-gdm-hack.rules` 被显式排除（它只属于 gdm3 分支）。

| 规则 | 作用 | 谁加的 |
| --- | --- | --- |
| `99-lsp-typec-powerfix.rules` | typec 端口 `change` 事件 → `systemctl --no-block start lsp-usb-powerfix.service`，支持已激活端口的后续事件 | 我们 |
| `99-lsp-usb-no-autosuspend.rules` | 任何 USB 设备/接口的 `add` 事件 → `RUN+=` 调 `lsp-usb-no-autosuspend.sh %p` 把 `power/control` 写成 `on`（相机被 uvcvideo 主动开了自动挂起，见 [usb-autosuspend-off.md](tuning/usb-autosuspend-off.md)） | 我们 |
| `99-chest-camera.rules` | 胸前相机 `05a3:9230` → `/dev/chest_cam_video`（`index=0`）与 `/dev/chest_cam_metadata`（`index=1`），`video` 组 0660 | **用户手加** |
| `99-left-hand-camera.rules` | 左手相机 `0abd:8050`，按 **USB 路径** `platform-fc800000.usb-usb-0:1:1.0` 定位 → `/dev/camera_left`，`video` 组 0660 | **用户手加** |
| `99-right-hand-camera.rules` | 右手相机，**只按 USB 路径** `platform-xhci-hcd.11.auto-usb-0:1:1.0` 定位（不锁 VID/PID）→ `/dev/camera_right`，`video` 组 0660 | **用户手加** |
| `99-lcd_voice_bridge.rules` | 三块 ESP32-S3 串口板（同为 `303a:1001`），**靠 MAC（`ID_SERIAL_SHORT`）区分** → `/dev/lcd_voice_bridge` / `/dev/glove_left` / `/dev/glove_right`，0666 | **用户手加** |
| `99-meta-quest.rules` | Oculus（VID `2833`）USB 设备节点 `MODE=0666 GROUP=plugdev`，供非 root 使用 adb；不覆盖其他 Android 厂商 | 现场设备规则 |
| `99-gdm-hack.rules` | GDM 首次 HDMI 热插拔卡死修复（仅 gdm3 分支单独拷） | 上游 |
| `80-net-setup-link.rules` | 网卡命名 | 上游 |

> ⚠️ **用户手加的这 5 条是"场地相关"的，换硬件/改拓扑就会失效**，别当成通用规则：
> - 左右手相机是**按 USB 路径**匹配的（`platform-fc800000.usb…` / `platform-xhci-hcd.11.auto…`），
>   插到别的口或换个集线器，路径变了就匹配不上（symlink 不出现，且**不会报错**）；
> - 三块 ESP32 板子 VID:PID 完全相同，**只能靠 MAC** 区分 —— 换板子必须改规则里的 MAC；
> - 仓库只维护 Quest 专用的 `99-meta-quest.rules`；非 Quest 设备按需核对并添加规则。
> 排查这类 symlink 用 `udevadm info -q all -n /dev/video0` 看 `ID_PATH` / `ID_SERIAL_SHORT` 实际值；
> **完整的定位方法**（`lsusb` → 端口路径 → `ID_PATH` → 写规则）见 [device-mapping-method.md](../usb/device-mapping-method.md)。
> 另注：左右手相机的 **VID/PID/序列号完全相同**，这是它们必须按口区分的原因。

## 4. 配置与资源

| 内容 | 源（`overlay/`） | 镜像内路径 | 说明 |
| --- | --- | --- | --- |
| 上位机源码与预编译 | 独立 `oem-host` 阶段调用 `tools/sync-oem-host.sh` 生成 `opt/oem-host/` | `/opt/oem-host/`，root:root、服务只读 | `rootfs` 阶段之后获取 GitLab `board/oem-host` 的 `main` 最新提交、安装依赖与预编译；`SOURCE` 记录 URL、分支与完整 SHA。`.git` 和构建机 `build/oem-host.git` 缓存不进入镜像；提交变化只使上位机及 `board` / `image` 阶段重做，镜像体检核对来源及无 `.git`。见 [源码同步](oem-host-upstream-sync.md) |
| 上位机采集报告 | `etc/oem-host.env` | `/var/log/oem_host_log/collect/reports` | `example-project_COLLECTION_REPORT_DIR` 复用 kiosk 已有可写目录与日志清理策略；不使用受 `ProtectSystem=strict` 限制的上游默认目录。见 [上位机契约](oem-host-integration.md) |
| 上位机工位复位 | 上游 `deploy/image-reset/`、`etc/sudoers.d/99-lsp-kiosk` | `/opt/oem-host/deploy/image-reset/` | 固定隔离 Python `worker.py --request` 创建 UUID 临时服务，无固定 reset `.path/.service`。六个原件与 overlay 核对；应用与九项系统摘要从本镜像生成，不复制上游设备摘要。目录 0755、文件 0644、root:root；体检核对保护和摘要。仅构建集成，真重置未执行，见 [上位机契约](oem-host-integration.md#工位复位与数据盘挂载) |
| 上位机采集准备 | 上游 `deploy/collection-service/{worker.py,oem-host-collection-service.path,oem-host-collection-service.service}` | `/usr/local/lib/oem-host-collection-service/worker.py`、`/etc/systemd/system/` 两个同名单元 | 三文件 root:root 0644，父目录 0755；只 enable `.path` 监听固定请求，不运行 worker 或采集、不扩 sudo 权限。体检比较同提交来源和安装副本；回执/实际请求另验，见 [上位机契约](oem-host-integration.md#采集服务准备) |
| 数据卡 fstab | `scripts/build-lsp-image.sh` `image` 阶段生成 | `/etc/fstab`，0644 root:root | 内置 `LABEL=Data1T /run/lsp_data_1T ext4`，与初始化器统一使用 `nosuid,nodev`；`x-systemd.wanted-by=` 仅卷标设备，配套 `device-bound`、`nofail`、5 s 设备超时；不创建 `/mnt/data_1T`。`tools/verify-image.sh` 检查挂载选项、条目与公开入口边界；见 [init-sdcard.md](../storage/init-sdcard.md) |
| adb 主机密钥 | `home/board/.android/{adbkey,adbkey.pub}` | `/home/board/.android/` | 目录 **0700**、私钥 **0600**、公钥 0644；属主 board |
| 采集更新引导目录 | `home/board/updater-bootstrap/` | `/home/board/updater-bootstrap/` | 顶层目录 **0775 board:board**；保留内部文件权限与软链，使用目标 board 家目录 UID/GID 归属，不套用系统 overlay 的 root:root / 去组写归一规则。镜像检查核对目录属性，overlay 部署使用相同目录约定 |
| 设备配置 | `home/board/.lsp/config/device.yaml` | `/home/board/.lsp/config/` | 文件 **0660**、目录 **2775（setgid，属主 board:board）** —— 上位机（`User=kiosk`）**免 sudo** 改写的依据是 **kiosk ∈ board 组**：`/home/board` 本身是 0750，不在组里连目录都进不去。⛔ 2026-09-26 实测：构建脚本漏了 `gpasswd -a kiosk board` ⇒ 新刷的镜像里 HMI 改设备配置 Permission denied（`tools/verify-image.sh` 报"kiosk 不在 board 组里"），已补 |
| 登录横幅素材 | `opt/banner.txt`（字标，`00-header` 打印）；`opt/logo.txt`（图形标，**仍在镜像里但 00-header 不再打印** —— 2026-09-26 用户要求登录横幅里不显示）| `/opt/` | `update-motd.d/00-header` 读取字标 |
| motd 片段 | `etc/update-motd.d/{00-header,30-sysinfo}` | 同路径 | 登录欢迎信息（**用户自己写的，不要顺手改**；2026-09-26 用户点名改过两处：版本行加 `example-project` 前缀、`30-sysinfo` 的 IP 改成 `ip -4 -o addr show scope global` —— 原委见 [hostname-and-motd.md](hostname-and-motd.md)） |
| journal 串口隔离 | `etc/systemd/journald.conf.d/10-lsp-console.conf` | `/etc/systemd/journald.conf.d/` | `ForwardToConsole=no`；独立 `lsp-journal-serial.service` 只输出本次开机的 warning 及以上。见 [journal-serial.md](journal-serial.md) |
| journal 持久化与上限 | `etc/systemd/journald.conf.d/10-lsp-journal-size.conf` | 同路径 | `Storage=persistent` + `SystemMaxUse=200M` + `SystemKeepFree=1G`；完整日志供开机回溯 |
| systemd 状态行 | `etc/systemd/system.conf.d/10-lsp-status.conf` | 同路径 | `ShowStatus=auto`，与 journal 串口输出分别控制 |
| 镜像版本指纹 | （构建脚本 `rootfs` 阶段生成，不在 overlay 里） | `/etc/lsp-image-release` | **0644 root:root**，8 个字段：`LSP_IMAGE_{BOARD,DESKTOP,KERNEL,GIT_SHA,GIT_DIRTY,BUILD_TIME,BUILD_HOST,SCRIPT_SHA256}`。`DESKTOP` 记录 `ubuntu-desktop` / `xfce4` / `kde`，用于拒绝跨桌面复用 rootfs，见 [缓存规则](../build/build-internals.md#1-不再压缩完立刻解压阶段间直接复用-buildrootfs)。源码副本没有 `.git` 时，git 字段写 `unknown`（可用 `LSP_GIT_SHA=<sha>` 显式传入），以脚本 sha256 兜底。静态体检检查桌面字段唯一有效。见 [boot-time-optimizations.md](boot-time-optimizations.md) 第 7、11 条 |
| 时间同步 | `etc/chrony/chrony.conf`、`etc/default/lsp-time-sync`、`usr/local/sbin/lsp-time-sync` | 同路径 | 国内 NTP 与现场 conf.d / sources.d；chrony 后台校时并 `rtcsync`。新增 `lsp-time-save.service` 首次同步收敛后保存存档，不门控本地启动；`image` 阶段刷新出厂基准并写 `/etc/lsp-time-seed.json`。诊断区分 RTC 开机有效性、存档与 NTP 状态，见 [time-sync.md](tuning/time-sync.md) |
| 硬件 RTC 单元 | `usr/lib/systemd/system/rtc-hym8563.service` | 同路径 | `hwclock --hctosys --utc`；**只拷文件、没 enable，将来也别 enable**（内核 `rtc_hctosys()` + chrony `rtcsync` 已经做了两个方向，它是多余的）—— 见 [time-sync.md](tuning/time-sync.md) |
| APT 排除 | `etc/apt/preferences.d/{no-snap,no-libreoffice}`（另有 `rockchip-multimedia-ppa`：`Pin-Priority: 1` 压住 PPA 版，**Origin 必须与实际 PPA 一致，否则 pin 静默失效**）| 同路径 | `Pin-Priority: -1`：apt 直接跳过，不报依赖错（只防新装）。LibreOffice 历史上还靠 `rootfs` 阶段的 `PURGE_LIBREOFFICE` 清旧 rootfs 残留，2026-09-14 已删（实测无残留，纯死代码）；防回归仍在 `tools/verify-image.sh`「移除项」 |
| ROS 签名密钥 | `usr/share/keyrings/ros-archive-keyring.gpg` | 同路径 | 内置，构建期不联网（见 [apt-sources-deb822.md](../build/apt-sources-deb822.md)） |
| 厂商 SDK 排除 | `etc/dpkg/dpkg.cfg.d/01-lsp-vendor-sdk` | 同路径，0644 | `rootfs` 阶段在厂商 deb 解包前安装；仅过滤 FFmpeg / RKAIQ 的精确 SDK 路径，升级 / 重装继续生效；见 [镜像裁剪](../build/image-size-trimming.md) |
| NM 全局托管 | `usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf` | 同路径 | 管理 WiFi、Ethernet、GSM、CDMA；其余虚拟接口由应用管理 |
| 有线固定 IP / DHCP 角色 | `etc/NetworkManager/system-connections/lsp-ethernet-*.nmconnection` | 同路径，0600 root:root | 按两个 GMAC 的 `ID_PATH` 匹配；产品默认单实例服务端 `<HOST_13>/24`。开发板接上游时显式关闭直连自动连接并启用对应客户端，客户端无限重试、不自动切角色；见 [ethernet-direct.md](ethernet-direct.md) |
| 直连 DHCP 触发与配置 | `etc/NetworkManager/dispatcher.d/90-lsp-ethernet-direct`、`etc/lsp-ethernet-direct/dnsmasq.conf` | 同路径；dispatcher 0755，配置 0644 root:root | 仅直连配置活动时发 DHCP；关闭 DNS，不下发默认网关 / DNS，不启用共享网络 |
| 桌面默认值 | `usr/share/glib-2.0/schemas/99-lsp-desktop.gschema.override`、`etc/dconf/db/local.d/{00-favorite-apps,01-desktop-behavior,02-removable-media,03-default-terminal,04-numlock}` | 同路径 | 永不熄屏/不锁屏/不休眠；收藏夹保留 Chromium、Nautilus、Terminal、mpv，去掉 Yelp / Rhythmbox；GNOME Terminal 作为默认终端，兼容 schema、`Ctrl+Alt+T` 和 `x-terminal-emulator` 三入口一致；NumLock 默认开启并记忆用户状态，不加锁，见 [桌面会话](desktop-session.md#默认终端与-numlock)。可移动介质不自动挂载，保留手动挂载，见 [removable-media-policy.md](removable-media-policy.md) |
| GNOME 新 4K 屏默认缩放 | `etc/xdg/autostart/lsp-gnome-display-defaults.desktop`、`usr/local/libexec/lsp-gnome-display-defaults` | 同路径，0644 / 0755 root:root | GNOME Wayland 登录时，一次性将未保存配置的原生 4K 单屏设为 200% 并保存标准 `monitors.xml`；保留用户选择，非 4K / 多屏跳过，见 [默认缩放](gnome-display-defaults.md) |
| 壁纸与锁屏 | `usr/share/backgrounds/{wallpaper,lockscreen}.png` | 同路径 | 由 `tools/make-assets.sh` 产出 |
| plymouth 素材 | `ubuntu-logo.png`、`ubuntu-logo-icon.png`、`bgrt-fallback.png`、`usr/share/plymouth/themes/spinner/watermark.png` | `/usr/share/plymouth/…`、`/usr/share/pixmaps/` | **与包同名，必须晚于装包拷贝** |
| initramfs 钩子 | `etc/initramfs-tools/conf-hooks.d/plymouth` | 同路径 | 让 plymouth 进 initramfs |
| 固件/内核后处理 | `etc/initramfs/post-update.d/zz-update-firmware`、`usr/local/sbin/lsp-merge-dtb` | 同路径 | 重建启动分区 `dtbs/`，保留自定义 dtbo；运行依赖 `device-tree-compiler` 提供 `fdtoverlay` / `fdtget`，verify-image 离线检查包和命令 |
| GDM 配置 | `etc/gdm3/custom.conf` | 同路径 | Wayland + 自动登录（自动登录用户由脚本 `sed` 填入） |
| swapfile 配置 | `etc/default/lsp-swapfile` | 同路径 | `SWAP_SIZE_MB`（构建期由 `LSP_SWAP_SIZE_MB` 覆盖写入）、`SWAP_PRIORITY`（**只能 0..32767**，负值 `swapon` 静默忽略） |
| swap 内核参数 | `etc/sysctl.d/99-lsp-swap.conf` | 同路径 | `vm.swappiness=10`：8G 内存 + eMMC，不主动换出匿名页（**这个才是"少用 swap"的开关**，不是 `swapon -p`） |
| 电源键阈值 | `etc/default/lsp-powerkey` | 同路径 | `LSP_POWERKEY_HOLD_{GRACEFUL,RETRY,FORCE}`（毫秒，默认 2000/7000/10000；graceful/retry 设 0 = 禁用该级），`LSP_POWERKEY_{GRACEFUL,RETRY}_CMD`（替换那两级执行的命令，测试时用 `/bin/true` 隔离）、`LSP_POWERKEY_DRYRUN` 调试开关、`LSP_POWERKEY_{LOG,LOG_MAX_KB,SET_LP_OFF_TIME}`。启用时三者必须递增；`FORCE` 必须小于 PMIC 硬件长按（守护进程把它放宽到 12 秒；**那个定时器关不掉**，见 [rk806-pwron.md](../hardware/rk806-pwron.md) §4.1） |
| 采集账户免密关机 | `etc/sudoers.d/99-data-collection-poweroff` | `/etc/sudoers.d/` | **0440 root:root**，构建期 visudo 校验；仅 `board` 无参数调用 `/usr/sbin/poweroff`，兼容 `/sbin/poweroff`。内容与权限纳入镜像体检；只读授权验证见 [sudo-policy.md](kiosk/sudo-policy.md#采集账户的免密关机) |
| kiosk 免密 sudo 白名单 | `etc/sudoers.d/99-lsp-kiosk` | `/etc/sudoers.d/` | **0440 root:root**，构建期 `visudo -cf` 校验。授权关机/重启、数据卡初始化、设备绑定、`collect_system.service` 的 enable/start 与单独 stop、隔离 Python 复位 worker 固定请求入口；维护授权见下行。kiosk 不加入 sudo 组，不授权更新器安装、数据盘 worker、其它业务单元 stop 或通用 install/udevadm。完整边界与检查见 [sudo-policy.md](kiosk/sudo-policy.md) |
| 设备绑定工具 | `usr/local/sbin/lsp-set-rule` | 同路径 | **0755 root:root**。`list` / `show` / `set [--dry-run] <角色> <VID:PID> …` / `restore <角色>`。⛔ **VID:PID 必选**：一次只改这个型号那一行，别的型号的行原样保留。角色分两级：**文件级** `camera-left` / `camera-right` / `camera-chest`（`port=<DT 节点名>` 或 `serial=<序列号>`，同一型号二选一）、`lcd-voice-bridge`（`lcd=` / `glove_left=` / `glove_right=`，没给值的沿用当前文件里的值，一次重写三行）；**单设备级** `lcd` / `glove_left` / `glove_right`（`serial=<序列号>`，只改自己那一行，`show` / `restore` 也只作用于这一行）。新生成的相机 / tty 规则均为 `MODE="0666"`、不设置 GROUP；现有现场规则不批量改写。⛔ **规则行的形状写死在脚本里**（`emit_line`：按口 `KERNELS`+`ENV{ID_VENDOR_ID}`；按 SN `ATTRS{idVendor}`+`ENV{ID_SERIAL_SHORT}`；tty 同 tty 写法）⇒ `RUN+=` / `PROGRAM==` 注入不进来。写完自己跑 `udevadm control --reload-rules` + `trigger` + `settle --timeout=10`。审计 `/var/log/lsp-set-rule.log`（0640 root:adm）。见 [lsp-set-rule.md](lsp-set-rule.md) |
| 设备绑定原始副本 | `usr/share/lsp-set-rule/orig/*.rules` | 同路径 | **0644**，4 份（左右手/胸前相机 + LCD 手套），`lsp-set-rule restore` 用。构建脚本从**同一份** `overlay/etc/udev/rules.d/` 装到 `/etc/udev/rules.d/` 和这里，避免以后两处漂移 |
| 维护模式开关 | `usr/local/sbin/lsp-maintenance` | 同路径 | **0755 root:root**。`on` 校验密码后临时启动 GDM；`off` 用系统级临时单元 `lsp-maintenance-off.service` 停 GDM、撤维护标记并按 HDMI 状态恢复 kiosk，避免桌面终端关闭中断退出任务。依赖 `systemd-run`；`status` / `passwd` 与审计日志权限不变。见 [maintenance-mode.md](kiosk/maintenance-mode.md) |
| 维护模式 sudo 白名单 | `etc/sudoers.d/99-lsp-maintenance` | `/etc/sudoers.d/` | **0440**，构建期 visudo 校验；仅放行 kiosk 调用 `on/off/status` 和无参数状态查询，`passwd` 不授权 kiosk。进入维护模式仍由脚本校验口令，见 [maintenance-mode.md](kiosk/maintenance-mode.md) |
| 维护模式密码 | （构建脚本 `install`） | `/etc/lsp-maintenance.passwd` | **0600 root:root**，内容为 `openssl passwd -6` 的 sha512-crypt hash。来源是构建变量 **`LSP_MAINT_PASSWORD`**（默认 `<SECRET>`）；显式置空 = 不写文件 ⇒ `lsp-maintenance on` 一律被拒（安全但不可用）。见 [maintenance-mode.md](kiosk/maintenance-mode.md) |
| kiosk 启动引导页 | `usr/share/lsp-kiosk/loading.html` | `/usr/share/lsp-kiosk/` | 仅保留背景和慢启动提示；HEAD 连接探测后导航，失败 1 秒后重试。opaque response 不证明 HTTP 200；不覆盖浏览器冷启动空窗，见 [Chromium 启动评估](chromium-cold-start.md) |
| kiosk 网址 | `etc/default/lsp-kiosk` | `/etc/default/` | `LSP_KIOSK_URL=http://127.0.0.1:8080/`，通过着陆页 fragment 传入。改 URL 须与 socket、Chromium allowlist 一起核对，再重启 kiosk；`LSP_KIOSK_WAIT_S` 当前不使用 |
| kiosk 逃逸面：切 VT 不给 login | `etc/systemd/logind.conf.d/99-lsp-kiosk.conf` | `/etc/systemd/logind.conf.d/` | `[Login]` + `NAutoVTs=0`。⛔ **唯一有效**的开关 —— 实测 `mask getty@tty2..6` **无效**（logind 拉的是别名实例 `autovt@tty2.service`）。它只防『kiosk 没起来』那段窗口期（kiosk 跑着时 cage 的 `K_OFF` 已经挡住键盘切 VT）。判据 `busctl … NAutoVTs` = `u 0` |
| kiosk 逃逸面：浏览器策略 | `usr/share/lsp-kiosk/10-lsp-kiosk.json` | `/usr/share/lsp-kiosk/` | kiosk 启动时 `+install`、停止后 `+rm`，GDM 启动前清残留。allowlist 放行本地 HMI 与着陆页，其它导航被 blocklist 拦截；`PasswordManagerEnabled=false` 禁用新密码保存。策略存在不等于每项都生效，具体边界与回归见 [escape-hardening.md](kiosk/escape-hardening.md)、[session-design.md](kiosk/session-design.md) |
| 采集更新器辅助模块 | `home/board/updater-bootstrap/boot_{glove_update,confirm_update,maintenance,source_migration,lcd_display}.py` | `/usr/local/sbin/` | 原样安装五个独立 Python 模块，**0644 root:root**；供协调器按功能加载，源码仍保留在 bootstrap 目录 |
| 采集更新器凭据与配置 | `etc/data-collection-updater/` | `/etc/data-collection-updater/` | 全部 root:root；目录 0755，`release-public.pem` 0644（Ed25519 公钥）；`boot.json` / `bos-device-credentials.json` / `download-bos.json` / `download.json` / `gitlab-deploy-token` 均为 0600。Git 只记可执行位，构建使用显式 `install -o root -g root -m`，镜像检查核对六个文件的权限与属主；overlay 部署也强制此规则。`boot-enabled` / `device-id` 不预置（运行期由现场流程生成） |
| 采集 release 树 | `opt/data_collection/` | `/opt/data_collection/` | 单版本初始基线：`releases/` 只保留 `v1.1.9`，绝对软链 `current` 指向 `/opt/data_collection/releases/v1.1.9`；`previous` 不预置，由更新器在版本切换时维护。`.install.lock` 为 0600。`copy_overlay_dir "opt"` 使用 `cp -a` 保留内容和软链，再由 `normalize_overlay_perms()` 归一为 root:root、移除目录及普通可读文件的组/其他写权限；紧权限文件保留原权限，顶层为 0755 |
| 采集运行期目录 | （构建脚本 `install -d`） | `/var/lib/data-collection-updater` 0700、`/var/lib/data-collection-updater-status` 0755、`/var/cache/data-collection-updater` 0755、`/var/backups` 0755 | 全部 root:root。⚠️ 0700 是**脚本自己的语义**：`boot-data-collection-update` 用 `0o700` 建 state_dir，并对每个路径跑 `trusted()`（拒符号链、拒组/其他可写、属主必须是 root）—— 放宽不会报错，但会把私有语义抹掉 |
| `/run` 运行期目录 | `usr/lib/tmpfiles.d/data-collection-updater.conf` | 同路径 | `d /run/data-collection-updater 0755 root root -`。⚠️ **`/run` 是 tmpfs**：镜像里 `mkdir` 出来的目录一开机就没了，只能靠 tmpfiles 每次开机重建（`boot-data-collection-update` 自己也会建，这条保证"首次更新之前它就在"） |

**构建期生成、不在 `overlay/` 里的**（改它们要改脚本，不是改文件）：

- ROS 环境注入**两处**、覆盖范围不同，都要有：
  · `overlay/etc/profile.d/ros2.sh` → `/etc/profile.d/ros2.sh`（**登录 shell**；构建脚本
    `install -m 0644`。📁 2026-09-26 起内容**搬进 overlay**——用户口径"从构建脚本里拿出来放
    overlay，放脚本里不好维护"；它必须带 bash 守卫，见 AGENTS.md「两个隐蔽的坑」）；
  · `~board/.bashrc` 追加一行（**交互式非登录 shell**：tmux 新 pane、直接敲 `bash`）——这是
    "追加到已有文件"，没法用 overlay 表达，仍在构建脚本里。
  **两处都带 `-z "${ROS_DISTRO:-}"`（已 source 就跳过）**：登录 shell 先读 profile.d、再经
  `.profile → .bashrc` 读第二遍，没这条时 `setup.bash` 被 source **两次** ⇒ 实测登录 shell
  初始化 **0.55~0.59 s**，加守卫后 **0.29~0.32 s**（省 0.26 s，2026-09-26 开发板实测）。
  ⚠️ profile.d 那条守卫仍必须写在 **`if` 条件里**（dash 安全那条不能破）；`.bashrc` 那条是
  "先按 `setup.bash` 关键字删旧行 + 删注释，再追加新行" ⇒ 重跑 `rootfs` 阶段幂等，且能把老 rootfs 里
  没有守卫的旧行替换掉。
- `/var/lib/AccountsService/users/board`、`~board/.dmrc`（默认会话）、`~/.config/gnome-initial-setup-done`
- **`board` 账号的附加组**（构建脚本里逐个 `gpasswd -a`）：`sudo`（`useradd -G sudo`）、
  `video` / `audio` / `render` / `plugdev` / `adm` / `systemd-journal`。
  ⛔ **两个组只有"全新解压底包 + 首次跑 `rootfs` 阶段"才会踩到**（默认复用 `build/rootfs` 时组早就在）：
  `render`（udev 的 sysusers 建）与 `systemd-journal`（systemd 的 sysusers 建）在底包里都没有。
  2026-09-26 实测：`systemd-journal` 那条原来写成"组不存在就跳过"，于是**全新构建**的镜像里
  `board` 只进了 `adm`，`journalctl` 只回 `-- No entries --`（板上 `verify-image.sh` 报
  "board 不在 systemd-journal 组"）。现在两个组都按同名同用途 `groupadd --system` 补建。
  判据：`id board` 含 `systemd-journal`；`sudo -u board journalctl -n 1` 有输出。
- **`kiosk` 账号**（2026-09-22）：`useradd -m`，再 `gpasswd -a kiosk board video/audio/render/plugdev`。
  ⛔ **2026-09-23 用户要求"kiosk 从 sudo 组里去掉"** ⇒ 不再 `-G sudo`，并加幂等 `gpasswd -d kiosk sudo`；
  组与 `board` 的差别只有"没有 sudo"（**`board` 组必须给**：上位机跑在 kiosk，见上面"设备配置"那行）。
  `usermod -p '!'` 把密码**锁死**
  （不可密码登录，只由 systemd 以 `User=kiosk` 拉起）。建在 `scripts/build-lsp-image.sh`
  创建 `board` 的同一段里；离线和 live 断言在 `tools/verify-image.sh` 的「kiosk 用户」组。
  会话契约见 [session-design.md](kiosk/session-design.md)
- **维护模式密码**（2026-09-23）：构建变量 `LSP_MAINT_PASSWORD`（默认 `<SECRET>`）经
  `openssl passwd -6` 写 `/etc/lsp-maintenance.passwd`（0600 root:root）。用户明确"维护密码
  不能在运行时设置，提前做到镜像里"；运行期换密码用 `sudo lsp-maintenance passwd`。
- **kiosk 逃逸面加固**（2026-09-22）：`systemctl mask ctrl-alt-del.target`（实测**不误伤**
  `reboot.target`/`poweroff.target`，`lsp-powerkey` 的优雅关机不受影响）+ 装上面那两个文件。
  回归测试 `tests/kiosk-lockdown.sh`（板上 27 通过 / 0 失败）
- **开机进 kiosk 的两个开关**（2026-09-22）：`systemctl enable lsp-kiosk.service` +
  `systemctl disable gdm3.service`（后者删掉 `/etc/systemd/system/display-manager.service`
  这个别名链接）。GDM 本体还在，**`sudo lsp-maintenance on` 回桌面**（只 `start` 不 `enable`，
  重启自动回 kiosk；⛔ 别再 `enable gdm3`，那会废掉"默认 kiosk"）；
  ⛔ 刻意不用 `mask`（mask 还得先 unmask）
- `/usr/share/glib-2.0/schemas/99-lsp-branding.gschema.override`（壁纸路径）
- `/etc/motd`（清空）、`/etc/hosts`（`127.0.1.1` 同步主机名）

### 采集更新器的入口与运行时

三个脚本以现场提供的文件为基线；协调器及 bootstrap 的 `boot_update.py` 同步适配
新镜像的 `board` 默认账户，避免现场安装覆盖回源机账户。下载器与安装器内容保持原样。
构建统一安装为 `0755 root:root`。
服务、OEM 协调器配置、`boot.json` 的 `downloader`、下载配置的 `installer` 使用上述完整名称。
协调器使用的五个 `boot_*.py` 辅助模块从 bootstrap 原样安装到 `/usr/local/sbin/`，权限为 `0644 root:root`。

当前协调器源码版本为 `5.4.1`，支持 `boot_update_protocol` 1、2、3、4。入口与协议依据：
boot-data-collection-update（镜像源码：`overlay/usr/local/sbin/boot-data-collection-update`） 的
`UPDATER_VERSION`、`SUPPORTED_BOOT_PROTOCOLS` 与 `main()`。
本次文件集成与静态检查不代表完成真实发布、版本切换或外设升级验证。

协调器仍由 `/usr/bin/python3 -I` 启动。选择 BOS 下载配置时，`bos_environment()` 验证独立 venv，
再通过 `DATA_COLLECTION_DOWNLOAD_PYTHON` 指定下载器的 Python；默认路径为
`/opt/data-collection-bos-venv/bin/python`，要求 root 管理、无组/其他写权限、隔离系统 site-packages。
该 venv 随工程离线携带，五项固定依赖以 `updater-bootstrap/requirements-bos.txt` 为准；
其中本机扩展面向 CPython 3.12 / AArch64，静态元数据检查不能替代板上导入与下载验证。
许可证保留在 `lib/python3.12/site-packages/*.dist-info/` 下的 `LICENSE*` 或 `licenses/` 中。
GitLab 路径使用系统 Python，不依赖 BOS SDK。下载器的 BOS `--install` 被拒绝；
协调器将下载验证与安装器的 `--stage-only` 分开执行，不能把下载命令当作版本激活命令。

协调器的默认设备配置与 `boot.json` 均指向 `/home/board/.lsp/config/device.yaml`；
`SystemBackend.prepare_lcd_health()` 与 `lcd_runtime_health()` 以 `board` 校验 LCD 健康目录和回执属主，
与两个业务服务一致。协调器仍以 root 运行；直接调用原样安装器时需用
`DEVICE_CONFIG_PATH` 覆盖其源机账户默认路径。
三个单元在构建时显式离线 disable，清除复用 rootfs 的启用链接，首次启动保持停止；
构建不操作宿主或现场机器的运行状态。`boot-enabled` / `device-id` 不预置。新版更新、维护及安装保护 drop-in
由现场 `updater-bootstrap/install_rollout_agent.sh` 配置，本次三脚本导入不等于完成启用。

外设烧录尚未验证：`boot_glove_update.py` 的串口路径要求 `esptool==5.3.1`，
当前构建只声明 apt 的 `esptool`；协调器默认 LCD 工具路径为 `/usr/local/bin/esptool`，
也需与目标安装位置核对。BOS venv 的 SDK 依赖齐全不能证明这些系统 Python 依赖满足。

## 5. 启动分区与设备树

| 内容 | 源（`overlay/`） | 镜像内路径 | 说明 |
| --- | --- | --- | --- |
| Type-C 角色覆盖层 | `boot/firmware/dtbs/rockchip/overlay/` 中各板的 `*-usbcon-fixedsrc.dtbo` 与 `*-dwc3-rolehost.dtbo`，由 `<board>.overlays` 选取 | `/boot/firmware/dtbs/rockchip/overlay/` | 固定 source + data host，并让 DWC3 在 role none 时仍保持 host；两项一起启用 |
| 该目录的说明 | `boot/firmware/dtbs/rockchip/overlay/README.md` | **不进镜像** | 只拷 `*.dtbo` |

机制：构建将自定义 dtbo 拷进启动分区，按 `<board>.overlays` 选择启用项，
与 `config/uEnv/<board>.uEnv` 的基础项合并到 `lsp-dtb-merge.conf`，构建期合成 DTB 后由 `ubuntuEnv.txt` 的 `fdtfile=` 指向它。
缺少合并工具时才回退到 U-Boot 的 `overlays=`；没有板卡清单时追加全部自定义 dtbo。

设备上更新 initramfs 时会暂存并重建 DTB，保留内核包没有的自定义 overlay，同名取新包；
合并 / 发布失败保护旧启动文件，具体见 [dtb-update.md](dtb-update.md)。

## 6. 对"厂商东西"打的补丁（不在 `overlay/` 的纯脚本改动）

| 补丁 | 位置 | 为什么 |
| --- | --- | --- |
| mask `usbdevice.service`（保留厂商包与 unit 本体） | 构建脚本主机策略块 | 不支持 device 模式，禁止厂商 gadget 初始化或手动启动；重新配置时清除旧 gadget/NM/getty/MTP/HID 配置 |
| 删除 `fire-config` / `lbc-config` / `lubancat-config` | 同上 | 厂商 TUI 假设官方镜像布局、含 `dd` 整盘备份，且会留悬空链接 |
| `zz-update-firmware` 保住自定义 dtbo | `overlay/etc/initramfs/post-update.d/` | 原版 `rm -rf dtbs/` 会删掉启动分区里的自定义 overlay，**静默**让 `overlays=` 失效 |
| `plymouth-quit-wait.service` 覆盖 | `overlay/etc/systemd/system/…d/` | 最终等待动作为 `/bin/true`；保留无屏检查，kiosk 前置退出 Plymouth，见 [session-design.md](kiosk/session-design.md) |
| PAM 失败延迟 `nodelay` | 构建脚本 | 输错密码不再白等 1~4.5 秒（`pam_faildelay.so` + `pam_unix.so nodelay`，并同步 `pam-configs/unix`） |
| 清理失效的 `pam_lastlog.so` 会话项 | 构建脚本 `rootfs` 装包后；`tools/verify-image.sh` PAM 检查 | 移除 `/etc/pam.d/login` 活动的 `session optional pam_lastlog.so`，避免 Noble 缺失模块在每次串口登录时产生加载错误；见 [串口日志](journal-serial.md#登录时反复出现-pam_lastlogso-加载失败) |
| 关闭自动更新检测 | 构建脚本 | mask 一批 timer/service + `20auto-upgrades` 置 0 + user 单元软链到 `/dev/null` |
| GNOME 桌面包裁剪 | 构建脚本 `apply_desktop_env` / `desktop` 会话；`tools/verify-image.sh` | 去掉 Yelp、图形升级、指定打印 / Ghostscript、文泉驿、Rhythmbox 和两个桌面元包；显式保留 Noble 组件及未裁剪的可选 Recommends，保护 `gstreamer1.0-packagekit`；包集、会话与镜像验证见 [image-size-trimming.md](../build/image-size-trimming.md) |
| DNS：systemd-resolved 兜底 | `overlay/etc/systemd/resolved.conf.d/99-lsp.conf` | `FallbackDNS=223.5.5.5 119.29.29.29`（只在上游 DNS 全缺时用；**不写 `DNS=`**，否则覆盖 DHCP 的内网 DNS）|
| 关闭 apt 翻译索引 | `overlay/etc/apt/apt.conf.d/99-lsp-no-translations` | `DISABLE_AUTO_UPDATE!=N` 时在 `rootfs` 阶段首个 chroot 会话前安装，首轮 update 即跳过翻译索引；见 [APT 配置](../build/apt-sources-deb822.md#53-varlibaptlists-的体积与处置) |
| 移除构建期加速痕迹 | 构建脚本（`rootfs` 阶段末尾） | `eatmydata` 与 `dpkg force-unsafe-io` 绝不能留在最终镜像（否则设备上 fsync 被跳过） |

### 6.1 Chromium RKMPP 硬件加速浏览器（本地 deb，不走 `overlay/`）

| 项 | 位置 | 说明 |
| --- | --- | --- |
| 浏览器 deb（3 个） | `packages/Chromium-rkmpp/` | `chromium` / `chromium-common` / `chromium-sandbox` `151.0.7922.169-1`（**不装 `chromium-l10n`**：中文界面包 9.3 MB，用户明确不要） |
| MPP / V4L2 桥的 deb | `packages/arm64/` | `libv4l-rkmpp 1.8.0-1`（新增）+ `librockchip-mpp1`/`-dev`/`-vpu0`/`rockchip-mpp-demos` 换成 `1.5.0+git20260811.c08762eb-1~noble1` |
| bag 预览（`.mcap` 默认打开程序）| `apps/lsp-mcap-view/` 源码；`packages/apps/manifest.tsv` 固定 arm64 deb；`overlay/usr/share/applications/lsp-mcap.desktop` | 包安装 `/usr/local/bin/lsp-mcap`、`lsp-mcap-open`、`/usr/share/mime/packages/lsp-mcap.xml`、`/usr/share/applications/lsp-mcap.desktop`；postinst 维护 MIME 数据库及默认关联，保留已有 `x-mcap` 配置，包不拥有共享 `mimeapps.list`。镜像装包后用 overlay 的 `NoDisplay=true` 隐藏启动图标，保留 MIME 关联和 CLI。见 [应用实例](../build/apps-examples.md) §1、[Rust 发布包](../build/rust-app-debs.md) |
| 硬件占用监控 | `apps/lsp-rk-monitor/` 源码；`packages/apps/manifest.tsv` 固定 arm64 deb | 0755 `/usr/local/bin/lsp-rk-monitor`；TUI / `--once` / `--json`。NPU/RGA debugfs 数据仍须 root，包不新增服务或权限。见 [硬件监控](../hardware/rk3588-hw-monitor.md)、[Rust 发布包](../build/rust-app-debs.md) |
| USB 归属诊断 | `apps/lsp-usb-map/` 源码；`packages/apps/manifest.tsv` 固定 arm64 deb | 0755 `/usr/local/bin/lsp-usb-map`；使用原有 sysfs / udev 数据，不改变现场绑定。镜像体检同时检查三个 Rust 包已安装、arm64 与源码指纹。见 [设备映射](../usb/device-mapping-method.md)、[Rust 发布包](../build/rust-app-debs.md) |
| 安装位置/方式 | 构建脚本 `rootfs` 阶段 | 先把 deb 暂存到 chroot 的 `/tmp/Chromium-rkmpp/`，再在**桌面会话里** `apt-get -y install --no-install-recommends /tmp/Chromium-rkmpp/*.deb`（用 apt 才能补 `libgtk-3-0t64`/`libdav1d7`/`libopenh264-7` 这些仓库依赖） |
| 开机节点服务 | deb 自带 `chromium-rkmpp-nodes.service` | postinst 已 enable；脚本再用 `systemctl --root=` 确认一遍。它把 `dec`/`enc` 写进 `/dev/video-dec0|enc0` 并设 `root:video 0660` —— Chromium 的 V4L2 解码器靠这两个假节点找到插件 |
| 桌面项 / 收藏夹 | deb 自带 `chromium.desktop` + `overlay/etc/dconf/db/local.d/00-favorite-apps` | 收藏夹写 `chromium.desktop`（**不是** snap 过渡的 `chromium-browser.desktop`） |
| 硬解开关 | deb 自带 `/etc/chromium.d/default-flags` | `--ozone-platform=wayland --enable-features=AcceleratedVideoDecoder,AcceleratedVideoDecodeLinuxGL,AcceleratedVideoEncoder`，由 `/usr/bin/chromium` 包装脚本 source。**别覆盖这个文件、别直接调 `/usr/lib/chromium/chromium`**（那样一定软解） |
| LSP 追加开关 | `overlay/etc/chromium.d/99-lsp-flags`（构建脚本 `install -m 0644`） | `--password-store=basic` —— 关掉 autologin 无密码场景下 gnome-keyring 的「Choose password for new keyring」弹窗（不填**每次开机都弹**，挡住上位机）。包装脚本 source `/etc/chromium.d/*`，追加语义、顺序无关；**别改包自带的 default-flags**。实测口径见 [chromium-rkmpp.md](chromium-rkmpp.md) §5.1 |
| 首次启动弹窗（ToS） | 构建脚本 `rootfs` 阶段给**包自带**的 `/etc/chromium/master_preferences` 补 `distribution.require_eula: false` | `chromium 151.0.7922.169` 的回归（Debian #1145071，官方 `.173` 用 `disable-tos-dialog.patch` 修掉）。不关的话**每台新机器**第一次开浏览器都要点 Accept。**只补一个键、不整份覆盖**（幂等 + grep 守卫 + 插完校验 JSON 合法，坏了回滚）；升到 `.173+` 后可删 |
| 固定路径库 | 构建脚本 `rootfs` 阶段的 `ln -sfn aarch64-linux-gnu/libv4l2.so.0 /usr/lib/libv4l2.so` | Chromium 固定路径别名直接连接运行库 SONAME，不依赖 `libv4l-dev` 的开发链接；见 [chromium-rkmpp.md](chromium-rkmpp.md) |
| 体检 | `tools/verify-image.sh` 分组「Chromium（RKMPP 硬件加速）」 | 覆盖上面每一项（含首次启动两个弹窗的开关）+ live 模式下服务状态与两个 `/dev/video-*` 权限 |

实测数据（工程机：H.264/VP9 1080p30 都 `mpp initializing` + 30 fps，渲染进程 CPU 4.2% vs 软解 47.7%）见
[chromium-rkmpp.md](chromium-rkmpp.md)。

## 7. 因为 `--no-install-recommends` 而必须显式加的包

全程 `--no-install-recommends`，凡是"只作为 Recommends 出现"的包都会被静默丢掉。
**这一类都集中在 `BASE_PACKAGES`（含原因注释）**，目前有：

| 包 | 不加会怎样 |
| --- | --- |
| `plymouth` + `plymouth-theme-spinner` | **开机没有 spinner，只剩内核日志**（素材在、引擎不在） |
| `iputils-ping` / `netcat-openbsd` / `tcpdump` | 板子上 `ping` / `nc` / `tcpdump` 都没有（排查网络时很别扭） |
| `chrony` / `cloud-guest-utils` / `zstd` / `xz-utils` | 时间同步、根分区扩容、镜像压缩 |
| `fake-hwclock`（`FAKE_HWCLOCK=Y` 时追加） | 无电池 RTC 时断电丢时间 |
| `chromium-sandbox` + `fonts-liberation` | 装 Chromium 时显式带上：`chromium-sandbox` 只是 `chromium` 的 Recommends（缺了沙箱起不来），`fonts-liberation` 是 `chromium-common` 的 Recommends（网页字体）。见 §6.1 |
| `dbus-user-session` + `rtkit` | 缺了没有 systemd 管的**会话总线** ⇒ wireplumber exit 70、整机没声音、gnome-session 回落（三个症状同一根因）；`rtkit` 是 `pipewire-bin` 的 Recommends |
| `dnsmasq-base` | 板载有线直连按需 DHCP 服务；只装可执行程序，不安装全局 dnsmasq 服务。GPL-2 或 GPL-3，noble arm64 提供，见 [ethernet-direct.md](ethernet-direct.md) |
| `bash-completion` | 只是 `bash` 的 Recommends ⇒ TAB 补全静默失效（补全脚本在，但没人加载它们） |
| `util-linux-extra` | `hwclock` 被 Ubuntu 24.04 拆到这里，而这包只是 `util-linux` 的 `Suggests` |
| `cage` | kiosk 会话的单应用 Wayland 合成器（2026-09-22 加）；没有它 `lsp-kiosk.service` 起不来 |

`APP_PACKAGES` 显式包含 `adb`，默认不装带通用规则的 `android-sdk-platform-tools-common`；
`rootfs` 阶段重新配置时清理复用 rootfs 中的旧包及旧通用规则。
本仓库只维护 Quest VID `2833`，见 [app-and-device-config.md](app-and-device-config.md)；
其他厂商按需补规则，不承诺通用放行。

## 8. 用户自己写的，不要顺手改

- `overlay/etc/update-motd.d/30-sysinfo`
- `overlay/opt/`
- `overlay/home/`
- **`overlay/etc/udev/rules.d/` 下这 5 条是用户为自己的硬件手加的**（详见 §3 的表与下面的警告）：
  `99-chest-camera.rules`、`99-left-hand-camera.rules`、`99-right-hand-camera.rules`、
  `99-lcd_voice_bridge.rules`、`99-meta-quest.rules`

> 它们和 `99-lsp-*.rules` 一样都属于相对上游的定制，但归属不同：
> `99-lsp-*.rules` 由构建工程维护，这 5 条用于具体现场设备。
> 要改它们（哪怕只是"优化"匹配方式）**先问用户**，因为它们绑定的是现场硬件的 USB 路径与 MAC。

## 9. 上游带的、我们没动 / ## 11. 相对上游的完整改动清单 → 见单独一篇

"哪些是我们改的、哪些是上游带的责任"**已搬成单独一篇** →
**[upstream-diff-and-untouched.md](upstream-diff-and-untouched.md)**
（排查前先分清责任，再看本文 §0~§8 的总账）。


## 10. 快速自检：从"镜像里的东西"反查"谁装的"

```bash
# 板上：看某个文件是谁装的（dpkg 还是我们）
dpkg -S /usr/local/bin/lsp-usb 2>/dev/null || echo "不是包装的 → 来自 overlay"
# 板上：跑体检，看我们的定制项是否生效（按分组输出）
./tools/verify-image.sh
# 本地：看某个文件在构建脚本里怎么装的
grep -n "<文件名或路径关键字>" scripts/build-lsp-image.sh
# 本地：看我们相对上游改过哪些 overlay 文件
git log --name-only --pretty=format: origin/24.04-panfrost..HEAD -- overlay/ | sort -u
```

---
