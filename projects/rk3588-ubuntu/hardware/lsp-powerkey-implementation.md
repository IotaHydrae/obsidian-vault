# `lsp-powerkey`：按住 2 秒正常关机 / 10 秒强制断电（落地实现）

> 本文是那个"电源键最后手段通道"的**实现与实测记录**：状态机、为什么要 `-static -s`、
> 板上实测的三级行为、以及 2026-10-02 重做时抓到并修掉的"按住期间忙等"。
> 引脚与用户态事件见 [rk806-pwron.md](rk806-pwron.md) §1~§3、寄存器级手段见
> [rk806-pmic-registers.md](rk806-pmic-registers.md)；app 的构建/打包规则见
> [../build/apps.md](../build/apps.md) §7。
> ⚠️ 正文**原样搬自 [rk806-pwron.md](rk806-pwron.md) §8**（节号保留）。

## TL;DR

- 需求：**按住 2 秒**触发正常关机、**按住 10 秒**强制断电（PMIC `DEV_OFF`），松手取消。
- **零内核改动**：状态机在用户态 C 程序 `apps/lsp-powerkey/`（静态链接，见 §8「为什么是 `-static`」）。
- ⚠️ 2026-10-02 重做时抓到老实现"force 触发后忙等 100% CPU、连松手都读不到"，已修；
  回归脚本 `tests/powerkey-hold-timer.sh`（工程机 通过 12/失败 0）。


## 8. "按住 2 秒正常关机 / 10 秒强制断电"的落地（已实现并实测通过）

需求（用户确定）：**从"按下"开始计时，不等松手**；无屏使用，GNOME 弹窗不管。

| 按住时长 | 动作 | 为什么 |
| --- | --- | --- |
| < 2 s 松手 | 什么都不做 | 防误触 |
| ≥ 2 s | `systemctl --no-block poweroff` | 常态路径，走完整软关机（有文件系统收尾） |
| ≥ 7 s | 再发一次 `poweroff` | 兜底：2 秒那次可能被卡顿别的东西吃掉 |
| ≥ 10 s | 写 PMIC `DEV_OFF` 直接断电 | 2 秒没关成基本说明用户态已卡死；这一级**不再尝试软关机**，也不依赖 systemd |
| 2~10 s 之间松手 | 取消 10 s 那级（已发出的软关机不回滚） | |

三级独立可测（`/etc/default/lsp-powerkey`）：
`LSP_POWERKEY_HOLD_{GRACEFUL,RETRY,FORCE}`，设 0 = 禁用该级（force 不可禁）。
`LSP_POWERKEY_{GRACEFUL,RETRY}_CMD` 可替换那两级执行的命令
（只验"10 秒强制断电"时把 GRACEFUL_CMD 设成 `/bin/true`，2 秒那级就只记日志）；
`LSP_POWERKEY_DRYRUN=1` 只打日志、不动系统。
`LSP_POWERKEY_DEVICE_NAME` 可改认哪个输入设备（默认 `rk805 pwrkey`）：`tests/powerkey-hold-timer.sh`
就是靠它造一个同名 uinput 假设备，把三段阈值缩到几百毫秒，**不动真电源键、不关机**地验完整状态机
（2026-10-02 加，工程机 通过 12 / 失败 0）。

配置阈值单位为毫秒；启用时 retry ≥ graceful、force ≥ retry，force 必须小于硬件的 12 秒阈值。
PMIC 配置是易失寄存器，每次服务启动都由 `--setup` 重设。

| 调试配置 | 作用与约束 |
| --- | --- |
| `LSP_POWERKEY_DRYRUN=1` | 仅记日志，跳过软关机、强制断电及 PMIC 设置 |
| `LSP_POWERKEY_DEVICE_NAME` | 默认 `rk805 pwrkey`；测试可改为独立 uinput 设备名 |
| `LSP_POWERKEY_SET_LP_OFF_TIME` | 默认十六进制 `b6`；寄存器 0x76 的 bit7 必须保持，不能随意覆盖 |
| `LSP_POWERKEY_LOG` | 默认 `/var/log/lsp-powerkey.log`，留空关闭文件日志，journal 输出保留 |
| `LSP_POWERKEY_LOG_MAX_KB` | 默认 256 KiB，达到上限截断重开 |

### 8.1 2026-10-02：重做时抓到并修掉的"按住期间忙等"

