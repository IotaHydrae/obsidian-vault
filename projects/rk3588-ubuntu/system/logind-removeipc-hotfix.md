# 老镜像一键补 `RemoveIPC=no`（现场运维用）

> 2026-09-26 那版之后的镜像里已经有 `/etc/systemd/logind.conf.d/50-lsp-no-remove-ipc.conf`；
> **更早的镜像没有** ⇒ 现场"ssh 进去看日志、退出来外设全断"还会复现。
> 用 `tools/lsp-fix-logind-no-removeipc.sh` 一条命令补上（幂等、可 `--dry-run`）。

## TL;DR

- 脚本**自包含**（不依赖仓库其它文件）：scp 到目标机、`sudo bash` 跑即可。
- ⚠️ **老镜像的采集账户是 `cat`**（新镜像才是 `board`）：`RemoveIPC=no` 是全局的、与账户名无关，
  但 **linger 那一步要落到正确的账户** ⇒ 脚本自动在 `board`/`cat` 里挑存在的那个，也可 `--user` 指定。
- 装完会自动 `systemctl restart systemd-logind` 并用 `busctl … RemoveIPC` 自检 `b false`；
  产出的文件与镜像里那份**逐字节一致**。
- ⛔ 现场别写 `curl … | sudo bash`：sudo 要密码时会从管道读密码把脚本吃掉；
  要写成 `curl -o /tmp/fix.sh && sudo bash /tmp/fix.sh`，且**必须 bash 不能 sh**。
- 只读体检：`tools/lsp-verify-removeipc.sh`（两阶段金丝雀法，不用停采集栈）。

## 16.1 内网 HTTP 分发（不用逐个 scp）

2026-09-26 那版之后的镜像里已经有 `/etc/systemd/logind.conf.d/50-lsp-no-remove-ipc.conf`；
**更早的镜像没有** ⇒ 现场"ssh 进去看日志、退出来外设全断"还会复现。补的方法：

⚠️ **老镜像的采集账户是 `cat`**（不是 `board` —— 用户 2026-09-29 指出；新版镜像才是 `board`）。
`RemoveIPC=no` 是**全局**的、与账户名无关，但 **linger 那一步要落到正确的账户**上：
脚本会自动在 `board` / `cat` 里挑**存在**的那个（两个都在就都给开），也可以 `--user <账户>` 指定。

```bash
# 仓库里那个脚本是自包含的（不依赖仓库其它文件），scp 到目标机直接跑即可
scp tools/lsp-fix-logind-no-removeipc.sh <运维机>:/tmp/
sudo bash /tmp/lsp-fix-logind-no-removeipc.sh            # 落 ①配置 ②linger（自动认 board/cat）→ 重启 logind → 自检
sudo bash /tmp/lsp-fix-logind-no-removeipc.sh --dry-run  # 只看会做什么
sudo bash /tmp/lsp-fix-logind-no-removeipc.sh --no-restart   # 只落文件（下次重启生效）
sudo bash /tmp/lsp-fix-logind-no-removeipc.sh --no-linger    # 只补 ①（不动 linger）
sudo bash /tmp/lsp-fix-logind-no-removeipc.sh --user cat     # 明确指定老镜像的 cat 账户
```

等价的手工命令（不想用脚本时，一行）：

```bash
sudo install -d -m 0755 /etc/systemd/logind.conf.d \
  && printf '[Login]\nRemoveIPC=no\n' | sudo tee /etc/systemd/logind.conf.d/50-lsp-no-remove-ipc.conf >/dev/null \
  && sudo systemctl restart systemd-logind \
  && sudo loginctl enable-linger cat          # ⚠️ 老镜像是 cat；新镜像写 board
```

判据（脚本自己会跑一遍）：`systemd-analyze cat-config systemd/logind.conf | grep RemoveIPC` 是
`RemoveIPC=no`，且 `busctl get-property org.freedesktop.login1 /org/freedesktop/login1 \
org.freedesktop.login1.Manager RemoveIPC` → `b false`。

**手动检查（不用 root，两条命令搞定）**：

> 想一条命令看全（段数/信号量/按 UID/谁在映射/多少已 `(deleted)`/SysV/判定）用
> `dds-shm-status.sh`（`tools/dds-shm-status.sh`，也在内网 HTTP 上：
> `curl -fsSL http://host-05.example.com/dds-shm-status.sh -o /tmp/dds-shm-status.sh && sudo bash /tmp/dds-shm-status.sh`）。
> 下面是它背后用的那几条原始命令。

