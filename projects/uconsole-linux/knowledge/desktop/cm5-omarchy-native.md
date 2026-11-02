# CM5：在原生 Arch Base 上验证 Omarchy 核心桌面

## TL;DR

- 4GB CM5 / 官方 IO Board 已在原生 Arch Linux ARM 上运行 Hyprland、Omarchy Quickshell 和 UWSM 普通用户会话。
- 手动 tty7 / seat0 会话与 1280×720、scale 1.5、transform 0 的独立虚拟输出可用；浏览器内菜单启动 Nautilus、终端逐键输入均已实测。
- Hyprland 与 Quickshell 在实际 UI 操作期间分别完成 949、56 个新增 V3D bin/render 作业，确认两者使用 GPU。
- 本轮验证桌面核心和原生应用启动链；完整 Omarchy 系统部署、AI 应用套件、登录管理器与音频仍有独立验收范围。

## 已验证的运行形态

2026-10-30，目标为 4GB CM5 / 官方 IO Board、已启动的 Arch Base，使用项目内核
`6.18.57-v8-16k-uconsole`。桌面直接运行在该 Arch 系统中，普通账户为 `pi` / UID 1000。
`loginctl` 记录 tty7、seat0 的 user 会话；UWSM 用户管理器承载 compositor 和应用。

| 组件 | 本次版本或状态 |
| --- | --- |
| Omarchy 核心载荷 | 上游 4.0.6，本地 `omarchy-cm5-test` 包 |
| Hyprland / Aquamarine | 0.56.2 / 0.15.1 |
| Quickshell | 0.3.2.r7.g2b4501a |
| UWSM / Mesa | 0.27.1 / 26.2.6 |
| WayVNC | 0.10.2 |
| 输出 | `CM5-NATIVE`，1280×720 @ 60 Hz，scale 1.5，transform 0 |
| 手动系统单元 | `cm5io-omarchy-native.service`，active/running |
| 用户单元 | shell、WayVNC、foot 三个 `cm5io-native-*` 单元均 active/running |

实测观察：

- 浏览器查看该 Hyprland 自己的虚拟输出，Omarchy 栏、菜单、终端和文件管理器正常显示。
- 实际点击 Apps → Files 后，Nautilus 窗口出现；客户端记录为 `org.gnome.Nautilus`，可见且接受输入。
- 在终端逐键输入命令，观察到预期的 echo 标记；客户端记录为原生 Wayland foot。
- 应用菜单保留上游 `uwsm-app -- gtk-launch` 路径，补齐了[隔离试跑](cm5-omarchy-arch-trial.md)未验证的原生 UWSM 启动链。
- Quickshell 的 Qt/OpenGL 日志报告 Broadcom / V3D；按稳定 DRM client ID 去重后，
  Hyprland 的 bin/render 作业各增加 949，Quickshell 各增加 56。

这些计数是实际操作采样窗口的观察值，不能作为 FPS、功耗、长期稳定性或其他板子的阈值。
判据是两个进程都出现新的 V3D 完成作业，而不只是打开了 render node。

## 可复用的手动会话方法

使用独立 desktop entry 和 Hyprland Lua 配置，避免将完整系统安装入口混入桌面启动。
手动系统单元通过 `User=pi`、`PAMName=login`、tty7 和 logind 建立会话，
再执行 `uwsm start -- <DESKTOP_ENTRY>`。检查 seat0 会话是否 active，而不只检查进程存在。
本轮没有启用显示管理器、改变默认 target 或设置桌面开机启动。

compositor 的启动 hook 先调用 `uwsm finalize`，导出 Wayland socket、
`HYPRLAND_INSTANCE_SIGNATURE` 和图形环境，再启动属于 `graphical-session.target` 的用户单元。
这样 Omarchy 原版应用启动器可以使用用户 systemd / D-Bus 环境；停止 UWSM 会话也能停止附属应用。

无实体屏幕时，Hyprland 仍通过 DRM 获取 allocator，再创建自己的 headless 输出。
Aquamarine 的 headless backend 本身不提供 DRM fd，渲染设备和 seat 权限仍须真实可用。
不能把旧 labwc 的 `WLR_BACKENDS=headless` 环境直接当作 Hyprland 原生启动配置。

在该 compositor 的用户会话环境内，输出配置方法为：

```sh
hyprctl output create headless CM5-NATIVE
hyprctl eval 'hl.config({debug={disable_scale_checks=true}})'
hyprctl eval 'hl.monitor({output="CM5-NATIVE",mode="1280x720@60",position="0x0",scale=1.5,transform=0})'
hyprctl -j monitors all
```

仅在目标输出尚不存在时创建它。核对实际 JSON 中的 width、height、scale、transform 和 disabled；
不要只检查命令退出码。`debug.disable_scale_checks` 保留精确 1.5 缩放，
先前试验中错误的 `misc.disable_scale_checks` 被拒绝，未正确设置时实际缩放曾变为 1.6。
1280×720 的等效横屏方式保持与 uConsole 一致的像素和缩放目标，减少远程鼠标坐标转换。

WayVNC 只监听板端回环地址，通过 SSH 隧道和主机 noVNC 显示这个原生输出。
连接参数、主机身份验证材料和现场网络信息留在忽略目录，不写入知识篇。
复用旧 viewer 时先确认现有监听进程和端口，更新为本次安装已验证的 SSH 主机密钥。

## 用户主题

4.0.6 的生成主题位于 `~/.local/state/omarchy/current/theme`。使用
`OMARCHY_THEME_HEADLESS=1 omarchy-theme-set tokyo-night` 可生成用户状态并跳过
全局 post-theme hooks；Foot 的原版配置引用这个 state 路径。Shell 重启后读取新
颜色，GTK 使用普通用户 dark 偏好。旧试跑的 config/current 链接不代表生成主题。

