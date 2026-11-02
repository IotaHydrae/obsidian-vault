# 启动时间的可优化项清单（状态 / 结论 / 判据）

> 本文汇总已实施或评估过的启动改动；当前启动值和测量口径以 [boot-time.md](boot-time.md) 为准。
> **状态图例**：✅ 已做 ｜ ⏸️ 暂缓（有明确理由） ｜ ❌ 已否掉（实测后放弃） ｜ ⏳ 未做。

## TL;DR

| # | 项 | 状态 | 一句话结论 |
| --- | --- | --- | --- |
| 1 | HMI 启动路径 | ✅ 部分 | 字节码预编译、浏览器/后端并行和 socket 预绑定；引导页已精简，后端就绪不等于首帧 |
| 2 | 内核 + initramfs 那 3.9 s | ⏸️ | 只有"去掉 initramfs"能拿 1.7 s，2026-09-29 用户拍板暂缓；**低风险替代（裁 initrd）已落地** |
| 3 | U-Boot 1.47 s | ✅ | 去掉 NVMe 启动目标：`Total 1455 → 639 ms`（−0.83 s） |
| 4 | Chromium 启动路径 | ✅ 部分 / ❌ | 启动期后台联网已禁；文件预热收益不足，未采用；实验不能确定启动延迟根因 |
| 5 | NetworkManager 的 systemd 就绪语义 | ✅ | 改为 `Type=simple` 后进程启动即可放行；不代表 DHCP 或网络可用 |
| 6 | cage 纯黑底色 | ✅ | fork 的 cage 支持 `--background`；黑段 0.57 → 0.43 s |
| 7 | 流程类（防再踩"板上是旧版本"） | ✅ 大部分 | 镜像版本指纹 ✅、unit 参数断言 ✅（2026-10-03，改成字面量判据、不执行二进制）；DDS 孤儿段按用户决定不做 |
| 8 | 三处底色不统一 | ✅ | 当前统一跟随上位机 `#f3f6fa`，不再使用旧渐变方案 |
| 9 | 缺数据卡时 bootup 拖 ~90 s | ✅ | `x-systemd.devices-timeout` 拼写修好 |
| 10 | 换更轻量的 Chromium | ❌ | 暂不自建浏览器；现有实验不足以量出替换收益 |
| 11 | 卡不在时白等 90 s（fstab 选项拼写） | ✅ | 错拼写被 systemd 静默忽略 ⇒ userspace 白等 1min30s；已加热修工具 + verify 断言（2026-10-03） |
| 12 | 内核 initcall 分析 | ✅ | GMAC / RAID6 的启动期成本已在第 14 / 15 条移除，不能重复算收益 |
| 13 | Plymouth 与 Cage 交接 | ✅ | kiosk 在 Cage 接管前退出 Plymouth；本地引导页也需等待浏览器绘制，不覆盖 Chromium 冷启动空窗 |
| 14 | 内核里的 ETH（GMAC probe 618 ms） | ✅ | 改模块（三件套 `=m`）：app 首帧 **−0.63 s**、kernel 3.425 → 2.640 s；eth0/eth1 与插线能力保留（udev 在 app 首帧后 1.6 s 加载） |
| 15 | RAID6 开机基准 510 ms（btrfs 带进来的） | ✅ | 关 `CONFIG_BTRFS_FS` ⇒ `RAID6_PQ` 降为 `=m`：kernel 2.640 → **2.556 s**、app 首帧 3.367 → **3.101 s**；btrfs 不再支持（用户拍板），md/dm-raid RAID6 能力保留为模块 |

---

## 1. HMI 启动路径 —— ✅ 部分

后端初始化与 Chromium 冷启动并行；后端 `Uvicorn running` 不能当作浏览器绘制或显示器出画面的时点。
当前实测端点见 [boot-time.md](boot-time.md)，本节只记改动和历史实验。

- **已做：镜像里预编译字节码**。`scripts/build-lsp-image.sh` 在 chown/收权限**之后**跑
  `python3 -m compileall -q /opt/oem-host/app`（已核实在脚本里）；`tools/verify-image.sh` 断言 `.pyc ≥ .py`。
  板上 A/B：**1.926 s → 1.793 s**（隔离测 1.75 → 1.61 s）。
