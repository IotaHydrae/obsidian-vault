# CM5：不接屏幕验证 labwc 桌面

## TL;DR

- CM5 IO Board 可用 labwc headless 输出运行桌面，经 USB SSH 隧道和 WayVNC 在电脑查看、操作。
- 虚拟输出与渲染器是不同层；当前预编译包的 GLES2 渲染器可以使用 CM5 的 V3D render node。
- 用受控画面、真实键鼠、编辑保存和 GPU 任务证据验证；不以服务 active 代替桌面功能。
- 当前用户选择模拟 uConsole：720×1280、transform 270、scale 1.5；本文的正常方向步骤保留为最初对照。
- 本文是已安装系统的手动测试会话，不改变 CM5IO 镜像只提供无桌面配置的限制。

## 运行方式

前提是 CM5IO 已运行 Ubuntu Base，普通用户有 render/video 组权限，USB 固定地址可访问。
复用固定 Wayland 软件包（工程源码：`image/packages/wayland-stack/README.md`），由目标 APT 安装其运行依赖。
另需 Mesa DRI 驱动，以及 foot、grim、wlr-randr、WayVNC；窗口/面板验证使用 Waybar、Thunar、Mousepad。
本次直接安装到板上，不需要生成镜像或重编内核。

使用独立配置目录和普通用户会话，核心环境为：

```sh
export XDG_RUNTIME_DIR=<OWNED_RUNTIME_DIRECTORY>
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1
export WLR_RENDERER=gles2
export WLR_RENDER_DRM_DEVICE=/dev/dri/renderD128
dbus-run-session -- /usr/lib/uconsole/wayland-stack/bin/labwc -C <TEST_CONFIG_DIRECTORY>
```

运行目录须由该用户独占、权限 0700；render node 先按实机核对，不能跨硬件固定套用。
在 compositor 的 autostart 中设置虚拟显示并启动客户端：

```sh
wlr-randr --output HEADLESS-1 --custom-mode 1280x720@60Hz --scale 1 --transform normal
```

测试配置采用正常方向与 scale 1，不继承 uConsole 的 270°/1.5 缩放及电池/背光模块。
当前包的配方只选择 GLES2；不要假定设置 `WLR_RENDERER=pixman` 即可使用软件渲染。
包内确有 headless 后端；可由运行日志确认其创建及实际输出。

WayVNC 在板端仅监听 `127.0.0.1:5900`，通过已验证的 USB SSH 建立转发：

```sh
ssh -N -L 127.0.0.1:15900:127.0.0.1:5900 <USER>@<HOST_01>
```

电脑可用 VNC 客户端连接本地 15900，或以本地 noVNC/websockify 提供浏览器界面。
浏览器代理同样只绑定回环地址；不需要向 Wi-Fi/LAN 开放 VNC 端口。

## 功能判据

| 验证层 | 证据与判据 |
| --- | --- |
| 虚拟显示 | wlr-randr 报告输出启用、目标分辨率、正常方向和 scale 1 |
| 实际画面 | grim/VNC 图像包含受控窗口及已知内容，尺寸与输出一致 |
| 远程键鼠 | 经 VNC 输入命令并看见结果；拖动窗口后位置改变 |
| 编辑保存 | 在 GUI 中输入、保存指定内容，再经独立 SSH 读取对应文件核对 |
| GPU 渲染 | 日志显示实际 GL renderer；同一 compositor 的 v3d DRM client 任务数有提交 |

`/proc/<PID>/fdinfo/*` 可能重复引用同一 DRM client；按 `drm-client-id` 去重，避免累加同一计数。
任务计数用于确认渲染路径，不自动形成 FPS、流畅度或稳定性合格线。
截图门户、PipeWire 共享是另一路径，见[截图依赖](screenshot-portal.md)与[真实取帧验证](screencast-validation.md)。

## CM5IO 实测观察（2026-10-29）

4GB CM5、官方 IO Board、Ubuntu Base 24.04.5、`6.18.57-v8-16k-uconsole`，无连接的 HDMI 显示器：

- 安装固定 labwc/wlroots 0.20.2、Waybar 0.15.0，目标系统使用 Mesa 25.2.8。
- `HEADLESS-1` 为 1280×720、60 Hz、normal、scale 1；两个 HDMI connector 均为 disconnected。
- EGL/GL 日志显示 Mesa / Broadcom / `V3D 7.1.10.2`；同一 client 的 bin/render 任务由 123 增至 212。
- 浏览器 VNC 显示面板和终端；真实按键执行 `echo cm5-input-ok`，鼠标拖动 Thunar 窗口可见位置变化。
- Mousepad 输入并保存 `cm5-headless-edit-ok` 加换行；独立 SSH 按完整内容断言成功。
- grim 截图为 1280×720，含同一次编辑窗口和文件管理器；dpkg audit 无输出。
- 收尾温度 37.3°C、保护标志 0；只是功能测试期间的观察，没有长时间压力结论。

