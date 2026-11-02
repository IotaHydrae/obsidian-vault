# wlroots 0.17.1：RGA 与 Pixman 的 render-pass 对照

实验版可运行：cached 缓冲的同步复制慢于 Pixman；两端 WC 缓冲时 RGA 快于 Pixman，
但 RGA 自身耗时基本不变，主要差别来自 CPU 读取 WC 内存的代价。
这是离屏 render-pass 测量；当前实验版的 Chromium SHM 回退不能据此判断完整 kiosk 帧率。
补丁和构建入口在 packages/wlroots-rga（镜像源码：`packages/wlroots-rga/README.md`），未装入运行系统或镜像。
逐帧 producer、SHM 上传与真实 DRM dumb 输出的对照见
[wlroots-rga-replay.md](wlroots-rga-replay.md)。

## 实现边界

- Ubuntu noble `wlroots 0.17.1-2.1build1` 源码及其 Debian 补丁作为基线；`-Drga=enabled` 编译。
- `WLR_RENDERER=rga` 显式选择混合 renderer；auto 和 Pixman 构造器仍选择原 Pixman 路径。
- 只加速新 render-pass API：linear、单平面、offset 0、32 位同格式、等尺寸整数复制 / 裁剪。
  XRGB8888 允许 opaque OVER；ARGB8888 只允许 SRC。旋转、缩放、透明混合、SHM、fill 回退。
- 默认 RGA3 `3_0`，另测 `3_1`、RGA2 `2`。RGA2 基准强制 cached / uncached DMA32 heap；
  普通客户端缓冲物理地址未知，使用 RGA3。普通 system heap 成功不证明实际页在 4 GiB 以上。
- `importbuffer_fd` handle 按纹理 / 目标生命周期缓存，每任务固定 core，使用普通单任务同步 API。
  纯 RGA pass 不开启 CPU access，Pixman 操作按需 START，后续 RGA 前 END。
- DMA-BUF poll 分别以源 POLLIN、目标 POLLOUT 等待已有隐式 fence；poll 不替代 cache sync。
  提交同步完成后才返回；CPU END / poll / 映射失败会传播失败，部分 SRC 任务失败可幂等重绘。
- 补丁给 DRM dumb-buffer 增加 CPU DMA_BUF_SYNC，也修正新 pass 错误传播；
  这些共同修正即使 RGA 被编译禁用也生效，因此补丁库不是逐字节等同上游库。

隐式 fence 依据：目标 `kernel-6.1/drivers/dma-buf/dma-buf.c` 的 `dma_buf_poll`；
同步任务依据：`drivers/video/rockchip/rga3/rga_job.c` 的 `rga_request_wait` / `rga_request_submit`。
未做真实 GPU producer pending-fence 故障注入，调用方仍须保证 pass 期间没有并发读写同一 buffer。

## 测量方法

2026-10-14，LubanCat-5 V2、8 GiB、Rockchip 6.1.172 #10，RGA driver 1.3.13，
librga 2.2.0-1 / API 1.10.1，Pixman 0.42.2，GCC release 构建。
固定 `taskset -c 4`（A76 2.304 GHz），performance governor，停止图形服务和 HDMI 调度。

wlroots-rga-bench.c（镜像源码：`tests/wlroots-rga-bench.c`） 每操作每引擎 5 轮 × 60 帧，每轮预热 5 帧，
轮次交错先后顺序。各引擎分配独立 cached `system-dma32` 源 / 目标，内容、尺寸和 stride 相同。
计时包围 begin / add / submit，包括实际 CPU 缓存交接、fence poll 和同步任务等待；
不包括分配、初始化、预热及像素回读。输入是已同步写完的 CPU 图案，计时期间源内容固定。

下表时间是 5 个轮次中位数的中位数；加速比 = Pixman / RGA，小于 1 表示更慢。
CPU 是每轮调用进程 CPU 时间 / 60 帧的中位数，不包含系统异步 worker 的全部 CPU。
该负载没有真实显示扫描、GPU 客户端、每帧 producer 写入或多个 surface 合成。