- **已做：并行启动 + socket 预绑定**。后端初始化不阻塞 Chromium 启动；监听口提前绑定，浏览器请求可排队。
  2026-10-16 将复制 HMI 的 170,106 字节着陆页缩为 1,114 字节的启动引导页；HEAD 探测避免重复下载主页正文，
  连接失败仍按秒重试。它不能提前于浏览器绘制，也不证明业务就绪。测量和直连取舍见
  [Chromium 启动评估](chromium-cold-start.md)，运行契约见 [会话启动契约](kiosk/session-design.md)。
- **已否掉：预热页缓存**。清单式预热（478 个真实模块文件 / 37 MB）0.42 s + 启动 0.99 s = 1.41 s，
  对比基线 1.62 s 只省 0.2 s；扫目录式更差（合计 1.85 s **> 基线**）。页缓存热时的下限是 0.66 s。
- 判据：`Started oem-host.service` → `Uvicorn running` 的差（journal monotonic）。
  这只量后端进程启动，不代表 HMI 已绘制或可交互。

## 2. 内核 + initramfs 那 3.9 s —— ⏸️ 暂缓（替代方案已落地）

- **实测：去掉 initramfs 能省 1.7 s**（2026-09-29 晚在板上真跑过）：U-Boot 手工
  `ext4load … /vmlinuz` + `booti ${kernel} - ${fdt}`（中间的 `-` = 不要 initrd），cmdline 根设备换
  `root=PARTUUID=<UUID>` ⇒ `systemd-analyze` **1.735 + 2.356 = 4.091 s**（对照带 initramfs 3.42 + 2.40 = 5.82 s），
  `Started lsp-kiosk` 5.20 → 3.695 s。
- ⛔ **用户 2026-09-29 拍板暂缓**（原话"感觉改动有点过于大了"）：1.7 s 是相对已拿到的 −7.4 s 的边际收益，
  代价是动整条引导链 —— 前置条件：① cmdline 根设备必须换 PARTUUID（无 initramfs 时内核不认 `LABEL=`，
  实测卡在 `rootwait`）；② 引导脚本里"传 ramdisk"那步必须一起去掉（`booti` 只认 uImage/FIT，裸 cpio 会被拒；
  光改名/删 `initrd.img` 会直接断引导链，实验时把板子搞挂过一次）；③ `/boot/firmware` 的 fsck 改由
  systemd 做；④ dtb 合并重建钩子从 `initramfs/post-update.d/` 挪到 `/etc/kernel/postinst.d/`；
  ⑤ growroot/swapfile 不受影响；⑥ `update-initramfs` 行为收敛。
  恢复办法：长按电源键断电 → 用刷机 SD 卡启动 → 挂 eMMC 的 boot 分区把文件名改回来。
- **已落地的低风险替代**：只裁 initrd 内容 —— `overlay/etc/initramfs-tools/conf.d/10-lsp-modules.conf`
  的 `n=dep`（已核实）⇒ initrd 18.3 → 14.2 MB、解包后 59 → 45 MB、**解包窗口 0.58 → 0.46 s**，
  总启动在噪声内。明细与安全验证：`notes/system/initramfs.md` §3.2.1。
- **已排除**：内核段里**没有大空档**（最大间隔 0.19 s）⇒ "某个 initcall 卡住"这个方向不成立，
  `initcall_debug` 优先级降低。

## 3. U-Boot 1.47 s —— ✅ 已做

- **改法**：`u-boot/include/configs/rockchip-common.h` 去掉 `BOOT_TARGET_NVME(func)`
  （已核实在仓库里，带 `LSP:` 注释）；**内核侧 DTB 一个字节没动**（新旧 itb 里 DTB 逐字节相同）。
- **根因**：`CONFIG_NVME=y` ⇒ 默认 env 的 `boot_targets` 带 `nvme0` ⇒ distro boot 先扫 NVMe ⇒
  触发 PCIe 枚举；U-Boot 自己的 DTS 里 `&pcie3x4` 是 `okay` ⇒ 空槽训练失败 ⇒ 驱动重试
  **50 次 × `msleep(10)` = 517 ms** + 刷 50 行 `PCIe Linking... LTSSM is 0x0`。
  ⚠️ **插了 NVMe 反而更慢**：链路 up 后驱动还会刻意 `msleep(1000)`。
