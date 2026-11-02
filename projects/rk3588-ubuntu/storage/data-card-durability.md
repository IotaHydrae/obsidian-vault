# 采集数据在数据卡上的"落盘时机"（掉电会丢什么）+ 兜底守护 `lsp-sync-data`

> 2026-09-26 立档。起因：现场看到数据卡上**部分日期目录属主/权限全是 `?`**（`d?????????`），
> 怀疑"写入没 sync + 突然断电"。本文记录**实测结论**、采集侧的落盘现状，以及我们镜像侧
> 加的那道兜底。

## TL;DR

- 现象：数据卡上部分日期目录属主/权限显示 `d?????????` —— §1 说明它**不是"属主丢了"**。
- 实测：只"少 sync + 强制重启"**不会**产生 `?`（ext4 场景，§2）。
- 采集侧落盘现状（v1.0.20 源码）见 §3；**镜像侧兜底是 `lsp-sync-data`**（app，默认 enable，§4）。
- 怎么验、以及"还能做什么"见 §5。

## 1. 现象的含义：`d?????????` 不是"属主丢了"

`ls -l` 是"`readdir()` 取名字 + 逐条 `lstat()` 取元数据"两步。`d` 还在（来自目录项的
`d_type`）、但权限/链接数/属主/时间全是 `?` ⇒ **`lstat()` 失败**：目录项在、inode 元数据读不出来。
⛔ 所以这不是 chown/chmod 能修的；"整目录所有条目都 `?`"才可能是"父目录缺 `x`（搜索）权限"，
**只有部分条目** `?` 说明是那几个 inode 本身读不出来。

## 2. 实测：只"少 sync + 强制重启"**不会**产生 `?`（ext4）

2026-09-26 工程机（数据卡 `/dev/mmcblk1p1` 953.7G **ext4** 标签 `Data1T` → `/mnt/data_1T`），
连做 8 轮，每轮：

1. 在卡上按"日期目录 + 写文件 + 改名 + 删目录"猛造 15 秒（另在卡上放 256MB vfat 镜像做对照）；
2. `sync -f /`（**只刷 eMMC 根分区**，保住板子）；
3. `systemctl reboot -ff`（**不 sync** 直接重启 = 文件系统层面的突然掉电）。

结果：

```
每轮重启前 /proc/meminfo: Dirty=1.8~6.0 MB（有时 Writeback>0）  ← 卡上确实有没落盘的数据
每轮重启后: 卡上 '?' 条目数 = 0（连续 8 轮）
收尾 e2fsck -fn: Pass 1~5 全过、零修复；fsck.vfat 亦干净
```

- **ext4 + journal 会自愈**：journal 每 ~5 秒（`commit=`）提交，重放后一致 ⇒ 最坏只丢
  "最近几秒的更新"，**不会**留下读不出来的 inode。
- 所以那张截图更像下面三种之一：① 卡是 **FAT32/exFAT**（无 journal，掉电打坏 FAT 表/目录链）；
  ② 卡自身**退化**（坏块/映射表老化，`stat` 时好时坏）；③ **物理拔电**打到了**卡内部缓存/FTL**
  —— 主机侧 `reboot -ff` 不会切卡的供电，测不到这一层。
- 想稳定复现 `?`：用 FAT/exFAT 的**空卡/空分区** + **物理断电**（拔电/硬关）反复试；
  只想演示"症状本身"可以用 `dmsetup create flaky --table "0 <扇区数> error"`（读盘直接 EIO）。

## 3. 采集侧的落盘现状（v1.0.20 源码）

| 位置 | 行为 |
| --- | --- |
| `data_collection_trigger/system_power.py::power_off_host()` | **关机前 `os.sync()`**（注释原话：episode 收尾刚写完 mcap/MP4/metadata，断电前刷页缓存）⇒ 正常关机这条路是安全的 |
| `data_collection_trigger/video_preview.py::_fsync_file/_fsync_dir` | 预览 JPEG 写完后 fsync 文件 + fsync 目录 |
| `data_collection_uploader/...sdk.py` | 历史/状态文件走"临时文件 + flush + `os.fsync` + `os.replace`"的原子写 |
| ⚠️ `data_collection_trigger/episode_recorder.py::_seal()` | **写 `metadata.json` 没有 fsync**（`open(...,'w')` + `json.dump` + `close`）。而 `metadata.json` 是**封口时一次性写入**、又正是上位机的**验收判据**（按 mtime 判"这轮采到没"） |
| ⚠️ `data.mcap` / `cam_*.mp4` | 长写文件，全程没有 fsync ⇒ 任何时刻拔电丢最近 ≤5 秒 + 尾部未 finalize |

即：**缺口不在"全程没 sync"，而在"封口这个语义点没有落盘"**。直接拔电时，最后一集
（或它的 metadata.json）可能丢 —— 这正是下面那道兜底要解决的。

## 4. 镜像侧兜底：`lsp-sync-data`（app，默认 enable）

