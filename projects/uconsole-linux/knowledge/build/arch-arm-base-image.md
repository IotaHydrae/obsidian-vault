# Arch Linux ARM Base：接入现有树莓派内核与镜像导出

> rootfs 发行版可以独立选择；启动闭包仍需匹配项目内核、固件、initramfs 和板级配置。

## TL;DR

- 固定原始 rootfs 的 SHA-256、独立签名和签名者，随后执行完整、签名必需的 pacman 更新。
- Debian 格式内核包只作为文件容器，先暂存再复制；保留 Arch 的 `/lib -> usr/lib`。
- 用 mkinitcpio 为项目 release 生成 initramfs；发行版服务名、包名和网络所有权分别适配。
- chroot 中显式 bind 根目录，让 libalpm 的 `CheckSpace` 看得到 `/`；退出前清理该 keyring 的 GPG daemon。
- 构建和只读检查不能替代原生启动验证；Omarchy 等桌面另作上层配置。

## 最小接入方法

1. 独立 provider/profile、TOML 设置、包清单、来源 recipe 和私有输入快照。
2. 校验 Generic AArch64 归档与 detached signature；信任入口使用官方发布的完整指纹。
3. 私有挂载命名空间内装配 rootfs，使用独立 `/dev`、`/run`，避免连接宿主服务。
4. 初始化 Arch ARM keyring，完整更新 rolling 系统，并保留实际包版本清单。
5. 导出前移除构建用 GPG keyring；首次启动从签名包中的公钥初始化独立 keyring。
6. 去除 generic 内核和无关固件；暂存提取项目 Image、DTB 和模块，保护 usrmerge。
7. Arch 服务负责账户、SSH host keys、网络、根分区扩展与 mkinitcpio；共享文件导出器只处理镜像布局。
8. 检查最终镜像的来源标记、启动引用、包数据库、工具、账户、服务和默认关闭的可选功能。

具体命令和持久配置以 Arch Base 说明（工程源码：`image/arch-base/README.md`）为准。

## 已定位的跨发行版差异

| 边界 | 本工程选择与原因 |
| --- | --- |
| 包与服务名 | Arch `openssh` / `sshd.service`、`networkmanager`、`python`；不能照搬 APT 清单 |
| 用户管理 | 去掉 generic `alarm`，项目普通用户使用 `wheel`，root 密码锁定 |
| 固件 | `raspberrypi-bootloader` 把启动文件放在 `/boot`；显式复制到 FAT 的 `/boot/firmware` |
| 无线 | `linux-firmware-broadcom` 是当前 Arch ARM 的包名；不是 `linux-firmware-brcm` |
| 内核安装 | 不执行 Debian postinst；模块写 `/usr/lib/modules`，保留 `/lib` 链接 |
| initramfs | mkinitcpio 使用明确的目标 release，不使用宿主 autodetect；原生文件与 FAT 副本一致 |
| 网络 | generic 默认启用 networkd/resolved；选 NetworkManager 时禁用并 mask 其服务与激活 sockets |
| 包下载 | 项目内核未启用 Landlock；当前 pacman 的 `DisableSandboxFilesystem` 保留 seccomp 和签名验证 |

## libalpm 磁盘空间检查与清理

2026-10-29 在 x86/QEMU 的真实构建中，pacman 报：

```text
error: could not determine cachedir mount point /var/cache/pacman/pkg
error: failed to commit transaction (not enough free disk space)
```

**观察：** chroot 的 `/etc/mtab -> ../proc/self/mounts` 只列出 `/dev`、
`/run`、`/proc`、`/sys`，没有 `/`。主机磁盘实际有足够空间。

**对照：** 在私有命名空间中把准备好的 rootfs bind 到自身，再进入 chroot，
`/proc/self/mounts` 出现 `/`；相同缓存目录的 `pacman -Sw bash` 完成下载和签名完整性检查。
因此保留 `CheckSpace`，修复挂载可见性，不通过删除空间检查掩盖问题。

失败事务还可能留下 `gpg-agent --homedir /etc/pacman.d/gnupg`，使 `/dev`
卸载报 busy。清理应在 chroot 内对这一个 homedir 执行 `gpgconf --kill all`，
再按相反顺序卸载，并保留主要构建错误。不要终止宿主其他 keyring 的进程。

## 设备独立的 pacman keyring

构建中 `pacman-key --init` 会生成本地私钥；把整个构建 keyring 带进镜像，
会让克隆设备共享本地信任身份。只删除私钥也不完整，因为 keyring 软件包更新
还会调用 `--populate`，它需要本地 secret key 做签名。

本工程在导出前删除整个构建 keyring，保留签名包的
`/usr/share/pacman/keyrings/archlinuxarm{.gpg,-trusted,-revoked}`。首次启动的
oneshot 按 `--init` → `--populate archlinuxarm` → 停止该 homedir 的 GPG daemon
→ 写成功 marker 的顺序执行。无需联网，中断时下次启动重试。
这是官方 ArchISO `pacman-init.service` 模式在持久根文件系统上的一次性适配。
镜像同时保留 Arch 自身 CA bundle 链接，避免引入构建机本地 trust anchors。

## 信任与验证边界

- 2026-08-05 的 Generic 归档使用固定 SHA-256 和官方 build key 签名；MD5 只作传输记录。
- 固定归档不能固定后续 rolling 仓库；receipt 与包清单记录实际版本，不声称完整可复现。
- 离线 rootfs 能执行命令、initramfs 引用一致、文件系统检查通过，仍不能证明硬件启动、网络 DHCP 或 GPIO 收发。
- 早先在 Ubuntu 宿主中运行 Arch/Hyprland 的证据只支持用户态可行性，见
  [独立桌面试跑](../desktop/cm5-omarchy-arch-trial.md)。
- `/lib` 链接破坏的根因与恢复边界见 [usrmerge 暂存规则](usrmerge-kernel-staging.md)。

## 实现与证据

- Arch 后端（工程源码：`scripts/image_backends/arch_base.py`）、镜像检查器（工程源码：`tools/image_inspect.py`）。
- Arch 接入回归（工程源码：`tests/test_arch_base.py`）、检查器测试（工程源码：`tests/test_image_inspect.py`）。
- 构建与挂载对照日志位于忽略的 `image/work/arch-base-20261029/`；实际产物状态见 [开发状态](../../project/status.md)。
- 以上结论限定于本工程当前 CM5 IO Board、Raspberry Pi Linux 6.18 与 Arch ARM aarch64 输入。