进一步按用户要求确认 GPU 加速：`eglinfo -B -p wayland` 的实际 Wayland OpenGL / OpenGL ES
上下文均为 Broadcom / `V3D 7.1.10.2`，版本为 OpenGL 3.1、OpenGL ES 3.1。
普通用户 `es2gears_wayland` 的独立 V3D client 在 3 秒采样中完成 bin/render 各 184 个任务，
同一 labwc client 各完成 368 个；按 client-id 去重，未累加重复 FD。
`gpu-proof.png` 可见同一次齿轮窗口，任务统计来自该应用与合成器的进程；
`gpu-confirm-final.json` 保留完整上下文、前后值及判据，测试进程随后停止。
这确认受测 GLES 应用与当前桌面合成器的硬件渲染，不把任务数换算成显示 FPS。
统计含义见固定内核 `drivers/gpu/drm/v3d/v3d_drv.c` 的 `v3d_show_fdinfo()`：
`v3d-jobs-*` 计完成任务；硬件完成中断经 `v3d_job_update_stats()` 更新统计。

用户随后要求与 uConsole 一致，已将同一虚拟输出改为 720×1280、transform 270、scale 1.5，
并写入 autostart；这与 Ubuntu 桌面后端既有显示计划相同。横屏像素仍为 1280×720，
逻辑空间约 853×480。面板任务列表改用图标以适应小屏宽度；编辑器最大化后可完整显示。
`uconsole-output.log` 与 `uconsole-layout.png` 记录更改后的实际输出和画面。

板上 `cm5io-headless.service` 是手动启动的静态服务，运行目录为 `/run/cm5io-headless`。
停止和再次启动可使用：

```sh
sudo systemctl stop cm5io-headless.service
sudo systemctl start cm5io-headless.service
```

客户端测试单元随主会话停止，系统默认启动目标仍为 multi-user.target。
主机隧道和浏览器代理是本次测试进程；USB 断连、重启后需重新建立连接。
原始部署脚本、截图、GPU/输出记录及编辑结果在忽略的 `image/work/cm5io-headless-20261029/`。

## CM5IO Noctalia 试用（2026-10-29）

用户随后要求试用猫头鹰桌面，安装仓库校验过的 `uconsole-noctalia` 5.3.0+uconsole1，
替换本次测试会话的 Waybar/Mako；保留 labwc、虚拟输出、USB SSH 和 WayVNC。
配置独立放在测试目录的 `noctalia/config.toml`，通过 `NOCTALIA_CONFIG_HOME` 选择；
移除无关 DSI 背光/电池小部件，试用时关闭自动锁屏及输出熄灭。
原 autostart/rc.xml 先备份；修改后的手动会话再次启动时仍启动 Noctalia，不设开机自启。

- 实际完成首次启动、应用菜单、控制中心、从菜单启动 Foot 及键盘命令输入。
- Noctalia 自身报告 Broadcom / `V3D 7.1.10.2` / OpenGL ES 3.1；菜单操作间其独立 V3D
  client 的 bin/render 完成任务各增加 247，不能由此推导 FPS 或长期流畅度。
- 安装用户 PipeWire/Pulse/WirePlumber 与所需门户依赖；Noctalia 通过明确的 runtime/socket
  连接现有用户音频服务。当前默认 sink 是 `auto_null`，不作为可听声音或硬件音频验证。
- 软件包校验和 dpkg audit 无异常。软件包和运行日志在忽略的 `image/work/cm5io-noctalia-20261029/`。

当前系统服务形式的测试会话不属于 logind 图形登录，Noctalia 报 `NoSessionForPID`，
图形 polkit agent 注册失败；图形提权和会话锁定链尚未打通，不能扩展为完整桌面验证。
Monitor 页显示缩放 176%，而 wlr-randr 和 Noctalia 的输出初始化日志分别为 1.5 / 1.500，
逻辑区域为 853×480；该 UI 读数差异的原因尚未隔离，没有据此改变实际输出。
本轮没有生成或刷写镜像，也没有更改 CM5IO 构建配置档。

## 范围与未知项

已验证的是该 CM5 的基础 Wayland 桌面、远程画面/输入、文件编辑及 V3D 渲染。
Noctalia 的基础菜单/应用功能见上节；硬件音频、剪贴板、门户、锁屏、长期稳定性和自动登录链仍未验证。
虚拟输出未覆盖 HDMI/KMS 实体扫描输出、显示器 EDID、背光、触摸或屏幕唤醒。
现有镜像未重建、未刷写；没有将这套测试会话加入镜像默认配置。