- **实测**：同一块 eMMC、同一套系统上 U-Boot 自报 `Total: 1455 → 639.147 ms`（−816 ms，−56%）；
  ⚠️ 口径：先在**空 SD 卡**上写同一份 loader 量到 724.899 ms，多的 ~86 ms 是"SD 在位时 SPL 先探卡"，
  所以 **eMMC↔eMMC 的 639 ms 才是干净数字**。
- **板上验证法（不碰 eMMC）**：新 `idbloader.img`（@32 KiB）+ `u-boot.itb`（@8 MiB）写到**空 SD 卡**前 32 MiB
  （先 `dd` 清零）→ 插卡重启：SPL 先试 SD，U-Boot 在 `mmc1` 找不到 `boot.scr`，自动落回 eMMC 那套系统。
  判据：banner 构建日期变了、`Hit key to stop autoboot` 之后**不再有**那 50 行 LTSSM、末尾 `Total:` 变小。
- ⚠️ 这块 loader 2026-10-02 又重建过一次（`bootcmd` 去掉 Android/AVB 回退链，见 [U-Boot 变更记录](../build/uboot-changelog.md) §1），
  **639 ms 这个数字是 09-30 那版量的**，之后没有重量过。
- 细节：`notes/hardware/boot-serial-log.md` §5.2/§5.3；与 NVMe 的取舍另见 `notes/storage/nvme-performance.md`。

## 4. Chromium 启动路径 —— ✅ 部分 / ❌ 文件预热未采用

- **已做：启动期不做后台联网**。`overlay/etc/chromium.d/99-lsp-flags` 显式补上
  `--disable-background-networking --disable-component-update --disable-domain-reliability --disable-sync
  --no-first-run --no-default-browser-check`。原因：包自带的 `/etc/chromium.d/extensions` 里有
  `--enable-remote-extensions`，而 `/usr/bin/chromium` 包装脚本的规则是"**没看到**它才追加
  `--disable-background-networking`" ⇒ 原来浏览器会做启动期后台联网，工厂**无网**时就是一串 DNS/连接超时。
  判据：`tr '\0' '\n' < /proc/$(pgrep -x chromium|head -1)/cmdline | grep -E 'background-networking|component-update|domain-reliability|disable-sync|no-first-run'`
  —— 六个都在。⚠️ 收益**只在无网环境**体现。
- **已否掉：文件预热**。一次预热实验只让主页 HTTP 200 提前 0.52 s，同时推迟了后端就绪；HTTP 200 不是 FCP
  或物理首帧。`/proc/<pid>/maps` 文件大小和设备总读取量都不能当作 Chromium 的实读量，也没有证据确认 eMMC
  或其它启动任务是唯一根因。实验与边界见 [chromium-cold-start.md](chromium-cold-start.md) 和
  [boot-io-profiling.md](boot-io-profiling.md)。

## 5. NetworkManager 的 systemd 就绪点 —— ✅ 已做

- 历史记录中 NetworkManager 单元约 4.8 s，且移除 chrony 的一条网络排序没有改变相关 target 的等待链。
  单元时长本身不能说明它在等待 DHCP。**drop-in 删不掉单条 `After`**（systemd 255.4 实测，
  主 unit 那条会被并回来），所以只改 chrony 依赖没有改变该启动事务。
- **最终做法**：`overlay/etc/systemd/system/NetworkManager.service.d/10-lsp-ready-early.conf`
  （`Type=simple`，systemd 在进程启动后放行）+ 配套给更新器补有界等 NM startup
  `overlay/etc/systemd/system/data-collection-boot.service.d/10-lsp-wait-network.conf`
  （`nm-online -s -q --timeout=15`，只用于需要网络的业务）。
- 上游 `Type=dbus` 等待 NetworkManager 的 D-Bus 名称被取得，不等于等待 DHCP 或连接激活完成；`network.target`
  也不承诺网络可达。改为 `Type=simple` 改变 systemd 的启动就绪语义，不会让 Wi-Fi 关联或 DHCP 自身变快。
- 历史对照中 `systemd-analyze` userspace 为 **6.61 → 3.76 s**、总计 **10.06 → 7.23 s**、
  `graphical.target` **6.57 → 3.73 s**。这是 systemd 启动指标，不是 HTTP、FCP、物理首帧或网络可用时间的 A/B；
  不能据此声称节省了 DHCP 等待或得到相同的端到端收益。当前网络分工见 [boot-network-gating.md](tuning/boot-network-gating.md)。

