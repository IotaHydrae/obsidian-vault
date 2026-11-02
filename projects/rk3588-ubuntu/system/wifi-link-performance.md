# WiFi 链路：SSH 挂死与大文件传输慢

> 决定性根因是**同一个 AP 内的客户端↔客户端转发不可靠**（与信号强弱无关）：把两端分到不同 AP、
> 或一端改走有线，SSH 与吞吐立刻恢复正常。

## TL;DR

- **判据**：同一时刻、同样的探测，**同 AP 的对端**失败率高、**跨 AP / 有线对端**为 0 ⇒ 问题在 AP 的
  站间转发，不在板子、不在 sshd、不在信号、不在 SSH 复用。
- **修法（实测）**：把一端挪到另一个 AP —— `nmcli con mod "<连接>" 802-11-wireless.bssid <另一 BSSID>`；
  代价是锁了不再自动漫游，回退用 `802-11-wireless.bssid ""`。大文件优先走有线中继。
- **实测前后**（本机 → 板子，30 MB rsync）：同 AP **<1 MB/s 且常超时** → 换 AP 后**上行 2.55 s（≈11.8 MB/s）、
  下行 2.84 s（≈10.6 MB/s）**；SSH 握手失败率 **8/39 → 0/10**。换过去的 AP 信号还更弱（-69 vs -67 dBm）。
- **次要因素**（都真实、但都**不足以**解释本问题，别只治它们）：客户端 WiFi 省电（NM 默认 `powersave=3`）、
  板子 sshd 的 DSCP 标记（`IPQoS lowdelay throughput`）、SSH 复用里死掉的 master socket。
- **不要用"裸 TCP 建连"下结论**：三次握手只是 1~3 个小包，同 AP 那条坏路径上它 10/10 成功；
  完整 SSH 握手（kex 多包往返）才会暴露问题。

## 症状

| 症状 | 说明 |
| --- | --- |
| `ssh` 认证后无输出/卡死，`Ctrl-C` 不回 | 握手/数据阶段的突发被丢，TCP 退避 |
| `ping` 0% 丢包、RTT 几十~几百 ms 脉冲 | ICMP 单包能过，掩盖了突发丢包 |
| rsync/scp ~1 MB/s（协商速率 174 Mb/s） | 满帧反复重传 |
| 同一台机器换有线 / 换 AP 就正常 | 指向 AP 站间转发 |

## 定位：四路径对照（一次判死）

同一台板子、同一时刻，从四条路径做**同样的** SSH 握手探测：

| 发起方 → 板子 | 路径 | SSH 握手失败率 | 结论 |
| --- | --- | --- | --- |
| 笔记本（与板子同 AP） | AP 内站间转发 | **8 / 39 ≈ 21%** | ✗ |
| 笔记本（锁到另一 AP） | 跨 AP（走有线骨干） | **0 / 10** | ✓ |
| 工程机（在另一 AP 上） | 跨 AP | **0 / 10** | ✓ |
| 有线服务器 | 跨网段走路由 | **0 / 15** | ✓ |

分层探测（判断卡在哪一层）：

```bash
# 裸 TCP 建连（小包）——这条永远成功，别用它下结论
for i in $(seq 5); do timeout 4 bash -c 'exec 3<>/dev/tcp/<板子>/22' && echo ok || echo FAIL; done
# 完整 SSH 握手（kex 多包往返）——问题常只在这里暴露
for i in $(seq 10); do timeout 8 ssh -o BatchMode=yes -o ControlMaster=no -o ControlPath=none board@<板子> true && echo ok || echo FAIL; done
# SSH banner（服务器第一个数据包，通常也正常）
timeout 4 bash -c 'exec 3<>/dev/tcp/<板子>/22 && head -1 <&3'
```

看对端 `Send-Q` 判断"谁没 ACK"（在板子上跑）：

```bash
ss -tnp | grep ':22'      # Send-Q 持续 >0 且不降 ⇒ 板子发出的数据没被 ACK
```

## 修法

```bash
# ① 把一端挪到另一个 AP（实测最有效）：同名 SSID 常有多个 BSSID，先列出来
nmcli -f IN-USE,SSID,BSSID,CHAN,SIGNAL dev wifi list | grep -i '<SSID>'
nmcli con mod "<连接名>" 802-11-wireless.bssid <另一个 AP 的 BSSID>
nmcli con up  "<连接名>"
#    回退：nmcli con mod "<连接名>" 802-11-wireless.bssid ""

# ② 大文件优先走有线中继（实测 321 MB：PC→有线 22.7 s≈14 MB/s；有线→板子 41 s≈8 MB/s）

# ③ 顺手关掉主机 WiFi 省电（次要因素，但真实存在）
nmcli con mod "<连接名>" 802-11-wireless.powersave 2
#    全机：/etc/NetworkManager/conf.d/99-wifi-powersave-off.conf → [connection] wifi.powersave = 2

# ④ SSH 客户端保活（避免断链后永久挂着）
#    ~/.ssh/config: ServerAliveInterval 10 / ServerAliveCountMax 2 / ConnectTimeout 10
```

## 为什么

- **AP 内站间转发**：A→B 同 AP 时，AP 要"收一次 + 发一次"，占两次空口；且不少 AP/固件对站间转发的
  排队与重传处理很差（这台表现为**丢突发**：单包过、连续包丢）。跨 AP / 走有线时各占各自的一段空口。
- **与信号强弱无关**：换到更弱的 AP（-69 dBm）后吞吐反而好 10× ⇒ 瓶颈是路径而非射频质量。
- **为什么只有 SSH 敏感**：kex/认证是多包、连续的往返；ICMP/裸 TCP 只需 1~3 个小包，天然更容易过。
- **次要因素各自的真相**：客户端省电会让接收方向延迟 ACK（关掉后失败率仍 ~40%）；板子 sshd 的
  DSCP 标记改成 `IPQoS none` 后失败率也没变；死掉的 mux master 只让*后续*连接报
  `read from master failed`，清掉 socket 即恢复。

## 边界（未证实）

- 这台 AP 的站间转发为什么丢突发（固件/队列/隔离策略）**无法从客户端侧证实**，只有四路径对照做支撑。
- 同 SSID 的多个 AP 由信号强度决定选哪台 ⇒ 会挑到"看着强、转发坏"的那台；客户端侧只能事后锁定 BSSID。
- 背景扫描 `bgscan`：NetworkManager 1.46 没有 per-connection 属性，关不掉（贡献未知）。
- 驱动固件省电 LPS（Realtek `rtw_lps_mode`）实测与吞吐无关。

## 相关

- [ssh-latency.md](ssh-latency.md)：SSH 连接的逐段耗时与连接复用（ControlMaster）——本页讲"链路为什么卡"，
  那页讲"连一次要多久、怎么复用"。
- 板子无线模块：RTL8822CE（`lspci 10ec:c822`，驱动 `rtl88x2ce`；**模块名却叫 `8852be`**，别按名字找芯片）。