| 负载 | Pixman ms | RGA3 `3_0` ms | 加速比 | Pixman / RGA 进程 CPU ms |
| --- | ---: | ---: | ---: | ---: |
| 1920×1080 全帧复制 | 0.856 | 1.244 | 0.69× | 0.855 / 0.029 |
| 1920×1080 中心 1/4 面积复制 | 0.325 | 0.705 | 0.46× | 0.326 / 0.028 |
| 3840×2160 全帧复制 | 3.595 | 4.332 | 0.83× | 3.593 / 0.030 |
| 3840×2160 中心 1/4 面积复制 | 1.213 | 1.733 | 0.70× | 1.212 / 0.030 |

两个 RGA3 在本次全帧复制中接近；RGA2 在同样 DMA32 缓冲上更慢。

| 分辨率 / 全帧复制 | RGA3 `3_0` ms | RGA3 `3_1` ms | RGA2 ms |
| --- | ---: | ---: | ---: |
| 1920×1080 | 1.244 | 1.233 | 1.670 |
| 3840×2160 | 4.332 | 4.240 | 6.162 |

同进程纯 Pixman 的全帧复制约 0.84–0.86 ms / 3.54–3.63 ms；
另编的未修改原版约 0.801 ms / 3.575 ms。原版也快于当前 RGA，同步延迟无加速。
默认 RGA3 的 alpha 回退约慢 9%–11%，scale 回退约慢 50%–51%，
原因包括 RGA 已导入缓冲后的 CPU 缓存维护成本。SoC 温度为 35.2–37.9℃。
没有预设速度验收阈值；这里报告测量，不把释放 CPU 等同于提高帧率。

## 正确性与证据

复制 / 裁剪检查可见 RGB 的数学判据、未覆盖区域和 stride padding，XRGB 的 X 字节不比较。
源图案在 4096×4096 范围内每个坐标具有不同 RGB，避免重复图案掩盖错误源偏移。
透明 / 缩放与同库纯 Pixman 的单帧和每轮最终输出比较。
另测 source crop + destination 平移、双 clip、极小 clip 回退、同 pass 的 RGA → Pixman → RGA。
本轮缓冲格式为 XRGB8888；ARGB8888 分支尚未做独立板测。
预热、正确性和计时阶段都检查 jobs / fallbacks / errors，不把错误排除在预热之外。

三核的 1080p / 4K 正式样本、1919 宽 padding 和 RGA3 system heap 均退出 0，像素差异 / errors 为 0。
两个加速操作的计时阶段各 300 次 job、0 fallback；alpha / scale 各 300 次 fallback、0 job。
RGA2 + system 参数在分配前以退出码 2 拒绝。
另用 300 帧 4K copy 观察 kernel load，指定核对应 scheduler 出现 83% / 51% / 97% 负载，
作为真实硬件活动的证据；负载窗口可能包含前一核，不据此断言排他性或加速比。

现有 Cage 使用临时 LD_LIBRARY_PATH 成功加载补丁库，运行 weston-simple-shm 后正常退出。
这是库加载与启动冒烟测试，没有测整屏输出 oracle、Chromium 帧率或真实 GPU producer 并发。
测试后恢复了原 kiosk / 上位机状态及 HDMI 调度，HTTP 返回 200。


## 为什么两个引擎不能共用 DMA-BUF

目标 BSP 的 `rk_system_heap.c` 在 CPU START / END 时只同步已映射的 attachments。
RGA import 经 `rga_mm_import_buffer` 建立持续到 handle release 的映射；之后 CPU 访问要做
整块缓存维护，clip 也付全帧成本。共用缓冲会增加纯 Pixman 的同步开销，使加速比失真。
独立缓冲下的纯 Pixman 与单独构建的未修改 wlroots 基线接近。
混合 renderer 自己回退时仍支付这些缓存成本，这是方案的实际开销。

## cached 与 uncached 的同板对照

同一天、同库、同 CPU 与计时范围，重新测 `system-dma32` / `system-uncached-dma32`，
两端使用相同 heap，各 renderer 缓冲仍独立。RGA3 `3_0`，每项 5 轮 × 60 帧、预热 5 帧。
1080p 先 cached 后 uncached，4K 反向；每次运行内交错两引擎。SoC 为 35.2–37.9℃。

| 负载 | Pixman cached ms | RGA cached ms | Pixman uncached ms | RGA uncached ms |
| --- | ---: | ---: | ---: | ---: |
| 1080p 全帧复制 | 0.827 | 1.240 | 6.680 | 1.245 |
| 1080p 中心 1/4 复制 | 0.322 | 0.681 | 1.689 | 0.664 |
| 4K 全帧复制 | 3.599 | 4.328 | 25.519 | 4.311 |
| 4K 中心 1/4 复制 | 1.217 | 1.735 | 6.441 | 1.734 |