## 6. cage 的纯黑底色 —— ✅ 已做（"Chromium 空窗"那一半仍不动）

- **已做**：fork 的 cage 支持 `--background=#RRGGBB`；配置
  `overlay/etc/default/lsp-kiosk` 的 `LSP_KIOSK_BACKGROUND=#f3f6fa`，
  unit 里 `ExecStart=/usr/bin/cage --background=${LSP_KIOSK_BACKGROUND} -- /usr/bin/chromium …`（已核实）。
  实测黑段 **0.57 → 0.43 s**（30 fps 抓帧逐帧比对）。细节：[合成器底色说明](kiosk/compositor-customization.md)、
  `packages/cage-lsp/`。
- **边界**：合成器底色不能覆盖 Chromium 提交的 buffer，也不能消除浏览器冷启动空窗。
  kiosk 会话保留浅色（`color-scheme=default` + `gtk-theme=Yaru`，只覆盖 kiosk 用户）以减小窗口与 HMI 的色差。
  旧路径曾观察到导航窗口底色变化，不代表当前每次导航均无灰帧，也不属于本轮首帧提速证据。
- ⚠️ 原方案是"把上位机主题色**硬编进合成器**"（用户当时否掉："上位机颜色又不是固定的"）。
  现在底色是**运行期配置**，换配色只改 `/etc/default/lsp-kiosk`、不用重编 cage ⇒ **当初"放最后"的理由已消除**。

## 7. 流程类（防再踩"板上跑的是旧版本"） —— ✅ 大部分已做（2026-10-03）

- ✅ **unit 参数断言已做**（2026-10-03）：`tools/verify-image.sh` 从 unit 的 `Exec*` 行抽出 `lsp-*` 命令，
  ① 断言路径在镜像里存在（抓改名）② 断言 `--flag` 字面量能在目标文件里找到（脚本 grep 文本、ELF `grep -a`）。
  ⛔ **不执行二进制**：实测 `lsp-powerkey --help` 不打印帮助而是**直接跑起来**（去开 `/dev/input/event1`）——
  用 `--help` 验参数既有副作用、又对没有 `--help` 的工具必然误报。板上实跑：11 处调用、1 个参数断言全过。
- ✅ **镜像版本指纹已做**（2026-10-03）：构建脚本 `rootfs` 阶段写 `/etc/lsp-image-release`
  （板卡 / 内核 / git sha（拿不到写 unknown）/ 脏标记 / 构建时间 / 构建机 / **脚本 sha256**），
  `tools/verify-image.sh` 会把 7 个字段打出来并断言存在 —— 现场"板上是哪一版"一眼能比。
- ROS 共享内存孤儿段：正解在采集程序里 `shm_unlink`（当前按用户决定不自动清，见
  `notes/system/dds-shm-orphans.md`）。

## 8. 三处底色 —— 已统一

Cage 的 `LSP_KIOSK_BACKGROUND`、本地着陆页、上位机工号页跟随 `#f3f6fa`。
上位机换皮肤时同步这三处，细节见 [kiosk/compositor-customization.md](kiosk/compositor-customization.md)。
这不等于浏览器启动完成，也不能用底色替代内容首帧。

## 9. 缺数据卡时 bootup 拖 ~90 s —— ✅ 已修（**新建镜像**；存量设备见第 11 条）

- 根因：`lsp-init-sdcard` 往 fstab 写的选项名拼错 —— `x-systemd.devices-timeout=5`（多一个 s），
  正确是 `x-systemd.device-timeout`。systemd **静默忽略**不存在的 `x-systemd.*` 选项，于是按默认 90 s
  设备超时等：实测 `mnt-data_1T.mount` 到 `[93.97 s]` 才 `failed with result 'dependency'`。
- 已核实修复：`overlay/usr/local/sbin/lsp-init-sdcard` 的 `FSTAB_OPTS="defaults,noatime,nofail,x-systemd.device-timeout=5"`。
  工厂常态（卡开机前插好）本来就不受影响。细节：`notes/storage/init-sdcard.md`。

## 10. 换一个更轻量的 Chromium（保留硬解） —— ❌ 暂不做

- 现状：板上 chromium 是**普通 Ubuntu deb**（`151.0.7922.169-1`，无 Rockchip 补丁），硬解 =
  Chromium 自带 V4L2 VDA + `libv4l-rkmpp` + 伪节点 `/dev/video-dec0` 三件套 ⇒ 理论上换个轻量 build 能保住。
