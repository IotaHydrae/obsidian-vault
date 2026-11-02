# 手机 Linux 桌面：选择原生 Termux 或 PRoot，并验证 VNC 会话

> 先按软件来源和内核兼容性选择运行环境，再独立验证桌面显示、中文输入、剪贴板与会话清理；能投屏不代表 Linux 桌面体验相同。

## TL;DR

- 原生 Termux 运行其仓库适配的软件；PRoot 提供另一套发行版用户空间，但仍共享 Android 内核。
- VNC 可提供独立 Linux 桌面，Termux:X11 可在 Android 显示桌面，两者与 PRoot 的选择相互独立。
- 关闭查看器通常只断开显示；停止桌面必须验证相关进程与会话资源退出。

## 按需求选环境

| 方案 | 适合 | 限制 |
| --- | --- | --- |
| 原生 Termux + XFCE | Termux 仓库已有所需软件，追求较少层次 | 不能直接安装任意 Debian 软件包 |
| Termux + PRoot 发行版 + XFCE | 需要 Debian／Ubuntu 用户空间及其软件包 | 系统调用转换有成本，受 Android 内核兼容性限制 |
| Root chroot | 明确需要相应权限和原生系统调用语义 | 要额外处理挂载、设备访问和生命周期，仍共享宿主内核 |

PRoot 通过用户态路径／系统调用转换提供类似 chroot 的环境；来宾显示为 root 不等于获得 Android 系统 Root，也不会自动提供完整 systemd、Docker、独立内核或 x86 软件兼容。

2026-10-30 的 OnePlus 7 Pro 案例最终采用原生 Termux XFCE + TigerVNC；早期 Debian PRoot 方案未实施。相关内核的兼容问题报告只用于评估风险，没有在该机复现，因此不把报告写成该设备已发生的故障。

## 显示协议与运行环境分开选

- **VNC**：电脑查看器连接手机上的桌面会话；可以不依赖手机当前 Android 画面。
- **Termux:X11**：把 Linux 图形输出显示在 Android 上，可再配合 scrcpy；也可以用于 PRoot 环境。
- **scrcpy**：传输 Android 显示内容，不能自动代替 VNC 的独立桌面和输入机制。

USB VNC 链路示例，前提是手机已经启动经认证、仅回环监听的 VNC 服务：

```text
手机 Termux：XFCE → TigerVNC 127.0.0.1:5901
电脑查看器：127.0.0.1:25901 → ADB 转发 → 手机 5901
```

电脑执行：

```sh
adb forward tcp:25901 tcp:5901
vncviewer 127.0.0.1::25901
```

第二条使用 TigerVNC 查看器的显式 TCP 端口写法。5901 和 25901 都是示例，应按实际服务配置；多设备时显式指定 ADB 目标。转发不提供 VNC 认证，需分别确认手机监听地址、电脑监听地址和认证方式。

## 最小验收清单

| 功能 | 通过标准 |
| --- | --- |
| 桌面与键鼠 | 完整桌面显示，点击与终端交互正确 |
| 中文输入 | 远端输入法能出现候选并提交文字，快捷键无冲突 |
| 中文剪贴板 | 电脑→远端、远端→电脑分别核对内容 |
| 浏览器 | 页面实际渲染、脚本与目标 HTTPS 访问通过 |
| 断开重连 | 关闭查看器后再次连接，原会话仍在 |
| 停止清理 | 保存文件后停止会话，VNC、XFCE、输入法与私有 D-Bus 会话按设计退出 |
| 既有环境 | SSH、CLI 工具和已有数据仍可使用 |

不能只凭 VNC 服务端口打开或窗口出现就宣布桌面可用。远端输入法快捷键可能被电脑输入法先截获，须实际提交中文验证；双向剪贴板也要分别测。

## 资源与生命周期

“当前可用内存”不是桌面净开销。比较同任务前后的进程归属与 [内存压力](android-memory-pressure.md)，并单独评估长期后台、浏览器负载、音视频和待机恢复。

关闭窗口、断开 USB、移除 ADB 转发、停止桌面是不同动作。会话管理应只清理自己启动的进程与转发，避免全局杀死 Termux 或其他 SSH／Agent 会话；停止前保存桌面内文件。

`phone-desktop`、`phone-desktop-session` 是个人脚本入口，不是标准软件包命令。通用部署应记录其依赖和生命周期，认证文件按密码保管，不复制进笔记。

## 参考

- [Termux 软件包](https://github.com/termux/termux-packages)
- [PRoot Distro：实现及限制](https://github.com/termux/proot-distro)
- [TigerVNC：Xvnc 参数](https://tigervnc.org/doc/Xvnc.html)、[查看器端口写法](https://tigervnc.org/doc/vncviewer.html)
- [PRoot 兼容性问题 #417](https://github.com/termux/proot/issues/417)；问题报告不是本机复现证明。
