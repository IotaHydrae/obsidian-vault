# 采集栈"六路全断 + 语音报错 + 不自愈"：logind 的 RemoveIPC 删掉了 Fast DDS 的共享内存

> **根因（2026-09-26 定稿，板上 A/B 实测）**：与 USB / 外设硬件无关 —— systemd-logind 的
> `RemoveIPC=yes`（发行版默认）在"用户被判定释放"时，删掉该 UID 名下**所有 POSIX 共享内存 / 信号量 /
> SysV IPC**。采集栈整个跑在 `board` 名下，而 Fast DDS 的共享内存传输就靠 `/dev/shm/fastrtps_*`（段）
> 与 `/dev/shm/sem.fastrtps_*`（信号量）⇒ 对象被删后**发布端照常 `publish()`（写进已注销的段），
> 读端再也收不到数据** ⇒ LCD 桥 3 s 话题超时 ⇒ 六路"断连" + `ERROR_RUN(6)` + 语音报错，且**锁存不自愈**。

## TL;DR

- **触发**：logind 认为"这个用户走了"。`linger=no` 时**任何一次正常登出**都算（老镜像 ⇒"ssh 进去看眼日志再退出"必中）；
  `linger=yes` 只兜住普通登出，**强拆 user slice 照样触发**。
- **现象**：LCD 六路"断连" + `阶段=故障运行/ERROR_RUN(6)` + 语音"出现错误"，**不自愈**，而发布端进程还活着
  （相机照打 `fps=30`）。
- **修法（已进镜像）**：`RemoveIPC=no`（治本）+ 预置 `linger`（少一次 user-manager 拆装，缓解）。
  老镜像一键补 → [logind-removeipc-hotfix.md](logind-removeipc-hotfix.md)。
- **现场判定**：段没了/`(deleted)` ⇒ DDS 故障（**重启采集栈**，别拔线）；节点在但数据不来 + 内核有
  `over-current`/`reset`/`error -71` ⇒ 硬件。口诀见 §3。
- ⛔ **两条不能用的判据**（都是实测证伪的）：① 进程 fd 里那些 `(deleted)` 的
  `shm-event-notifier-error-accounting-*` 是 **liblttng-ust** 建的、**正常进程一启动就有**；
  ② `ros2 node list` 计数在正常态也只报 3 个（实际 ~11 个）。
- ⛔ 本文只做记录与判定，**没改 Fast DDS / 厂商 release 一行**（用户要求：DDS 侧改动等上级决定）。
- 另一类"六路全断"（单路外设偶发断连被程序放大）→
  [collection-peripheral-error-escalation.md](collection-peripheral-error-escalation.md)；
  停服务后 `/dev/shm` 留下的**孤儿段**是第三回事 → [dds-shm-orphans.md](dds-shm-orphans.md)。

---

## 1. 通俗版：为什么"ssh 退出来"会让节点全崩

**不是节点崩了，是它们共用的那块内存被人搬走了。**

采集栈的几十个进程（相机、手套、Quest、LCD 桥、触发节点……）靠 ROS 2 的**共享内存**互相传数据：
写的人往一块"白板"上贴纸条，读的人直接看白板。白板就是 `/dev/shm/` 下的文件（`fastrtps_*`），
排队靠几个"信号量"（`sem.fastrtps_*`）。**这块白板是以 `board` 用户的身份建的。**

systemd 的登录管理器 `systemd-logind` 有一条发行版默认打开的规则：**一个用户"彻底退出了"之后，
把他名下所有进程间共享的东西（共享内存 / 信号量 / SysV IPC）统统删掉**。本意是防残留垃圾，
设计假设是"用户退出了 ⇒ 他名下的进程也没了"。而采集栈正好是这条假设的反例：

```
你退出最后一个 board 的 ssh 会话
   └─ logind 认为"board 这个用户走了"        ← 只看会话/用户管理器，不看还有没有别的 board 进程
        ├─ 拆掉 board 的 user manager / /run/user/1000
        └─ RemoveIPC：删掉 board 名下的所有共享内存和信号量 ← 采集栈正在用的那块"白板"就在里面
```

删完之后现象特别迷惑：**写的人还在写（写进已从目录消失的内存），读的人再也读不到，谁都不觉得自己出错了。**

| 看到的现象 | 真正的原因 |
| --- | --- |
| 相机节点照打 `fps=30`、手套照打 `published … frames` | 发布端还在写那块已注销的段 |
| 订阅端（LCD 桥、触发节点）一条也收不到 | 白板已不在 `/dev/shm`（进程手上的句柄变成 `(deleted)`），新进程更找不到 |
| LCD 报"6 个设备全部断连 + 语音出现错误" | LCD 桥只按"这个话题 3 秒没数据"判定，**分不清**拔线 / 休眠 / 白板没了 |
| 不会自己恢复 | 面板上的错误位是**按位锁存**的，只有收到"新鲜正常数据"才清，而数据永远不会再来 |

