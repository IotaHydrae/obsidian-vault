# 数据 SD 卡初始化：`lsp-init-sdcard`

镜像内置 `LABEL=Data1T` 的设备触发挂载配置：已有 ext4 数据卡插入后，自动挂到
`/run/lsp_data_1T`，成功后才发布 `/mnt/data_1T` 链接。无卡时不等待设备，也不保留可写的 eMMC 入口。
`lsp-init-sdcard` 负责新卡的分区、ext4 格式化、卷标、权限与自检，不是已有数据卡自动挂载的前提。
数据卡统一使用 `nosuid,nodev`：禁止介质文件借 setuid/setgid 位或文件 capabilities 提权，
并禁止使用介质内的设备节点；普通读写、程序执行、格式化与热插拔流程保持不变。
镜像内命令为 `/usr/local/sbin/lsp-init-sdcard`（0755），源文件在 `overlay/usr/local/sbin/`。

**最终布局（硬要求）**：**一个 ext4 分区吃满整张卡** —— 起始扇区 2048（1 MiB 对齐），
结束扇区 = 整盘最后一个扇区（1 TB 卡实测 `Start 2048 / End 2000011263`，占整盘 99%，
只留 1 MiB 对齐空隙），文件系统覆盖整块分区（脚本自检 ≥99%）。

**范围**：只做**数据卡**。**不含**刷机卡功能（做 SD 启动卡 / 刷 eMMC 用
`tools/make-sd-image.sh`（镜像源码：`tools/make-sd-image.sh`） + `tools/write-sd.sh`）。
卡上原来是不是系统布局无所谓 —— 用过的启动卡不再做刷机用途时照样可以当数据卡
（本命令**不按卷标拒绝**）。**当前系统盘**（承载 `/`、`/boot/firmware` 的那块盘）始终拒绝。
已有 ext4 分区未铺满整盘时，未传 `--force` 也会拒绝：**不卸载、不改分区起点、不格式化**。
`--force` 明确允许整盘重新分区与格式化，会删除原数据；本工具不做保留数据的扩容。
设备映射、分区枚举、文件系统探测或只读验读失败时拒绝写盘，不能据此认定为新卡。
首 4 KiB 与启动卡 RKNS 位置的验读仅用于拦截明显读取失败，不是完整介质健康测试。

## TL;DR

- `lsp-init-sdcard` 把一张新卡初始化为数据盘：**整盘重建分区 → ext4（卷标 `Data1T`）→ 写
  `/etc/fstab`（`LABEL=Data1T`）→ 挂到挂载点**。
- 镜像默认支持 `Data1T` 卡**插卡自动挂载**、拔卡停止挂载、重新插入恢复；默认入口 `/mnt/data_1T`。
  挂载时拉起 `lsp-sync-data`，移除时一起停止（见 §4.1）。
- `/mnt/data_1T` **只在卡挂载成功后存在**：卡实际挂到 `/run/lsp_data_1T`，再发布符号链接入口。
  无卡、挂载失败或卸载后收回入口，不留可写的 eMMC 目录（§4.2）。
- 已有 ext4 不重建分区；未铺满整盘的布局返回 **2**，并保留数据和现有挂载。
- 上位机通过 **ssh** 调用它（§1）；原脚本的问题与逐条修正见 §2。
- 板上实测（SanDisk 1 TB）见 §3；取舍与其它说明见 §4；自测/复现命令见 §5。
- 镜像 `image` 阶段内置数据卡 fstab 条目，但**不创建 `/mnt/data_1T`**；入口只由挂载成功后的服务发布。

## 1. 上位机怎么调用

```bash
# 预览（不动任何东西）
ssh board@<设备IP> "echo <口令> | sudo -S -p '' /usr/local/sbin/lsp-init-sdcard --dry-run"
# 正式初始化
ssh board@<设备IP> "echo <口令> | sudo -S -p '' /usr/local/sbin/lsp-init-sdcard"
```

退出码：`0` 成功 / `1` 用法或环境错（没插卡、容量不达标…）/ `2` 安全校验拒绝（系统盘，或未授权重做的 ext4 布局）/
`3` 设备操作失败。成功时最后会打印 13 项自检结果。