写上面那个测试时，**第一次跑就抓到老实现的一个 bug**：force 那级触发之后、在等下一次重试的
1 秒窗口里，老代码算出的"下一个到点时刻"是**已经过去的** `down + hold_force_ms`，于是外层循环
一路 `continue`、**再也不调 `select()`** —— 结果是 100% CPU 空转，而且这段时间**连"松手"都读不到**
（测试里 ⑤ 的松手事件就是这么丢的）。

* 实测（工程机，`tests/powerkey-hold-timer.sh` 的 ⑥，按住 1.2 s 采一次 CPU 时间）：

  | 版本 | 1.2 s 墙钟内消耗的 CPU |
  | --- | --- |
  | 修之前 | **1200 ms**（≈1:1，纯忙等） |
  | 修之后 | **0 ms** |

* 修法：`hold_timer_next_deadline()` 不再拿"最迟的 force 时刻"当兜底，而是**已经进了 force 重试
  阶段就返回 `force_retry_deadline`**，否则返回"下一个还没触发的级"。
* 现场影响面：真机上 force 那级成功写 `DEV_OFF` 就直接断电了，所以只有**写失败/DRYRUN**时才会
  走到这段（而那正是 `FORCE_MAX_TRIES` + `do_emergency` 要兜的路径）。

### 实现位置（**零内核改动**；源文件在 app 目录 `apps/lsp-powerkey/` 里）

> 2026-09-14 起这个守护进程从 `overlay/` 挪到了 **`apps/lsp-powerkey/`**：有源码的东西只提交源码，
> 编译产物（交叉编译的静态二进制）落 `build/apps/lsp-powerkey/`、**不进 git**。
> 编译 `./build.sh -a lsp-powerkey`；`rootfs` 阶段自动 `make -C apps/lsp-powerkey install DESTDIR=<rootfs>`。
> 约定见 [../build/apps.md](../build/apps.md)。

* `/usr/local/sbin/lsp-powerkey`（C 源：`apps/lsp-powerkey/src/lsp-powerkey.c`，
  配置在 `apps/lsp-powerkey/files/`；交叉编译的静态二进制）：
  * 自己 `read()` `/dev/input/eventN`，**按设备名 `rk805 pwrkey` 找节点**（编号会漂移，
    实测见过 event1 → event8/event9）；
  * 用 `CLOCK_MONOTONIC` + `select()` 超时推进三级（内核**不发 `value=2`**，超时必须自己设）；
  * 只依赖按下/松开两个事件，**不需要等到松手**；
  * `--setup` 模式：启动时把 PMIC 长按兜底从 6 秒放宽到 **12 秒**（写 `0x76=0xB6`，
    只动 bit5:4，bit7 保持 1，见 [rk806-pmic-registers.md](rk806-pmic-registers.md) §4）。
* `lsp-powerkey.service`：`User=root`（`eventN` 是 `root:input 0660`，且要写 sysfs）、
  `Restart=always`、`ExecStartPre=... --setup`。
* `logind.conf.d/99-lsp-powerkey.conf`：`HandlePowerKey=ignore` + `HandlePowerKeyLongPress=ignore`，
  外加 `Handle{Suspend,Hibernate}Key*` / `HandleLidSwitch*` 全部 `ignore`
  —— 把"睡不睡"的处置权从 logind 收回来。
  ⛔ **这个文件必须有 `[Login]` 段**（2026-09-21 实测踩过）：logind 的 drop-in 是 ini 格式，
  键写在任何 section 之外会被**静默丢弃**，日志只留一行
  `99-lsp-powerkey.conf:17: Assignment outside of section. Ignoring.`，
  生效值退回默认的 `HandlePowerKey=poweroff` ⇒ **短按 ON 键直接关机**（而 `lsp-powerkey` 不
  `EVIOCGRAB`，logind 同样看得见这个键，两家会抢）。判据：
  `busctl get-property org.freedesktop.login1 /org/freedesktop/login1 org.freedesktop.login1.Manager HandlePowerKey`
  → `s "ignore"`；`journalctl -u systemd-logind -b | grep 'outside of section'` → 无输出。

#### 为什么是 `-static`、为什么再加 `-s`（2026-09-14 补记）

> 2026-09-14 之前这个决定**没有任何文档**，只留了个 `-static` 在编译命令里。
> 结果就是：任何人看到 857 KB 的静态二进制，第一反应都是"改成动态省 780 KB"。
> 把理由和实测数字记在这里。

**这个程序的画像（x86 交叉编译实测）**：动态版只 `NEEDED libc.so.6` + `ld-linux`；源码里**没有任何**
`getpwnam`/`getaddrinfo`/`dlopen`/`setlocale` 调用 —— 静态链接最经典的坑（NSS 模块、
`dlopen` 插件、locale）在这里**都不存在**，所以静态对它是安全的。

