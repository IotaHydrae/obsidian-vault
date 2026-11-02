# 开机时 cage 会被 SIGHUP 拉掉几次

> 现象：开机后 `lsp-kiosk` 反复"起 cage → ~1.1 s 后整条单元退掉 → 立刻重启"，一次开机 0~5 轮；
> 撞上启动限流就是 `start-limit-hit`、屏幕停在控制台。**触发条件是"开机早期 + `ExecStartPost`
> 直接跑 `lsp-keyfilter --tap-numlock`"** —— 把它包一层 `sh -c` 就不再复现。

## TL;DR

- 判据：`journalctl -b -t lsp-kiosk-exit` 里出现 `result=success code=killed status=HUP`
  ⇒ 是**内核 tty 层送的 SIGHUP** 打掉的 cage（systemd 停止路径实测是 `status=ABRT`）。
- A/B：`ExecStartPost` 换成 `-+/bin/true` → 不出现 HUP；把 tap-numlock 包一层 `sh -c` → 不出现；
  直接按原样跑 → 出现。**稳态下**（HMI 已在、plymouthd 已退）连做 3 次 restart → 0 次 HUP。
- `StartLimitIntervalSec=0` 是保底（避免真的停在 `failed`）；工程机当前这次开机 `NRestarts=0`。
- ⏳ 状态：**未修**（复现样本还小，HUP 本来就是 0~5 的偶发），临时口径是"保住限流 + 别把 tap 放回
  `ExecStartPost/Pre`"。NumLock 现已搬进合成器，这条路径只在手动跑守护进程时才会走。
- 附带坑（同一次排查踩到）：**`-O2` 会把"恒返回 0"的函数整段删掉**（死代码消除）——
  `strings -a <bin> | grep '<日志串>'` 就能确认；`gcc -O2 -fdump-tree-all` 找第一个丢掉它的 pass；
  这类遮蔽 `-Wall -Wextra` **不报**，要 `-Wshadow`。

**现象**：开机后 `lsp-kiosk.service` 反复"Starting → 起 cage → ~1.1 s 后整条单元退掉 → 立刻重启"，
一次开机实测 0~5 轮；撞到 systemd 的启动限流（10 s 内 5 次）时直接
`Failed with result 'start-limit-hit'` ⇒ **kiosk 是 failed 状态，屏幕停在控制台**（最坏结果）。
肉眼看到的就是"spinner 之后闪一下底色/控制台、再闪回来，最后才出页面"。

**怎么量到的**（探针就两条 Exec 钩子，不碰主流程）：

```ini
# /etc/systemd/system/lsp-kiosk.service.d/99-tmp-marker.conf  —— 测完删掉 + daemon-reload
[Service]
ExecStartPre=+/bin/sh -c 'logger -t lsp-cage-marker "pre-exec mark plymouthd=$(pgrep -c -x plymouthd)"'
ExecStopPost=+/bin/sh -c 'logger -t lsp-kiosk-exit "result=$SERVICE_RESULT code=$EXIT_CODE status=$EXIT_STATUS"'
```

判据：`journalctl -b -t lsp-kiosk-exit` 里出现 `result=success code=killed status=HUP` ⇒ 主进程（cage）
是被**内核 tty 层送的 SIGHUP** 打掉的（不是 systemd 的停止路径：那条实测是 `status=ABRT`）。
典型一开机（monotonic）：

```
[  7.077] pre-exec mark plymouthd=1        ← 最后一条 ExecStartPre 跑完，cage 即将 exec
[  8.136] lsp-kiosk.service: Deactivated successfully / Scheduled restart job (counter 1)
           lsp-kiosk-exit: result=success code=killed status=HUP   ← 距 cage exec ≈ 1.06 s
[  9.294] 同样再来一次                                                    ≈ 1.16 s
[ 10.93 ] 第三次成功 → Started（之后长期稳定）
```

**相关性证据（都是同一块板、同一批探针，配置只改一个变量）**：

- 把 `ExecStartPost` 换成 `-+/bin/true`（其余不动）→ 不再出现 HUP；
- 把 `lsp-keyfilter --tap-numlock` 包一层 `/bin/sh -c '…'` 再跑 → 不出现 HUP；
- 直接按单元里那行跑（`ExecStartPost=-+/usr/local/sbin/lsp-keyfilter --tap-numlock`）→ 出现 HUP；
- **稳态**（HMI 已在、plymouthd 已退）下 `systemctl restart lsp-kiosk` 连做 3 次 → **0 次 HUP**，
  说明这是**开机瞬时**才有的竞态，不是每跑必现；
- 工程机 `board@<HOST_09>` 当前这次开机 `NRestarts=0`、journal 里 0 条 restart/timeout ⇒ **至少在工程机上
  这次没复现**（开发板外设更多、开机更慢，更容易踩上）。

