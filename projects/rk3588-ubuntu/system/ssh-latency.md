# SSH 到板子的连接耗时：逐段实测 + 连接复用（2026-09-24）

> 起因：用户问"测一下 ssh 连接到开发板的耗时，并分析链路上各部分占用时间"。
> **一句话结论：0.5 s 的单条连接里没有一处是板子慢** —— 大头是 Wi-Fi 往返 + SSH 协议往返 + 密码认证；
> 开 `ControlMaster` 复用后**后续每条 44 ms（≈12–25×）**，仓库里的两个"连很多次"的工具已经默认开。

---

## TL;DR

- 逐段实测：SSH 到板子一条连接约 **0.5 s**（协议往返 ~0.3 s + 密码认证 ~0.25 s）。
- 收益最大的优化是**连接复用**（`ControlMaster` + `ControlPersist`，已落地）⇒ 后续连接 **0.03~0.04 s**（~15×）。
- 明确**不做**的事（省得白花时间）见 §5；复现命令见 §6。
- 这解释了"脚本里反复 ssh 为什么慢"，`tools/sync-to-board.sh` 已默认开复用。

## 1. 链路与两端状态

```
本机 : wlp0s20f3 <HOST_06>/22（Wi-Fi）   ← `ip route get <HOST_07>` → dev wlp0s20f3
板子 : wlan0 / p2p0 <HOST_07>/22（同为 Wi-Fi，走 AP 一跳）
```

两端都不慢、配置也没问题（实测值）：

| 检查 | 结果 |
| --- | --- |
| 板子负载 | `load average: 0.15, 0.10, 0.21`（空闲） |
| `sshd -T` | `usedns no`（不查反解）、`gssapiauthentication no`、`printmotd no` |
| 服务端 PAM session | **1.4 ms**（`journalctl -o short-precise`："Accepted password" → "session opened"） |
| motd 脚本 | 全量跑 45 ms（`00-header` 10 + `30-sysinfo` 25 + `98-fsck-at-reboot` 6），**但有缓存**：实测某次会话只花 1.4 ms（`pam_motd.so motd=/run/motd.dynamic` 只在脚本更新时重建） |
| 本机 `SSH_ASKPASS` 脚本自身 | 1.5 ms（喂密码那一层可以忽略） |

## 2. 一次连接的逐段（暖链路，`ssh -vvv` 每行打时间戳）

```sh
SSH_ASKPASS=… setsid -w ssh -vvv -F /dev/null -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null board@<HOST_07> true 2>&1 \
  | perl -MTime::HiRes=time -ne 'BEGIN{$t=time} printf("%7.3f %s", time-$t, $_)' \
  | grep -E 'Local version string|Remote protocol version|kex: algorithm|Authenticated to|channel_input_open_confirmation|exec request accepted'
```

| 段 | 累计(s) | 本段 | 说明 |
| --- | --- | --- | --- |
| 本地 version string | 0.013 | **13 ms** | TCP 握手（1 RTT）；**冷启动 77–99 ms**（Wi-Fi 省电唤醒） |
| 收到远端 version | 0.042 | 29 ms | 版本交换（约 1.5 RTT） |
| KEX 算法选定 | 0.058 | 16 ms | 协商 |
| **Authenticated** | 0.410 | **352 ms** | KEX 交换 + 公钥试探 + 密码认证（大头） |
| channel open 确认 | 0.530 | 120 ms | 会话建立 |
| exec 已接受 | 0.554 | 24 ms | 远端跑 `true` |
| **合计** | **~0.55 s** | | 冷启动 0.8–1.1 s |

多次取样（每条 5 次，取中位数附近的实测值）：

| 配置 | 实测 |
| --- | --- |
| 完整连接（先试公钥，再输密码） | 0.50 / 0.54 / 0.54 / 0.62 s |
| 跳过公钥（`-o PreferredAuthentications=password -o PubkeyAuthentication=no`） | 0.51 / 0.55 / 0.55 s ⇒ **省 40–70 ms** |
| 到认证失败为止（`-o PreferredAuthentications=none`，不发密码） | 0.29 / 0.30 / 0.31 s ⇒ **认证段 ≈ 0.25 s** |
| TCP 连接（`/dev/tcp` + `date +%s.%N`） | 暖 16–22 ms / 冷 77–99 ms |
| 对照：同法连工程机 <HOST_09> | 0.45–0.75 s（**同量级 ⇒ 不是开发板特有**） |

