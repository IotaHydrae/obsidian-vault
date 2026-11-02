# Ubuntu 精简桌面的浏览器与影音应用

> Ubuntu 首轮使用 Firefox、Celluloid、Amberol，应用层与外壳和板级适配分开。

## TL;DR

- Firefox 使用 Mozilla 签名 APT 原生 deb；播放器使用 Noble ARM64 仓库。
- 仅桌面配置安装这三项，默认打开方式和小屏设置由 Ubuntu 后端维护。
- CM4 功能证据不能推广到 CM5/Rockchip 的渲染和硬解；视频判据见专门条目。

## 方法：来源、默认关联、布局与媒体链分别验收

核对签名来源与实际包版本，准备干净用户作默认值对照，并保留已有用户的高优先级设置。
检查文件/目录打开方式、150% 缩放下的完整控件、已知媒体与 MPRIS 控制往返。
渲染、解码与实体声音/同步各取证；桌面软件可准备不等于新镜像或其他核心板已验证。

## 软件来源与更新责任

| 应用 | 来源 | 选择依据 |
| --- | --- | --- |
| Firefox | Mozilla 官方签名 APT，原生 ARM64 deb | 原生 Wayland、完整浏览器功能与持续安全更新 |
| Celluloid | Ubuntu Noble ARM64 仓库 | GTK/libmpv 前端、Wayland 和 MPRIS 支持 |
| Amberol | Ubuntu Noble ARM64 仓库 | 本地文件/文件夹队列、自适应界面和 MPRIS 支持 |

Ubuntu Noble 的 `firefox` 是转向 Snap 的过渡包，不能把普通 Ubuntu 源的包名当作原生
浏览器实现。工程限定 Mozilla 源的签名密钥和 Firefox 包优先级；镜像记录实际安装版本，
设备仍通过软件源接收更新。记录镜像构建时的浏览器版本便于定位问题，不长期冻结安全更新。
Amberol 直接依赖的 GStreamer good 插件随发行版安装，应用清单另显式安装
`gstreamer1.0-libav`，使 AAC/M4A 等常见音乐格式不只依赖最基础的音频插件。
另安装 `fonts-noto-cjk`；实机在只有 Noto Core 时，Firefox 受控中文页出现缺字方框，
浏览器能够打开网页并不等于中文内容可读。
Mozilla 密钥轮换须重新核对官方指纹并更新工程材料，不能关闭签名校验绕过失败。

Celluloid 和 Amberol 的 MPRIS 接口用于向 Noctalia 提供曲目信息、播放和暂停控制。
Noctalia 自身提供媒体客户端，因此无须仅为同一控制路径额外常驻另一套媒体面板。
固定的 Noctalia 5.3.0 会过滤标题、艺人、专辑三项都为空的播放器；只观察到 MPRIS
`Playing` 和进度推进，不能据此认定外壳一定会列出该媒体。受控测试文件应带独立已知
元数据，并分别检查播放器公开的 `Metadata`、外壳内部的播放器列表及控制后的实际状态。
它既在启动时扫描，也监听会话总线名称变化，不能仅凭“安装前启动外壳”就归因为缺少重启。
发行版 MIME 默认保存在 `/usr/share/applications/mimeapps.list`，使标准位置与旧式
`~/.local/share/applications/mimeapps.list` 的用户映射均优先；不能以重置用户配置强制接管。
Amberol 的旧版桌面文件未声明部分 canonical 音频 MIME，工程同时提供 `Added Associations`；
文件夹明确默认由 Thunar 打开，不因播放器能够导入文件夹就改为播放器打开。
仅省略目录关联不够：Amberol 上游桌面文件声明 `inode/directory`，本次实机安装后
GIO 自动候选曾选择它。发行版默认须显式指定 `inode/directory=thunar.desktop`，
同时保持用户自定义文件管理器的更高优先级。

## 小屏与解码的验证边界

uConsole 横屏物理分辨率为 1280×720，当前 150% 缩放提供约 853×480 的逻辑空间，
扣除顶栏后可用高度更少。上游称界面自适应或提供默认窗口尺寸，均不能证明所有对话框可用。
应实际检查主窗口、播放控制、字幕/设置、长曲名和文件选择；优先最大化或调整应用自己的
布局，不通过降低用户已确认的全局缩放来掩盖窗口溢出。

