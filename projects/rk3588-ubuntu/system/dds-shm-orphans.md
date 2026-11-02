# 停服务后 `/dev/shm` 留下的 Fast DDS 孤儿段

> 与 logind `RemoveIPC` **无关**的另一件事：`systemctl stop collect_system` 之后，`/dev/shm` 里
> 会留下一堆 `fastrtps_*` 文件 —— **没有任何进程持有**，但**占着 tmpfs（也就是 RAM）**，
> 不清理会一次次累积。

## TL;DR

- 原因：Fast DDS 的 POSIX shm 段是 `/dev/shm` 里的**文件**，正常退出会 `shm_unlink`；
  节点被 systemd SIGTERM（再 SIGKILL）时不一定来得及做 ⇒ 文件留下来。
- ⛔ 别和 `RemoveIPC` 故障混为一谈：

| | 段文件 | 进程 maps | 谁干的 |
| --- | --- | --- | --- |
| RemoveIPC 故障 | **没了**（或变成 `(deleted)`） | 有 `fastrtps…(deleted)` | systemd-logind 释放用户时删 IPC |
| 本文这件 | **还在** | 一个都没有（持有者 = 0） | 进程没 `shm_unlink`，纯遗留 |

- 只读查看：`tools/dds-shm-status.sh`（段/信号量数量、按 UID 计数、`(deleted)` 计数、判定行）。

**现象**：`systemctl stop collect_system` 之后，`/dev/shm` 里仍有一堆 `fastrtps_*` 文件，**没有任何进程持有**
（`maps` 里查不到），但**文件占着 tmpfs**（也就是 RAM）。不复位/不清理就会一次次累积。

**为什么**：Fast DDS 的 POSIX shm 段是 `/dev/shm` 里的**文件**；正常退出时会 `shm_unlink`，但节点被
systemd SIGTERM（再来 SIGKILL）时不一定来得及做 ⇒ 文件留下来。⛔ **别和 [RemoveIPC 故障](collection-dds-shm-fault.md) 混为一谈**：

| | 段文件 | 进程 maps | 谁干的 |
| --- | --- | --- | --- |
| RemoveIPC 故障 | **没了**（或变成 `(deleted)`） | 有 `fastrtps…(deleted)` | systemd-logind 释放用户时删 IPC |
| 本文这件 | **还在** | 一个都没有（`持有者=0`） | 进程没 `shm_unlink`，纯遗留 |

**工程机实测（`<HOST_09>`，2026-09-29，脚本 `tests/ros-shm-leak.sh`）**：

```
起始（清场后）：   fastrtps 文件=0   占用=0.0MB   持有者=0
第 1 轮 起栈后：   段=39  fastrtps 文件=98   占用=11.0MB  持有者=20   阶段=已就绪/READY(3)
第 1 轮 停栈后：   段=20  fastrtps 文件=66   占用= 6.1MB  持有者= 0   ← 遗留
第 2 轮 起栈后：   段=49  fastrtps 文件=118  占用=16.2MB  持有者=20   ← 旧的 6.1MB 还在，又叠加新的
第 2 轮 停栈后：   段=28  fastrtps 文件=82   占用=10.3MB  持有者= 0   ← 累积
清理后：           fastrtps 文件=0   占用=0.0MB   持有者=0
```

⇒ **一次"起栈→停栈"留 ~4~6 MB**（段文件 550 KB 一个），**不清就累积**（本轮 6.1 → 10.3 MB）。

**"是不是只有手动停才有？"—— 2026-09-29 又量了一轮（工程机，同一脚本手工跑）**：

| 场景 | 段 | fastrtps 文件 | 占用 | 持有者 | 结论 |
| --- | --- | --- | --- | --- | --- |
| 起栈后（READY） | 36 | 94 | 10.4 MB | 19 | — |
| **静置 90 s**（什么都没做） | 36 | 90 | 10.3 MB | 19 | **不涨**（段是"在用"的，稳态不泄漏） |
| 手动 `systemctl stop collect_system` 后 | 18 | 62 | **5.1 MB** | 0 | 留（36→18 段：一部分节点自己清掉了，剩下的是没来得及 `shm_unlink` 的） |
| 一个普通 DDS 参与者被 **`kill -9`** | 19 | 64 | **5.6 MB**（+0.5 MB） | 1 | **留** ⇒ 非正常退出("崩溃/被强杀")这一类都会留，不只手动停 |