三条理由，第 1 条是真正针对这个程序的：

1. **它是"最后手段"通道。** 它的活儿是 2s 软关机 / 7s 重试 / **10s 直接写 PMIC 断电**。
   系统正处在 apt 刚换掉 `libc.so.6`、或 `ld.so` 被弄坏的状态时，动态版**连启动都起不来**
   （`Restart=always` 只会空转），按电源键毫无反应；静态版没有这个失效模式。
2. **切断"交叉工具链 glibc 版本 ↔ 目标镜像基线"的耦合。** 动态版实测最高要 **`GLIBC_2.38`**
   （符号 `__isoc23_strtol`：GCC 15 默认 `-std=gnu23` 让 `strtol` 解析成 C23 版本符号）。
   noble 是 2.39 **刚好够**；换 22.04 基线（2.35）就直接 `GLIBC_2.38 not found`。
   静态链接后，构建机用什么工具链、镜像是什么发行版都不再相干。
3. **少一类"静默失效"。** 镜像全程 `--no-install-recommends`，历史上踩过多次
   "少个包 → 功能悄悄没了"（`dnsmasq-base`/`systemd-resolved`/`plymouth`）。
   静态版把这一个文件从"运行时依赖"里彻底摘出去。

代价（也要知道）：

| | 静态 | 动态 |
| --- | --- | --- |
| 体积 | **857,344 B**（`-s` 后 **729,568 B**） | 73,488 B |
| 运行时依赖 | 0 个 `NEEDED` | `libc.so.6` + `ld-linux-aarch64.so.1` |

* **libc 的安全更新不会自动跟上**：动态程序随 libc 升级自动受益，静态版必须重编重刷。
  对这个几百行、只用基础 libc 调用的守护进程暴露面很小，但不是零。
* ⚠️ **静态只保住了"计时器活着"，没保住"关机动作一定成功"**：2s/7s 那两级是
  `execl("/bin/sh", "sh", "-c", <cmd>, …)`（`src/lsp-powerkey.c:282`），走的仍是**动态的
  `/bin/sh`**。libc 半损时那两级照样可能失败 —— 真正不依赖用户态的是 10s 那级
  （写 `/sys/rk806single/debug` + `reboot(RB_POWER_OFF)`）。别高估静态的作用。

**`-s` 是安全的**（链接期 strip，实测逐节比对）：

```
$ aarch64-linux-gnu-gcc -O2 -static      → 857,344 B
$ aarch64-linux-gnu-gcc -O2 -static -s   → 729,568 B   （省 127,776 B / 15%）
节表差异：只有 .symtab、.strtab 消失，没有任何新增节
.text    501,720 B  逐字节相同
.rodata  114,120 B  逐字节相同
.data      6,868 B  逐字节相同
.note.gnu.build-id  仍在（值会变 —— 它本来就是"本次链接产物"的指纹）
```

也就是说 `-s` **只删符号表**，指令与数据一个字节都没动，程序行为完全一样；
唯一代价是崩溃时（coredump/gdb）看不到函数名。要排查时用
`make -C apps/lsp-powerkey LDFLAGS=-static` 重建一份带符号的即可。

⛔ **不要为了省体积把它改回动态链接**（省下的 780 KB 在 5.6 GiB 的镜像里是噪声），
理由见上面第 1、2 条。

  **本工程的实际环境（2026-09-14 板上实测）**：跑的是带桌面的系统、使用时**不接显示器**
  —— 桌面会话照常启动（自动登录），所以 GNOME 的 inhibitor 一直在：

  ```
  WHAT=handle-power-key:handle-suspend-key:handle-hibernate-key
  MODE=block   WHO=board / gdm   WHY=GNOME handling keypresses
      进程: /usr/libexec/gsd-media-keys      ← board 会话和 gdm greeter 各持一份
  ```

  这种情况下 logind 本来就被挡住，**轮不到它决定**。实测轻点 127 ms：

  ```
  [守护进程] KEY_POWER 松开，按住 127ms（不足 2000ms）→ 什么都不做
  [logind]   Power key pressed short.        ← 它只是"看见了"，没有动作
  板子照常运行
  ```

  所以在这套环境里，drop-in 的作用是**第二层保险**（GNOME 在时由它兜，GNOME 不在时由
  drop-in 兜），不是"没有它就会误关机"。

  **那层保险为什么值得留（`systemctl stop gdm` 造出"没有 GNOME"的环境后实测）**：
  守护进程设成 `DRYRUN=1`（保证它不可能关机），同样 95 ms 轻点：

  | `HandlePowerKey` | 轻点结果 |
  | --- | --- |
  | `ignore`（本工程 drop-in） | **什么都不发生**（守护进程日志：`按住 95ms（不足 2000ms）→ 什么都不做`） |
  | `poweroff`（删掉 drop-in 后的发行版默认） | **立刻开始软关机、板子随即关掉** |

  即：**一旦 GNOME 不在（图形会话没起来/挂了/hang 后被 watchdog 拉起、或将来真做无桌面版本），
  logind 就会拿走电源键，而且它不看到底按了多久**（边沿触发，碰一下就关）。
  drop-in 就是为这种"GNOME 缺位"的情况准备的，装上不花任何代价。

  ⚠️ 反过来说：**验证"logind 会不会抢"必须在没有 GNOME 的会话里做**，
  跟接不接显示器无关 —— 不接显示器时桌面会话照样在跑、照样有 inhibitor。
