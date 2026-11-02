# `lsp-init-sdcard` 的板上实测记录

> 本文是 [init-sdcard.md](init-sdcard.md) 的**板上实测记录**：主流程（出厂 exfat → 单个吃满整卡的 ext4）、
> 幂等性、安全护栏（指定系统盘必须拒绝）、用 loop 假卡测"重建分区表"、以及 fstab 改用 `LABEL=` 的改造与验证。
> 命令接口、原脚本 15 处问题的逐条对照、其它取舍与开机自动初始化见主篇。
> ⚠️ 正文**原样搬自主篇的 §3**（节号保留）。

## TL;DR

- 实测卡：SanDisk 1 TB；exfat → ext4 **34 s**；再跑一遍**不动分区表/文件系统**（幂等）。
- 安全护栏：显式指定系统盘会被**拒绝**；分区表重建用 loop 假卡验证过。
- fstab 改成 **`LABEL=Data1T`**（2026-09-15 用户要求），卡可在设备间通用。



### 3.1 主流程：exfat（模拟出厂新卡）→ 单个吃满整卡的 ext4

> ⚠️ 下面这段是 **2026-09-12 的实测输出**，那时 fstab 写的还是 `UUID=` —— 2026-09-15 起改成
> `LABEL=Data1T`（原因、约束与新的实测输出见 §3.5）；分区/格式化/自检那部分没变。

先把卡做成"出厂状态"：`mkfs.exfat -L Data1T /dev/mmcblk1p1`（分区仍是工厂布局：
**起始扇区 131072 = 64 MiB 偏移**，末尾到整盘末尾），然后跑脚本：

```
[14:28:06] 目标整盘: /dev/mmcblk1（现有分区 /dev/mmcblk1p1）
[14:28:06] /dev/mmcblk1 当前分区:
      mmcblk1     953.7G
      └─mmcblk1p1 953.6G exfat  Data1T
[14:28:06] 现状: 分区=/dev/mmcblk1p1 格式=exfat 起始扇区=131072 末尾空缺=0
[14:28:06] 计划: 重建分区表=Y 格式化=Y
[14:28:06] 分区吃到末尾但要格式化（新卡/--force）→ 顺手重建分区表，把工厂偏移也收回来
  $ wipefs -a /dev/mmcblk1
  $ echo ",,83" | sfdisk --quiet --label dos /dev/mmcblk1   # 单分区：1MiB 起 → 整盘末尾
[14:28:08] 新分区: /dev/mmcblk1p1（起始扇区 2048，954G，末尾空缺 0）
[14:28:08] 新卡出厂默认是 exfat —— 按设备需要改为 ext4（exfat 没有属主/权限位，chown 无意义）
[14:28:08] 格式化 /dev/mmcblk1p1 为 ext4（卷标 Data1T，954G）—— 秒级到一分钟，请等待
  $ mkfs.ext4 -F -m 0 -L Data1T -E lazy_itable_init=1 /dev/mmcblk1p1
[14:28:42] 格式化完成（耗时 34s）；ext4 已覆盖整块分区（954G）
[14:28:42] fstab 目标行: UUID=2369b88c-… /mnt/data_1T ext4 defaults,noatime,nofail,x-systemd.device-timeout=5 0 2
[14:28:42] fstab 里原有: UUID=3dea027f-… /mnt/data_1T ext4 …        ← 按挂载点整行替换，不重复追加
[14:28:42] fstab 已更新（当前该挂载点条目数: 1）
[14:28:43] ---- 结果自检 ----
  ✓ 分区 = /dev/mmcblk1p1        ✓ 文件系统 = ext4         ✓ 卷标 = Data1T
  ✓ UUID = 2369b88c-…            ✓ 分区起始扇区 = 2048     ✓ 分区吃到整盘末尾 = yes
  ✓ ext4 覆盖整块分区 = yes      ✓ 分区/整盘容量占比（99%：954G/954G） = yes
  ✓ fstab 条目数 = 1             ✓ 挂载点 = /mnt/data_1T    ✓ 挂载源 = /dev/mmcblk1p1
  ✓ 属主 = board:board               ✓ 可写 = ok
[14:28:43] 完成：/dev/mmcblk1p1（Data1T，954G）已挂到 /mnt/data_1T，属主 board   （退出码 0）
```

最终布局（`fdisk -l` / sysfs 实测）：

```
  Device         Boot Start        End    Sectors   Size Id Type
  /dev/mmcblk1p1       2048 2000011263 2000009216 953.7G 83 Linux
  起始扇区=2048  末尾扇区=2000011263  整盘末尾扇区=2000011263  占比=99%
```

即：**一个 ext4 分区，从 1 MiB 一直吃到整盘最后一个扇区**，工厂那 64 MiB 偏移也收回来了
（1 TB 卡上少掉的 1 MiB 是 MBR 对齐空隙，任何单分区布局都一样）。
之后以 board 用户在 `/mnt/data_1T` 建/删文件成功 ✓，`ls -ld` = `drwxr-xr-x 3 board board` ✓。

### 3.2 幂等：再跑一遍不动分区表、不动文件系统

