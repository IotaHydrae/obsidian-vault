# Ubuntu CM4 的 labwc 外壳对比边界

> Waybar 保持默认，Noctalia 作为可选外壳；功能、资源、启动和实体画面应分别设定判据。

## TL;DR

- CM4 两套外壳共用 transform 270/scale 1.5，Noctalia 优化后菜单获用户确认明显流畅。
- deb 按固定配方和回执复用；私有依赖不能污染普通应用，硬件解码与 GPU 适配另验。
- 配对空闲 PSS 约为 Waybar 137.7 MiB、Noctalia 187.1 MiB，音频另计，不是内存上限。
- 普通用户门户、认证、音频和清理已有证据；冷启动、完整外设、CM5/其他 GPU 不据此视为通过。

## 比较方法

分别定义功能往返、短时资源与启动边界；每轮固定软件、输出、用户服务和统计进程范围。
功能结果以表中实际输入/输出为判据，资源只报告观察值；冷启动与实体画面单独取证。
先核对包回执和运行映射，再比较外壳，避免混入 ABI、私有库或统计范围变化。

## 固定条件与构建边界

Ubuntu Base 24.04.5 ARM64、同一 CM4、同一 labwc/wlroots 0.20.2 与发行版 Mesa；
两种外壳共用普通用户、输出 transform 270 和 scale 1.5。用户已确认横屏方向、轨迹球、
Ctrl+Alt+T 终端快捷键及 150% 文字大小合适。保留控制台 `fbcon=rotate:1`；两种旋转
接口不能按数字相等推断。Kanshi 与安装回执持久保存图形输出设置。

Waybar 为 0.15.0；Noctalia 为原生 C++23 的 5.3.0，使用 Noble 仓库 GCC 14。
固定 URL、哈希和构建选项见Wayland 配方（工程源码：`image/packages/wayland-stack/recipe.json`）
与 Noctalia 配方（工程源码：`image/packages/noctalia/recipe.json`）。私有依赖不替换系统
Mesa、Wayland、PipeWire 或正在运行的 WirePlumber 服务。

Noctalia 源码归档固定值未改变，两个渲染改动由配方中的独立补丁携带。最终增量打包
复用已有对象，耗时 66.027 秒，无源码重编译。已安装的 14,639,404 字节 `.deb` 的
SHA-256 为 `b170d9f487b21e2fabacc39df8915531f54591e87055a43853603b3b934c9e75`；
包回执、本机缓存校验及设备 dpkg 校验通过。固定环境下的镜像构建直接复用包和回执，
不需要每次编译；复用条件见[构建知识](../build/userspace-package-reuse.md)。

## 已观察到的功能

Waybar 的 V3D 提交、实体输入、门户选择与 Noctalia 的 EGL、PAM 锁屏往返有 CM4 证据。
背光范围 0–9 时使用 `ceil(max/10)` 整数步进，不能用会取整为零的百分比增量。
Noctalia 首次配置启用 polkit，显式绑定 DSI-1/2 背光；已有用户 TOML/GUI 状态保留。
电量另依赖 UPower D-Bus，不能仅靠 ELF 推导。具体功能判据见后文。

## 菜单掉帧、优化与判据

固定配置加两项矩形补丁后，配对 GPU render 时间/提交从约 22 ms 到约 14.8 ms，
不能当作屏幕 FPS。像素相等与实体交互各有判据，详见[渲染优化](noctalia-rendering.md)。

### 锁屏与解锁的独立过渡

普通动画开关不影响锁屏效果池；`[lockscreen] transition=[]` 从下一个新锁屏周期生效。
工程已配置，实机临时禁锁例外保留；统一策略见[电源管理](../power/desktop-power-policy.md)。

## 功能往返验证（2026-10-21）

以下判据先于检查确定，设备观察只支持列出的路径：

