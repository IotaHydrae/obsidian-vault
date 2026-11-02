# CM5：在独立 Arch ARM 用户空间试跑 Omarchy

## TL;DR

- CM5 已在隔离 Arch Linux ARM 目录中运行 Hyprland＋Omarchy Quickshell 外壳，菜单、输入和基本应用启动可用。
- 复用现有 Ubuntu 的内核与 V3D 设备；这不是 Arch 原生启动，也没有生成磁盘镜像或刷写。
- 首轮嵌套 Wayland 输出被宿主拒绝，改用 Hyprland 独立 headless 输出和第二条 USB SSH/VNC 链路。
- 仅验证桌面核心；应用启动做了测试适配，UWSM、系统服务、音频、认证和恢复链留给原生 Arch Base。

## 试验结构与可信输入

宿主为官方 CM5 IO Board 上的 4GB CM5、Ubuntu Base 24.04.5、项目内核
`6.18.57-v8-16k-uconsole`。已有 labwc/Noctalia 服务保留。
Arch rootfs 解包到 `/var/lib/machines/cm5io-omarchy-test`，使用不带 `--boot` 的 systemd-nspawn。
不绑定宿主 `/boot`、`/etc`、system bus；只传入 GPU render node、用于初始嵌套的
Wayland socket 和本次拥有的 runtime。容器 UID 1000 对应宿主普通用户，默认口令已锁定。

| 输入/组件 | 本次记录 |
| --- | --- |
| Arch Linux ARM | Generic AArch64 tarball，由 TUNA 镜像取得，829,367,415 字节 |
| 基础归档传输校验 | mirror `.md5` 为 `23eec86365b24f7913c403e8f4e8719b`；非包签名 |
| 软件包仓库 | Arch Linux ARM 签名仓库，加优先的 Omarchy stable/aarch64 |
| Omarchy 源码载荷 | `omarchy` / `omarchy-settings` 4.0.6-1，源码标签提交 `237f1b4071a0bff2296c6d740ba2b9fd695ebfa0` |
| Hyprland | 官方 Omarchy 包 `0.56.2-7` |
| Quickshell | 官方 Omarchy 包 `0.3.2.r7.g2b4501a-1` |
| Mesa / Qt | Arch ARM `26.2.6` / `6.12.0` |

仓库和包的版本闭包必须配套；不能因主仓库版本更高就替换 Omarchy 的 compositor/runtime。
Omarchy 签名公钥指纹来自固定源码：`40DFB630FF42BCFFB047046CF0134EE680CAC571`。
官方源码载荷 SHA-256 和独立签名均核验；仅提取脚本、QML 和配置，不运行系统 provisioning。
Arch 内部完成一致的 `pacman -Syu` 安装，包签名检查保持开启。
基础包内即使存在内核/固件，也只写该目录内的 boot 路径；实际运行内核一直是宿主项目版本。

## 两个环境差异的处理

### 精简内核与下载环境

项目内核不支持本次 pacman 下载沙箱所需的 Landlock，报：
`restricting filesystem access failed because Landlock is not supported by the kernel`。
只在这个隔离 rootfs 的 pacman 调用中使用 `--disable-sandbox`；没有关闭包签名检查。
rootfs 自带 resolv.conf 指向未运行的 resolved runtime，因此复制宿主实际 resolver 到容器内普通文件。

上游大包发生 TLS 提前断流时，按 HTTP Range 续取，核对 Content-Range、最终长度、SHA-256 和签名；
不能将未完成的 `.tar.gz` 当源码树使用。原始归档与取得的签名/metadata 均留在忽略目录。

### 非原生图形登录与显示链

Aquamarine 0.15.1 的嵌套 Wayland 首帧在 labwc 0.20.2 被拒绝：
`xdg_surface has never been configured`。改 preferred mode 未解决。
源码回调顺序提示配置确认与首帧提交可能有关，但本轮没有修补或完成根因隔离。
Hyprland 内部输出/客户端仍可查询；随后创建独立 headless 输出，禁用失败的 WAYLAND-1。