- **为什么频繁**：老镜像 `linger=no` ⇒ 每次登出都重演一次；"ssh 看一眼再退出"正好是这个动作。
- **为什么有时不崩**：从"用户被释放"到"某个读端踩上那段失效内存"有 **1~13 s** 延迟，
  且**只有"还没 attach / 要换段"的读端中招**（§5）⇒ 同一动作命中率不是 100%。
- **恢复**：`sudo systemctl restart collect_system`（重建 DDS 段），之后外设要**长按 LCD 2 秒**才会重新拉起。
  ⛔ 不要拔插设备。

## 2. 准确的触发口径

触发条件不是"拆 slice"这个动作本身，而是 **logind 判定该用户被释放**：

| 动作 | 会不会触发 | 依据 |
| --- | --- | --- |
| **正常登出**（最后一个会话结束） | `linger=no` 时**会** | 现场 12:07 那次就是；老镜像的常态 |
| **强拆** `systemctl stop user-1000.slice` | **会**（绕过 linger） | A/B 与金丝雀实验都用它 |
| `loginctl terminate-user board` | `linger=yes` 时**不会**（正规收会话 ≠ 释放） | 2026-09-26 15:09 实测"什么都没删" |
| 只 `systemctl stop user@1000.service` | **不会**（不拆 slice/会话），LCD 全程无反应 | 2026-09-26 实测 |

⇒ 所以 `linger` 只能算缓解，**`RemoveIPC=no` 才是把这条路径堵死**。

## 3. 现场判定：是 DDS 坏了还是设备真掉线

```bash
ros2 node list --no-daemon | wc -l                 # 0/极少 = 发现坏了（⚠️ 正常也可能只报 3 个，见 §5）
ls -l /dev/shm | grep -c ' board '                   # board 名下的段没了/骤减 = 被 RemoveIPC 清过
systemctl status collect_system                    # 进程都在 = 不是崩溃
ls -l /dev/ttyACM* /dev/video* /dev/glove_* /dev/lcd_voice_bridge   # 节点都在 = 不是拔线
journalctl -k --since "-10min" | grep -Ei 'over-current|reset (high|full)-speed|error -(71|110|32)'
                                                   # 有 ⇒ 电气/hub 问题，换线换口查供电
```

- **恢复**：DDS 故障就 `sudo systemctl restart collect_system`，**别拔线**；之后长按 LCD 2 秒拉起外设脚本。
- **现场区分**：看 LCD 日志**第一条** `错误下发` 是哪一路 —— 后面累加到 `mask=0x3F` 是**锁存累积**，
  不代表六路都坏；`journalctl -k` 干净 ⇒ 不是硬件。
- 只重启 `collect_system` 只能重建段，**面板里的锁存位**要长按关机（进 OVER 会清 `_latched_error_payload`）才复位。
- ⛔ **故障态下 LCD 按键也会失效**：按键链路是 `LCD 固件 →(串口)→ lcd_voice_bridge →(ROS 话题)→ keyboard_trigger`，
  中间那跳依赖 DDS；触发节点的读端一旦中招，事件发出去也没人消费 ⇒ 现场只剩 ssh / 断电重启能救。
  给厂商的建议：流程控制的关键路径别只依赖 DDS（文件/FIFO/Unix socket 兜底），或给一个不依赖 DDS 的恢复入口。

## 4. 修法（已进镜像）

| 改动 | 位置 | 判据 / 断言 |
| --- | --- | --- |
| **`RemoveIPC=no`（治本）** | `overlay/etc/systemd/logind.conf.d/50-lsp-no-remove-ipc.conf`，构建脚本 `install -m 0644` | `tools/verify-image.sh`「数据采集系统」组：静态查文件 + `[Login]` 段 + `RemoveIPC=no`；live 查 `busctl … Manager RemoveIPC` = `false` |
| **`linger` 预置（缓解）** | 构建脚本放 `/var/lib/systemd/linger/board`（0 字节 root:root 0644） | 同组存在性/权限断言。用户 2026-09-26 决定**保留常驻**：少一次 user manager 拆装；根治仍靠上面那条 |
| 老镜像一键补 | `tools/lsp-fix-logind-no-removeipc.sh`（自包含、幂等，落同一份文件 + `restart systemd-logind` + `busctl` 自检） | 见 [logind-removeipc-hotfix.md](logind-removeipc-hotfix.md) |
| 复现/监控 | `tests/dds-shm-fault-repro.sh` + `tests/dds-link-monitor.py`（唯一一个**故意制造故障**的测试） | 见 `tests/README.md` |