* 强制断电走 `/sys/rk806single/debug` ← **`w 72 01`**（`DEV_OFF` = 0x72 bit0）。
  实测写下去**立即断电**（[rk806-pmic-registers.md](rk806-pmic-registers.md) §4.5）。
  ⚠️ 顺序：**先把日志 fsync 落盘，再写寄存器**——反过来的话 PMIC 已经掉电，
  日志只写出一半（实测留下过一行 `写 D`）。

### 板上实测记录（2026-09-14，开发板 `<HOST_07>`）

**测试 A：只测 2 秒软关机**（`RETRY=0`、force=10000 不影响）

```
[2026-09-14T15:2x:xx +xxx s] KEY_POWER 按下，开始计时
[2026-09-14T15:2x:xx +xxx s] 到点【按住 2 秒】→ systemctl --no-block poweroff
```
→ 关机后 `journalctl -b -1` 结尾是 `systemd-shutdown[1]: Syncing filesystems...`
→ `Journal stopped` = **真的是软关机**，不是硬切。✅

**测试 B：只测 10 秒强制断电**（`GRACEFUL_CMD=/bin/true`，`RETRY=0`）

```
[...] KEY_POWER 按下，开始计时
[...] 到点【按住 2 秒】→ /bin/true                    ← 2 秒那半被屏蔽
[...] 到点【强制断电】→ 写 DEV_OFF（w 72 01）          ← 第 10 秒，随后板子立即掉电
```
→ 断电后开机读日志，这条记录**在**（fsync 生效）。✅

**GNOME 那个弹窗不影响本需求**（用户确认：设备平常不接屏幕，且守护进程不等松手）。
已查到的事实留档：

* logind 那侧 `HandlePowerKey=ignore` **在带 `[Login]` 段的 drop-in 上**是真生效的（debug 日志明写
  `Handling of handle-power-key (edge) is disabled, taking no action.`）；
  ⚠️ **但这个结论对文件内容敏感**：2026-09-21 发现我们仓库里那份 drop-in 漏了 `[Login]`，
  开发板（<HOST_07>）上生效值其实还是默认的 `poweroff`（logind 只打
  `Assignment outside of section. Ignoring.`）⇒ 短按 ON 键会直接关机。所以判据必须是
  **`busctl` 读实际属性 + `journalctl -u systemd-logind` 查解析抱怨**，别只看某一块板的 debug 日志。
* 但按一下仍会弹 gnome-shell 的 "60 秒后关机" `endSessionDialog`；
* 试过并**无效**的手段：`org.gnome.settings-daemon.plugins.power power-button-action='nothing'`
  （写进系统 dconf 库、并重启了 `gsd-power` 与 `gsd-media-keys` —— 处理电源键的其实是
  **`gsd-media-keys`**，其二进制含 `do_config_power_button_action`、
  `Adding system inhibitors for power keys`，不是 `gsd-power`）；
* 系统总线 / 会话总线上抓不到任何 `PowerKey` 信号或 `EndSessionDialog.Open` 调用，
  在 greeter 会话（`c1`）的总线上也抓不到 —— 弹窗是桌面组件**本地**触发的，
  具体触发点**仍未定位**（GNOME Shell 46.0 源码里搜不到 `PowerKey`/`KEY_POWER`）。
* 结论：**不要**为了消掉这个弹窗去改 GNOME；无屏场景直接忽略。