| 路径 | 判据与实际结果 |
| --- | --- |
| 文件选择门户 | 普通用户发起 `FileChooser.OpenFile`，真实 GTK 选择后 response 为 0，URI 为 `file:///etc/hostname` |
| 剪贴板 | Noctalia 向独立 wlr-data-control 客户端传输 UTF-8，与反方向独立数据源到 Noctalia 均逐字节相等；原剪贴板为空，测试后仍为空 |
| 图形权限认证 | 从图形会话的终端以 pi 运行 `pkexec --disable-internal-agent /usr/bin/id -u`；Noctalia 显示 polkit 面板，认证后输出 0、退出码 0，面板关闭 |
| 音频 | `pw-play` 经 PipeWire 连接到 bcm2835 Headphones 的左右声道，ALSA 为 RUNNING，扬声器 GPIO11 为高；用户确认三声提示音“能听到，声音正常” |
| 启动器与终端 | 实际快捷键打开终端和 Noctalia 启动器，选择 Mousepad 后进程属于 UID 1000，未映射私有库且无非空私有库环境覆盖 |
| 注销与清理 | `noctalia msg session logout` 后，原包装进程、labwc、clients、kanshi 与 Noctalia 五个 PID 全部退出；四个独立音频服务 PID 保持不变 |

音频第一次在 40% 音量播放时未获听觉确认；第二次临时调至 60%，采用更明显的提示音，
用户确认后恢复到 40%。这是当前扬声器路径验证，未验证耳机插拔、录音或蓝牙音频。
PAM 锁屏往返已在先前完成；本轮 Noctalia 注销后可重新启动普通用户会话，仍启用 300 秒锁屏。
切换期间一个短暂 `_greetd` scope 报 `No PIDs left`/`resources`，记录保存后仅清除该空 scope
的失败标记；当前 failed units 为空。该记录不作为冷启动或 greeter 稳定性结论。

两条实现细节会影响验证可信度：

- `noctalia msg clipboard-text` 读取文本历史，可能与当前图像剪贴板不同，不能充当原始
  剪贴板备份。独立客户端先枚举所有 MIME 并读取实际数据，再做往返；本轮原始选择为空，
  无需持续驻留恢复非文本数据源。保留了历史，不使用 `clipboard-clear` 清除用户内容。
- polkit agent 注册在图形 logind 会话。SSH 中仅复制 Wayland/D-Bus 环境不能使进程属于
  该会话，因此从合成器快捷键打开的终端触发只读 `id -u` 授权，验证认证后结果。
  初次门户注入仅使用虚拟键盘组合键未完成选择；采用 evdev Ctrl+L/Return 后完成同一次请求。

## 双会话暖切换复验（2026-10-24）

正常 greetd 入口顺序启动两套会话，上一套自有进程全部退出，三个用户音频服务保持。
Mako 的通知创建、查询与按 ID 关闭完成，实际 Screenshot 与 Noctalia 取色见[截图门户](screenshot-portal.md)。
PAM session 没有 lastlog 缺失日志，不扩大为新的口令认证判据。一次 FileChooser 请求超时已取消，
未计为成功；旧 Wayland 连接结束后的 GTK portal 重启恢复，不改生产配置。
证据在 `~/.cache/uconsole-desktop-20261024/functional/waybar/`，物理冷启动另验。

## 屏幕共享的真实取帧验证（2026-10-24）

两套外壳均完成请求→真实 PipeWire FD/节点→已知红/蓝窗口像素的功能往返。
原始 720×1280 buffer 携带 transform=3，客户端需要应用元数据。
判据、观察与限制集中在[ScreenCast 验证](screencast-validation.md)，不以节点存在或计数宣称 FPS。

## 匹配条件的空闲资源观察

同一 CM4、Ubuntu、内核、labwc、Mesa、输出 transform 270/scale 1.5 和用户服务，
顺序切换 Waybar 再切回 Noctalia。各自会话启动后等待 20 秒，采集 5 次、间隔 2 秒；
无编译进程、前台应用或锁屏，Noctalia IPC 确认面板关闭且未锁。两者保留各自日常默认外观，
Waybar 为纯色背景，Noctalia 保留壁纸；这是所交付配置的开销观察，未单独隔离背景差异。