**为什么不改成"换个身份跑采集"**（用户问过"不让采集服务跑 board 就行吗"）—— 实测**确实免疫**（§5），但：

| 方案 | 效果 | 代价 / 风险 |
| --- | --- | --- |
| A. 换专用系统用户 | 免疫（已实测） | 要动一大片：`/home/board/.lsp/{config,log}`、`/mnt/data_1T/rosbags`、`dialout`/`video` 组、udev `GROUP=`、`lsp-set-rule` 规则；**厂商 release 里写死了 `/home/board/...`** ⇒ 必须厂商配合；还得保证该身份**永无会话**（连 `sudo -u` 都会开 PAM 会话） |
| **B. `RemoveIPC=no`（已做）** | 免疫**所有**用户、**所有**释放路径 | 构建期一行，已 A/B 验证；不挑身份 |
| C. 纪律：采集栈只许 systemd 启动、限制 board 登录入口 | 防的是**另一条**问题（手工 ssh 里跑 `run_collect_system.sh` ⇒ 落在会话 cgroup 里，会话一结束整棵树被杀，与 IPC 无关） | 纯策略，成本低，可顺手做 |

⇒ **B 已经兜住这一条，不建议为它去改厂商的运行用户**；A 可作为以后"彻底不依赖 logind 行为"的方向。

## 5. logind 的行为边界（实测，回答"进程还在用为什么照样删"）

| 对象 | `RemoveIPC` 的行为 |
| --- | --- |
| **POSIX 共享内存 / 信号量**（`/dev/shm/*`，Fast DDS 用的就是这类） | **只按属主 UID 直接 `unlink`** —— 进程 fd 还开着、还 `mmap` 着也照样删（实测 `inuse-shm` 的持有者一直活着，还是没了）。⇒ **已经 attach 的读端还能继续读**（inode 还在），**需要重新 attach / 换段的读端永久失联** —— 这就是"同一动作有时报、有时不报"的原因 |
| **SysV 段** | **有"在用"检查**：`nattch > 0` 的**保住**，`nattch = 0` 的删 |
| 判断"用户走了"的依据 | **只看会话 / 用户管理器**，**不看这个 UID 还有没有别的进程在跑**（否则采集栈那 11 个 board 进程活着就不会被删） |

- ⇒ 这不是 bug，是**假设不成立**（"用户退出 ⇒ 该用户的进程也没了"）；采集栈是
  "系统服务（`system.slice`，ssh 拆不到）+ `board` 身份"这个假设外的组合。SysV 那点保护也不覆盖我们用的那条路。
- **现场指纹**：`/dev/shm` 被清空、但 `nattch>0` 的 SysV 段还在 ⇒ 基本就是 logind 的 `RemoveIPC` 干的。
- **换身份实验**：同一次触发下，`board`（有 ssh 会话）名下对象**全删**；`dctest`（uid 998 系统用户、无会话）、
  `dctest2`（uid 1002、无会话）名下各 4 个对象**完好**。⇒ 决定因素是"**有没有被 logind 管理过（会话）**"，
  **不是 UID 数值**；反过来说这条方案很脆 —— 那个身份哪天真登了一次（ssh / `su` / `sudo -u`），立刻重新中招。

## 6. 复现

> 一键版：`tests/dds-shm-fault-repro.sh` + `tests/dds-link-monitor.py` —— 先起独立链路监控
> （逐秒记 12 路话题条数与故障标记 + 5 个采集进程的 CPU + LCD「错误下发」计数），再由系统级单元反复
> `stop user-1000.slice`，每轮 45 s；结果在 `~/dds-marks-<RUNID>.txt` 与 `~/dds-mon-<RUNID>.{csv,events}`。
> ⚠️ 两个坑：结果文件别放 `/tmp`（sticky 目录 + `fs.protected_regular`）；判"多久没数据"要用**每秒增量**，
> 用累计计数的话收到第一条之后 STALL 再也不触发。

手工三步（工程机上随时可做，触发就是"强拆"，一定会让 logind 释放用户）：

```bash
# A. 拿干净基线（外设需长按 LCD 2 秒；没人按可用一条合成 KeyEvent 代替）
sudo systemctl restart collect_system
# B. 触发
sudo systemctl stop --no-block user-1000.slice
ls -l /dev/shm | grep -c ' board '        # 应 1 秒内从 55 掉到 0（只剩下 root 的 lttng 文件）
# C. 验证修法：RemoveIPC=no 生效（busctl … RemoveIPC → b false）后重复 B：计数不变、LCD 不再报错
```