## 输入来源与唯一兼容补丁

原始 `omarchy`、`omarchy-settings` 4.0.6 输入经过 GPG 验证。
本地测试包选取其 CLI、shell、主题、设置和字体载荷；来源、逐文件清单、签名依据和改动记录保存在构建回执。
这份本地裁剪包不等同于上游完整 Omarchy 系统配置，也不把原始输入签名描述成本地新包的签名。

唯一源码兼容补丁位于 `omarchy-version` 的包查询候选列表：保留
`omarchy-dev`、`omarchy` 的优先顺序，最后增加 `omarchy-cm5-test`。
这样在仅安装本地测试包时可以显示版本，并维持官方包存在时的原有优先级。
本地包声明 `inotify-tools` 作为文件监视依赖，并规范化文件模式；发布修订和摘要以对应回执为准。

Hyprland 使用本次独立配置，Omarchy 的 shell 用户配置选择已有主题并适配虚拟屏幕。
应用启动器沿用原文件；完整 provisioning、factory reset、系统迁移和默认 autostart 不作为本轮入口。

## 复核证据与范围

本轮原始证据位于忽略的 `image/work/cm5io-omarchy-native-20261030/`：

| 证据 | 支持的结论 |
| --- | --- |
| `native-proof.json` / `native-proof-final.json` | UID、内核与包版本、输出参数、可见客户端、会话与 renderer |
| `session-current.log` | 手动系统单元与三个用户单元当前运行状态 |
| `gpu-before.json` / `gpu-after.json` | 相同进程与 DRM client ID 的采样计数 |
| `gpu-verification.json` | 首轮 Hyprland 949、Quickshell 56 的 bin/render 差值 |
| `gpu-final-verification.json` | 主题应用后再次增长 779（Hyprland）和 48（Quickshell） |
| `native-desktop.png` / `final-menu-launch-proof.log` | 最终画面与菜单启动 Foot 的 UWSM application scope |
| 本地包 provenance / manifest / version oracle | 已验签输入、选取文件、版本 CLI 补丁及官方包优先级 |
| 浏览器实际点击与逐键输入 | 菜单启动文件管理器、真实终端输入和输出 |

欢迎辅助终端在菜单启动的终端通过输入验证后关闭；这不要求辅助终端用户服务一直 active。

图形查询与截图应在目标用户会话环境中执行，例如 `hyprctl -j clients`、
`hyprctl systeminfo` 和 `grim -o CM5-NATIVE <OUTPUT>.png`。
GPU 验证复用[无屏桌面记录](cm5-headless-desktop.md)的按进程、稳定 client ID、前后差值方法；
不能用 UI 截图、renderer 名称或单独的 DRM fd 替代实际 GPU 作业证据。

本轮只在已运行的 Arch 上安装和测试运行层，没有重新构建镜像、刷写或重启板子。
既有 USB 暖重启失联问题由[USB 调试组合记录](../usb/cm5-io-board-usb-debug.md)单独跟踪，
本次桌面功能结果不建立对该问题的解释或恢复保证。

尚未建立完整 Omarchy 的系统 provisioning、SDDM/自动登录、AI 应用套件、
音频输入输出、锁屏认证、升级/快照恢复、实体显示与长期运行验收。
虚拟屏幕上的 V3D 成功也不能替代实体显示链、视频硬件解码或散热/供电稳定性验证。

## 在仓库中复用

核心包配方（工程源码：`image/packages/omarchy/README.md`）固定上游 4.0.6 的两个包、
签名、公钥、MIT 许可证和唯一版本识别补丁。`make image-omarchy-package` 只离线
重打包脚本、QML 和字体；不要求 ARM 编译器，也不连接或更改设备。
导入验证器同时检查当前构建器、配方、补丁、输入身份和包文件清单，避免将旧实验产物
当作当前工具的输出。原始包签名证明输入来源；本地重打包的输出另行签名。

手动会话生成器（工程源码：`image/desktops/omarchy/README.md`）独立处理用户、UID、home
和载荷清单。默认只预览，显式应用也不会启用或启动服务；主题仍采用上述 state 路径
和 headless 选项。应用文件、生成用户主题和启动会话是分别执行的步骤。

本地包保留部分安装、更新和迁移脚本作为数据，完整入口并未适配；不能据此执行全量
安装器或自动更新。底层内核、GPU、PAM/logind、应用依赖和 USB 恢复也不属于数据包
构建的验证结果。普通 Arch Base 镜像仍不自动安装这个桌面层。

## 源码与关联资料

- [Omarchy 4.0.6 固定源码](https://github.com/omacom/omarchy/tree/237f1b4071a0bff2296c6d740ba2b9fd695ebfa0)：shell、CLI 与应用启动路径。
- [UWSM 文档](https://github.com/Vladimir-csp/uwsm)：desktop entry、登录生命周期、finalize 和用户应用服务。
- [Hyprland 0.56.2 Compositor.cpp](https://github.com/hyprwm/Hyprland/blob/v0.56.2/src/Compositor.cpp)：headless、DRM 和 Wayland backend 选择。
- [Aquamarine 0.15.1 Headless.cpp](https://github.com/hyprwm/aquamarine/blob/v0.15.1/src/backend/Headless.cpp)：headless backend 的 DRM fd 行为。
- [Arch Base 镜像方法](../build/arch-arm-base-image.md)：rootfs/内核/镜像与设备部署边界。
