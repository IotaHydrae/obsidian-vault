# 板级集成踩坑：NumLock 补按的位置、没重编的二进制、`/dev/shm` 不可写

> 三个"只在板子上才暴露"的坑，都是先看到奇怪现象、再定位到具体原因。
> 共同点：**症状看着像功能没生效，其实是构建/环境问题**。

## TL;DR

- ⛔ **别把 `lsp-keyfilter --tap-numlock` 放回 `ExecStartPost`/`ExecStartPre`**：开机早期会被 SIGHUP
  打断（连带 cage 重启，见 [cage-sighup.md](cage-sighup.md)）；现在 NumLock 由合成器负责
  （见 [compositor-customization.md](compositor-customization.md)）。
- ⛔ **改了 app 源码没重编二进制 = 静默用旧的**：板上看到的行为与新代码不符时，
  先比 `md5sum`（仓库产物 vs 板上 `/usr/local/...`），别急着怀疑逻辑。
- ⛔ **开发板上编镜像时 kiosk 会"一直闪"**：`/dev/shm` 出现一次不可写（权限/空间瞬时问题），
  Chromium 直接 FATAL 退出 → systemd 重启 → 再崩，循环。事后 `/dev/shm` 又是正常的
  （`1777 root:root`），journal 里也没有明显线索 ⇒ 遇到"闪个不停"先查这一段。

> ⚠️ 2026-09-30 起 NumLock 的补按已经**搬进合成器**（[compositor-customization.md](compositor-customization.md)），本节讲的 `lsp-keyfilter` 补按路径
> 现在只在手动跑那个守护进程时才走；结论（"别把 tap 放回 ExecStartPost/Pre"）仍然成立。

**现象**：`lsp-kiosk.service` 里 `ExecStartPost=-+/usr/local/sbin/lsp-keyfilter --tap-numlock`，但板上装的
`lsp-keyfilter` 是**加 numlock 之前**编的那版（`strings -a /usr/local/sbin/lsp-keyfilter | grep -ci numlock`
= **0**）⇒ 不认识这个参数 ⇒ 走进主循环**常驻不退** ⇒ `ExecStartPost` 永远不返回 ⇒ systemd 的
`TimeoutStartSec`（默认 90 s）到点把整个 cgroup（cage + Chromium）**杀掉重拉**，于是
**kiosk 每 90 秒被推倒重来一次**（一次开机实测 3 次 `start-post operation timed out` /
`Failed with result 'timeout'`；屏幕上就是"每隔一会儿画面重载一遍"）。

**判据与修法**（板上执行，已实测）：

```bash
journalctl -b -u lsp-kiosk | grep -cE 'start-post operation timed out'   # 修前 ≥1，修后 0
systemctl show -p SubState --value lsp-kiosk                              # 修前 activating(start-post)，修后 running
journalctl -u lsp-keyfilter | tail                                        # 修后应有 numlock turned on (attempt N …)

# 修：把仓库（板子上那份 ~/ubuntu-lsp-rk3588）里已经编好的新二进制 + 单元装上去
sudo systemctl stop lsp-kiosk.service
sudo install -m 0755 ~/ubuntu-lsp-rk3588/build/apps/lsp-keyfilter/lsp-keyfilter /usr/local/sbin/lsp-keyfilter
sudo install -m 0644 ~/ubuntu-lsp-rk3588/apps/lsp-keyfilter/files/etc/systemd/system/lsp-keyfilter.service \
     /etc/systemd/system/lsp-keyfilter.service
sudo systemctl daemon-reload && sudo systemctl enable --now lsp-keyfilter.service
sudo systemctl start lsp-kiosk.service
```

**教训（镜像构建侧）**：`overlay/` 里的 unit 可以随镜像更新，**`apps/*` 的二进制不会**——只要
`build/apps/<app>/<bin>` 没被重装进 rootfs，就会出现"unit 调新参数、二进制是旧的"。
把 app 参数加进 unit 时，构建必须能重编/重装那个 app（`./build.sh -a <app>` 或整轮重建），
并在板上核对 `strings`/`--help` 是否认识新参数。

