# 完整日志留在 journal，串口独立输出告警

> journald 不再同步写串口；独立的 `lsp-journal-serial.service` 回放并跟随本次开机的 warning 及以上日志。

## TL;DR

- journal 明确使用 `Storage=persistent`，保留完整日志，容量上限仍为 `200M`，保留磁盘余量 `1G`。
- `ForwardToConsole=no`；串口写入堵住时，只阻塞独立读取进程，不阻塞 journald 的接收路径。
- 串口目标固定为 `/dev/ttyFIQ0`，仅用于输出，不读取输入、不重置终端、不挂断串口登录。
- `journalctl` 经管道交给独立 writer；登录切换造成 tty 失效时，writer 重开设备，避免整段启动告警反复回放。
- 登录时出现 `PAM unable to dlopen(pam_lastlog.so)`，应清理 Noble 已失效的可选会话项，见下节。
- systemd 的 `[OK]` / `[FAILED]` 状态行由独立的 `ShowStatus=auto` 控制，与 journal 转发无关。
  构建的启动参数同时显式设置 `systemd.show_status=auto`，避免 `quiet` 将状态行关闭。

## 实现与边界

`journald.conf(5)` 明确说明 console 转发是同步的：慢或挂住的串口可能堵住 journald，进而影响向它写日志的服务。
此结论来自 systemd 的接口文档；本工程没有据此声称启动时间改善了多少。

独立服务将以下命令的原始可读文本经管道交给 `/usr/local/sbin/lsp-journal-serial`：

```sh
journalctl --boot --follow --no-tail --priority=warning \
  --output=short-monotonic --no-hostname --no-pager
```

`--boot` 限定本次开机，`--no-tail` 在跟随前先回放本次开机已有的全部告警，避免丢掉服务启动前的故障。
writer 以 `O_NOCTTY` 打开 tty，不改 termios、不获取控制终端。
遇到 `EIO`、`ENXIO`、`ENODEV`、`ENOENT` 时关闭并重开设备，保留当前缓冲区的未写出偏移。
unit 使用 `StandardInput=null`、`StandardOutput=null`，错误写 journal，不将读出的日志写回 journal。
真实服务重启仍会从本次开机开头回放告警；该实现没有跨服务重启的去重状态。

串口重新登录时，getty 的 `vhangup` 会使旧 tty fd 失效。
原先 `StandardOutput=tty` 让 `journalctl` 直接持有该 fd；systemd 255.4 的 `journalctl`
监视 stdout 的 `EPOLLHUP|EPOLLERR`，出错退出后，`Restart=on-failure` 配合 `--no-tail` 重放全部启动告警。
现在 `journalctl` 的 stdout 始终是管道，由 writer 处理 tty 失效；普通登录不应触发读取进程退出或回放。
告警仍可能与人工输入交错；通过串口排查时可临时停止该服务，journal 会继续保存日志。

SPL、BL31、U-Boot 和内核的串口通路独立存在。这次修改没有接管这些启动阶段。
warning 筛选会包含驱动告警；日志等级来自发送者，warning 不等于一定需要维修。

## 登录时反复出现 `pam_lastlog.so` 加载失败

Noble 的 `/etc/pam.d/login` 仍可能包含 `session optional pam_lastlog.so`，
但目标 `libpam-modules` 不提供该模块。串口登录或 `PAMName=login` 会话因此产生：

```text
PAM unable to dlopen(pam_lastlog.so): /usr/lib/security/pam_lastlog.so: cannot open shared object file: No such file or directory
PAM adding faulty module: pam_lastlog.so
```

构建在 `rootfs` 装包后仅移除活动的 `session optional pam_lastlog.so` 行；
`tools/verify-image.sh` 的 PAM 分组检查不再有活动引用。
存量板先备份 `/etc/pam.d/login`，清理同一行后重新登录即可，无需重启；
不复制旧模块或改动认证、账户检查及其他会话项。