常用参数：

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--device PATH` | 自动挑 | 目标分区或整盘。给整盘时会自动用它已有的分区（不会每次重建分区表） |
| `--label NAME` | `Data1T` | 卷标。**fstab 就是用它挂载的**（`LABEL=<NAME>`）→ 换卡不用改 fstab；只允许 字母/数字/`._-`、≤16 字符（空白与反斜杠是 fstab 的转义字符，会让那一行解析歪；不合规直接拒绝，退出码 1） |
| `--mountpoint DIR` | `/mnt/data_1T` | 必须为无空白的绝对路径；解析符号链接后拒绝系统目录及其它盘的现役挂载点 |
| `--owner USER` | `board` | 挂载点属主 |
| `--min-size-gib N` | `900` | 整盘容量下限，小于它拒绝执行（`0` = 不检查） |
| `--dry-run` / `-n` | 关 | 只打印将执行的命令 |
| `--force` / `-f` | 关 | 已经是 ext4 也重新格式化 |

带值参数缺值或为空时返回 1；显式传入空 `--device` 不会退回自动挑卡。

## 2. 原脚本的问题与修正（逐条）

原脚本（用户提供）能跑通，但有 15 处会咬人的地方。**加粗**的是会导致数据/系统风险的：

| # | 原脚本 | 问题 | 现在 |
| --- | --- | --- | --- |
| 1 | `DEVICE="/dev/mmcblk1p1"` 写死 | **换机/换口就找错盘或找不到**：从 SD 启动时卡是 `mmcblk0`，走 USB 读卡器是 `sda1`（实测过） | 支持 `--device`；默认自动挑「SD / USB 存储里容量最大的一块」 |
| 2 | `PASSWORD="<SECRET>"` + `sudo -S -v` | **口令进了脚本/仓库**，且和设备 board 口令不一致；谁拿到脚本谁能提权 | 删除。脚本要求 root，非 root 直接拒绝并打印正确调用方式 |
| 3 | `USER="cat"`（后来改 board） | `USER` 是 shell 的当前用户变量，覆盖它会串味 | 改名 `OWNER`，默认 `board` |
| 4 | 分区大小检查只比**分区**、不达标 `exit 1`（注释写"用 fdisk 重建分区"但没实现） | 新卡出厂分区常不铺满整卡 → 脚本直接退出，卡用不了 | 比**整盘**容量（`--min-size-gib`）；**凡是要格式化（新卡 / `--force`）就重建分区表**：单分区 2048 → 整盘末尾，工厂偏移一起收回来（`sfdisk`，MBR；>1.8 TiB 自动 GPT）。已在跑的数据卡（已是 ext4 且吃到末尾）分区表与文件系统都不动 |
| 5 | 没有"别动系统盘"护栏 | **一旦指定错设备就格式化掉 eMMC/系统盘** | `is_system_disk()`：承载 `/`、`/boot/firmware` 的盘一律拒绝（退出码 2）；自动挑卡只在 SD/USB 里选 |
| 6 | 只 `umount $DEVICE` | 该盘其它分区还挂着时 `mkfs` 会 busy | 正常卸载该盘**所有**分区；busy 等失败立即停止，不用 lazy unmount 后继续格式化 |
| 7 | `yes \| sudo mkfs.ext4 …` | `yes` 无意义；mkfs 输出被吞；950 GB 格式化几十秒**全程无输出**（违背"耗时操作不许静默"） | 去掉 `yes`；`mkfs.ext4 -F -m 0 -L … -E lazy_itable_init=1`；打印命令 + 前后日志 + 耗时（板上实测 **34 s**） |
| 8 | 用 `e2label` 判断/设置卷标 | 卡是 **exfat** 时 `e2label` 直接报错 → 脚本以为卷标不对、反复失败 | 用 `blkid` 判断格式/卷标；只在 ext4 上用 `e2label` 补卷标；识别到 exfat 会明确打印"出厂默认 exfat → 改为 ext4" |
| 9 | `sed -i "\#…\#s\|UUID=[a-f0-9-]*\|…\|"` 改 fstab | ① exfat 的 UUID 是**大写**十六进制（实测 `EFE5-EE60`），`[a-f0-9-]` 匹配不到 → 静默改不动；② 直接 `-i` 改 `/etc/fstab`，中断可能留半个文件；③ 只换 UUID，fs/options 变了不更新 | 按**挂载点**整行重写：`awk` 生成 → 写 `/etc/.fstab.lsp-init.$$` → `chmod/chown` → `mv -f` **原子替换**；自检"该挂载点条目数 = 1"。写的是 **`LABEL=<卷标>`**（2026-09-15 起，不再写 UUID=，见 §4.1） |
| 10 | `sudo mount -a &>/dev/null` | 挂载失败被吞掉，脚本还报成功 | `mount "$MOUNTPOINT"` 失败即 `die`；自检 `findmnt` 的挂载点与挂载源 |
| 11 | `chown -R $USER:$USER $MOUNTPOINT` | **递归** chown 一张有数据的 950 GB 卡很慢，还会改掉历史文件属主 | 只 `chown` 挂载点本身（不递归） |
| 12 | 无幂等/无自检 | 重复执行会不会又格式化？出错只知道 exit 1 | 幂等（已是 ext4 就跳过，`--force` 才重做）；结尾 **13 项自检**（格式/卷标/**fstab 用的挂载源 = `LABEL=`**/起始扇区/吃满末尾/ext4 覆盖整分区/容量占比/fstab 条目数/挂载点/挂载源/属主/可写）；退出码 0/1/2/3 分类 |
| 13 | `THRESHOLD=$((900*1024*1024*1024))` 写死 | 同 4 | `--min-size-gib N`（默认 900，`0` 关闭） |
| 14 | 参数散落（`DEVICE`/`LABEL`/`MOUNTPOINT` 各写一份，mkdir 又写死一遍） | 改参数容易只改一半 | 全部参数化，默认值集中在文件头 |
| 15 | 没有 `--dry-run` | 破坏性操作没法先预览 | 新增 `--dry-run`：打印每条命令，不执行 |

### 2.1 关于 exfat：为什么最终格式化成 ext4

新卡出厂是 exfat，但设备端的数据要归 `board` 用户读写（`chown`）——
**exfat 没有 POSIX 属主/权限位**，挂上来没法保证归 board（只能在挂载参数里 `uid=/gid=`，
一改参数就变），而且 `rsync -a`、`tar` 保留权限这些都会失真。
所以脚本对非 ext4（含 exfat）的卡统一重新格式化成 **ext4** 并打卷标 `Data1T`。
卡上的数据要给 PC 用就走设备端共享/rsync，**不要把卡拔下来插 PC**（那会绕过 fstab 与属主约定）。

## 3. 板上实测记录 → 见 [init-sdcard-board-tests.md](init-sdcard-board-tests.md)

主流程、幂等、安全护栏、loop 假卡测重建分区表、`LABEL=` 改造的实测记录**已搬成单独一篇** →
**[init-sdcard-board-tests.md](init-sdcard-board-tests.md)**。


## 4. 其它说明与取舍

- **分区表类型**：重建时用 MBR（`dos`），>1.8 TiB 才用 GPT（MBR 上限 2 TiB）；
  新表的分区类型码是 `83`（Linux）。已经在跑的数据卡不动它的分区表，
  所以那种卡上可能还留着出厂的类型码（如 `0x07`）—— 纯属观感，Linux 不看它。
- **"吃满整卡"的准确含义**：分区起点 2048 扇区（1 MiB 对齐，MBR 单分区的惯例；
  0~2047 留给分区表/对齐，任何工具的单分区布局都这样），终点 = 整盘最后一个扇区，
  ext4 覆盖整块分区。1 TB 卡上"少掉"的 1 MiB 就是这个对齐空隙（0.0001%）。
- **不按卷标挑卡**：卡上原来是不是 `system-boot`/`writable`（或 `-sd` 变体）都无所谓 ——
  用过的启动卡不做刷机用途时可以直接拿来做数据卡。本命令**不做**刷机卡相关的任何事。
- **`-m 0`**：数据盘不保留 5% 给 root（1 TB 卡上那是 ~48 GB）。
- **`-E lazy_itable_init=1`**：inode 表延迟初始化，格式化快（否则要写满 7629 个组）。
- **fstab 只由卷标 `.device` 拉起挂载**：开机已有卡和运行中插卡走相同路径；无卡不排挂载任务。
  `nofail` 与 `x-systemd.device-timeout=5` 保留为容错选项，不能靠超时替代设备触发。

### 存量设备：已有 ext4 卡却没有挂载

旧镜像没有默认数据卡 fstab 条目；刷机后即使插入 ext4、卷标为 `Data1T` 的旧卡，
开机初始化器也会按安全策略跳过 ext4，不能补齐挂载配置。GNOME 自动挂载又已禁用，
因此卡会保持未挂载；这不表示卡需要重新格式化。

先用 `lsblk -f`、`blkid` 核对文件系统与卷标，再检查 `/etc/fstab` 和
`systemctl status run-lsp_data_1T.mount lsp-data-card-path.service`。迁移方法见 §4.1：
只更新配置并启动挂载，保留原分区、文件系统、UUID 与数据。
旧 fstab 中若有 `x-systemd.devices-timeout`，应一并删除或改为单数 `device-timeout`；
systemd 会忽略拼错的选项。`tools/lsp-fix-fstab-device-timeout.sh` 只修拼写，不补缺失条目或迁移挂载点。

### 4.1 镜像内置自动挂载与热插拔

`scripts/build-lsp-image.sh` 在 `image` 阶段生成 `/etc/fstab` 时内置标准条目：

```text
LABEL=Data1T /run/lsp_data_1T ext4 defaults,nosuid,nodev,noatime,nofail,x-systemd.device-timeout=5,x-systemd.device-bound,x-systemd.wanted-by=dev-disk-by\x2dlabel-Data1T.device 0 2
```

`systemd-fstab-generator` 为卷标设备生成 `.device.wants/run-lsp_data_1T.mount`：
开机枚举或热插拔时，设备出现就启动挂载；`device-bound` 在设备消失时停止挂载。
`x-systemd.wanted-by=` **仅指定卷标设备，不加入 `local-fs.target`**，因此缺卡时不创建等待设备的挂载任务。
无需访问目录，也无需桌面会话。手动初始化会写入同样的策略，自定义卷标的设备单元名由 `systemd-escape` 生成。
按卷标挂载允许数据卡在设备间互换，但不能同时连接两张同为 `Data1T` 的卡。
`nosuid,nodev` 不改卡内文件的属主和权限位，只限制本次挂载的权限语义；未加入 `noexec`。
已有设备仅更新脚本不会改变现存 fstab 或当前挂载选项；需同步条目并在正常卸载后重新挂载，
不需要为此重新格式化，也不要以 `--force` 迁移挂载选项。
初始化写完 fstab 会立即 `daemon-reload`；重做卡期间临时 mask 挂载，防止分区枚举/写卷标与 mkfs 竞速，
成功或失败退出时都撤销本次的 runtime mask。用户已有的 mask 保留，并拒绝重做。

默认数据卡的真实挂载单元是 `run-lsp_data_1T.mount`，fstab 目标为 `/run/lsp_data_1T`。
挂载 drop-in 拉起 `lsp-data-card-path.service`，发布入口后才启动同步服务；移除时反向停止（§4.2）。
自定义卷标同样生效；自定义 `--mountpoint` 沿用直接挂载，同步服务仍只负责默认的数据路径。
旧设备升级时先安装当前入口服务、挂载 drop-in 与 tmpfiles 配置；若卡仍直接挂在 `/mnt/data_1T`，
先停止采集并正常卸载。备份 `/etc/fstab`，保留系统盘等其它条目，将默认数据卡条目替换为上面的标准行
（缺失则添加），移除旧的 `mnt-data_1T.mount.d/10-lsp-sync-data.conf`，再执行：

```bash
sudo systemctl daemon-reload
sudo systemd-tmpfiles --create /etc/tmpfiles.d/lsp-data-card.conf
sudo systemctl start run-lsp_data_1T.mount
findmnt --mountpoint /run/lsp_data_1T
readlink /mnt/data_1T
```

最后两项应分别显示数据卡挂载、`/run/lsp_data_1T` 链接目标。此次迁移不运行初始化器、不格式化卡；
卡不在时只更新配置，等待以后插卡触发，跳过上面的手动启动命令。

开机包装器和手动入口共用 `/run/lsp-init-sdcard.lock`，锁覆盖资格检查、写盘、fstab 与挂载。
包装器持锁后通过内部 `--lock-fd 9` 传给主工具；主工具核对锁文件与描述符，再复用同一把锁。
手动入口冲突返回 2，开机入口冲突跳过；缺少锁工具或锁不安全时拒绝初始化。

自动挂载不保证写入中直接拔卡的数据完整性；取卡前应停止采集并正常卸载。
`tests/init-sdcard-hotplug.sh` 在临时 ext4 loop 盘验证：无设备触发的反对照、自动挂载、设备移除、
重插恢复、封口同步、数据与 UUID 保留；另验证无卡/挂载失败/移除后路径不存在且写入失败、
开机残留入口清理、非空目录与无关符号链接保护、tmpfiles 不改已挂载卡的属主/权限。
当前配置在 Ubuntu 24.04 / systemd 255 的本机 loop 回归为 **36 通过 / 0 失败**
（2026-10-22，x86_64，内核 `7.0.0-38-generic`），包括出厂配置的设备触发、无开机等待依赖，
以及首次挂载/重插后的 `nosuid,nodev`、普通文件读写与直接执行。分区/格式化回归另为 **38 通过 / 0 失败**。
工程机此前已验证真实 SD 的分区表/卷标/UUID 保持，以及上位机挂载命名空间中 kiosk 可写；
该现场记录未覆盖 `nosuid,nodev`。本次未部署到开发板或工程机；此前真实 SD 的合成 remove/add 验证也未覆盖物理拔卡。
依据：`systemd.mount(5)` 的 `x-systemd.wanted-by=`、`x-systemd.device-bound`（Ubuntu noble / systemd 255）。

### 4.2 无卡时不留 eMMC 目录

不能直接把 SD 挂到 `/mnt/data_1T`：systemd 会创建挂载点，卸载或挂载失败后空目录仍留在 eMMC。
默认方案先挂到 `/run/lsp_data_1T`（底层是 tmpfs、未挂载时权限 0000），挂载成功后由
`lsp-data-card-path` 发布 `/mnt/data_1T -> /run/lsp_data_1T`；无卡时这个入口不存在。

`lsp-data-card-path.service` 用 `BindsTo=` + `After=run-lsp_data_1T.mount`，确保先挂载后发布、
停止时先停同步再收回入口、最后卸载。`ExecStopPost` 覆盖启动失败；重插重新发布。
`tmpfiles.d` 的 `r!` 在开机时清理上次留下的入口，普通 `--create --remove` 不会收回在用的入口。
真实路径的 `d` 规则用 `:0000 :root :root`，只给新建的底层目录设权限；已挂载卡的属主/权限保持。
迁移只移除**空**的旧目录；非空目录、无关文件/符号链接都拒绝删除，保留其中数据。

上位机的 `ReadWritePaths` 改为真实的 `/run` 路径（由 tmpfiles 预建），不绑定公开入口；
否则开机无卡时建的挂载命名空间不能正确处理之后的插卡。初次初始化在 SD 上创建缺失的
`rosbags/`，让上位机能解析公开入口，已有数据/目录权限不动。
板上验证了实际系统配置：插入/移除/重插时，上位机命名空间中的就绪判据为 **True/False/True**，
两次封口都刷到卡；默认初始化器完整流程在自建 128 MiB loop 盘通过，公开入口/属主/权限/采集根正确，
卸载后入口消失。未改现场 SD 的分区或卷标，也未改上位机源码。

## 5. 复现/自测命令

```bash
# 在设备上（root）
lsp-init-sdcard --help
lsp-init-sdcard --dry-run                 # 预览
lsp-init-sdcard                           # 正式初始化
lsp-init-sdcard --device /dev/mmcblk0p2   # 期望：拒绝，退出码 2
# 卷标（fstab 挂载源）相关
lsp-init-sdcard --label 'a b'                      # 期望：拒绝，退出码 1（不能有空格）
lsp-init-sdcard --label 'too-long-label-1234567'   # 期望：拒绝，退出码 1（>16 字符）
lsp-init-sdcard --dry-run --min-size-gib 0         # 打印的 fstab 目标行应为 LABEL=…
awk '$2=="/run/lsp_data_1T"{print $1}' /etc/fstab  # 期望：LABEL=Data1T（不是 UUID=…）
findmnt --target /mnt/data_1T ; ls -ld /mnt/data_1T ; awk '$2=="/run/lsp_data_1T"' /etc/fstab
sudo -u board touch /mnt/data_1T/.t && rm /mnt/data_1T/.t && echo "board 可写 ✓"
```

`tools/verify-image.sh` 会检查：脚本存在且 0755、`--help` 正常、非 root 时拒绝执行、
含"拒绝操作系统盘"护栏、默认挂载点/属主正确、**fstab 那行用 `LABEL=` 而不是 `UUID=`**，
以及 live 模式下 `--dry-run` 能走通。

### 5.1 开机自动初始化：`lsp-init-sdcard.service`（2026-09-30 加）

用户要求："开机时如果检测到插入的 SD 卡不为 ext4 格式、且容量为 1T，那就格式化成数据卡，
注意不要误伤 SD 启动卡。"

- **单元**：`overlay/etc/systemd/system/lsp-init-sdcard.service`（`Type=oneshot`、`RemainAfterExit=yes`、
  `TimeoutStartSec=900`），`WantedBy=multi-user.target` 且 **`After=multi-user.target`** ——
  刻意排在开机事务之后：格式化 1T 卡是分钟级动作，绝不能挡住 kiosk/上位机起来。
  （代价：这张卡第一次被格式化的那一小段时间里，`/mnt/data_1T` 还挂不上；上位机自己的
  "内存卡初始化测试"步骤仍然可以随时手动调 `lsp-init-sdcard`，两边都幂等。）
- **包装脚本**：`overlay/usr/local/sbin/lsp-init-sdcard-boot`，四道闸门（**全部满足**才动手，
  否则秒退、退出码 0）：

  | # | 闸门 | 判据 |
  | --- | --- | --- |
  | 1 | 是 **SD 卡**（不是 USB 盘） | `/sys/block/mmcblk*/device/type` == `SD`；另外跳过承载 `/`、`/boot/firmware` 的盘 |
  | 2 | 容量 ≥ `--min-size-gib`（默认 **900 GiB**，即现场那张 1T 卡） | `blockdev --getsize64` |
  | 3 | 卡上**没有 ext4** | 整盘或任一分区 `blkid -o value -s TYPE` 都不是 ext4 |
  | 4 | 32 KiB 处**不是 Rockchip `RKNS` 启动头** | `dd bs=512 skip=64 count=1 \| head -c4` —— 这就是"别误伤 SD 启动卡"那道闸 |

  ⇒ 满足时调用 `lsp-init-sdcard --device <整盘>`（分区表/格式化/fstab/挂载/属主/自检全交给它，
  **fstab 仍然写 `LABEL=Data1T`，按卷标挂载**）。已有 ext4 卡只跳过初始化；其自动挂载由镜像内置 fstab 负责。
- ⚠️ **闸门 4 的取舍**：卡片曾经做过启动卡、后来被重建成数据卡时，0~1 MiB 空隙里会**残留**
  `RKNS` 字节（实测用户那张 1T 数据卡就是这样，虽然它是 ext4+Data1T，先被闸门 3 拦住）。
  所以一张"以前是启动卡、现在又不是 ext4"的 1T 卡会被本服务**拒绝**（失败方向是安全的那边）——
  这种卡走上位机/手工 `sudo lsp-init-sdcard --device /dev/mmcblk1` 即可。
- **并发**：包装脚本用 `flock -n /run/lsp-init-sdcard.lock` 防重入（开机服务与上位机手动调用撞上时，
  后者抢不到锁就直接退出）。
- **日志**：journal 里只有英文里程碑（开机日志可读）；工具本身的中文输出写
  `/var/log/lsp-init-sdcard-boot.log`。
- ⛔ **镜像里不许预建 `/mnt/data_1T`（2026-09-30 用户要求）**：没插数据卡时这个目录会落在
  eMMC 根分区上。现在只在卡挂载成功后发布入口，失败/卸载后收回（§4.2），
  `tools/verify-image.sh` 检查镜像没有预建公开入口，并有标准的设备触发数据卡 fstab 条目。
- **板上实测（开发板 <HOST_07>，2026-09-30）**：
  ① 真卡（1T ext4 `Data1T`，上面还有 `rosbags/` 数据）→ `already has an ext4 filesystem; leaving it untouched`，
     跑前跑后 MBR / 32 KiB 区 / 分区头 1 MiB 的 sha256 与 `blkid` **逐字节一致**，数据仍在；
  ② 1T 稀疏 loop 假卡（无 ext4、无 RKNS）+ `--dry-run` → 打印完整计划（`wipefs` → `sfdisk` 单分区吃满 →
     `mkfs.ext4 -F -m 0 -L Data1T` → fstab `LABEL=Data1T` → mount `/mnt/data_1T` → chown board）；
  ③ 同一张假卡在 32 KiB 写上 `RKNS` 后 → `carries a Rockchip bootloader header (boot/flash card); refusing to format`；
  ④ `--min-size-gib 2000` → `is 954G (< 2000 GiB); not a 1TB data card, nothing to do`；
  ⑤ 服务本体：`systemctl start` 8 ms 秒退（`status=0/SUCCESS`），journal 里那行是英文；
  ⑥ **真机端到端（同一张 1T 卡）**：先 `dd` 抹掉前 16 MiB + `wipefs -a` 伪装成"**无文件系统**的卡"
     （同时抹掉了它遗留的 `RKNS`），然后 `systemctl restart lsp-init-sdcard.service`：
     `mkfs.ext4` 11:57:11 → 11:57:44 **约 33 s**，结果
     `mmcblk1p1 ext4 LABEL=Data1T` 挂在 `/mnt/data_1T`、fstab 那行是 **`LABEL=Data1T`**、
     属主 `board:board` 0755、自检三项（挂载源/属主/可写）全 ✓。
  ⑦ **真机端到端（**"全新卡 = exfat"**，用户 2026-09-30 指出这才是出厂形态）**：把卡做成出厂样子
     —— 单个 `0x07` 分区 + `mkfs.exfat`（`blkid` = `TYPE="exfat"`，UUID `6EBD-A987`），
     再 `systemctl restart lsp-init-sdcard.service`：
     `[12:01:20] /dev/mmcblk1 (954G) is not ext4 (current filesystem: exfat); initializing it as the data card`
     → `mkfs.ext4` 12:01:20 → 12:01:54 **约 34 s** → 结果与 ⑥ 相同（`ext4` + `LABEL=Data1T` +
     挂 `/mnt/data_1T` + fstab `LABEL=Data1T` + 属主 board + 自检全 ✓）。
     ⚠️ 前置：板上原本没有 exfat 工具，这次为测试装了 `exfatprogs`（1.2.2，`mkfs.exfat`）；
     镜像里**不需要**它（格式化只做 ext4，识别 exfat 靠 `blkid`）。
     ⚠️ 一个小细节：`mkfs.exfat` 刚写完时 `lsblk` 可能还显示旧的 `ext4 Data1T`（块设备缓存），
     `udevadm trigger --subsystem-match=block && udevadm settle` 后就对了；`blkid` 一直是准的。
     ⚠️ 一个坑：服务是 `Type=oneshot` + `RemainAfterExit=yes`，**`systemctl start` 在它已 active 时
     是空操作**，要重跑得用 `systemctl restart`（我第一次就踩了：以为在跑，其实什么都没发生）。

## 6. 相关文档

- [removable-media-policy.md](../system/removable-media-policy.md) —— 为什么这张卡不会被自动挂载
- [sd-card-performance.md](sd-card-performance.md) —— 这张卡的读写性能实测（含 USB3 读卡器对比）
- [system-integration.md](../system/system-integration.md) —— 新增命令的四处同步清单