- **语义**：递归盯采集根（默认 `/mnt/data_1T/rosbags`），**只在 `metadata.json` 落盘时**
  （`IN_CLOSE_WRITE` / `IN_MOVED_TO` —— 后者覆盖"临时文件 + rename"的原子写法）对数据盘
  `syncfs()` 一次；**只刷那一块盘**，不碰 eMMC 根分区。
- **细节**：episode 目录是运行期新建的 ⇒ 收到 `IN_CREATE|IN_ISDIR` 就补 watch；
  `IN_Q_OVERFLOW` 保守刷一次；触发按 `LSP_SYNC_DATA_COOLDOWN_MS`（默认 1000ms）**合并不丢**
  （窗口内来的触发记下来，窗口一到再刷）；启动时刷一次；SIGTERM 干净退出。
- **单元**：`lsp-sync-data.service`（`User=root`、`Restart=always`、
  **`ConditionPathIsMountPoint=/run/lsp_data_1T`**（没插数据卡就整个不启动）、
  `Nice=19` + `IOSchedulingClass=idle`（不跟采集抢 IO））；构建期 **`systemctl enable`**。
  **Condition 只在启动时判定**，不会监听挂载事件。真实卡先挂到 `/run/lsp_data_1T`，
  `lsp-data-card-path.service` 发布 `/mnt/data_1T` 后拉起它；服务用 `After=` + `PartOf=` 跟随入口服务，
  移除时停止、重插后重新启动。无卡时公开路径不存在，避免写到 eMMC。
  插卡挂载配置与旧设备升级要求见 [init-sdcard.md §4.1](init-sdcard.md#41-初始化后的自动挂载与热插拔)。
  新建目录需要一个事件周期补 watch；封口测试先用 `/proc/<PID>/fdinfo` 确认目录监视已建立。
- **根目录恢复**：每次事件/空闲 poll 检查根目录 inode 和根 watch；目录被删、移走或卸载后
  清理旧 inotify 实例，路径再次出现时扫描新树并刷一次恢复。闲置时不扫描整树、不周期刷盘。
  默认数据卡路径必须是挂载点；原挂载消失后，即使下面有同名目录也不监视/刷根文件系统。
  独立守护的 SD 卸载/重挂测试验证存活恢复；集成 systemd 后，设备移除/重插会停止并重启服务，
  封口再次触发 syncfs，分区表与 ext4 UUID 不变。
- ⛔ **刻意不做周期性 sync**：`sync` 不会多写数据（只是把内核本来要写的脏页提前写，
  对卡寿命几乎无额外影响），但"等卡 flush"的抖动是真实的 —— 周期性刷会周期性打断采集写盘。
  周期兜底属于另一层：挂载 `commit=1`、或 `vm.dirty_expire_centisecs=200`。
- ⛔ **它保不住卡自己的内部缓存**：物理拔电那一刻卡里在飞的数据，host 侧任何 sync 都无能为力；
  要真正的零丢失，采集侧必须在封口点自己 fsync（见 §5）。

配置在 `/etc/default/lsp-sync-data`：

| 参数 | 默认与作用 |
| --- | --- |
| `LSP_SYNC_DATA_ROOT` | `/mnt/data_1T/rosbags`，递归监视的采集根 |
| `LSP_SYNC_DATA_MOUNT` | `/mnt/data_1T`，公开链接指向真实数据卡，仅刷该文件系统 |
| `LSP_SYNC_DATA_NAME` | `metadata.json`，封口信号文件名 |
| `LSP_SYNC_DATA_COOLDOWN_MS` | 1000 毫秒，合并窗口内触发，窗口结束后补刷，触发不丢 |
| `LSP_SYNC_DATA_DEBUG` | 1 输出逐个 inotify 事件，默认关闭 |

## 5. 怎么验 / 还能做什么

**验证**

```bash
sudo /usr/local/sbin/lsp-sync-data --check      # 打印监视根/挂载点/信号名/合并窗口
bash tests/data-sync-probe.sh                   # 板级回归：启动刷一次 / 普通写不刷 / 写 metadata.json 刷 /
                                                # 新目录里 rename 进来的 metadata.json 也刷 / 根目录删除与移动恢复 / SIGTERM 干净退出
                                                # 隔离真实 inotify（仅 syncfs 用桩）：通过 9 / 失败 0；真实 SD 卸载/重挂另测通过
tools/verify-image.sh                           # 离线体检里的「采集数据兜底（lsp-sync-data）」组
```

**按性价比还能做什么**

1. **采集侧在 `_seal()` 里 fsync**（首选，改一行级）：`f.flush(); os.fsync(f.fileno())`，
   再 fsync 一次 episode 目录（他们自己 `video_preview.py` 里就有现成 helper）——
   这才是"封口即安全"，我们的守护只是独立于 ROS 的第二道保险。
2. 数据盘挂载加 **`commit=1`**、`sysctl vm.dirty_expire_centisecs=200`：零代码把回写窗口压到 1~2 秒。
3. **别把要托管的数据放 FAT/exFAT 卡上**（无 journal，掉电容易直接打坏目录链）。