**带监控那次实测（第 1 轮就命中）**：13:18:50 触发 → 12 路话题合计 ~204 条/s 一直稳到 13:19:02 →
13:19:03 相机+Quest 先停 → 13:19:04 LCD 错误下发=1 → **13:19:05 起 0/s**；同期 LCD 位
`0x0E`（三相机）→ `0x3E`（+双手套）→ `0x3F`（+Quest）；而相机/手套进程照常在 publish、CPU 时间继续涨。

**金丝雀实验（决定性的那条）**：造 board 名下的 POSIX shm + POSIX 信号量 + SysV 段各一，逐秒采样后拆 slice：

```
SAMPLE 13:24:12  shm_files=57  lsp_owned=55  sysv_seg=1
=== TRIGGER 13:24:13  systemctl stop --no-block user-1000.slice ===
SAMPLE 13:24:14  shm_files=4   lsp_owned=2   sysv_seg=0      ← 1 秒内全没
```

`sysv_seg` 1→0 是判定性的：**按 UID 连 SysV 段一起删，只有 logind 的 `RemoveIPC` 会做**
（`/dev/shm` 没有 tmpfiles 规则、厂商脚本里没有任何 `/dev/shm` 操作、内核日志无 USB/挂载事件）；
配置侧 `busctl get-property … Manager RemoveIPC` = `b true`（`/etc/systemd/logind.conf` 没显式写 = 发行版默认）。

**修法 A/B（同一台工程机、同一动作）**：

| | 触发前 | 触发后 |
| --- | --- | --- |
| `RemoveIPC=yes`（默认） | 55 个 board 对象、`sysv_seg=1` | 1 s 内只剩 2 个 root 文件、`sysv_seg=0`；~13 s 后 12 路话题全归零，LCD 错误 0→3（锁存，不复原） |
| `RemoveIPC=no` | 同上 | `busctl … RemoveIPC` = `false`；金丝雀 55~57 全程不动；端到端 2 轮拆 slice：**12 路全程 202~205 条/s 不掉、LCD 错误计数 0** |

## 7. 影响面与未决

- LCD 的"设备断开"文案是**误报**：它只按"话题 3 秒没数据"判定，区分不了 休眠 / 拔线 / DDS 坏。
  （顺带回答：**Quest 确实会自己休眠** —— 不佩戴/长时间无操作后息屏，是头显侧行为；但本次**不是**它：
  故障那一秒 LCD 还收到 `Quest头显=75%`、`adb devices` 一直看得到，而且有线相机 + BLE 手套同时掉。）
- 故障态是**锁存**的：数据恢复后 LCD 也不会自己变绿，现场只能重启采集栈（+ 长按 LCD 清锁存）。
- 厂商那份 `fastdds_localhost.xml` 本身就是一版修复（防"误切 WiFi 导致节点全崩"）：
  靠的是 `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` + `UDPv4(interfaceWhiteList=127.0.0.1)`，
  **SHM 只是顺带加的性能选项** ⇒ 若将来要在 XML 里去掉 `local_shm`，不会破坏那版修复，但**上线前必须复测一次 WiFi 切换**。
- ⏳ 未决：① XML 关 SHM 是否真生效、性能代价多大（相机 fps / CPU）—— **未经用户同意不动 DDS**；
  ② 监控侧"锁存不自愈"要不要改成主动上报"已恢复"（属采集 release，不属本镜像仓库）；
  ③ 镜像重编后按现场流程（ssh 登出 / `terminate-user` / 重启）各跑一遍回归；
  ④ "对象被删 → LCD 报错"那 1~13 s 延迟由哪一跳决定（只影响观感）。
- ⛔ **被证伪 / 未证实**的中间结论（别再拿它们当判据）：
  · 进程里 `(deleted)` 的 `shm-event-notifier-error-accounting-*` 是 **liblttng-ust** 建的，
    `ros2 launch` 建好后 fork 继承，**每个 ROS 进程都有 8 个**；一个 domain 99 的全新探针节点同样有 8 个，
    而真正的 `fastrtps_*` 段正常 ⇒ **不能用来判定故障**；
  · `ros2 node list` 计数：采集栈正常跑着（LCD READY、相机 30 fps）也可能只报 3 个，实际 ~11 个 ⇒ 要先标定再用；
  · "段被按 PID 复用误删"只是**领先假设**，未证实（要厂商在 Fast DDS 侧确认 `SharedMemManager` 的清理逻辑）。