- 但：① 现成轻量版（ungoogled / Thorium / Debian chromium）都**不带 H.264**、多半也不开 V4L2 VDA，
  `chromium-headless-shell` 连显示都没有 ⇒ 只能自己编（40+ GB 源码、24 核跑 3–8 小时）；
  ② 当前预热实验只测了单次主页 HTTP 响应，不能据此给替换浏览器的收益设上限或归因到冷读 I/O。
- 明细（含"两块从不被 mmap 的死重量 46 MB，删了只省磁盘不省时间"）：`notes/system/browser-choice-hardware-decode.md` §6.7。

---

## 11. 卡不在时白等 90 s（fstab 选项拼写） —— ✅ 已修（2026-10-03）

- **实测**（开发板 `<HOST_07>`，eMMC，数据卡没插）：`systemd-analyze` 报
  `3.547s (kernel) + 1min 30.448s (userspace)`；**画面那条链不受影响**（`graphical.target` 仍在 2.0 s），
  被拖住的是 `local-fs` 之后的单元与"bootup 完成"这个标记。
- **根因**：`/etc/fstab` 里数据卡那行写的是 `x-systemd.devices-timeout=5`（**多一个 s**）。
  systemd 对不存在的 `x-systemd.*` 选项**静默忽略** ⇒ 按默认 `DefaultDeviceTimeoutSec=90` 等。
- **为什么不自动好**：这行是 `lsp-init-sdcard` 初始化数据卡时写的；脚本 2026-09-30 已修，
  但**已写进 fstab 的设备不会跟着更新**（fstab 不在 `overlay/` 里）。
- **修法**：`tools/lsp-fix-fstab-device-timeout.sh`（幂等、`--check` 只读、默认先备份再 `daemon-reload`）。
  板上 A/B：改前 `--check` rc=1、device 单元 `JobRunningTimeoutUSec=infinity`；
  改后 rc=0、**`JobRunningTimeoutUSec=5s`**。
  更直接的判据（2026-10-03 板上实测，不用重启）：手动 `systemctl start mnt-data_1T.mount`（卡不在）
  ⇒ rc=1、**耗时 5.27 s**；而管理器默认 `DefaultDeviceTimeoutUSec=1min 30s`（错拼写被忽略时走的就是它）。
- **重启后端到端实测（2026-10-03，同一天）**：
  `systemd-analyze` = `3.441s (kernel) + 5.672s (userspace) = 9.113s`（修前是 **1min 30.448s** userspace）
  ⇒ **省下 84.8 s**；`graphical.target` 不受影响（2.195 s）；kiosk 开机自动起（`active`）；
  重启过程本身 6 s 就回来了（关机 18:36:12 → 开机 18:36:18，boot id `af2d924a`）。
  ⚠️ 现在 userspace 里最大的一项**就是那 5 s**（`x-systemd.device-timeout=5` 在等没插的卡）：
  想再省可把它调小（如 2 s），代价是慢一点的卡在开机时可能来不及枚举 —— 属可选微调，未做。
- **防复发**：`tools/verify-image.sh` 两条断言（镜像 fstab + `lsp-init-sdcard` 源文件里不得出现错拼写）、
  `tests/fstab-timeout-tool.sh` 锁工具行为（**18 通过 / 0 失败**）。
  ⚠️ 教训：断言**只查非注释行** —— 脚本里那段解释性注释本身就写着错拼写，grep 整份文件会把它自己命中
  （与 2026-10-03 修的 `console=tty1` 断言同一类坑）。
- ⛔ **仍未修**：采集发布包 v1.0.19 / v1.0.20 里的 `sdcard_init.sh` 也写着错拼写（`overlay/opt/`，用户保护目录）
  —— 需上位机侧改。明细见 [../storage/init-sdcard.md](../storage/init-sdcard.md)。

---

## 12. 内核 initcall 分析 —— 已完成并实施 GMAC / RAID6 调整

- 方法：cmdline 加 `initcall_debug ignore_loglevel` + 串口抓包 + 内核自带
  `scripts/bootgraph.pl` 画图；**完整方法、Top 榜与坑见
  [../hardware/kernel-initcall-profiling.md](../hardware/kernel-initcall-profiling.md)**。
