# kiosk 启动事件与显示测量口径

> 当前启动实测和时间基准以 [boot-time.md](../boot-time.md) 为准；会话行为以 [session-design.md](session-design.md) 为准。

## 当前启动顺序

HDMI 检查单元异步启动上位机 socket、后端和 kiosk。socket 先绑定本地端口；后端初始化与 Chromium 冷启动并行。
kiosk 只等待后端进程启动，不等待 HTTP 就绪。Plymouth 在 Cage 接管前退出；Chromium 加载极简引导页，
HEAD 收到响应头后导航至 HMI，连接失败按秒重试。引导页也要等浏览器绘制，不能遮住 Chromium 自身的冷启动。
详见 [会话启动契约](session-design.md) 和 [Chromium 启动评估](../chromium-cold-start.md)。

## 事件口径

以下时点不能互相替代：

- systemd 单元进入 `Started` 状态或进程创建。
- 后端报告 `Uvicorn running`。
- 浏览器请求收到 HTTP 200；这只证明文档响应，不证明浏览器已绘制。
- 浏览器记录到 FCP；这不证明 HDMI 已扫描输出，也不证明页面可交互。
- HDMI 物理显示首帧；需用屏幕采集或等效硬件探针测量。

当前启动样本是热重启测量；仓库记录没有冷上电的 HDMI 物理首帧数据。完整时钟来源、串口误差和当前样本见
[boot-time.md](../boot-time.md)。`fake-hwclock` 修改墙钟，不会让 `CLOCK_MONOTONIC` 跳变。

## 已记录的画面过渡

2026-09-30 的一次 KMS 抓帧在着陆页导航到 HMI 时观察到短暂的 Chromium 深灰窗口帧：帧 85 变暗、帧 86 为灰色，
帧 87 起显示 HMI，合计约 70 ms。该观察只描述那次 Chromium 151 / Cage 配置，不代表每次启动的首帧时间。
深灰来自 Chromium 窗口 surface，不是 Cage 背景；改合成器背景不能消除这段导航过渡。

曾用跨站 iframe 避免文档导航，但会话 Cookie 在 Chromium 151 的第三方上下文中不可用，登录流程失败，方案已撤回。
视觉过渡与合成器选择的对照见 [compositor-customization.md](compositor-customization.md) 和
[compositor-candidates.md](compositor-candidates.md)。