```bash
# ① RemoveIPC：看两处 —— 配置文件（下次启动会读）与 logind 内存里的当前生效值
systemd-analyze cat-config systemd/logind.conf | grep -iE '^RemoveIPC'   # → RemoveIPC=no
busctl get-property org.freedesktop.login1 /org/freedesktop/login1 \
       org.freedesktop.login1.Manager RemoveIPC                          # → b false（b true = 还没生效）
ls -l /etc/systemd/logind.conf.d/50-lsp-no-remove-ipc.conf               # 文件在不在（老镜像里就是没有）

# ② linger：生效值 + 标记文件（两个都要看）
loginctl show-user board -p Linger                                         # → Linger=yes（老镜像是 cat）
loginctl list-users                                                      # 一眼看全部：LINGER 列
ls -l /var/lib/systemd/linger/                                           # → 0 字节的 board（或 cat）

# ③ 一条命令体检（只读，不用 root；脚本自己会同时看上面这些）
./lsp-fix-logind-no-removeipc.sh --check
```

`--check` 在开发板上的实测输出（配好 / 被人把 conf 删掉两种情况）：

```
$ ./lsp-fix-logind-no-removeipc.sh --check
RemoveIPC  配置=RemoveIPC=no     logind=b false
linger     board  =yes（有标记文件）
== OK（rc=0）

$ rm /etc/systemd/logind.conf.d/50-lsp-no-remove-ipc.conf ; ./lsp-fix-logind-no-removeipc.sh --check
RemoveIPC  配置=读不到            logind=b false        ← logind 还没重启，所以内存里仍是对的
linger     board  =yes（有标记文件）
== 没配好：见上面（rc=1）
```

**开发板验证（2026-09-29）**：模拟老镜像，删除配置与 linger 后运行脚本，
重新写入 drop-in、打开 `board` linger、重启 logind；自检得到 `RemoveIPC=no`、
`busctl=b false` 和 linger 已开启，退出码为 0。
当前内置副本与 overlay 都只保留关键说明和 `[Login] / RemoveIPC=no`。

已配好的机器再跑（幂等）：`conf 已正确（跳过）` / `linger board 已开（跳过）` /
`logind 已重启` / 三条 `自检 ✓` / `== OK（rc=0）`。
账户识别也测过（这块板只有 `board`）：自动认到 `board` ✓；`--user cat`（不存在）→
`✗ 没有 cat 这个账户` + `== 失败（rc=1）`（fail loud，不会静默通过）。

⚠️ 脚本只解决"用户被释放时删 IPC"这条根因；现场如果已经出现过全断，**采集栈要重启才会恢复**
（`systemctl restart collect_system` + 长按 LCD 2 秒，原因见 [collection-dds-shm-fault.md](collection-dds-shm-fault.md)）。

### 16.1 内网 HTTP 分发（2026-09-29 用户要求，不用逐个 scp）

**标准做法：编译服务器上装 nginx，文件发进 docroot**（不起自定义服务）：
（完整、可照抄到别的机器的步骤见 [intranet-script-http.md](intranet-script-http.md)；下面是最短版。）

```bash
# 一次性：服务器上装 nginx（Debian 默认已 enabled，开机自启）
sudo apt-get install -y nginx-light
# 以后每次更新脚本：仓库 → docroot（nginx 立即生效，不用 reload）
tools/lsp-fix-http.sh publish          # 默认 /var/www/html、端口 80
tools/lsp-fix-http.sh url              # 打印现场可用的 URL
```

当前部署：服务器 `<HOST_05>`（`example-projectDell`）的 **`/var/www/html/`**，
`http://host-05.example.com/<名字>`（文件：`fix-logind-no-removeipc.sh`、`verify.sh`、`repro.sh`、
`dds-link-monitor.py`、`SHA256SUMS`、`README.txt`，`fix.sh` 是指向新名的符号链接）。
服务器上跑的是发行版 nginx（`systemctl is-enabled nginx` = enabled），**没有**任何自定义单元。
（开发机 `<HOST_06>` 上那套临时的 `systemd --user` + 8899 已经停掉删掉了。）

目标机（老镜像）上任选一条 —— ⚠️ **脚本要用 `bash`**（里面有 `read -d`/数组等 bash 特性，`sh` 会报
`read: Illegal option -d`）：