```
[14:29:03] 现状: 分区=/dev/mmcblk1p1 格式=ext4 起始扇区=2048 末尾空缺=0
[14:29:03] 计划: 重建分区表=N 格式化=N
[14:29:03] 分区 /dev/mmcblk1p1 已经是 ext4 且吃到整盘末尾（起始扇区 2048）—— 分区表与文件系统都不动
[14:29:03] 已经是 ext4，跳过格式化与分区表重建（要强制重做加 --force）
（13 项自检全过，退出码 0；UUID 不变 2369b88c-… → 2369b88c-…）
```

`--device /dev/mmcblk1`（给整盘）同样走"用已有分区"的路，不会重建分区表 ✓。
要"无论如何重做一遍"（重建分区表 + 重新格式化）就加 `--force`。

### 3.3 安全护栏：指定系统盘必须拒绝

```
$ sudo lsp-init-sdcard --device /dev/mmcblk0p2      # 根分区
[14:18:22] 警告: mmcblk0 承载 /（/dev/mmcblk0p2）—— 这是系统盘，拒绝操作
[14:18:22] 错误: 拒绝操作系统盘 /dev/mmcblk0（安全校验） 2      → 退出码 2
$ sudo lsp-init-sdcard --device /dev/mmcblk0        # eMMC 整盘 → 同样退出码 2
```

### 3.4 重建分区表（用 loop 假卡测，因为真卡分区本来就是满的）

造一张 2 GiB 的"假卡"，做成"出厂 200 MiB exfat 小分区"：

```
loop0  2G
└─loop0p1 200M exfat Data1T
```

跑 `lsp-init-sdcard --device /dev/loop0p1 --min-size-gib 0 --mountpoint /mnt/looptest`：

```
[14:19:00] 分区 /dev/loop0p1 没铺满整盘 → 重建分区表
  $ wipefs -a /dev/loop0
  $ echo ',,83' | sfdisk --quiet --label dos /dev/loop0
[14:19:01] 新分区: /dev/loop0p1（2.0G）            ← 名字从 lsblk 读回，不再自己拼
[14:19:01] 分区 /dev/loop0p1 当前格式: exfat → 改为 ext4
  ✓ 13 项自检全过（挂载点 /mnt/looptest、属主 board:board）
```

清理：卸载 → 删掉测试 fstab 行 → `losetup -d` → 删镜像文件 ✓（`/etc/fstab` 里 `grep -c looptest` = 0）。
> 这轮抓到并修掉一个真 bug：原先用 `case … mmcblk*→p1; *→1` 拼分区名，
> 对 `/dev/loop0` 拼成了 `/dev/loop01` → 永远等不到设备。现在一律从 `lsblk` 读回。

### 3.5 fstab 改用 `LABEL=`（2026-09-15，用户要求）

**为什么改**：`UUID=` 是**每张卡各不相同**的，所以设备侧 fstab 只能对应"某一张卡" —— 换一张新卡
就得改一次 fstab（上位机要 ssh 上去改，或者把卡插回去重跑一遍 init）。改成 `LABEL=Data1T` 之后，
**任何一张按这套流程初始化过的卡，插到任何一台设备都会挂到同一个挂载点**，换卡零改动 ——
这就是"卡可以在设备之间用"。

设备侧 fstab 现在的样子（板上 `--dry-run` 实测输出）：

```
[11:49:49] fstab 目标行: LABEL=Data1T /mnt/data_1T ext4 defaults,noatime,nofail,x-systemd.device-timeout=5 0 2
  $ 重写 /etc/fstab：删掉挂载点 /mnt/data_1T 的旧行后追加上面这条（mktemp + mv 原子替换）
```

**"按挂载点重写"这条路径单独验过**（合成 fstab，不碰真 `/etc/fstab`）：旧 `UUID=` 行被删掉、
新的只有一条、无关行（`/` 与 swapfile）原样保留：

```
    # /etc/fstab
    UUID=old  /  ext4 defaults 0 1
    /swapfile none swap sw 0 0
    LABEL=Data1T /mnt/data_1T ext4 defaults,noatime,nofail,x-systemd.device-timeout=5 0 2
  ✓ 旧 UUID= 行已删掉   ✓ 新 LABEL= 行已写入（且只有 1 条）   ✓ 无关行原样保留
```

**卷标校验**（空 / 带空格 / 超长都当场拒绝，退出码 1 —— 绝不写出一条坏 fstab）：

```
  --label 'a b'                    → rc=1  错误: --label 只允许 字母/数字/._-（不能有空格或特殊字符…）
  --label 'too-long-label-1234567' → rc=1  错误: --label 最长 16 个字符（ext4 卷标上限），当前是 22 个
  --label ''                       → rc=1  错误: --label 不能为空
```

**代价与约束**：

- ⚠️ **两张同卷标的卡不能同时插**：内核按卷标解析，重复卷标会让第二张挂不上。现场是单卡槽、
  一次一张，够用；真要同时用两张，就给它们不同的 `--label`，各自配一条 fstab。
- ⚠️ **卷标限制 `[A-Za-z0-9._-]{1,16}`**（见上面的校验）：空白与反斜杠是 fstab 的转义字符。
- **已经在用的设备/卡**：重跑一次脚本即可（它按挂载点重写那一行，旧 `UUID=` 会被换成 `LABEL=`）；
  不想重跑就手工把该行第 1 个字段改成 `LABEL=Data1T`，再 `mount -a` 验证。
- 读卡上的数据不受影响：改的只是"怎么找到这张卡"，文件系统与内容都没动。
