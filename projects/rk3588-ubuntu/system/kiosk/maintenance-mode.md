# 维护模式：kiosk ↔ board 桌面（`lsp-maintenance`）

> 管理员输入密码临时切到 board 桌面：先标记维护模式、停止 kiosk，再 `systemctl start gdm3`。
> GDM 不 enable；重启后按 HDMI 连接状态决定是否启动 kiosk。

## TL;DR

- 脚本 `overlay/usr/local/sbin/lsp-maintenance`，子命令 `on|off|status|passwd`。
- `/run/lsp-kiosk/maintenance` 防止 HDMI 热插拔抢回 kiosk；`off` 清除它并重新检查 HDMI。
  拔掉 HDMI 仍会停止 HMI 后端，GDM 保留；退出维护时无 HDMI 就保持 kiosk 停止。
- `off` 交给系统级临时单元 `lsp-maintenance-off.service` 执行；GNOME 终端随桌面关闭后，退出任务仍会完成。
- 进入失败（包括 GDM 启动后未保持 active）返回非零：先停 GDM，再清标记并重新检查 HDMI。
  停止或恢复失败会明确报错；未能停止 GDM 时保留标记，防止两个会话抢占显示。
- 密码**构建期**写进镜像：`LSP_MAINT_PASSWORD`（默认 `<SECRET>`）经 `openssl passwd -6` 落到
  `/etc/lsp-maintenance.passwd`（**0600 root:root**，sha512-crypt）；置空变量 = 不写文件。
  运行期换密码用 `sudo lsp-maintenance passwd`（交互输入，不落明文、不进 `ps`）。
- ⛔ 白名单是免密的，所以脚本必须兜底：**没配密码文件就直接拒绝 `on`**
  （自测可临时 `LSP_MAINT_ALLOW_NO_PASSWORD=1`），否则等于免密后门。
- sudo 白名单 `overlay/etc/sudoers.d/99-lsp-maintenance`（0440）只放行 `on`、`off`、`status` 和无参数查询，对象是 `kiosk`；
  `passwd` 仅管理员可调用，脚本也拒绝 `SUDO_USER=kiosk`。
- 审计：每次动作往 `/var/log/lsp-maintenance.log`（0640 root:adm）写一行；交互式工具，输出保持中文。
- 实测（开发板）：错密码拒绝、对密码进 board 桌面、`off` 回 kiosk、`gdm3` 事后仍未 enable、
  白名单很窄（`sudo rm` / `systemctl stop gdm3` 全被拒）。
- 切回桌面进**哪种会话不由本脚本决定**：`on` 负责互锁并启动 GDM，会话类型看
  AccountsService 的 `Session/XSession` + `/etc/gdm3/custom.conf` 的 `WaylandEnable`。
  当前**默认选择 Wayland**（`ubuntu-wayland`），`gdm-hack` 在存在非 gdm 登录会话时退出；
  实际会话以 logind 为准，见 [desktop-session.md](../desktop-session.md)「桌面会话默认 Wayland」。

