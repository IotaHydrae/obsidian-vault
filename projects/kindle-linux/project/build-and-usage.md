# kindle-linux

面向 Kindle 青春版 2019（KT4 / 第十代）的 Alpine ARMv7 开发工程。
**USB 自动启动试验已回滚：启用标记已停用，残余启动钩子已移除，USB 存储卷已安全卸载。自动启用命令保持禁用。**
保留 Amazon 启动链、签名内核与系统根分区，从用户存储区的独立文件系统镜像手动运行 chroot。

已在 KT4 上安装并验证 Alpine 3.24.3 ARMv7 的手动 chroot，使用 512 MiB 独立镜像。
**保留原系统和安全启动；没有执行整机备份、刷写或恢复模式切换。**
工程组织参考 `uconsole-linux`：人类 Makefile、机器状态 Makefile.agent、板型声明、固定输入和分层验证。
应用开发见 [墨水屏与第三方应用](../knowledge/apps.md)，使用方法见 [手动 chroot](../knowledge/chroot.md)，恢复边界见 [刷写与恢复](../knowledge/flashing.md)，实机状态见 [状态](status.md)。

## 主机命令

需要 Python 3.11+、GNU make、OpenSSH；验证 Alpine 签名另需 GnuPG。

```sh
./build.sh
make check
make -f Makefile.agent handoff
make rootfs-fetch
make rootfs-check
make alpine-shell
make alpine-check
```

默认入口仅显示帮助；`make check` 和 handoff 全程离线、不连接 Kindle。
`rootfs-fetch` 联网获取固定的官方压缩包及签名，`rootfs-check` 检查本机缓存。
`rootfs-fetch` 和 `rootfs-check` 不部署到设备。`alpine-install`、`alpine-shell`、
`alpine-bootstrap` 和 `alpine-check` 是显式设备操作，用于安装、手动使用、最小软件配置及实机验证；不会被默认构建或离线检查调用。

## 只读连接设备

在 KOReader 开启 SSH，先通过 OpenSSH 建立并核验主机密钥，再设置本机配置：

```sh
cp .device.toml.example .device.toml
# 修改 host，KOReader 默认端口为 2222。
make probe
python3 tools/device.py probe --output work/device-probe.json
```

配置、原始探测结果、下载缓存和备份均被 Git 忽略。不要把序列号、设备地址或备份提交到仓库。
已有设备配置会保持不变。探测仅报告事实，不证明可安全恢复或可启动第三方 rootfs。

## 工程布局

| 路径 | 内容 |
| --- | --- |
| `board/kindle-kt4/` | 板型与保留厂商系统的策略 |
| `manifest/` | Alpine 输入、签名公钥与研究源码固定信息 |
| `tools/project.py` | 离线检查与 JSON 接手接口 |
| `tools/rootfs.py` | 获取和验证发行版输入 |
| `tools/device.py` | 显式执行的只读设备探测 |
| `tools/chroot.py`、`tools/chroot_check.py` | 安装、手动运行及独立实机验证 |
| `image/alpine.sh` | 私有挂载、最小设备目录和退出清理 |
| `apps/`、`image/display.sh`、`tools/display.py` | 中文显示与 KOReader 触摸插件示例 |
| `docs/` | 架构、状态、恢复调查和接口契约 |
| `tests/` | 主机侧离线契约测试 |
| `.cache/`、`work/` | 忽略的下载和本机工作数据 |

chroot 使用私有挂载命名空间、loop 占用检查和退出清理。
进入 USB 存储模式前先退出 chroot；Alpine 仍为手动运行，不接管 Kindle PID 1。
USB 网络自动启动独立于 Alpine，配置及验证范围见 [USB 开发](../knowledge/usb.md)。

## 在屏幕上显示自己的程序

```sh
make display-demo  # 更新并显示 Alpine Python 生成的中文页面
make app-install   # 安装 KOReader 触摸插件样例；重新打开阅读器后加载
```

显示示例已在实机读回截图核对。当前插件已通过 USB 存储更新并完成文件校验；用户已实际打开面板并导出诊断，其余交互待验证。
详见 [应用开发](../knowledge/apps.md)。这些显式设备命令不会由 `make check` 自动调用。

## 从 KOReader 插件开始开发

起点是 [kindledev.koplugin](../reference/kindledev-plugin-readme.md)：中文按钮、保存计数、设备信息和返回。

```sh
make app-check-local  # 无需设备，测试纯 Lua 状态与回调
make app-install      # SSH 上传校验完整代码，保留旧版
make app-check        # 设备 Lua 执行同一组行为检查
make app-preview      # KOReader 未运行时启动真实界面预览
```

详见 [插件开发流程](../knowledge/koreader-plugin.md)。当前源码已通过 USB 存储部署，离线状态可用 `make app-status` 查看；真实界面已通过 USB 启动并读回截图核对。

## 不依赖热点的 USB 开发

```sh
make app-install-usb  # 已挂载的 Kindle 用户区中更新插件
make usb-probe        # 检查是否出现 Kindle USB 网卡
make usb-configure    # 网卡出现后配置工程自有的主机固定地址
```

USB 文件更新和手动 USB SSH 均已有验证记录，本机连接配置使用固定 USB 地址，
完整步骤与状态见 [USB 开发](../knowledge/usb.md)。

插件 0.2.0 已通过 USB 更新，增加“USB 有线连接”和“导出 USB 诊断”。
原生 ;un 与插拔没有生效；新启动器需要先拔线，再从 KOReader 插件手动启用，
等待就绪提示后接线。0.2.1 已完成实机手动启用和 SSH 验证。
`make usb-autostart` 已暂停并拒绝执行。恢复步骤和状态见 [USB 开发](../knowledge/usb.md)。

本机已准备 0.2.1，修正旧 KDB 线缆路径缺失时的兼容检测，并保留未知即拒绝的检查。
52 项本机测试通过；这些测试不代表设备自动启动成功。此前的 0.2.1 手动 USB 连接曾通过 root SSH、插件行为检查、Alpine 和真实界面预览验证；设备端启动入口现已清理并通过主机读回核验。

## 手动 USB 连接

```sh
ssh -p 2223 root@<HOST_02>
make app-check
make alpine-shell
```

手动启用网络后，本机地址为 <HOST_01>，Kindle 地址为 <HOST_02>。
上述 SSH 命令仅适用于已启用的手动网络会话。USB 自动启动已经回滚，
接线使用普通存储模式；未配置 USB 共享上网。