**当前判断**：竞态发生在**"plymouthd 交还 tty1" 与 "cage 抢占 tty1"之间**——`plymouth quit` 是 cage 前
最后一步，而 plymouthd 真正退出（内核 `disassociate_ctty` → 给该 tty 的当前会话/前台进程组发 SIGHUP）
比 cage 抢下 tty1 晚；谁先谁后决定 cage 会不会吃到这一发 SIGHUP。**机制没有最终定死**（没抓到内核调用栈），
但"只有开机、只有带 plymouthd 的那一次、且与 ExecStartPost 的时长强相关"这三点是实测的。

**候选修法（2026-09-29 用户拍板取 2 + 3，1 与 4 先不做）**：

1. ~~让 cage 等 plymouthd 真退完再起~~（`plymouth --quit --wait`；实测本版本 plymouthd **没有**
   `--retain-splash`，代价是这 1 s 会露出控制台 —— 与 9-24 那次改动的目标冲突，**不做**）。
2. ✅ **已落地：不让这条 tap 以 `ExecStartPost` 的形式出现** —— 改成常驻 `lsp-keyfilter.service`
   自己盯合成器进程：主循环每个 tick（≤500 ms）扫一遍 `/proc/*/comm`，进程名 ==
   `LSP_KEYFILTER_COMPOSITOR`（默认 `cage`）且 **pid 没见过**（用一个 8 项的 seen 表记住，
   重启过渡期新旧 cage 同时存在也不会重复触发）就 `spawn_numlock_ensure()` 补按一次。
   kiosk 单元里的 `ExecStartPost=... --tap-numlock` **删掉**；`--tap-numlock` 这个命令行模式保留，
   但只作手动排查用。顺带好处：过滤器（重）启动而合成器早就在跑时，第一个 tick 就会补按
   （以前靠启动时无条件 TAP，开机那时合成器还没起来，要空转 20 次 10 s 才报
   `could not confirm numlock`，那条噪音也一起没了）。
3. ✅ **已落地：兜底不治根** —— `lsp-kiosk.service` 的 `[Unit]` 加 `StartLimitIntervalSec=0`
   （关掉"10 s 内 5 次"的启动限流），保证再怎么被 HUP 也不会把 kiosk 拖成 failed
   （failed 时屏幕停在控制台，是工厂机上最糟的状态）。
4. ~~换掉 plymouth 这条链~~（cage 里的最小 splash 客户端 / 给 cage 打背景色补丁）—— 改动最大，先不做。

**验收（开发板 <HOST_07>，改完连开 3 次机）**：

```
每开机: kiosk: active  restarts=0
        journalctl -b -u lsp-kiosk | grep -c 'Scheduled restart job'   → 0     （改前 0~5）
        journalctl -b -u lsp-kiosk | grep -c 'start-limit-hit'         → 0
        journalctl -b -u lsp-keyfilter | grep -c 'tapping NumLock'     → 1     （每个新 cage 只补按一次）
        journalctl -b -u lsp-keyfilter | grep -c 'numlock turned on'   → 1     （LED 自检确认）
   kiosk 稳态 restart 一次: 过滤器连打 3 行 tapping（旧 cage + 过渡期两个新 cage），cage 最终 active
```

⇒ 3/3 开机"零重启"（样本还小，HUP 本来就是 0~5 的偶发；`StartLimitIntervalSec=0` 是保底）。

#### ⛔ 顺带踩到的坑：`-O2` 把"恒返回 0"的函数整段删掉（2026-09-29）

第一版 `watch_compositor()` 不工作，现象是**板子上日志里根本没有 `new compositor` 那一行**。
根因是我在 `list_proc_by_comm()` 里写了 `ssize_t n = read(fd, comm, ...)`，**遮蔽了外层那个
"收进来几个 pid"的计数器 `n`** ⇒ 函数恒返回 0 ⇒ 上层 `if (!n) return;` 短路 ⇒ GCC 在
`102t.ccp2`（条件常量传播）把整段补按逻辑**当死代码删掉**（`-O0` 编译出来还有日志字符串，
`-O2` 的产物里连字符串都没有）。

判据/教训：

- 看到"代码明明写了、日志却没有"，先怀疑**死代码消除**：`strings -a <bin> | grep '<那句日志>'`
  —— `-O0` 有、`-O2` 没有 ⇒ 就是被优化掉了，去查让编译器能证明分支恒假的那个条件（通常是
  变量遮蔽、把 `=` 写成 `==`、或返回值被写成常量）。
- 定位是哪个 pass 干的：`gcc -O2 -fdump-tree-all -c …`，按编号顺序找第一个丢掉那句字符串的
  dump（这次是 `102t.ccp2`）。
- 遮蔽在 `-Wall -Wextra` 下**不会报警**（只有 `-Wshadow` 会）。以后写这类小工具可以顺手加
  `-Wshadow`；本 app 暂不改编译开关（会牵出一堆既有告警）。