当前实际输出是 **1280×720、normal、scale 1.5**，与 uConsole 横屏像素/缩放等效。
试过 720×1280＋transform 3 时，当前 Hyprland/WayVNC 组合鼠标坐标错位，因此保留等效横屏方式。
labwc/Noctalia 原有输出仍为 720×1280＋270°＋1.5，没有改变。

Hyprland Lua parser 不能使用旧 `hyprctl keyword`；它可能退出 0 却返回拒绝文字，需检查正文。
运行时配置使用 `hyprctl eval 'hl.monitor({...})'`。
本轮需要 `hl.config({debug={disable_scale_checks=true}})` 才保留精确 1.5，否则回退到 1.6。

## 核心外壳与应用启动适配

不运行 Omarchy 的 Arch 系统安装、磁盘、引导或首次 provisioning。
试验用户复制独立 shell.json、选择已有主题，关闭空闲锁屏/屏保，菜单只列已安装的应用。
首次直接运行 Quickshell；官方 launcher 使用 systemd-cat，而容器没有独立 journald。
保留 private session bus 与正确 HYPRLAND_INSTANCE_SIGNATURE，不导入宿主总线。

菜单原通过 `uwsm-app -- gtk-launch` 启动应用，本轮没有完整 UWSM 用户会话，原调用未启动 Foot。
仅在隔离源码的 `shell/services/AppLibrary.qml` 中，改用
`gio launch /usr/share/applications/<id>.desktop`；原文件已备份。
这验证真实应用启动，不验证原版 UWSM/service scope 语义。后续 Arch Base 应恢复并验证原生登录链。
源码调整后正常重启 Quickshell，关闭文件监视/重载弹窗，避免同时混用两代 QML 引擎状态。

## 实测证据（2026-10-29）

- 浏览器第二个入口显示 Omarchy 菜单、时钟、工作区和终端；实际按键输出 `arch-omarchy-ok`。
- 菜单 Apps → Foot 启动新终端并自动平铺，新窗口内实际键盘输入输出 `menu-launch-ok`。
- Qt RHI 日志：Broadcom / `V3D 7.1.10.2` / OpenGL 3.1 / Mesa 26.2.6。
- 同一采样窗口按 drm-client-id 去重，Hyprland bin/render 完成任务各增加 221，Quickshell 各增加 9。
  这确认两者硬件渲染；不是 FPS 或稳定性测量。
- 51.6°C 时保护标志为历史 `0x50000`；内核记录过数次欠压并在约两秒后恢复。
  当前低位为零不抹去供电不足的历史，不能将此试跑作为供电或超频稳定性结论。

原始包闭包、签名、部署脚本、GPU/客户端 JSON 与截图在忽略的
`image/work/cm5io-omarchy-20261029/`。
主服务 `cm5io-omarchy-test.service` 是静态手动单元；VNC/外壳是本次附属临时单元，未设开机启动。
停止主单元会结束附属进程；当前重启后需要重新准备虚拟输出和查看入口，不是产品化启动接口。

## 后续 Arch Base 的边界

本轮支持继续设计 Arch Base rootfs provider，并复用工程的板型、内核、启动后端和文件导出边界。
下一阶段先验证 minimal rootfs 的账号/SSH/网络/首次扩容/原生启动，再接可选 Omarchy 桌面。
Arch 的 pacman、mkinitcpio 和系统服务需要独立集成；不能直接复用 Ubuntu 的 APT/postinst 流程。
完整 Omarchy 的 Btrfs/快照/更新恢复、声音、认证、锁屏、实体显示、长期运行均未验证。
没有据此放宽现有 CM5IO 只允许无桌面的构建配置，也没有生成或刷写 Arch 镜像。

## 源码依据

- [Omarchy 固定源码](https://github.com/omacom/omarchy/tree/237f1b4071a0bff2296c6d740ba2b9fd695ebfa0)：MIT，shell/README、Lua 配置、安装包清单与签名公钥来源。
- [Arch Linux ARM Generic AArch64](https://archlinuxarm.org/platforms/armv8/generic)：用户空间输入；不套用平台刷写步骤。
- [既有 CM5 无屏验证](cm5-headless-desktop.md)：与本轮保留的 Ubuntu/Noctalia 环境及 V3D 判据关联。