## 3. 归因：哪些是瓶颈、哪些不是

| 项 | 实测 | 结论 |
| --- | --- | --- |
| 后量子 KEX（`sntrup761x25519-sha512`）vs 经典 `curve25519-sha256` | 0.55 s vs 0.55 s | **没差别**，别为它调算法 |
| 公钥试探（`id_rsa`/`id_ecdsa`/`id_ecdsa_sk`/`id_ed25519_sk`/`id_xmss`） | 跳过省 40–70 ms | 小头，配好公钥/关掉即可 |
| 密码认证 | ≈0.25 s（含 1–2 个 RTT + 服务端 `pam_unix`） | 换公钥能省，且是复用前的前提 |
| SSH 协议往返（版本+KEX） | ≈0.30 s（TCP 起算，6–8 个 RTT × 20–25 ms + 两端调度） | Wi-Fi 上的硬成本 |
| 服务端（sshd/PAM/负载） | PAM session 1.4 ms、load 0.15 | **板子不背锅** |
| Wi-Fi 省电 | 冷连接多 60–90 ms | 想稳可 `iw dev wlan0 set power_save off` |

## 4. 优化：连接复用（已落地，收益最大）

```sh
ssh -o ControlMaster=auto -o ControlPath=/tmp/lsp-ssh-%r@%h:%p -o ControlPersist=10m board@<HOST_07>
```

**实测**：

```
A/B（走 ~/.ssh/config 的普通 ssh）:
  主连接被关掉后：新建 1128 ms（冷）→ 之后 44 ms（auto 自动重建）
  有主连接时    ：44 ms / 44 ms / 44 ms          ← 约 12–25×
sync-to-board.sh：第一次 5.77 s（含建主连接）→ 第二次 3.06 s（rsync 扫 2.07 G 文件树是剩下的主要开销）
ssh -O check -o ControlPath=/tmp/lsp-ssh-board@<HOST_07>:22 board@<HOST_07> → Master running (pid=…)
```

仓库里已经这么接的（`ControlPath` **固定成同一个路径**，所有工具共用一条主连接）：

| 位置 | 改动 |
| --- | --- |
| `tools/sync-to-board.sh` | `RSYNC_OPTS+=(-e "ssh $MUX_OPTS $SSH_ARGS")`；`LSP_SSH_NO_MUX=1` 可关 |
| `tests/opt-flags-bench.sh` | `SSH_OPTS` 里同样带 `MUX_OPTS`（它要来回几十条 ssh）；不影响被测程序的口径（被测耗时是板上跑的） |
| 本机 `~/.ssh/config` | 只对 `<HOST_07>` / `<HOST_09>` 两个 Host 的复用块 + `ServerAliveInterval 30` |
| 本机 `~/.ssh/known_hosts` | 补了两台板子的主机公钥（否则不带 `StrictHostKeyChecking=no` 的 ssh 首次会卡在交互提示） |

### 4.1 ⚠️ 板子换 IP / 换 SSID 后必须同步 `Host` 行（2026-10-08 踩到）

复用块是**按 `Host` 匹配**的：板子从 `<HOST_07>` 换到 `<HOST_11>` 后 `Host` 没跟着改
⇒ **所有 ssh/scp/rsync 都退回冷启动**，链路一抖动就卡成 20 s 级（TCP 指数退避），表现就是
"ssh 连不上、退都退不出去"。修法（已落地）：把新地址加进 `~/.ssh/config` 那一行；想一劳永逸
就写成网段通配 `Host <PRIVATE_NETWORK>`（覆盖整个 /21，板子在两个 SSID 之间换 IP 都命中）。

实测（2026-10-08，板子 `<HOST_11>`，与本机同一个 AP）：

```
冷启动（建主连接）      20.16 s 失败 → 重试 24.53 s 成功   ← 抖动期的握手会退避到几十秒
复用后                 0.06 / 0.05 / 0.05 s
rsync 200 KB（复用）    0.27 s
rsync 200 KB（不复用）  20.10 s                            ← 74×，且这次撞上停滞直接失败
```

