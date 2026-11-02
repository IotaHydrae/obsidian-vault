# Chromium 启动路径与优化边界

> 启动引导页已缩到 1,114 字节并保留重试。地址栏 WebUI 开关未证明提速，保持默认。移除网卡驱动持有 RTNL 时的固定等待后，临时模块的三次重启样本中 Cage → FCP 中位数提前约 0.87 秒；另一次 DC 冷上电验证正常，尚无原驱动 DC 对照，正式镜像未带修复。文件预热未采用。

## 极简启动引导页

`overlay/usr/share/lsp-kiosk/loading.html` 不再复制 HMI 表单、完整 CSS 和时钟；正常启动只有同色背景，
5 秒后仍未进入 HMI 才显示启动提示。HEAD 探测收到响应头即导航，避免探测阶段下载主页正文。
GET-only 路由目前对 HEAD 返回 405，`no-cors` 返回 opaque response；因此探测只判断传输是否完成，
不验证 HTTP 200 或业务健康。失败 1 秒后重试，同时只有一个在途请求；挂起请求没有新增应用层超时。
运行契约见 [session-design.md](kiosk/session-design.md)。

保留引导页是为了启动期恢复。直接打开 HMI 可以减少一次探测与导航，但没有测出明确首帧收益；
并且开发板连续连接失败对照中，Chromium 的直接导航自动重试间隔依次约为 1.08、6.14、30.01 秒。
后端恢复后确实会自动进入，不能描述成“永不恢复”；极简页保留传输失败后的 1 秒重试，避免依赖该退避。
HMI 已显示之后的故障不由引导页处理，两条路径均如此。

## 会话热启动验证（2026-10-16）

LubanCat-5 V2、eMMC `/dev/mmcblk0p2`、Rockchip 6.1.172、Chromium 151、HDMI 已连接。
只重启图形会话，没有整机重启、清页缓存或物理断电；后端就绪与采集等启动期竞争不能由本轮复现。

- 原页 / 直接 URL 各三次会话热启动的 KMS 首个非底色帧均约 0.59 秒，未显示明确优势。
  它不是 FCP、可交互或 HDMI 光学首帧，也不是物理冷启动测量。
- 注入后端接受请求后直接关闭连接并退出，直连的三次失败后退避约 1/6/30 秒，恢复后 HMI/API 正常。
  新引导页在相同前三次连接失败后继续 HEAD 重试，第四次后端恢复后出现主页 GET、`/api/state` 和
  `/api/readiness`；KMS 抓帧确认显示真实登录页。

另做原页与新页交替各三次的 HTTP 对照：后端始终就绪，沿用同一浏览器 profile 与缓存，不带 trace。
起点为 Cage 单元的 `ExecMainStartTimestampMonotonic`，终点为 loopback 抓包中的客户端请求，
通过每轮 `time.time_ns()` / `time.monotonic_ns()` 锚点对齐抓包墙钟与单调时钟。

| 从 Cage 启动到请求（秒） | 原页 1 | 原页 2 | 原页 3 | 新页 1 | 新页 2 | 新页 3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 主页文档 GET | 1.1018 | 1.0793 | 1.1389 | 1.0181 | 1.0335 | 1.0425 |
| `/api/state` GET | 1.2285 | 1.1793 | 1.2410 | 1.2341 | 1.2263 | 1.2409 |

| 指标 | 原页 min / median / max | 新页 min / median / max |
| --- | --- | --- |
| Cage 启动 → 主页 GET（秒） | 1.0793 / 1.1018 / 1.1389 | 1.0181 / 1.0335 / 1.0425 |
| Cage 启动 → API GET（秒） | 1.1793 / 1.2285 / 1.2410 | 1.2263 / 1.2341 / 1.2409 |
| 探测 → 主页 GET（毫秒） | 80.5 / 108.2 / 126.9 | 42.1 / 43.8 / 46.2 |

主页请求中位数提前约 68 ms，API 请求基本不变；六次 kiosk 均无自动重启。
该结果支持减少引导页的重复工作，不能证明可交互或整机冷启动显著变快。
本轮未获得有效 FCP trace，也未采集温度与频率，不能据此确定 CPU/I/O 瓶颈。
原始抓包、时钟锚点和单元时间在开发板 `/tmp/lsp-kiosk-warm-http-ab-20261016/`；
抓包可能含 cookie，不应公开或入库。

## 重启后首次启动 A/B（2026-10-16）