2026-10-23 开发板核对版本为 `libpam-modules 1.5.3-5ubuntu5.7`、`login 1:4.13+dfsg1-4ubuntu3.2`：
前者包清单不含模块，修复前的 login 配置仍有上述引用。
Linux-PAM 从 1.5.3 起已弃用并默认不构建 `pam_lastlog`，原因是旧登录记录格式非 Y2038 安全，
即使在 64 位架构也如此；见 [官方 NEWS](https://github.com/linux-pam/linux-pam/blob/v1.5.3/NEWS#L15)
与 [构建默认值](https://github.com/linux-pam/linux-pam/blob/v1.5.3/configure.ac#L717)。
这不是 journal 筛选失效，无需降低串口告警等级。

若登录时另有 `Data1T` 缺卡超时及依赖失败，核对旧 fstab 是否仍把数据卡加入 `local-fs.target`；
按[数据卡迁移说明](../storage/init-sdcard.md#41-镜像内置自动挂载与热插拔)改为仅设备触发。

## 单元接收等级与空间限制

`bluetooth.service.d/10-lsp-log-level.conf` 设置 `LogLevelMax=notice`，
在写入 journal 前丢弃该单元的 info/debug，抑制正常 A2DP 端点注册/注销噪音。
notice、warning、error 等更高等级仍保留。此限制也会丢弃其它 Bluetooth info 提示，
排查这些消息时需要临时调整该单元的等级；串口读取仍只显示 warning 及以上。

BlueZ 直接写 journal socket，本工程在 systemd 255.4 上验证 `LogFilterPatterns` 对该路径无效，
`LogLevelMax` 有效。前者是单元 stdout/stderr 的过滤项，不能写进 journald.conf 代替。
可用 `systemctl show bluetooth -p LogLevelMax` 核对当前等级。

持久日志容量由 `SystemMaxUse=200M`、`SystemKeepFree=1G` 控制，
以 `systemd-analyze cat-config systemd/journald.conf` 核对配置，
以 `journalctl --disk-usage` 查看实际占用。它们是 journald 的标准键。

## 关键脚本的日志等级

普通脚本即使打印了 `ERROR` 字样，也不会自动得到 err 等级。
相关 unit 使用 `systemd-cat --priority=info --stderr-priority=err` 执行脚本：

- SD 初始化、数据卡入口发布/收回、首启扩容、swapfile、MPP 采样、开机更新协调器：stdout 为 info，stderr 为 err。
- Type-C 电源纠正是尽力重试，stderr 为 warning。

失败信息明确写 stderr；外部命令的 stderr 也按上述等级进入 journal。
脚本退出码和原有初始化、扩容、更新行为不变，手动执行时仍是普通文本。
SD 初始化脚本打开锁文件时，错误重定向只作用于该操作，不永久吞掉后续 stderr。
用户的上位机和采集发布树不在本次日志等级修改范围内，它们的完整输出仍可从 journal 查询。

## 查询与现场应用

```sh
journalctl -b -u lsp-kiosk -u oem-host
journalctl -b -1 -p warning
systemctl status lsp-journal-serial --no-pager
systemd-analyze cat-config systemd/journald.conf
systemd-analyze cat-config systemd/system.conf
```

修改 overlay 后，源码同步与现场应用是两步，见 [overlay-apply.md](../build/overlay-apply.md)。
应用后 `systemctl daemon-reload`，重启 `systemd-journald`，再 `systemctl enable --now lsp-journal-serial`。
`ShowStatus` 属于 PID 1 配置，可由 `systemctl show -p ShowStatus` 检查；旧镜像当前启动的 `quiet`
仍将它设为 no。新镜像的显式启动参数在启动后生效，无需为了现场日志应用而重启整块板。

## 验证

`tools/verify-image.sh` 检查持久化、关闭同步转发、读取服务与开机自启、独立状态行配置及关键脚本等级。

2026-10-23，LubanCat-5 V2 / `6.1.172 #13` 已部署新写入器，清理失效的 PAM 会话项及旧 fstab 的
`local-fs.target` 数据卡依赖。经 `/dev/ttyFIQ0`（1500000 8N1）连续两次退出并重新登录成功：
服务、journalctl 和 writer 的 PID 均保持不变，`NRestarts=0`；此前注入的唯一告警仅出现一次，
旧 PAM 错误未被回放，当段 journal 新增 `pam_lastlog` 错误与 Data1T 告警均为 0。
构建清理规则的隔离检查覆盖幂等、dry-run 不写入及认证 / 账户 / 其他会话项保留，
镜像检查以失效规则作反对照；既有 fstab 生成检查通过。
本次未重做真实数据卡的物理热插拔验证。

```sh
sudo bash tests/journal-serial.sh
```

测试复制生产 unit 到一个临时运行单元，输出到独立 PTY：

1. 验证启动前告警回放、运行中新告警跟随、info/debug 只留在 journal。
2. 关闭旧 PTY 并切换测试软链到新 PTY，验证进程与重启次数不变、待发告警仅一次、旧告警不回放。
3. 仅将测试单元降为 debug，填满 PTY，确认 writer 阻塞写入时 journal 仍接收新消息。
4. 使用 SD 初始化和数据卡入口 unit 的真实命令，在临时无效路径上验证错误等级与退出码。

压力日志使用 debug，不会刷到生产串口。测试清理自己的单元和 PTY，不改真实设备配置。
`--keep` 保留抓取结果；超时属于测试环境诊断上限，不是性能验收指标。

上述板上隔离回归为 13 通过、0 失败；压力阶段通过 `/proc/<writer-pid>/syscall` 观察到 tty 写入阻塞。
宿主机写入器测试还覆盖挂断恢复后 76,800 B 字节一致、termios 不变、EOF、SIGTERM 与安装契约；
普通 `cat` 在同一挂断条件下以 `EIO` 退出，证明刺激到达了失效 fd 路径。

## 依据

- `journald.conf(5)`：同步转发风险与独立 `journalctl --follow` 读取进程建议。
- `journalctl(1)`：boot、follow、no-tail、priority 选项。
- systemd 255.4 [`journalctl.c`](https://github.com/systemd/systemd-stable/blob/v255.4/src/journal/journalctl.c#L2147)：stdout 挂断 / 出错会结束跟随。
- systemd 255.4 [`terminal-util.c`](https://github.com/systemd/systemd-stable/blob/v255.4/src/basic/terminal-util.c#L474)：旧会话结束后 tty fd 需要关闭并重开。
- `systemd.exec(5)`：仅输出到 tty 时不获取控制终端。
- `systemd-cat(1)`：stdout/stderr 分别指定日志等级。