实机发现 Noble Amberol 0.10.3 的 192 像素封面和纵向留白导致底部按钮出屏；
调整窗口大小不能解决其最小内容高度。工程用 GLib 资源覆盖将封面改为 48 像素并收紧
留白，保留文字与按钮尺寸，截图中播放、音量、队列、随机和循环控件均完整可见。
无封面与带封面、中文长曲名两种输入均已检查，长标题按原生方式省略。
适配同时限定 Ubuntu 包版本和原始资源哈希，更新不匹配时退回原生布局待复验。
同 ID 的本地发行版 desktop 入口关闭 D-Bus 直接激活，确保 MIME 和应用入口使用包装器；
直接执行原始二进制仍是原生布局。此方法作用于应用布局，不依赖树莓派 GPU；其他
输出尺寸和发行版版本须单独验证。具体安装位置与更新步骤见
Ubuntu 后端说明（工程源码：`image/ubuntu-base/README.md#amberol-小屏布局`）。

验证播放器时，分别检查已知媒体的真实输出、进度、音画同步及 MPRIS 状态往返。
合成器有 V3D 提交或应用使用 Wayland，只证明对应显示路径，不能证明视频硬件解码。
CM4 规格列出的 H.264/H.265 硬件能力仍依赖当前内核、FFmpeg/GStreamer 和应用后端接通；
Celluloid 选择 libmpv 前端不会自动补齐缺少的解码实现。其他核心板必须重新验证该链路。
后续 CM4 测量确认 H.264 硬解已接通，显示侧仍需轻量 profile；Ubuntu 的 Celluloid
默认采用自动硬解与 fast profile，已有用户选项优先。两轮 720p30 稳定采样没有新增
输出丢帧，1080p30 仍有残余丢帧，详见[解码与输出分离验证](cm4-video-playback.md)。

功能判据与资源测量分开：成功播放一段媒体不能替代格式覆盖，也不能据短时进程内存给出
长期上限。浏览器本地页面验证不覆盖网站登录、在线 DRM、摄像头、会议或全部网页视频。
单实例应用须核对真实进程和总线 owner；调用退出方法返回成功不等于原实例已经退出。
首次启动须等待目标媒体和 MPRIS 服务就绪，固定短延迟失败本身不能作为解码失败的证据。

## CM4 增量部署的功能证据

CM4 Ubuntu Base 24.04.5 上安装 Firefox 158.0.1、Celluloid 0.26、Amberol 0.10.3。
Firefox 的原生 Wayland、受控中文网页与 JavaScript 交互已验证；Celluloid 的已知视频、
中英文字幕、主要播放控件及设置窗口已检查。Noctalia 能发现带元数据的播放器，
对 Celluloid 与 Amberol 的播放/暂停操作得到状态与进度往返印证。Amberol 的 MP3 和
干净实例中的 AAC 均有进度推进，暂停后保持不动；适配后的无封面、封面及长标题布局已检查。
测试期间静音，结束恢复原有 40% 音量与非静音状态；本轮没有肉耳音质或物理音画同步结论。
应用已增量安装到现有 SD 系统，工程两套桌面均可准备；已有导出镜像尚未包含本轮应用。

## 依据

- [Mozilla 的 Linux 安装与官方 APT 说明](https://support.mozilla.org/en-US/kb/install-firefox-linux)
- [Celluloid 的 Wayland、MPRIS 与配置说明](https://celluloid-player.github.io/)
- [Amberol 的功能范围](https://apps.gnome.org/Amberol/)
- [GLib 的资源覆盖接口](https://docs.gtk.org/gio/struct.Resource.html)
- [Amberol 0.10.3 封面尺寸实现](https://gitlab.gnome.org/World/amberol/-/blob/0.10.3/src/cover_picture.rs)
- [Noctalia 5.3.0 媒体发现与过滤源码](https://github.com/noctalia-dev/noctalia/blob/v5.3.0/src/dbus/mpris/mpris_service.cpp)
- [mpv 硬件解码说明](https://mpv.io/manual/stable/#options-hwdec)

源码输入及具体设备结果以[开发状态](../../project/status.md)为准；现有镜像与增量部署须分别记录。
