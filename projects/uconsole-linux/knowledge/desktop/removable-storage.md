# 精简桌面的回收站与卷挂载

> Thunar 之外还需 GVfs、UDisks2 与用户目录初始化，才能补齐普通用户存储功能。

## TL;DR

- 两套桌面显式安装 `gvfs`、`udisks2`、`xdg-user-dirs`，不依赖完整桌面元包。
- 从实际图形会话验证挂载授权；SSH 补齐 GUI 环境不等于活动会话。
- 往返校验文件内容并清理自有卷；当前虚拟卷验证不能代替 USB 热插拔和供电测试。

## 方法与原因：保留普通用户的卷管理权限

- `gvfs` 提供用户 D-Bus 激活的 GIO 后端。当前包含本地回收站和卷监视，不安装
  `gvfs-backends` 的网络、手机等额外协议。
- `udisks2` 提供系统 D-Bus 卷管理，沿用发行版 polkit 策略；不通过 root 文件管理器、
  通用 sudo 放权或修改策略文件来绕过普通用户权限。
- 会话启动器以登录用户运行 `xdg-user-dirs-update`，按 locale 初始化目录并保留已有映射，
  不使用 `--force`。不新增自动挂载或自动运行策略。

实现入口：桌面配方（工程源码：`image/desktops/recipe.json`）、
会话客户端启动器（工程源码：`image/desktops/payload/usr/libexec/uconsole-desktop-clients`）。
服务存在或软件包安装成功只证明接口准备好。先用隔离 HOME 验证目录策略，再从真实图形会话验证
回收站/受控卷的字节往返；物理热插拔留作独立硬件观察。

## CM4 字节往返与会话证据（2026-10-24）

环境为当前 SD 卡的 Ubuntu Base 24.04.5 ARM64、labwc + Noctalia 普通用户会话，
不是新导出镜像的首次启动。安装前 GVfs、UDisks2 和 XDG 目录工具均缺失；补装使用
`--no-install-recommends`，不升级其他软件包。实际版本为 GVfs `1.54.4-0ubuntu1~24.04.4`、
UDisks2 `2.10.1-6ubuntu1.5`、xdg-user-dirs `0.18-1build1`。

**回收站判据：** 普通用户创建具有唯一名称和已知字节的临时文件，移入 `trash:///` 后原文件
消失；根据原始路径找到对应条目，恢复至原位置，内容逐字节一致。本轮 UID 1000 完成往返，
仅清理自己创建的测试文件，没有清空用户回收站。

**用户目录判据：** 在隔离 HOME 中运行发行版更新工具，标准目录可生成；已有自定义目录映射
保持字节不变。本轮两者均通过。实际 pi 用户原先没有映射文件，正常更新后创建八个标准目录；
会话启动器已部署，原文件备份在设备 `/root/uconsole-desktop-storage-20261024/`，
没有为部署重启桌面或设备。

**卷管理判据：** 来自真实图形会话的 UID 1000 客户端可发现受控卷，经 UDisks 挂载后写入并读回
已知 UTF-8 字节，随后卸载并释放 loop。本轮使用 32 MiB 普通文件生成 ext4 卷，
客户端属于 `session-44.scope`，GIO 卷监视器识别到卷标签，`udisksctl --no-user-interaction`
完成挂载/卸载。字节往返一致，loop 删除后无测试挂载点或设备残留，没有操作实体磁盘。
临时终端已退出，测试前后的 `LockedHint` 均为 yes；300 秒锁屏策略保持原样。

本机非发布证据位于 `~/.cache/uconsole-desktop-20261024/storage/`，
包含 `loop-proof.json`、`loop-probe.py` 和 `xdg-user-proof.json`。

## 复用方法与未验证范围

从 SSH 复制 `WAYLAND_DISPLAY` 和 D-Bus 地址，并不会让调用进程加入图形 logind 会话。
卷权限检查应从真实图形会话启动普通用户客户端；SSH 的授权拒绝不能直接等同于桌面挂载失败。

文件支持的 loop 卷可以验证格式识别、UDisks 授权、挂载、读写与卸载链路，但不能替代真实 USB
设备的枚举、热插拔、供电、传输或安全拔出测试。没有外设时，保留这个验证边界。
新导出镜像、Waybar 实际会话和其他核心板的硬件结果也应分别记录。