- 实测（第一轮，改前）：`[0.000]` → `Run /init` = **3.624 s**，其中 initcall 自身 **2.007 s**；最慢三笔
  `rk_gmac_dwmac_driver_init` **618 ms**、`raid6_select_algo` **510 ms**、`deferred_probe_initcall` 309 ms。
- ✅ **第二轮（2026-10-03，第 14/15 条落地后重跑）**：内核阶段 **2.939 s（−0.685 s）**、initcall 合计
  **1.359 s（−648 ms）**、`calling` 908 → 884、initrd 18288K → 14492K。剩下 Top 全是"设备要不要"那一类
  （deferred probe 320 ms / of_platform 93 / cpufreq 73 / multicodecs 62 / rk3x_i2c 45 / rknpu 29）⇒
  **内核阶段里已没有白花的大钱**。
- ⛔ 测量陷阱（第二轮踩到）：`returned … after N usecs` 会把窗口里的**异步事件**算进去 ——
  `init_blk_tracer` 因此报 461 ms（其窗口里夹着 `Freeing initrd memory`）。细节见
  [../hardware/kernel-initcall-profiling.md](../hardware/kernel-initcall-profiling.md) §5 第 6 条。
- 第 15 条已让 RAID6 成为模块，启动期不再执行基准；不能再把关 benchmark 记为额外 510 ms 收益。
- 其余（GMAC/PCIe/NPU 改模块）是**产品权衡**（网口/NVMe 可用时刻会推后），不是纯启动优化。
- ⛔ 口径：带这两个调试参数的启动**本身慢 0.80 s**（刷串口），收益必须在**去掉参数后**复测。

## 13. Plymouth 与 Cage 交接 —— ✅ 已做

`lsp-kiosk.service` 在 Cage 接管前退出 Plymouth，不等待上位机 HTTP 就绪。Chromium 启动期间先由 Cage 底色接管；
引导页只能在浏览器开始绘制后显示，不能遮住浏览器自身的冷启动空窗。
当前单元顺序与 HDMI 生命周期见 [session-design.md](kiosk/session-design.md)。不由上位机在加载后调用 `plymouth quit`。

LVGL 直出 DRM 的 splash 行为单独记录在 [lvgl-drm-direct-boot.md](lvgl-drm-direct-boot.md)，不用于推断 kiosk 的启动时间。

---

## 14. 内核里的以太网（GMAC probe 618 ms） —— ✅ 已做（2026-10-03）

- 背景：`rk_gmac_dwmac_driver_init` 是内核 initcall 的 Top 1（**618 ms**，两路 GMAC 同步 probe）——
  但本产品**不用 RJ45**（一直用无线网卡），只需要"保留有线能力"。
- 改法（`kernel-6.1/arch/arm64/configs/rockchip_linux_defconfig`）：
  `CONFIG_STMMAC_ETH=m` + `CONFIG_STMMAC_PLATFORM=m` + `CONFIG_DWMAC_ROCKCHIP=m`、`# CONFIG_DWMAC_GENERIC is not set`。
  ⛔ 三件套必须同为 `=m`：`dwmac-rk.c` 调 `stmmac_probe_config_dt()`（在 `stmmac_platform.c` 里），
  内建代码引用模块符号会链接失败。`DWMAC_GENERIC` 只匹配通用 `snps,*`，本板 GMAC 是 `rockchip,rk3588-gmac` ⇒ 关掉更瘦、绑定更确定。
- **实测（同配置 floor + 无 plymouth，各一次真重启）**：
  app `first_frame` mono **3.996 → 3.367 s（−0.63 s）**；`systemd-analyze` kernel **3.425 → 2.640 s**；
  GMAC probe 从内核阶段（mono 2.076）挪到用户态 **mono 4.990**（app 首帧之后 1.6 s）。
- **有线能力保留的判据**：`lsmod` 见 `dwmac_rockchip`/`stmmac_platform`/`stmmac`（udev 自动加载）；
  `ip -br link` 有 `eth0`/`eth1`（`DOWN,NO-CARRIER` = 没插线）；NM 托管例外只有 `usb*` ⇒ **插线即可用**。
- 出处与方法：[../hardware/kernel-initcall-profiling.md](../hardware/kernel-initcall-profiling.md)（第 4 节杠杆表已更新为实测值）。

---