同轮还加了保活（`ServerAliveInterval 10` + `ServerAliveCountMax 2` + `ConnectTimeout 10`）：原来只有
`ServerAliveInterval 30`，链路断掉时客户端会挂住不返回，终端只能靠 `~.` 逃。

板子侧顺带查清的三件事（**都不是 sshd 的锅**：`sshd` active、PAM 正常、CPU/磁盘空闲）：

| 项 | 结论 |
| --- | --- |
| 两个 SSID 来回切（IP 在 `<HOST_11>` ↔ `<HOST_07>` 之间变） | NM 里另一个网络仍 `autoconnect yes` ⇒ 已设 `connection.autoconnect no`（2026-10-08 落地） |
| Realtek 驱动固件省电 `rtw_lps_mode=2`（MAX） | 改成 0（含重载驱动）**实测对延迟无改善** ⇒ 不是抖动成因；想回默认就去掉 `modprobe` 参数 |
| supplicant 背景扫描 `bgscan=simple:30:-65:300` | **关不掉**：板子是 NM **1.46.0**，既没有 per-connection 的 `802-11-wireless.bgscan`（`nmcli` 报 `invalid property`，别名 `wifi.bgscan` 同样报错），`man NetworkManager.conf` / `man nm-settings-nmcli` 里也**没有**任何 bgscan 开关 ⇒ 只能接受（这是 NM 硬编码的默认值） |

链路画像：RTL8852BE、`Power Management: off`、-58~-64 dBm、质量 48~94/100、MTU 1500、
ICMP **0% 丢包但 avg 57~90 ms / max 290~330 ms、`pipe 2`（成串到达）** ⇒ 属 AP/airtime 侧脉冲；
板子 CPU（load 1.1、PSI 0.00）与磁盘（61%、无 IO 错误）都可排除。

> 本页只讲"连一次要多久、怎么复用"；**链路本身为什么卡**（对端 WiFi 省电、同 AP 站间转发、
> STA 该绑 `wlan0` 还是 `p2p0`、双 WiFi 抢默认路由）见 [wifi-link-performance.md](wifi-link-performance.md)。

## 5. 明确不做的事（别浪费时间）

- ⛔ 别为"提速"换 KEX/cipher：后量子与经典实测同速。
- ⛔ 别去关 motd：`pam_motd.so motd=/run/motd.dynamic` 有缓存（一次会话 1.4 ms），全量 45 ms 也不值得动。
- ⛔ 别调 sshd：`usedns`/`gssapi` 本来就把常见的两个大坑关掉了。
- 唯一可选的板子侧动作：Wi-Fi 省电（冷连接多 60–90 ms）→ 配合 `ControlMaster` 基本无感。

## 6. 复现命令（照抄即可）

```sh
# TCP 连接耗时
for i in 1 2 3 4 5; do s=$(date +%s.%N); timeout 3 bash -c 'exec 3<>/dev/tcp/<HOST_07>/22' \
  && e=$(date +%s.%N) && awk -v a="$s" -v b="$e" 'BEGIN{printf "%.1f ms\n",(b-a)*1000}'; done

# 两组取样（完整 vs 不发密码）
/usr/bin/time -f '%e' ssh … board@<HOST_07> true
/usr/bin/time -f '%e' ssh -o BatchMode=yes -o PreferredAuthentications=publickey board@<HOST_07> true

# 服务端侧（精确到微秒）：Accepted password → session opened
sudo journalctl -u ssh -o short-precise --since "-1 min" --no-pager

# 主连接状态 / 主动断开
ssh -O check -o ControlPath=/tmp/lsp-ssh-board@<HOST_07>:22 board@<HOST_07>
ssh -O exit  -o ControlPath=/tmp/lsp-ssh-board@<HOST_07>:22 board@<HOST_07>
```

> 注：我们平时调试用的 ssh 都带 **`-F /dev/null`**（不受用户 `~/.ssh/config` 影响、结果可复现），
> 所以**手敲这类命令时要把 mux 参数显式写上**，或者别加 `-F /dev/null`。
