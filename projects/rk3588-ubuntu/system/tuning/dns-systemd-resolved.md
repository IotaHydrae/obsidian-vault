# DNS：systemd-resolved（换网络自动跟随）

> 设备会拿到户外用、网络环境会变，所以 DNS 必须跟着现场 DHCP 走：镜像用 **systemd-resolved**，
> `/etc/resolv.conf` 是标准的 stub 符号链接，内网下发的 DNS 优先、公共 DNS 只做兜底。

## TL;DR

- 三处：`BASE_PACKAGES` 显式加 `systemd-resolved`（它只是 `systemd` 的 **Recommends**，
  `--no-install-recommends` 会静默丢掉）+ overlay 的 `resolved.conf.d/99-lsp.conf` 只写
  **`FallbackDNS=`**（⛔ 不写 `DNS=`，否则会覆盖 DHCP 下发的内网 DNS）+ `board` 阶段 收尾建 stub 链接并 enable。
- ⚠️ 坑 1：构建期 `mount_chroot()` 会往 chroot 写**实文件** `resolv.conf`，而它 `rootfs` / `board` 阶段 各调一次
  ⇒ 必须在 **`board` 阶段 收尾**统一换成 stub 符号链接，否则镜像里会带上构建机的 DNS。
- ⚠️ 坑 2（只在调试时遇到）：在**运行中**的系统上 `apt install systemd-resolved` 后，
  dbus 的策略集里还没有 `org.freedesktop.resolve1.conf` ⇒ resolved 抢不到 D-Bus 名字且不重试，
  `resolvectl` 超时；重启 resolved 或整机即可。镜像里是构建期装好的，不存在这个问题。
- 判据：`ls -l /etc/resolv.conf`（stub 链接）、`resolvectl status`（Link 段能看到 DHCP 下发的 DNS、
  Global 段看到 fallback）。

## 实现（三处）

**需求**：设备以后可能拿到户外用，网络环境会变 —— DNS 必须跟着现场 DHCP 走，
同时内网下发的 DNS 也要能用。所以镜像用 **systemd-resolved**，
`/etc/resolv.conf` 是标准的 stub 符号链接（`../run/systemd/resolve/stub-resolv.conf`）。


| 在哪 | 做什么 |
| --- | --- |
| `BASE_PACKAGES` | 显式加 **`systemd-resolved`** —— 它只是 `systemd` 的 **Recommends**，`--no-install-recommends` 会静默丢掉（和 `plymouth` 同一类） |
| `overlay/etc/systemd/resolved.conf.d/99-lsp.conf` | `FallbackDNS=223.5.5.5 119.29.29.29` —— **只在一个上游 DNS 都没拿到时**才用（故意不写 `DNS=`，否则会覆盖 DHCP 下发的那份，内网域名就解析不了） |
| `stage_board()` 收尾 | `ln -sfn ../run/systemd/resolve/stub-resolv.conf` + `systemctl --root=… enable systemd-resolved` |

## 两个踩过的坑

**① 静态 resolv.conf 会被 `board` 阶段 覆盖（旧代码，已修）**：
`mount_chroot()` 为了构建期 chroot 能解析域名，每次都往 chroot 里写一份"宿主 DNS + 国内公共 DNS"
的**实文件**；它被 **`rootfs` 与 `board` 阶段 各调用一次**。旧代码在 `rootfs` 阶段 末尾拷一份
`overlay/etc/resolv.conf`（注释写着"最终镜像由 overlay 覆盖，不会把宿主 DNS 带进镜像"），
**但 `board` 阶段 的 mount_chroot 会把它覆盖回宿主那版** —— 2026-09-12 实测镜像里
`/etc/resolv.conf` 是 `101.226.4.6`（构建机所在网络的 DHCP DNS）打头的 3 行。
现在改成：构建期照旧用实文件，**`board` 阶段 收尾统一换成 stub 符号链接**，之后没人再写。

**② 在"运行中"的系统上装 systemd-resolved 会起不来（只在调试时有意义）**：
`apt install systemd-resolved` 之后即使 `systemctl enable --now` 成功，`resolvectl` 也会
`Failed to activate service 'org.freedesktop.resolve1': timed out (25s)`、DNS 全挂 ——
因为 dbus-daemon 是开机时启动的，**它的策略集里还没有**新装的
`/usr/share/dbus-1/system.d/org.freedesktop.resolve1.conf`，resolved 抢不到 D-Bus 名字，
而且不会重试。`systemctl restart systemd-resolved`（或重启）即可。
**镜像里是构建期就装好，开机时策略已就位，不存在这个问题**（重启验证过，见下）。

## 实测（开发板，2026-09-12）

```
# 开机后
$ ls -l /etc/resolv.conf
lrwxrwxrwx 1 root root 39 /etc/resolv.conf -> ../run/systemd/resolve/stub-resolv.conf

$ busctl --system list | grep resolve1
org.freedesktop.resolve1  718 systemd-resolve … systemd-resolved.service

$ resolvectl status | sed -n '/Link 5 (p2p0)/,/DNS Servers/p'
Link 5 (p2p0)
    Current Scopes: DNS
         Protocols: +DefaultRoute …
Current DNS Server: <HOST_03>
       DNS Servers: <HOST_03> <HOST_04>     ← NM 把 DHCP 的内网 DNS 推进来了

$ time getent hosts mirrors.tuna.tsinghua.edu.cn
real 0m0.028s
```

`resolvectl status` 的 Global 段还能看到 `Fallback DNS Servers: 223.5.5.5 119.29.29.29`。

## 顺带核对：厂商原版（工程机 eMMC）也是这套

只读挂载工程机 eMMC（`:  /dev/mmcblk0p2`）看过：
`/etc/resolv.conf` 同样是 `../run/systemd/resolve/stub-resolv.conf` 符号链接，
`systemd-resolved` 包已装、`dbus-org.freedesktop.resolve1.service` 链接在，
`/usr/lib/NetworkManager/conf.d/*.conf` 里也是 `dns=systemd-resolved`。
⇒ 我们这次是**跟原版对齐**，不是引入新机制。
（注意：离线看 eMMC 的 `/run/systemd/resolve/stub-resolv.conf` 里写着 `nameserver 8.8.8.8`，
那只是镜像里留下的静态残留；运行时 resolved 会把它渲染成 `127.0.0.53`。
不过它也说明原版默认拿 8.8.8.8 当上游 —— 国内常不可达，这正是我们配 FallbackDNS 的原因。）

---