## 15. RAID6 开机基准 510 ms（btrfs 带进来的） —— ✅ 已做（2026-10-03）

- **为什么会有它**：`lib/raid6/` 是软件 RAID6 的 GF(2⁸) 双校验运算库（跟机械盘无关）。内核里只有两处 select 它：
  `fs/btrfs/Kconfig:18`（btrfs raid56）与 `drivers/md/Kconfig:131`（`MD_RAID456`）。我们两处都没"用"，
  但 `CONFIG_BTRFS_FS=y` 把它拉成了 `=y` ⇒ `RAID6_PQ_BENCHMARK=y` 在开机把 6 套实现各跑一遍（**510 ms**，
  串口里那 6 行 `raid6: neonx8 gen() 6358 MB/s`）。
- **改法**：`# CONFIG_BTRFS_FS is not set`（用户 2026-10-03 拍板：启动介质只有 eMMC/TF，都是 ext4；
  以后 NVMe 数据盘也用 ext4）。连带效果：`RAID6_PQ` 降为 **`=m`**（由 `DM_RAID=m → MD_RAID456=m` select）
  ⇒ 基准只在 dm-raid 模块加载时跑，**启动阶段不再有这 510 ms**；md/dm-raid 的 RAID6 能力保留（模块）。
- **实测**：`dmesg | grep -c raid6` = **0**；`systemd-analyze` kernel **2.640 → 2.556 s**；
  app 首帧 mono **3.367 → 3.101 s**（这两笔含第 14 条的累计：kernel 3.425 → 2.556 ≈ **−0.87 s**）；
  kernel deb 小 **446 KB**；`modinfo btrfs` 失败、config 里 `# CONFIG_BTRFS_FS is not set`、`CONFIG_RAID6_PQ=m`；
  failed 单元 0、eth0/eth1 与 4 个 ETH 模块照旧。
- 备选（若将来要保留 btrfs 又想去掉基准）：`# CONFIG_RAID6_PQ_BENCHMARK is not set` —— arm64 上跳过基准
  仍会选 `neonx8`（与基准结果相同），零代价。

## 16. 顺带发现的两个线索 —— 一个已修、一个是假线索（2026-10-03）

1. ✅ **`lsp-init-sdcard.service` 没插卡时白等 3 s —— 已修**。
   原因：脚本里 `udevadm settle` + `6 × 0.5 s`（`WAIT_TOTAL_TICKS × WAIT_TICK_SEC`）的"等卡枚举"兜底循环。
   改法（`overlay/usr/local/sbin/lsp-init-sdcard-boot`（镜像源码：`overlay/usr/local/sbin/lsp-init-sdcard-boot`））：新增 `card_hint()`
   —— 只有当**某个 mmc host 上有卡设备（`mmcX:XXXX`）且它对应的 `mmcblkX` 不是系统盘**时才进等待循环；
   否则立即 `no SD card present (no non-system mmc card); skip the wait` 退出。
   **实测**：单元耗时 **3,388 ms → 160 ms（−3.23 s）**，从 `systemd-analyze blame` 第 1 名跌出前五；
   app 首帧 3.101 → 3.033 s（这服务本来就不在画面关键路径上，所以画面时间基本不变 —— 省的是 **bootup 完成时刻**）。
2. ❌ **"floor 单元被 `Conflicts=getty@tty1` 拖到 2.9 s" —— 假线索**。
   算一遍就清楚：`systemd-analyze critical-chain` 里的 `@289ms` 是**用户态时间**（systemd 起算），
   而 systemd 起于 monotonic ≈2.556 s ⇒ `2.556 + 0.289 = 2.845 s`，正好等于实测的激活时刻 2.857 s。
   也就是说**它已经在 systemd 起来后 289 ms 启动**，没有任何排序开销可压；`getty@tty1` 那次 boot
   也根本没启动（`ExecMainStartTimestampMonotonic=0`）。⇒ LVGL 路线的画面时间里，除内核阶段外没有可压的排序项。

> 附：`systemd-analyze` 的 **userspace ≈5.5 s** 主要不是任何单元慢，而是 `mnt-data_1T.mount`
> 按 `x-systemd.device-timeout=5` **在等那张没插的数据卡 5 s**（我们 2026-10-03 自己设的值）。
> 它不在画面链上，但决定"bootup 完成"这一刻；想再压就把它调到 2 s（代价：慢卡可能来不及枚举）。
