# 手动使用 Alpine chroot

TL;DR：Alpine 运行在用户区的 512 MiB ext4 镜像里，共用 Kindle 原内核。
每次手动进入，退出后清理挂载和 loop 关联；不接管开机流程，不替换原系统。

## 进入和执行命令

在工程根目录、已配置 `.device.toml` 的本机终端中执行：

```sh
make alpine-shell
```

出现 `alpine:` 提示符后即可使用 Alpine。输入 `exit` 返回本机终端。
一次只允许一个会话；运行安装、bootstrap 或实机检查前也要先退出交互 shell。
也可直接运行一条命令，命令结束后自动退出并清理：

```sh
python3 tools/chroot.py run -- cat /etc/alpine-release
python3 tools/chroot.py run -- uname -a
python3 tools/chroot.py run -- apk --version
```

默认由 KOReader 的 SSH 服务提供连接。退出 KOReader 或关闭其 SSH 后，
本机连接入口也会失效。没有额外安装 SSH 服务或新增监听端口。

## 文件与工作目录

镜像文件在 Kindle 可见的 `/mnt/us/alpine/rootfs.ext4`，实际 loop 后端使用
同一用户区的 `/mnt/base-us/alpine/rootfs.ext4`，避免把 loop 放在 FUSE 视图上。
Linux 文件权限、符号链接及软件包保存在镜像里的 ext4 文件系统中。

进入后的 `/root/projects` 可存放代码。`/tmp`、`/run` 和 `/dev` 是每次会话
新建的临时文件系统，退出后不保留；其他根文件系统目录的修改会保存在镜像中。
用户区镜像之外没有新增持久化系统服务。

## 安装与验证入口

```sh
make rootfs-fetch
make alpine-install
make alpine-bootstrap
make alpine-check
```

`alpine-install` 只在镜像不存在时创建，拒绝覆盖已有镜像。
它验证固定压缩包、分配 512 MiB 普通文件、格式化该文件、解包并验证原生程序。
安装回执在本机忽略的 `work/chroot-installation.json`。

`alpine-check` 是显式连接设备的硬件验证，不属于离线 `make check`。
它验证发行版、原内核、用户身份、最小 `/dev`、跨会话文件持久性、失败命令退出码，
以及成功和失败后无遗留挂载/loop 关联，以及并发会话被互斥锁拒绝，回执为 `work/chroot-check.json`。
这些验证不证明所有 Alpine 软件都兼容 Linux 4.1。

## 生命周期与使用约束

- 每次使用独立 mount namespace，通过互斥锁限制为一个会话。
- 使用厂商 loop0..12 之外的空闲 loop 节点，绑定后请求自动释放。
- chroot 内的 `/dev` 没有 eMMC 块设备节点，`/sys` 以只读方式挂载。
- 运行前确认厂商根分区仍为只读；操作结束再次核对原启动参数和挂载状态。
- 不运行 OpenRC init，不取代 PID 1，不修改 IDME、fuse、bootloader 或系统分区。
- 运行可信的前台程序；退出前结束其子进程。chroot 是开发环境，不是安全沙箱，
  root 和 `/proc` 仍能影响宿主进程，不能用它隔离不可信代码。

**进入 USB 存储模式或拔线前，先退出 chroot 并等待本机命令返回。**
当前工具没有与 Kindle 的 USB 导出服务联锁，不允许在镜像仍被挂载时导出其底层用户区。

不需要本环境时，先确认 `alpine-check` 的清理检查通过，再移除用户区的 Alpine 文件。
不需要恢复或重刷原系统。

## 最小开发软件与软件源

`make alpine-bootstrap` 在镜像内配置 manifest 声明的 HTTPS 软件源，安装
ca-certificates、curl、python3，并验证原生 Python、数学结果和 HTTPS 响应正文。
当前使用 Alpine 官方镜像列表中的 TUNA 镜像，只启用 v3.24/main。
本机和 Kindle 都观察到 dl-cdn 上部分大文件提前 EOF；切换镜像后安装成功。
未禁用 TLS 校验，也未使用 apk 的不信任包安装选项。

```sh
make alpine-bootstrap
python3 tools/chroot.py run -- python3 -c 'print(sum(range(101)))'
```

Alpine 基础压缩包由 SHA256 和 PGP 身份固定；附加软件来自受签名验证的发行版
软件源，revision 会随源更新，当前 bootstrap 不承诺逐字节可复现。
当前实测 Python 3.14.10、OpenSSL 3.5.9；观察结果在忽略的
`work/chroot-development.json`，完整已安装包列表在 `work/chroot-packages.txt`。