同一开发板、eMMC、6.1.172、Chromium 151.0.7922.169、3840×2160 HDMI，均使用极简引导页。
每次正常重启后测量首次 kiosk，内核页缓存不沿用上一次启动，浏览器 profile 和 HTTP 磁盘缓存保留。
这是软件重启后的冷缓存样本，不是新 profile、物理断电上电或单独清缓存重启会话。
两组都带相同 startup trace 与 `/proc` 采样，CPU governor 为 `performance`。

唯一有意改变的浏览器配置是关闭 `WebUIOmniboxPopup,WebUIOmniboxAimPopup`。
Chromium 151 的 `chrome/browser/ui/omnibox/omnibox_next_features.cc` 默认启用这两项；
`chrome/browser/ui/views/location_bar/location_bar_view.cc` 在创建隐藏地址栏时仍构造其 WebUI。
关闭后 trace 中两个 `chrome://omnibox-popup.top-chrome/` 导航消失，少一个 renderer；
这不改变 HMI 的 GPU 渲染设置，也不是只关闭 `PreloadTopChromeWebUI`。

起点取 Cage 的 `ExecMainStartTimestampMonotonic`；终点按 HMI URL、frame 和 navigationId
匹配浏览器 FCP。另列相同 `H1#login-title`（LCP 候选、面积 98,356）的首次绘制，避免把少量内容先出现当成相同页面内容完成。
两种终点都不能证明可交互或 HDMI 光学首帧。

| 样本 | Cage → HMI FCP（秒） | Cage → 标题 H1（秒） |
| --- | ---: | ---: |
| baseline 1 | 5.643376 | 5.643376 |
| baseline 2 | 5.686399 | 5.686399 |
| baseline 3 | 5.743069 | 5.743069 |
| omnibox off 1 | 5.803666 | 5.803666 |
| omnibox off 2 | 5.617949 | 5.617949 |
| omnibox off 3 | 5.609890 | 5.926564 |

| 指标 | baseline min / median / max（秒） | off min / median / max（秒） |
| --- | --- | --- |
| Cage → FCP | 5.643376 / 5.686399 / 5.743069 | 5.609890 / 5.617949 / 5.803666 |
| Cage → 标题 H1 | 5.643376 / 5.686399 / 5.743069 | 5.617949 / 5.803666 / 5.926564 |

off 3 的导航至 FCP 仅 162.587 ms，首次内容是面积 6,400 的图标；标题在导航后 479.261 ms 才绘制。
其余五次 FCP 与标题同帧。该样本的 FCP 匹配正确，但内容先后顺序不同，不能据此声称整个 HMI 在 163 ms 内就绪。
两组 Cage → FCP 中位数仅相差 68 ms；按共同标题则 off 中位数晚 117 ms。
三次样本不足以把这种波动解释为明确提速或退化，当前证据只确认减少了无用渲染进程。

采样窗口未发现 swap、内存回收或 OOM 增量，journal 未见相关 GPU/DRM/renderer 错误，服务均无自动重启。
off 3 的 GPU 初始化耗时与另两次相近，没有换成其它渲染路径的证据。
baseline 1 未记录动态频率和温度；baseline 2 的大核 policy6 为 2.256 GHz，其余已记录样本为 2.352 GHz。
不能把差异全部归因于功能开关，也不能忽略 trace 与采样开销。
原始 trace、采样、boot ID 和单元时间保存在本机 `/tmp/lsp-chromium-cold-analysis-20261016/`
的 `boot-baseline-{1,2,3}`、`boot-omnibox-off-{1,2,3}`，不将大 trace 入库。

## 网络初始快照阻塞浏览器

上述原驱动样本中，`BrowserProcessImpl::PreMainMessageLoopRun` 墙钟耗时约 0.93–1.30 秒，
主线程 CPU 时间只有数毫秒。Chromium 151 的源码与线程栈给出如下等待链：

```text
BrowserProcessImpl::PreMainMessageLoopRun
  → SystemNetworkContextManager → GetNetworkService
  → CreateNetworkServiceParams → NetworkChangeNotifier::GetConnectionType
  → AddressTrackerLinux::GetCurrentConnectionType（条件变量等待初始快照）
worker: AddressTrackerLinux::Init → netlink 地址/链路 dump → 等待 RTNL
NetworkManager: RTNL → stmmac_open → stmmac_hw_setup → msleep
```

另一次线程采样连续 47 个样本命中 Chromium worker 的 `__netlink_dump_start`，
首末样本跨 989 ms；这是采样覆盖范围，不是精确测得的整个 syscall 时长。
全系统栈在 8.306、8.575、8.842 秒三次同时捕获到该 worker 等待，以及 NetworkManager
在 `msleep → stmmac_hw_setup → stmmac_open → rtnl_newlink` 中持有 RTNL。
这不是等待 DHCP、DNS、`network-online.target` 或后端 HTTP 就绪。