统计范围统一为 labwc 递归子进程，另加经过 argv 核对的合成器包装父进程；
PipeWire/WirePlumber 另计，未把整个系统已用内存当成桌面内存。所有采样 PSS 均可读，
CPU 统计中的进程身份稳定，无漏读。

| 配置 | 桌面进程数 | 桌面 PSS（MiB） | CPU 范围／中位数（单核百分比） | 音频服务 PSS（MiB，另计） |
| --- | --- | --- | --- | --- |
| labwc + Waybar | 9 | 137.7 | 0–0.486 / 0 | 19.3 |
| labwc + Noctalia | 5 | 187.1 | 0.482–1.928 / 1.447 | 19.3 |

这轮 Noctalia 桌面 PSS 多约 49.4 MiB，含音频约 206.4 MiB；Waybar 含音频约
157.0 MiB。数值来自短时暖会话，不是内存上限或长期稳态承诺。Noctalia 切换前的暖会话
另一次观测为 181.7 MiB，说明使用阶段也会影响数值。先前 Waybar 约 116.6 MiB 的记录
未包含包装父进程，且窗口/阶段不同，应保留为历史观察，不能与新版统计直接相减。

两套配置均无已定义的资源合格线，因此只报告数值。ScreenCast 的后续功能验证见上节；
完整键盘覆盖、休眠恢复、物理冷启动及 CM5 当前完整镜像仍待验证。
会话资源与按电源键到桌面的整机时间分别记录。

## 后续启动的阶段边界

进程 starttime 来自 `/proc/<pid>/stat` 和 CLK_TCK，不是 ELF 初始化完成或首帧。
系统重启首个会话中，labwc→primary clients 多次约 3.27–3.33 s，暖会话曾仅 0.43 s。
Noctalia 尚未创建时已发生的等待不能归因外壳；退出清理的三秒超时也不在启动路径。

### 首次 Mesa 加载的调用计时

同一 CM4 的重启首会话曾有约 1.62 秒 Mesa 加载，暖会话约 0.073 秒；调用计时、缺页/输入块、
固定条件和后续 A/B/A 证据集中在[私有 Mesa 启动与隔离](private-mesa-startup.md)。
这是加载边界，不能据 graphical.target、进程创建或任意缓冲认定实体桌面就绪。

## 镜像构建与离线检查

`ubuntu-noble-labwc-noctalia` 直接复用固定 deb。Noble 默认过滤文档，桌面包需显式
path-include 保留许可证；检查 `dpkg --verify` 的输出而不只看退出码。
greetd 的标准启用形式是 `display-manager.service` 别名，检查器应核对真实 unit/别名，
不能强行添加 `graphical.target.wants/greetd.service` 来迎合错误 fixture。
导出镜像、运行安装和新卡验证分开记录；最新产物身份见[开发状态](../../project/status.md)，历史哈希不当成当前镜像。

## 可复用的构建经验与证据位置

Noctalia 的虚拟键盘兼容补丁复用现有 Wayland 主循环刷新，适配 Noble Wayland 1.22；
Meson release 实际为 `-O3`，回执记录并校验 introspection 生效选项。
安装 ELF 的私有 RUNPATH 配合直接 exec 保留 `/proc/self/exe` 的 PAM helper 重入语义；
显式执行动态加载器会使该路径指向加载器。应用程序环境不继承私有 `LD_LIBRARY_PATH`。
构建与依赖解析规则见包说明（工程源码：`image/packages/noctalia/README.md`）。

本机离线输入和观察记录位于 `~/.cache/uconsole-desktop-20261018/`，设备证据位于
`/var/lib/uconsole-desktop/20261018/`，后续启动与许可证修复记录在设备的 `20261021/`
同级目录；本机对应 `resume-{boot-evidence,session-startup,process-starts,live-followup}.json`
与 `resume-desktop.png`。证据目录名沿用开始日期；设备时钟未同步，不能
将其中壁钟时间直接用于性能计算。当前有效状态见[开发状态](../../project/status.md#ubuntu-labwc-desktop-checkpoint)。