RGA 全帧复制换 heap 后变化 +0.47% / −0.38%，本轮未显示明显收益。
两端 uncached 时的 Pixman / RGA 汇总耗时比为 5.36× / 5.92×，主要是 Pixman 变慢。
alpha / scale 仍回退：4K 混合 renderer 的 alpha 为 23.14 → 154.50 ms，
scale 为 5.53 → 22.68 ms。uncached 不能替代算子覆盖或自动提升混合负载性能。

BSP `rk_system_heap.c` 的 uncached 用户映射是 `pgprot_writecombine`，CPU START / END
跳过 attachment 缓存维护；仍保留 DMA-BUF 同步契约与设备完成等待。
`importbuffer_fd` 复用 handle 的 RGA 路径没有每 job 强制 flush，纯 RGA pass 也不访问 CPU，
因此缓存属性主要改变 CPU 侧成本。初始化、初次 import 不在计时范围内。

两种 heap 的正式样本、uncached padding / 混合路径、RGA2 uncached DMA32 冒烟均退出 0，
像素差异与 errors 为 0。原始轮次与汇总见 cache-results.json（镜像源码：`packages/wlroots-rga/cache-results.json`）。

## 真实 Cage / Chromium 的缓冲

板上当前生产 Cage 加载系统库；Cage 与 Chromium GPU 进程共享 `rockchipdrm` DMA-BUF，
attachment 为 Panthor。快照中无 system dma-heap 对象；生产 GBM 对象的格式、modifier、
CPU cache flags 未从 fdinfo / debugfs 确认，不能仅凭 exporter 名称断言所有对象都为 WC。

实验 Pixman / RGA 在 DRM 后端选择 dumb allocator，创建 flags=0，BSP CPU 映射为 WC。
Rockchip DRM CPU hooks 同步自身 sg-table，与 uncached system heap 的同步成本不同。
客户端 `wl_shm` 则通常是普通 cached CPU 内存，因此不能把两端 uncached 的倍率外推到它。

当前实验 renderer 沿用 Pixman impl，没有 `get_dmabuf_texture_formats` / `get_drm_fd`，
`wlr_renderer_init_wl_display()` 不发布 `wl_drm` / `linux-dmabuf`；普通 Chromium 提交 SHM，
本版不加速 SHM。直接导入 DMA-BUF 的基准仅证明局部 render-pass 能力。
后续真实合成器对照需要补客户端 DMA-BUF 纹理导入及协议能力；单独开放协议不足，
客户端 DMA-BUF buffer 本身没有 Pixman 所需的 `begin_data_ptr_access`。

## 复现与产物

按 实验构建说明（镜像源码：`packages/wlroots-rga/README.md`） 构建后，分别选择 `WLR_RGA_CORE=3_0|3_1|2`，
运行 `--width 1920 --height 1080` 和 `--width 3840 --height 2160`，均 `--frames 60 --rounds 5`。
另用 `--width 1919 --height 1080 --frames 8 --rounds 2` 检查非空 padding，
只在 RGA3 使用 `--heap system` 检查另一种缓冲来源。

构建入口已在板上从固定源包完整构建两份独立库及基准，语法、ShellCheck 与 --help 检查通过。
精简测量原始轮次和汇总保存在 results.json（镜像源码：`packages/wlroots-rga/results.json`）。

- 补丁 SHA256：`0508cbaf8ca712338c624ea1e38831b7d5539ee317f4f36148fd3e67a874559a`。
- 首轮 cached 基准 SHA256：`953fdf3866da3ce10a575d318ddb1d5b56286409848950b210fdd5645876287c`。
- cached / uncached 对照基准 SHA256：`d8ad313c7d864c0883af166cbae78e51fe2c10e9b547c2086605649773750cae`。
- 完整源码在本机 `build/wlroots-rga/wlroots-0.17.1/`。
- 开发板实验目录 `build/wlroots-rga-final-x11/`；最终日志在 `measurements-final/`，
  编译及测量退出码分别见目录外 `.build.exit` 和目录内 `measure-unique.exit`。