```bash
# 推荐：先落文件再跑
curl -fsSL http://host-05.example.com/fix-logind-no-removeipc.sh -o /tmp/fix.sh && sudo bash /tmp/fix.sh
# 带 sha256 校验
cd /tmp && curl -fsSLO http://host-05.example.com/fix-logind-no-removeipc.sh && sha256sum -c fix-logind-no-removeipc.sh.sha256 && sudo bash fix-logind-no-removeipc.sh
# 只想看会做什么
curl -fsSL http://host-05.example.com/fix-logind-no-removeipc.sh -o /tmp/fix.sh && sudo bash /tmp/fix.sh --dry-run
```

⛔ **别用 `curl … | sudo bash`**（用户最初设想的那种写法）：sudo 需要密码时会**从管道里读密码**，
于是把脚本当密码吃掉 —— 板上实测 `sudo: 3 incorrect password attempts`、脚本一行没跑。
非要用管道就先缓存凭据：`sudo -v && curl -fsSL http://host-05.example.com/fix-logind-no-removeipc.sh | sudo bash`（实测 rc=0）；
或 `sudo bash -c "$(curl -fsSL http://host-05.example.com/fix-logind-no-removeipc.sh)"`（注意必须是 `bash`，`sh` 不行）。

**板上实测（开发板 <HOST_07>，2026-09-29）**：`curl README.txt` ✓（内网通、无防火墙阻拦）；
三种写法各跑一遍都 rc=0；直接 pipe 那个坑也复现了（如上）。

同一个服务上也放了**复现脚本**（`tests/dds-shm-fault-repro.sh` → `repro.sh`，配套
`dds-link-monitor.py`、`SHA256SUMS`）—— 工程师排查用，**故意制造故障**：

以及**不故障注入的"修复验证"**（`tools/lsp-verify-removeipc.sh` → `verify.sh`）：

```bash
curl -fsSL http://host-05.example.com/verify.sh -o /tmp/verify.sh && sudo bash /tmp/verify.sh
# 或：cd /tmp && curl -fsSLO http://host-05.example.com/verify.sh \
#      && sha256sum -c --ignore-missing <(curl -fsSL http://host-05.example.com/SHA256SUMS) && sudo bash verify.sh
```

两阶段金丝雀法，**不碰采集栈、不断 ssh、不会把 LCD 打到六路全断**；账户自动认 `board`/`cat`
（`--user cat` 显式指定）：

```
== 验证 RemoveIPC（金丝雀法；不动采集栈）
   配置 RemoveIPC=no / logind b false
   A) board 登录/登出 1 轮（linger=yes）
      ✓ 1 轮后金丝雀还在
      · 其中「用户真被释放」发生了 0/1 次 （0 次 = 该账户还有别的会话或本来开着 linger ⇒ 段判别力有限，结论看 B）
   B) 临时账户 lsp-ipcprobe：真释放一次（不依赖 linger）
      释放是否真的发生：Y（Y=logind 已释放该用户）
      金丝雀：/dev/shm 文件 还在 / SysV 还在
== OK：RemoveIPC=no 已生效（用户被释放也不会删共享内存）
```

- 阶段 A = 现场那个场景（user manager 起来→被拆 / 登录→登出），用**采集账户本人**的金丝雀；
  每轮还会报"用户是否真被释放"（0 次说明该账户还有别的会话或开着 linger ⇒ 只是近似，判据看 B）。
- 阶段 B = **决定性**：临时账户（跑完删掉）造金丝雀 → `enable-linger` 建用户记录 →
  `disable-linger` + `terminate-user` 真释放一次 → 金丝雀活着 ⇒ `RemoveIPC=no` 生效。
  开发板上做过**反面对照**：把补丁临时挪走 + `restart systemd-logind`（= 没修的机器）→
  阶段 B 立即报"金丝雀被删 ✗ / rc=1"；还原后 rc=0 ✓。
- 「已完成」的机器上跑一次 `verify.sh` 拿到 `rc=0`，就是"修复成功"的现场判据。

```bash
cd ~ && curl -fsSLO http://host-05.example.com/repro.sh \
      && curl -fsSLO http://host-05.example.com/dds-link-monitor.py && chmod +x repro.sh dds-link-monitor.py
./repro.sh --dry-run        # 先只查前置条件
```

⚠️ 该脚本 2026-09-29 起**自动认采集账户**（新镜像 `board` / 老镜像 `cat`，可用 `LSP_REPRO_USER` 指定；
服务名 `LSP_COLLECT_SERVICE`、家目录 `LSP_REPRO_HOME` 也能覆盖）；显式指定的账户不存在会 rc=2 明说。
补过 `RemoveIPC=no` 的新镜像上跑不出故障（这正是"修复有效"的判据）。

---