需求原话："我们的上位机有个维护模式的后门，管理员输入密码进入维护模式之后，可以切换到 board 的桌面"，
且维护状态仅在本次开机有效；默认会话现在受 [HDMI 生命周期](session-design.md#hdmi-生命周期) 控制。做法：

- **脚本**：`overlay/usr/local/sbin/lsp-maintenance`（`on|off|status|passwd`）。
  - `on`：校验密码 → 写维护标记 → 停 kiosk → `systemctl start gdm3`；
    `off`：停 gdm3 → 清标记 → 重新检查 HDMI，有屏回 kiosk，无屏保持停止。
  - **只 `start`、绝不 `enable` GDM**，避免重启后自动进入桌面。
- **密码**：⛔ **构建期**写进镜像（用户明确"维护密码不能在运行时设置，提前做到镜像里"）：
  构建变量 `LSP_MAINT_PASSWORD`（默认值见 `scripts/build-lsp-image.sh`）经 `openssl passwd -6` 落到
  `/etc/lsp-maintenance.passwd`（**0600 root:root**，sha512-crypt）；置空该变量 = 不写文件。
  运行期换密码用 `sudo lsp-maintenance passwd`（交互输入，不落明文、不走 `ps`）。
- **sudo 白名单**：`overlay/etc/sudoers.d/99-lsp-maintenance`（0440），精确限定
  `/usr/local/sbin/lsp-maintenance on|off|status` 与无参数查询，禁止额外参数/`passwd`，放行对象是 **kiosk**（上位机跑在 HMI 会话里，用户 2026-09-23 明确）。
  ⛔ 因为这条是**免密**的，脚本里做了兜底：**没配 `/etc/lsp-maintenance.passwd` 就直接拒绝 `on`**
  （自测可临时 `LSP_MAINT_ALLOW_NO_PASSWORD=1`），否则等于免密后门。
- **审计**：每次动作往 `/var/log/lsp-maintenance.log`（0640 root:adm）写一行
  时间 / 调用者 / 来源 / 动作 / 结果；交互式工具，输出保持中文（不受 AGENTS §2.14 限制）。
- **板上实测（2026-09-23，开发板 `<HOST_07>`）**：错密码拒绝、对密码进 board 桌面、
  `off` 回到 kiosk、`gdm3` 事后仍未 enable、日志权限 0640 root:adm、
  白名单很窄（`sudo rm` / `sudo tail` / `systemctl stop gdm3` 全被拒）。
- 静态断言：`tools/verify-image.sh` 的「维护模式」组（脚本在、白名单 0440 且只放 lsp-maintenance、
  密码文件 0600 root:root 且 `$6$`）+「kiosk 会话」组里的 live 断言 `gdm3 未 enable`。

认证边界回归：`visudo -cf` 为 `parsed OK`；`cvtsudoers -f json` 只有四条精确命令；
隔离执行 `SUDO_USER=kiosk … passwd` 返回 1，拒绝发生在读密码/写密码文件之前。

换密码先生成并检查 hash，再在同目录写临时文件、设置 0600 root:root，最后 rename 发布。
生成、写入、权限或发布失败均保留原密码。`python3 tests/maintenance-tool.py` 使用真实 OpenSSL
和临时文件，服务操作用桩，覆盖进入失败恢复、恢复失败、认证与密码发布故障；
它验证服务状态控制流，不证明 GNOME 实际出画或真实文件属主。

## 从桌面终端退出后黑屏

直接在 GNOME 的 Kitty / GNOME Terminal 中执行旧版 `off`，停止 GDM 会关闭终端，
并终止其中的退出脚本。此时 GDM 与 kiosk 都 inactive，维护标记仍在，退出审计未写入；
HDMI 调度因标记存在而不能恢复 kiosk。开发板与工程机均复现这一状态。

`off` 通过 `systemd-run --wait --collect` 把内部退出动作交给系统 manager，先停 GDM，
确认停止后才清标记并调用 HDMI 调度。临时单元不继承桌面会话、终端或标准输入，
输出进入 journal；不能改成 `--user`、`--scope`、`--pty` 或 `--pipe`。
SSH 调用会等待任务完成并获得退出码；桌面调用的终端会关闭，结果看
`journalctl -b -u lsp-maintenance-off.service` 和维护审计。审计保留原调用者与来源。
内部 `__off` 仅限 root，不加入 kiosk 的 sudo 白名单。

隔离回归覆盖退出成功、HDMI 门控、任务启动与执行失败、审计上下文；
跨桌面生命周期必须另做真实终端测试，不能用服务桩的结果代替。
2026-10-14 验证：隔离回归 19/19 通过；开发板（内核 #10）从真实 GNOME Terminal 退出、
工程机（内核 #9）从真实 Kitty 退出，终端关闭后任务均继续完成，维护标记撤回、
kiosk 与后端恢复、HTTP 200。工程机现场确认上位机画面恢复；两项测试均未重启。

## 切换的时候 kiosk 停不下来（2026-10-09 查清并修掉）

**症状**：`lsp-maintenance on`（起 GDM）时，`lsp-kiosk` 卡在 `stop-sigterm` 满 **90 s**（systemd 默认
`TimeoutStopSec`）才被 SIGKILL，单元留 `failed(Result: timeout)`；那 90 s 里 Chromium **每隔 ~6 s 还在画**
（`wayland_frame_manager.cc:692`）⇒ 两个合成器同时活着。对照：本 boot 内**没让 GDM 跑过**时，
`systemctl restart lsp-kiosk` 的 `Stopping`/`Stopped` 是**同秒**的 ✓。

**排查（都实测过，别再重复）**：
- ❌ "GDM 与 kiosk 并发抢 DRM/VT"：`cmd_on` 改成**先停 kiosk 再起 GDM** 后**仍然卡** ⇒ 证伪。
- ❌ "卡在 DRM/VT ioctl"：采样 cage 是 **`S` + `do_epoll_wait`**（`/proc/<pid>/stack` 也是它）⇒ 证伪。
- ❌ "别人占着 DRM"：`/dev/dri` 持有者只有 `systemd` / `systemd-logind` / `cage`，GDM 没留残留进程 ⇒ 证伪。
- ✅ **真因（cage/libwayland 层面，不是我们的配置）**：cage 卡在事件循环里**不响应**停止信号 ——
  `SigBlk=[2,15]`（SIGINT/SIGTERM 已阻塞）、2 个 `signalfd` 已建、且 **signalfd 的 inode 确实在
  epoll 注册表里**（`/proc/<pid>/fdinfo/<epoll>` 里能看到那个 ino），但**手动 `kill -TERM` 与
  `kill -INT` 都无效**（8 s 内不动）⇒ 事件循环不去 dispatch 那个 signalfd。触发条件看起来与
  "本 boot 里 GDM 跑过"相关（进入维护模式后再回 kiosk 就会复现）。

**修法（两条一起）**：
1. `overlay/etc/systemd/system/lsp-kiosk.service`：**`KillSignal=SIGKILL`** —— kiosk 是**可丢弃会话**，
   与其等超时再 SIGKILL（实际结局本来就是它），不如一步到位；内核立刻收回 DRM master，
   下一个合成器接管（GDM/kiosk 都能正常起来）。保留 `TimeoutStopSec=20` 作兜底。
2. `overlay/usr/local/sbin/lsp-maintenance` 的 `cmd_on`：**先 `systemctl stop KIOSK_UNIT` 再起 GDM**
   （与 `cmd_off` 对称）。这条单独不足以防住上面的真因，但它让切换顺序明确、不再依赖 `Conflicts=` 的并发。

**判据**（回归时应看这三条）：`systemctl stop lsp-kiosk` **< 2 s**、journal 里 `Stopping` 与 `Stopped`
**同秒**且无 `stop-sigterm timed out`、`systemctl is-failed lsp-kiosk` = `inactive`。
停止耗时实测为 **101 ms**、`Stopping`/`Stopped` 同秒；
`SuccessExitStatus=SIGKILL` 让主动停止成为正常结束，避免每次拔 HDMI 都留下 `failed(Result: signal)`。
`tools/verify-image.sh` 核对停止预算、信号及成功退出状态。