⇒ **只有"参与者非正常退出"才会留**：手动停服务、`systemctl restart`、节点崩溃/OOM/被 SIGKILL、
systemd `Restart=on-failure` 把整栈拉起重启 …… 都算；**稳态长跑不会自己涨**。
风险只在"反复起停/反复崩溃重启"时累积（每轮几 MB），攒到 `/dev/shm` 满（本机 3.9 G）才会变成
"新 DDS 段建不出来"这种真故障。**正常一直跑的现场机器可以不管。**

**怎么看**（现场一条命令）：`dds-shm-status.sh`（段数/文件数/占用/持有者/判定），或

```bash
ls /dev/shm | grep -c fastrtps ; ls -l /dev/shm | grep fastrtps | awk '{s+=$5} END {printf "%.1f MB\n", s/1048576}'
df -h /dev/shm            # 看 tmpfs 占用
# 判"是不是孤儿"：持有者必须是 0（dds-shm-status.sh 会直接把持有者列出来）
```

**处置候选（还没做，等用户拍板）**：

0. ⛔ **结论（用户 2026-09-29）：不自动清，什么都不加** —— 原话"先不删了吧，容易埋雷"。理由就在
   下面①里那条"按名字前缀删、不区分进程/属主"的风险：我们这一代镜像只有一套 ROS 时才安全，
   但将来板上多跑一个 ROS/别人 ssh 上来调试就会误伤，属于"埋雷"。**留着手工处置 + 说明即可。**
   现场真遇到"/dev/shm 涨满"再按下面②③或"确认持有者=0 后手工清"处理。

1. ~~在 unit 里收尾清一次~~（**不做**，见上；保留作为"如果将来确实只有一套 ROS"的备选）：
   ```ini
   # /etc/systemd/system/collect_system.service
   ExecStopPost=-/bin/sh -c 'rm -f /dev/shm/fastrtps_* /dev/shm/sem.fastrtps_*'
   ```
   ⚠️ **它是按"名字前缀"删的，不区分进程/用户**（这就是 Fast DDS 的命名空间）：
     - 好处：lttng（`lttng-ust-wait-*`）、Chromium（走 memfd，`/dev/shm` 里根本没有它的条目）、
       PulseAudio（`pulse-shm-*`）这些**都不会被碰到**；板上实测"两个通配符命中 0 个 lttng 文件"。
     - 风险：**同机还有别的 Fast DDS 进程在跑**（别的 ROS domain、`ros2` CLI daemon、rviz、
       别人 ssh 上来敲的 ros2 命令、另一套采集栈）时，它们的段同样叫 `fastrtps_*` ⇒ **会被一起删掉**
       （那些进程手上的段随即变 `(deleted)`，等于人为制造 [RemoveIPC 故障](collection-dds-shm-fault.md) 那种"读端全聋"，直到它们重启）。
     - 我们的机器**只有这一套 ROS/DDS**，而且 systemd 保证 `ExecStopPost` 跑在**主进程停止之后** ⇒ 安全。
       多栈/多用户的机器别用这条，改成下面两种之一。
   - 更保守的写法：按属主 + 只删"没人持有"的，例如
     `find /dev/shm -maxdepth 1 -user board -name 'fastrtps_*' -delete`（老镜像换 `-user cat`；仍然会误伤
     同属主跑的别的 ROS）；或者先 `dds-shm-status.sh` 确认"持有者=0"再删。
2. **让 ROS 侧优雅退出**（正解）：给节点加 SIGTERM 处理，退出前 `shm_unlink` 自己创建的段 —— 要改上位机/采集程序。
3. **不处理**：重启即清（tmpfs），或定期手工 `rm -f /dev/shm/fastrtps_*`。

**复现/再测**：`sudo bash tests/ros-shm-leak.sh`（默认 2 轮，**跑完不删**、只打印怎么清；要清加
`--clean-orphans`，且先按上面那条确认「没有别的 ROS 在跑 / 持有者=0」）。
