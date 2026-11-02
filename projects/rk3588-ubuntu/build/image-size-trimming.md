# 镜像包集与体积裁剪

> Ubuntu Base 24.04.5 镜像已完成 GNOME 桌面与指定 SDK 裁剪，重包、离线体检及 SD 冷启动已验证。
> 当前已移除 9 个独立开发 / 文档根包，并由 dpkg 排除厂商 FFmpeg、RKAIQ 的开发文件。
> 保留维护桌面、HDMI kiosk、ROS、采集运行依赖、RGA / MPP 开发包与内核 headers。
> 新一轮 rootfs 的 `/usr` 实占减少约 281.77 MiB，最终镜像逻辑长度减少 281 MiB。
> 新 SD 的包一致性、MPP 硬件编解码及 GNOME 会话通过；启动仍有未解决的服务异常与 PAM 告警（§4）。

## 1. GNOME 桌面范围

只作用于 `--desktop=ubuntu-desktop`；XFCE / KDE 包集不变。

| 功能 | 移除根包 | 保留与影响 |
| --- | --- | --- |
| GNOME 帮助 | `yelp` | 不提供 Yelp 帮助入口；共享 WebKit 库由剩余依赖决定 |
| 图形升级 | `update-manager`、`update-notifier`、`ubuntu-release-upgrader-gtk` | 不提供升级 GUI 与通知；APT 命令行操作保留 |
| 打印描述、驱动与 PostScript | `foomatic-db-compressed-ppds`、`openprinting-ppds`、`printer-driver-pnm2ppa`、`ghostscript` | 移除这些打印支持；GNOME 设置依赖的 `libcups2t64` 等运行库保留 |
| 重复中文字体 | `fonts-wqy-zenhei` | 中文显示继续使用 `fonts-noto-cjk` |
| 音乐播放器 | `rhythmbox` | 保留 mpv；收藏夹去掉 Rhythmbox 与 Yelp |
| Ubuntu 桌面元包 | `ubuntu-desktop`、`ubuntu-desktop-minimal` | 元包硬依赖上述功能；改为显式安装保留的桌面组件 |

本轮不动 Vim、GNOME Shell / GDM / Settings / Nautilus、Noto CJK、Chromium / Cage、
ROS、OpenCV、MPP / RGA 与音频链。删除根包不代表同族运行库都能删，尤其不能按
`libcups*`、`libwebkit*` 或 `*-dev` 通配符卸载。

## 2. 构建与依赖维护

权威入口是 build-lsp-image.sh（镜像源码：`scripts/build-lsp-image.sh`） 的 `apply_desktop_env`
与 `desktop` chroot 会话。GNOME 清单依据 Noble `ubuntu-desktop` **1.539.2** 的 Depends，
显式安装除桌面裁剪项外的 47 个组件，再补终端、密钥环、portal、主题和本地解析等组件。

- 先安装保留清单，再 purge 已安装的裁剪根包，避免旧元包移除后把桌面组件当作孤儿。
- `DESKTOP_RECOMMENDS=N` 仍是默认；设为 `Y` 显式加入同版原元包未裁剪的 Recommends，
  继续排除既有 Snap / LibreOffice 禁用项及本轮帮助、音乐、打印裁剪项，不恢复完整 Ubuntu 桌面。
- 保留 `gstreamer1.0-packagekit`；`PURGE_SNAP` 的模拟保护网必须阻止连带卸载它、
  GNOME Shell 或显示管理器。PackageKit 不因去掉升级 GUI 就自动成为可删项。
- `DESKTOP_KEEP_PACKAGES` 可覆盖补充清单，但不能用它恢复本轮明确移除的功能。
- 00-favorite-apps（镜像源码：`overlay/etc/dconf/db/local.d/00-favorite-apps`） 只列保留应用；
  更新运行系统配置后需 `dconf update`，已有账户的个人覆盖值须另外核对。
- verify-image.sh（镜像源码：`tools/verify-image.sh`） 检查裁剪包缺席、维护桌面组件保留与收藏入口。
  静态体检不等于桌面能正常启动。

显式组件清单不会自动继承桌面元包未来新增的 Depends。更新 Ubuntu 元包版本或发行版时，
须重新比较 Depends、APT 模拟结果与实际会话；不能仅按当前 47 个包长期冻结桌面依赖。