本轮基线 BSP 的 `kernel-6.1/drivers/net/ethernet/stmicro/stmmac/stmmac_main.c` 中，
`stmmac_init_dma_engine()` 在 `stmmac_reset()` 前原有无条件 `msleep(1500)`；
该固定等待由 BSP 同步提交 `e4e9c64cb` 引入。它发生在网口 open 的 RTNL 临界区，
即使 HMI 只访问 loopback，也会经浏览器网络状态初始化进入启动关键路径。
本轮补丁仅移除这次固定等待，保留 DMA reset 的完成轮询、错误处理及 PHY reset 时序。
端到端收益取决于浏览器启动与网口初始化重叠，不能直接记为缩短 1.5 秒。

Chromium 目标版本源码依据：
[浏览器网络上下文初始化](https://chromium.googlesource.com/chromium/src/+/refs/tags/151.0.7922.169/chrome/browser/browser_process_impl.cc#1626)、
[网络服务启动参数](https://chromium.googlesource.com/chromium/src/+/refs/tags/151.0.7922.169/content/browser/network_service_instance_impl.cc#369)、
[AddressTrackerLinux 初始快照与 netlink dump](https://chromium.googlesource.com/chromium/src/+/refs/tags/151.0.7922.169/net/base/address_tracker_linux.cc#329)。
诊断数据在同一临时目录的 `boot-network-diagnostic`、`boot-network-holder-diagnostic`。

trace 里另一个长达 2.314 秒的 `SequenceBound::Construct` 是 startup tracing 自身后台
`StartBlocking()`，线程 CPU 仅 277 µs，末尾才记录 `StartupTracingController::Start`。
不能将它误认为字体初始化，或把它的全部墙钟时间当成生产配置可节省的延迟。

## 去除固定等待的模块验证

在同一 6.1.172 内核上临时加载重编 `stmmac` 模块，核对 `vermagic` 与符号 CRC；
重编基线与原模块的源码版本一致，nodelay 对照仅修改 `stmmac_main.c` 的固定等待。
沿用前述 `omnibox off`、极简引导页和相同探针，正常重启三次，单变量比较驱动调整。
三次 HMI FCP 均与同一标题 H1 同帧。

| nodelay 样本 | Cage → FCP / H1（秒） | BrowserProcess PreMain 整段（毫秒） |
| --- | ---: | ---: |
| 1 | 4.720161 | 31.973 |
| 2 | 4.743885 | 27.136 |
| 3 | 4.987513 | 28.719 |
| min / median / max | 4.720161 / 4.743885 / 4.987513 | 27.136 / 28.719 / 31.973 |

对照原驱动 `omnibox off` 三次：

- BrowserProcess PreMain 中位数由 954.359 ms 降到 28.719 ms。
- Cage → FCP 中位数由 5.617949 s 降到 4.743885 s，提前 0.874064 s。
- Cage → 共同标题 H1 中位数由 5.803666 s 降到 4.743885 s，提前 1.059781 s。

PreMain 数字包含该阶段的全部初始化，不能把剩余 27–32 ms 全部计为网络等待。
恢复默认 Omnibox 后另做一次确认：PreMain 31.603 ms，Cage → FCP / 同一 H1 为 4.912514 s。
该单样本支持取消地址栏开关后仍能消除网络快照长等待，不能替代三次默认配置 A/B。
最终保留驱动的最小源补丁，不向生产配置加入 Omnibox 功能开关。

网口回归中，原驱动每口三次 open 耗时 1,616.962–1,653.422 ms；
新模块每口十次为 104.034–123.882 ms，二十次均成功。
接线的 eth0 恢复千兆全双工，双向各 32 MiB 数据 SHA-256 校验通过，RX/TX 错误和丢包计数均为 0；
eth1 未接线，仅验证 open，不能宣称其物理链路或传输通过。

### DC 冷上电确认

正常关机并从串口确认 `reboot: Power down` 后，现场拔掉 DC 12V 电源约 5 秒再插回，
保留 HDMI、网线和串口连接。本次仍用 nodelay 临时模块、极简引导页和 startup trace，
Omnibox 保持默认；eMMC 启动，现场确认屏幕正常显示上位机。

| 单次 DC 样本事件 | 启动后单调时间（秒） |
| --- | ---: |
| Cage 启动 | 5.698381 |
| 后端就绪日志 | 6.807823 |
| HMI 导航 | 9.496908 |
| HMI FCP / 同一标题 H1 | 10.020472 |

按 HMI URL、frame 和 navigationId 核对，FCP 与 `H1#login-title` 同帧：
Cage → 两终点为 4.322091 s，导航 → 两终点为 523.564 ms。
BrowserProcess PreMain 整段为 32.915 ms，线程 CPU 为 10.954 ms，不能全部算作网络等待。
采样窗口频率稳定，温度 34.230–40.692°C；未见 swap、回收或 OOM 增量、相关 GPU/DRM/renderer 错误或服务重启。
eth0 恢复千兆全双工，双向各 32 MiB 的 SHA-256 校验通过，RX/TX 错误与丢包计数为 0。

这只有一个 nodelay DC 样本，没有原驱动 DC 对照，不能据此计算冷上电提速；
也没有硬件上电边沿锚点，表中时间不能当作 DC 上电到显示的耗时。
FCP 不等于可交互或 HDMI 光学首帧，现场屏幕确认只证明这次显示正常。

### 测试清理与覆盖边界

测试后已删除诊断单元、drop-in 和临时模块覆盖，并重启核对原 stock 模块路径、
`srcversion=7500A6F6F50A62FA519DB74`、`tainted=0`；Chromium 不再带 trace、调试端口或 Omnibox 开关。
kiosk、oem-host 和 socket 均正常且无重启，主页及两个 API 返回 200。正式内核包尚未安装，
开发板当前运行原驱动，后续正式包仍须验证。

这些结果覆盖软件重启、一次 DC 冷上电和已测网口场景，不覆盖长期链路稳定性或正式打包内核。
`stmmac_resume()` 也会经 `stmmac_hw_setup()` 调用该初始化函数，本轮未测试挂起/恢复生命周期；
系统默认不自动休眠不能替代这项验证。
数据在同一临时目录的 `boot-stmmac-nodelay-{1,2,3}`、`boot-stmmac-nodelay-default-ui`、
`boot-stmmac-nodelay-dc-cold`，
模块构建核对见 `module-comparison.json`，端口循环与数据回传见 `ethernet-*`。

## 文件预热评估

2026-09-30 的开发板 eMMC / Chromium 151 实验只使一次主页 HTTP 响应提前 0.52 秒，
同时推迟了后端就绪，收益不足以采用；没有证据支持将延迟归因到单一 I/O 根因。

### 实验范围

当时用后端访问日志中的 `GET / HTTP 200` 作为页面文档响应探针。它不是 FCP、HDMI 物理首帧或可交互时点。
样本来自单板热启动和启动后重启 kiosk 的对照，不是当前冷上电基线；当前启动测量见
[boot-time.md](boot-time.md)。

| 对照 | 主页 HTTP 200 | 观察 |
| --- | ---: | --- |
| 开机基线 | 11.02 s（kiosk 启动事件 4.90 s） | 两事件相差约 6.1 s，包含合成器、浏览器及页面请求路径 |
| 屏蔽三项用户服务 | 11.06 s | 与基线差异在该次测量噪声范围内 |
| 停止两个开机服务 | 11.18 s | 这两项干预没有显示出改善 |
| 启动后清页缓存并重启 kiosk | 1.47 s（重启 kiosk 后） | 启动上下文不同，不能据此量出开机延迟的 I/O 份额 |
| 预读 7 个 Chromium 映射文件 | 10.65 s，对照 11.17 s | 响应提前 0.52 s；后端就绪同时晚了约 0.7–1.3 s |

最后一项按当时 `/proc/<pid>/maps` 中位于 `/usr/lib/chromium/` 的 7 个文件列预热清单，映射文件大小合计约 317 MB，
实验单元读取这些文件以填充页缓存。maps 文件大小不是启动实际读取量，也不等于工作集；预热操作本身的读量也不能直接当成 Chromium 原本会读取的量。

### 结论边界

- 预热使一次 HTTP 200 提前 0.52 s，但后端就绪变晚；加上需要维护文件清单和启动单元，收益不足以采用。
- 少数服务的停用实验只说明这些具体干预没有改善该次探针时间，不能排除其它启动期工作。
- 低聚合 CPU 使用率或 iowait 不能证明没有 CPU、存储队列或单线程等待瓶颈。
- `/proc/diskstats` 是设备总量，不能归因给 Chromium；`maps` 是映射清单，不是实读统计。I/O 归因限制见
  [boot-io-profiling.md](boot-io-profiling.md)。
- `drop_caches` 不是物理冷启动，也不复现开机时的并发工作。现有实验没有确认 Chromium 文件预读或 eMMC 是启动延迟的唯一根因。

启动链事件定义见 [kiosk 启动口径](kiosk/startup-timeline.md)。若重新评估预热，应在同镜像、同硬件和相同终点下重复 A/B，
同步记录后端就绪、浏览器 FCP 与设备及服务级 I/O，并确认采集等其它启动任务未受影响。