**同一个坑的第二个实例（2026-09-29 用户报"开发板上的 keyfilter 白名单好像还是旧的"）**：
`apps/*/files/` 里的**配置文件**也一样会落后 —— 开发板上 `/etc/default/lsp-keyfilter` 还是
`LSP_KEYFILTER_ALLOW="0-9 a-z"`（`--check` 报 **36 keys allowed**），工程机上是
`"0-9 a-z keypad enter backspace capslock numlock arrows minus"`（**55 keys**）。配置和二进制都在
`apps/<app>/files/` 与 `build/apps/<app>/`，**重建镜像**或**在板上重装这四个文件**才会更新。
一键核对（任何板子都能跑，不用 root）：

```bash
/usr/local/sbin/lsp-keyfilter --check | grep -E 'allowlist|numlock'   # 36 = 旧，55 = 新
grep -n '^LSP_KEYFILTER_ALLOW' /etc/default/lsp-keyfilter
```

功能验证（真正验"哪些键会被转发/吃掉"）需要造一个 uinput 合成键盘、再看过滤器虚拟键盘上收到了什么：
先 `systemctl stop lsp-kiosk`（否则转发出去的键会打到上位机页面上），再跑
`/home/board/keyprobe.py`（开发板上留了一份，2026-09-29 实测 11/11：`a enter backspace capslock
numlock minus kp5 down` 转发，`f12 tab ctrl+w` 吃掉，跑完 `systemctl start lsp-kiosk`）。

## 在开发板上编镜像时 kiosk 会"一直闪"——`/dev/shm` 一次不可写把 Chromium 打成 FATAL（2026-09-30 实测）

**现象**：屏幕上画面反复重建（1~2 s 一次），`systemctl show lsp-kiosk -p NRestarts` 一路涨到 39；
journal 里主进程退出码是 **139（SIGSEGV）**。因为 unit 特意设了 `StartLimitIntervalSec=0`
（见 [session-design.md](session-design.md)：宁可一直重拉也不要停在 failed），所以它**会一直重试** ⇒ 屏幕看起来就是"一直闪"。

**直接原因**：那一分钟里 Chromium 突然写不了 `/dev/shm`：

```
cage[3343]: Creating shared memory in /dev/shm/.org.chromium.Chromium.smcAYp failed: Permission denied (13)
cage[3343]: Unable to access(W_OK|X_OK) /dev/shm: Permission denied (13)
cage[3343]: FATAL:... This is frequently caused by incorrect permissions on /dev/shm.
```

Chromium 的进程一 FATAL，cage 的主客户端就没了 ⇒ cage 退出（先 134、随后 139）⇒ systemd 立刻重拉
⇒ 再崩。事后 `/dev/shm` 又是好的（`1777 root:root`、kiosk 可写），journal 里**没有任何
chmod/unmount 记录**，chroot 里的 tmpfiles 规则与 dpkg postinst 也都不碰它 ⇒
**具体是谁在那 1 分钟里改坏的，没有拿到铁证**（复现实验里 `dpkg -i linux-image` 不改 /dev 权限）。

**已知的相关性（是线索不是结论）**：故障窗口（14:50:20 起）与"反复挂卸构建 chroot 的 `/dev`
（`mount -o bind /dev build/rootfs/dev`）+ `board` 阶段装内核"完全重合。构建期把**宿主的 /dev bind
进 chroot**，chroot 里进程对 /dev 的任何改动都会落到**正在跑的这套系统**上；那天还叠了好几层
bind、`umount -lf` 来回跑了很多次。

**处置（当前做法）**：在开发板上编镜像**之前先 `systemctl stop lsp-kiosk`**，编完再 start ——
即使再出现同样的窗口也不会变成屏幕狂闪。真再遇到，先留证据：
`stat -c '%a %U:%G' /dev /dev/shm` + `journalctl -b | grep -i shm`。

**排查经验**：`NRestarts` 涨得多快就能区分"偶发被 HUP"和"每次启动都崩"（1~2 s 一次 = 后者，
直接去看那一次运行的 stderr，别去查 [cage-sighup.md](cage-sighup.md) 那类竞态）。