依赖来源可用 `apt-cache show ubuntu-desktop=1.539.2` 与
`apt-cache show ubuntu-desktop-minimal=1.539.2` 核对；在线包信息见
[Noble ubuntu-desktop](https://packages.ubuntu.com/noble/ubuntu-desktop)。

## 3. 独立开发包与厂商 SDK

这些规则作用于构建 rootfs，开发板宿主保留完整 SDK 和构建依赖，不能照搬到运行系统。
在宿主直接卸载同一组根包会连带移除 88 个构建相关包，破坏后续原生构建。

| 对象 | 当前裁剪范围 | 保留范围 |
| --- | --- | --- |
| 独立 APT 包 | `librist-dev`、`libdvbv5-dev`、`libdvbv5-doc`、`libv4l-dev`、`libegl1-mesa-dev`、`libgbm-dev`、`libgl1-mesa-dev`、`libgles2-mesa-dev`、`mesa-common-dev` | 不按 `*-dev` 通配符删除；ROS / OpenCV 硬依赖的开发包保留 |
| 厂商 FFmpeg | codec / device / filter / format / util / swresample / swscale 的 7 个 `.a`、145 个头文件和 7 个 `.pc` | ffmpeg、ffprobe、共享库与编解码运行组件 |
| 厂商 RKAIQ | `librkaiq.a`、`libIspFec.a`、`libsmartIr.a` 及 rkaiq / IspFec 的 625 个头文件 | 共享库、IQ 数据与摄像头程序 |

独立包由 build-lsp-image.sh（镜像源码：`scripts/build-lsp-image.sh`） 的
`REMOVE_BUILD_PACKAGES` 在最终配置会话 purge，再让 APT 清理真实孤儿。
基线 rootfs 的模拟移除 **40 个包 / 26.045 MiB Installed-Size**；这是依赖模拟，
不能代替重建后的包清单与文件实占。

厂商包把运行文件与 SDK 打在同一 deb 中，不能整包卸载。构建在首次安装厂商包前部署
01-lsp-vendor-sdk（镜像源码：`overlay/etc/dpkg/dpkg.cfg.d/01-lsp-vendor-sdk`），
使用 dpkg 的 `path-exclude` 精确过滤上述路径，不在装包后手工 `rm`。
升级或同版重装也继续过滤，并撤回旧包已安装的对应文件；该配置保留在镜像内。
同包保留文件由 dpkg 正常维护，依据见 [dpkg(1)](https://manpages.ubuntu.com/manpages/noble/en/man1/dpkg.1.html)。

过滤集合在裁剪前约占 **264.59 MiB 文件实占**，但排除路径不会修改厂商包的
`Installed-Size` 元数据。不能把这部分收益计入 APT 大小，也不能把它当作新镜像已测减少量。
`dpkg --verify` 对主动排除的 SDK 文件报告 `missing` 属于预期；运行文件缺失仍是错误。
不用通配符忽略所有 missing，须按这份配置核对路径。

vendor-sdk-excludes.sh（镜像源码：`tests/vendor-sdk-excludes.sh`） 的隔离安装 / 升级 / 同版重装回归
通过 **13/0**；heredoc 回归 **38/0**、preflight **23/0**。
板上实测结果与镜像验收边界见 §4。

移除 `libv4l-dev` 后，保留的厂商运行组件仍需加载 `/usr/lib/libv4l2.so`。
构建将该兼容链接直连 `libv4l2.so.0`，不再依赖开发包提供的链接；
`ctypes` 实际 `dlopen` 已通过。不能仅以 ELF 依赖解析成功代替动态加载测试。

## 4. 验证与收益口径

每轮先记录目标机器、运行内核、根分区、包版本与图形状态，再保存 APT 模拟和实际包变化。
开发板运行系统可能有后装工具，不能直接当作构建 rootfs，更不能代替镜像内容。

| 验证 | 判据与边界 |
| --- | --- |
| 构建静态回归 | `bash -n`、warning 级 ShellCheck、heredoc 与 preflight 回归通过；不执行真实 APT |
| 包依赖 | `dpkg --audit` 无问题、`apt-get check` 成功；模拟卸载不牵连保留组件 |
| 桌面入口 | 生效收藏项都能通过 `Gio.DesktopAppInfo` 解析，清除已卸载应用的默认入口 |
| 中文字体 | Fontconfig 中文匹配与 Pango 实际文本布局无未知字形；这是字体链验证，不代替屏幕观察 |
| 维护桌面 | 实际进入 GDM + GNOME，logind 确认为 board 的 Wayland 会话，文件管理器与终端能启动 |
| kiosk 恢复 | 退出维护后 Cage / Chromium / 上位机恢复，HTTP 成功，维护标记清除，GDM 不 enable |
| 构建产物 | 重建 rootfs / 镜像后比较包数据库、文件实占与镜像大小；板上验证不能冒充新镜像验收 |

适当复用 现有测试（镜像源码：`tests/README.md`）；没有改 HDMI 门控、键盘过滤或浏览器策略时，
不重复跑这些模块的完整测试。新增失败、缺屏或未执行路径分别报告，不能都计为通过。

空间报告分开三种量：

1. **包标称大小**：已安装包的 `Installed-Size`，单位 KiB；厂商包可能没有此字段。
2. **文件实占**：`du -x -B1`，包含包文件、overlay / pip 与运行数据，受硬链接和稀疏文件影响。
3. **镜像文件长度**：`stat` 的字节数，含分区布局及预留空间；不等于有效文件占用。

不把 APT 模拟收益与文件实占混加，也不把磁盘减少量解释为内存减少量。

### 2026-10-13 桌面裁剪与完整构建基线

对象为 LubanCat-5 V2 开发板、BSP `6.1.172`、eMMC 运行系统，以及同板的缓存 `build/rootfs`。
缓存 rootfs 执行的是生产 `desktop` 会话的 APT 包操作，没有重跑完整 `rootfs` 阶段配置。

| 对象 | 实际移除包数 | `Installed-Size` 减少 | `/usr` 实占减少 |
| --- | ---: | ---: | ---: |
| 开发板运行系统 | 38 | 190.39 MiB | 191.40 MiB |
| 缓存 rootfs | 39 | 190.61 MiB | 191.62 MiB |

两者没有新增包或版本改变，`dpkg --audit` 为空、`apt-get check` 通过。缓存 rootfs 额外移除
`patch`，它随 update-notifier 退役后成为孤儿；运行系统的 `patch` 仍有依赖或人工保留。

实际 `lsp-maintenance on` 进入 board Wayland 会话，GNOME Shell、Nautilus 与 Terminal 启动成功；
中文匹配 Noto CJK，Pango 中文样例未知字形为 0，四个保留收藏项均有效。
`off` 后 Cage / Chromium active，上位机保留原进程，HTTP 8080 返回 200；维护标记清除，
GDM inactive / static、无 display-manager enable 链接，HDMI 调度恢复 unmasked。
本地 heredoc 回归 34/0、preflight 23/0；
包状态识别以真实 dpkg 数据库验证 live / offline 16 项，包括 hold 与 reinstreq 已安装状态。

随后以 Ubuntu Base 24.04.5 从底包完整重建，**2026-10-13 16:59 构建成功**。
新 rootfs 完整体检为 **560 PASS / 0 FAIL / 1 WARN / 1 SKIP**，
`dpkg --audit` 与 `apt-get check` 均返回 0；构建结束后宿主 kiosk 恢复，HTTP 返回 200。
这是桌面裁剪基线，未包含 §3 的新一轮开发文件裁剪；前表仍是运行系统 / 缓存 rootfs
的桌面阶段测量，不是这份 `.img` 的前后对照。

### 2026-10-13 开发包与厂商 SDK 的最终产物验证

同板、同一 24.04.5 构建基线下，9 个直接根包及 APT 孤儿共移除 **40 个包**，
新增包 **0**、保留包版本变化 **0**。厂商开发文件由 dpkg 过滤，
6 个厂商运行文件哈希不变。

| 测量 | 当前结果 | 边界 |
| --- | ---: | --- |
| rootfs `/usr` 文件实占减少 | 281.765625 MiB | 包与 SDK 裁剪的实测总收益 |
| 最终镜像逻辑长度 | 5,540,675,584 B | 相比同版基线 5,835,325,440 B 减少 281 MiB |
| 最终镜像磁盘实占 | 4,344,246,272 B | 受稀疏文件影响，与镜像逻辑长度分开统计 |
| 最终 rootfs / 镜像体检 | 各 622 PASS / 0 FAIL / 1 WARN / 1 SKIP | WARN：离线无 ttyFIQ0；SKIP：USB 自动挂起运行态；不能当作启动验收 |

`image` 阶段重包退出码为 **0**，构建结束后宿主图形恢复，HTTP 返回 200。
最终镜像使用只读 loop，两分区以 `ro,noload` 挂载；体检和清理均返回 0。
镜像的 SDK 排除配置和包数据库与 rootfs 一致，`libv4l2.so` 链接及 `ctypes dlopen` 验证通过。

同素材哈希、同命令下，裁剪 rootfs 与完整 SDK 宿主的 FFmpeg 硬件编码 / 解码
均完成 **60 帧**，退出码 **0**，`progress=end`，文件描述符命中实际 `/dev/mpp_service`。
两者的编码 / 解码测试每次均有 `client12 unready` 1 次；
`invalid mem pool ptr` 在编码中各 4 次、解码中各 0 次。
这些提示在本次对照中不是裁剪新增。该测试不代表所有编码参数或摄像头采集场景均已覆盖。

### 新 SD 的真实断电冷启动验收

已烧录并真实断电 / 上电，启动后根分区为 `/dev/mmcblk1p2`、boot 为 `/dev/mmcblk1p1`，
`boot_id` 改变，系统为 Ubuntu **24.04.5 / BSP 6.1.172**。
镜像 release 文件 SHA256 与烧录前独立保存的值一致，1522 行包清单精确一致。
本次通过串口验证，验收时无网络；eMMC 未改写。

9 个根包缺席、厂商 SDK 路径排除、6 个运行文件哈希、`dpkg --audit` / `apt-get check`
及四个运行库的实际 `dlopen` 均通过。运行系统体检为
**682 PASS / 0 FAIL / 2 WARN / 12 SKIP**。
体检的两项 WARN 分别为缺 `dumpimage`，无法精确核验 ATF；缺 `dtc`，overlay 仅做字符串检查。
`lsp-growroot.service` 与 `lsp-swapfile.service` 均 success / 退出 0，rootfs 显示 29G、swap 为 2G。

冷启动后硬编 / 硬解再次各完成 60 帧，退出码 0、`progress=end`，
均持有 `/dev/mpp_service` 文件描述符；`client12 unready` 每次 1 次，
编码 `invalid mem pool ptr` 4 次、解码 0 次，与裁剪前完整 SDK 对照一致。

新 SD 临时启动 GNOME 后，logind 的 board 会话为
`Type=wayland / Seat=seat0 / Active=yes / State=active`，
用户 `dbus.socket` 与 `gnome-session-wayland@ubuntu.target` 均 active。
从用户 manager 采集真实 `DISPLAY` / `XAUTHORITY` 后，`gio launch` 启动 Nautilus 并确认进程，
`gnome-terminal --wait` 实际执行 shell 并写出独立 `terminal-ok` 标记。
桌面测试、脚本单元及恢复步骤均成功 / 退出 0。

退出后维护标记消失，按 systemd 解析的实际 GDM 单元检查为 inactive / static；
kiosk active、Chromium 存在，oem-host 原 PID 1012 未变，HTTP 返回 200。
本次由 root 直接临时启停图形链，不覆盖 `lsp-maintenance on` 的密码认证；
未验证屏幕视觉效果或音频播放。

临时媒体 / 桌面观测脚本曾因 stderr 混入 JSON、环境未就绪或 GDM 别名判定而误报；
已修正输出分离、等待与实际单元检查，经阳性及负控验证后重测通过，旧日志保留。

仍有两项启动异常，不能据体检的 0 FAIL 宣称零启动失败：

- `fake-hwclock-load.service` 命中 `start-limit-hit`，每次 load 返回 0。
  官方 oneshot 没有 `RemainAfterExit`，成功后变 inactive，sysinit 完成前的新启动事务可再次拉起它。
  同版 systemd 255 隔离复现命中限流；加 `RemainAfterExit=yes` 后六次事务仍只执行一次。
  现场每次新事务的发起方尚未唯一确定。应修复 load 的完成状态，保留无电池时间兜底，
  不 mask 服务或关闭限流；见 [时间链](../system/tuning/time-sync.md)。
- `lsp-journal-serial.service` 首次退出 1，自动重试 1 次后 active。
  后续两次退出均紧随串口 getty 停止，诊断结束时累计重试 3 次。
  PTY 与 strace 对照证实 getty 的 `TTYVHangup` 会使旧 stdout 收到 `EPOLLERR|EPOLLHUP`，
  `journalctl` 因此退出 1；仅调整首启排序无法覆盖后续 getty 重启。
  管道输出加可重开 TTY 的写端原型通过三次挂断，无进程重启或告警重复；
  正式实现及真实 FIQ 串口修复回归尚未完成，见 [串口日志](../system/journal-serial.md)。

此外串口及 kiosk PAM 有 `pam_lastlog.so` 缺失告警，登录与会话仍成功，旧 eMMC 也有同告警。
已核对 Noble 官方 `libpam-modules 1.5.3-5ubuntu5.7` 不提供该模块，
但 `login 1:4.13+dfsg1-4ubuntu3.2` 的配置仍引用它；不是本次裁剪引入。
[PAM 1.5.3 NEWS](https://github.com/linux-pam/linux-pam/blob/v1.5.3/NEWS#L15-L20)
说明其记录格式不兼容 Y2038，默认不再构建。建议只清理 login 中失效的 optional session 行
及相邻旧说明，保留其它 PAM 规则；本轮尚未实施修复。
上述日志告警与体检的两项 WARN 是不同口径。

## 5. 既有清理与测量方法

- swapfile 在首启创建，不放进镜像；见 [swap](../system/tuning/swap.md)。
- 打包前清除 `/var/lib/apt/lists`，下载的 deb 保存到宿主 `build/apt-cache`；
  见 [APT 缓存](apt-cache.md)。保留下载缓存不等于把它装进镜像。
- 验证中额外执行 `chroot ... apt-get check` 可能重新生成
  `/var/cache/apt/{pkgcache.bin,srcpkgcache.bin}`；清理本轮新增缓存后再打包和统计，
  避免验证操作改变镜像大小，不删除宿主下载缓存。
- 关闭 APT 翻译索引；2026-09-12 旧系统索引约 204 MB，其中翻译约 61 MB，
  这只是旧测量的量级依据，不能沿用为本轮新收益；见 [APT 源](apt-sources-deb822.md)。
- 保持 `--no-install-recommends`、snap / LibreOffice pin 和构建加速痕迹清理。
  已显式保留的 Plymouth、bash-completion、会话总线等不能随裁剪移除。

对已只读挂载的镜像根分区或构建 rootfs，明确指定 dpkg 数据库：

```bash
target_root=/mnt/lsp-img # 替换为实际目标根目录
dpkg-query --admindir="$target_root/var/lib/dpkg" \
  -Wf '${db:Status-Status}\t${Installed-Size}\t${Package}\n' \
  | awk '$1 == "installed" { print $2 "\t" $3; total += $2; count++ }
         END { printf "packages=%d installed_mib=%.2f\n", count, total/1024 }'
sudo du -x -B1 -s "$target_root/usr" "$target_root/opt" "$target_root/var"
stat -c '%n %s bytes' images/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img
```

镜像使用只读 loop 与只读文件系统挂载，结束解除挂载并释放 loop。目录统计限定目标目录，
不遍历整个运行系统；不要把 `/home` 开发缓存、首启 swap 或现场恢复备份算成默认镜像包收益。

## 6. 保留项与后续候选

2026-10-13 工程机 APT 分析是运行系统快照，后装软件与更新使它不同于开发板及构建产物。
以下仅作为后续检查方向：

| 候选 | 核对重点 |
| --- | --- |
| 内核 headers | 当前保留；如需裁剪，先核对 DKMS / 外部模块需求，不碰现役 image、modules、DTB / overlay |
| RGA / MPP 开发包 | 当前保留，不扩大本轮 SDK 过滤范围 |
| 其余多媒体开发包与文档 | 逐组检查反向依赖；ROS 对部分开发包存在真实硬依赖 |

`cmake` / `cpp`、OpenCV、LLVM / Mesa、WebKit GTK 6、扫描库与 Ubuntu 壁纸包都可能被
ROS 或 GNOME 硬依赖；功能看似不用不代表可以单独删包。应用与内核模块裁剪另见
[应用构建](apps.md)、[内核模块裁剪](kernel-module-trimming.md)，不与桌面包收益重复计数。
