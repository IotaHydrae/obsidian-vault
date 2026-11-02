# LVGL 9.6.0：RGA 混合后端与 NanoVG GPU 绘制对照

> RK3588 上，本实验的 1080p 控件场景每帧：NEON 软件 2.81 ms、RGA + 软件
> 5.06 ms、NanoVG GPU 2.18 ms。GPU 比该 RGA 原型快约 2.32 倍；
> GPU 与软件存在像素差异，此倍率仅是完成帧耗时观察，不证明图像等价。

## 结果与适用范围

2026-10-15，LubanCat-5 V2、RK3588 8 GiB、内核 6.1.172 #10，
Mesa 25.2.8、panthor / Mali-G610，librga deb 2.2.0-1 / API 1.10.1_[9]。
LVGL 为原版 [v9.6.0](https://github.com/lvgl/lvgl/releases/tag/v9.6.0)，GCC 13.3、
`-O2`、软件启用 AArch64 NEON。没有安装或替换系统 LVGL、Cage、wlroots。

全屏离屏刷新，无 KMS、vsync 或显示呈现。每个场景每组交错测三轮，
每轮预热 30 帧、测 180 帧；表中取三轮 **平均帧时间的中位数**。
计时从 UI 更新开始，到同步 RGA 或 `glFinish`、最后 flush 完成。
静态字体 / 图片缓存复用，计时内没有像素回读；setup / 首帧成本单独记录。

| 场景 | NEON SW ms/帧 | RGA + SW ms/帧 | NanoVG GPU ms/帧 | RGA/GPU 耗时比 |
| --- | ---: | ---: | ---: | ---: |
| 800×480 普通矩形 / 图片 | 0.177 | 1.665 | 0.454 | 3.67 |
| 800×480 控件 | 0.986 | 1.826 | 1.441 | 1.27 |
| 1920×1080 普通矩形 / 图片 | 0.979 | 3.959 | 0.985 | 4.02 |
| 1920×1080 控件 | 2.807 | 5.060 | 2.177 | 2.32 |

1080p 控件中，GPU 比 NEON SW 快约 1.29 倍；800×480 两个场景则是 SW 最快。
不能据此判断较弱 CPU、其他 GPU 驱动或 RK3506 的倍率。
也不能将 RGA 原型的结果推广为完整 RGA 后端的性能上限。

| 控件场景 | SW 进程 CPU ms/帧 | RGA 进程 CPU ms/帧 | GPU 进程 CPU ms/帧 |
| --- | ---: | ---: | ---: |
| 800×480 | 0.985 | 1.406 | 1.158 |
| 1920×1080 | 2.805 | 3.301 | 1.283 |

三轮 wall 均值范围：800 控件 SW 0.984–0.988、RGA 1.825–1.829、
GPU 1.434–1.445 ms；1080p 控件 SW 2.807–2.810、RGA 5.059–5.064、
GPU 2.163–2.179 ms。没有速度 PASS 阈值，以上均为观测。
完整单轮均值、分位数、CPU 时间和冷启动成本在
原始结构化数据（镜像源码：`packages/lvgl-rga-gpu/results.json`）。

## 三条路径实际做了什么

- **SW**：LVGL 软件 draw unit，启用 NEON。
- **RGA + SW**：本实验新增 draw unit，仅接不透明、无圆角 / 渐变的矩形
  fill，以及不缩放 / 旋转 / 混合的静态 XRGB 图片复制；分别固定 RGA2 和 RGA3 core0。
  文字、圆角、阴影、alpha 等回退同一套 NEON SW。
- **GPU**：官方 `DRAW_NANOVG` 真正提交图形、字体纹理、阴影和图片绘制。
  未用以 CPU 栅格缓存纹理为主的 `DRAW_OPENGLES` 代表整套 GPU 绘制。
  实际 renderer 为 `Mali-G610 (Panfrost)`，不是 llvmpipe。

`opaque` 为 8 个移动 / 变色普通矩形、背景和一张移动棋盘格图；
`widgets` 加入文字、圆角、边框、阴影和 alpha 覆盖层。
动画由帧号驱动，输入、tick、字体和资源相同；每帧全屏 invalidate。
FULL 模式不发局部 invalidation 事件，数据中该事件计数为 0，实际工作量
应读 `flush_pixels=帧数×宽×高`，不能解释为没有脏区。

每轮控件场景共 6,660 个 draw task：RGA 处理 360 个、SW 回退 6,300 个；
GPU 执行全部 6,660 个，SW 回退为 0。普通场景共 1,800 个任务：
800×480 的 RGA 处理 1,795 个、回退 5 个；1080p 处理 1,798 个、回退 2 个。
偏移为 1 的矩形会被当前 librga 校验器拒绝，evaluate 提前让 SW 接管；
这是 **API 实测限制**，不把错误提示当作硬件规范。

SW / RGA 目标和图片都来自 cached `system-dma32`，保证 RGA2 页地址小于 4 GiB。
RGA 同步作业之间保持设备所有权，软件任务前和帧结束恢复 CPU 所有权，
缓存同步成本包含在帧时间内。1080p 控件每轮有 1,080 次缓存同步，
按整个 DMA-BUF 映射大小累计约 6.16 GB；该数是同步覆盖量，不是实际 DRAM 流量。
GPU 使用 tiled RGBA8 / stencil8 FBO，CPU / RGA 使用 linear XRGB8888，
因此这是后端端到端对照，不能称作纯硬件算力比。

## 正确性与负控

Oracle 为 `RELATIONSHIP`：同一版本、同一确定性场景，以独立 NEON SW 捕获为参照。
每组另测最终帧号 20 和 210，读回在计时外，统一为 RGB PPM，忽略 XRGB 的 X 字节。

- RGA 的 8 张捕获与 SW 逐像素一致；真实 completed jobs、日志、释放错误均为 0 错误。
- GPU 的 8 张捕获均有差异。帧 210 普通场景差异像素约 1.45% / 1.16%，
  控件约 17.19% / 13.07%（800×480 / 1080p），比例不代表主观失真程度。
  已定位普通场景差异分布在矩形边缘和棋盘格格线，NanoVG 图片默认使用线性采样。
  未认可 AA / 阴影差异的数值容差；精确相等条件不成立，图像等价结论为 `INCONCLUSIVE`。
- 两个分辨率都故意漏画一个矩形，捕获检出变化，负控通过。
- 三后端三轮的任务类型、几何 hash、完成帧数和刷新像素一致；
  GPU 计时阶段 `glReadPixels=0`，实际任务全部走 NanoVG。

runner 完成退出 5，表示存在 GPU 正确性无法判定项；不能把“程序画完了”
或主程序路径检查的 PASS，当作 GPU 与软件图像一致。
这些数据未测局部刷新、旋转 / 缩放、动态图片上传、复杂 layer 或真实屏幕节拍。

## 复现中需控制的两件事

**GPU 频率**：只写 `performance` 不够。现场仍测到 200 MHz，原因是
RK3588 GPU 的 `opp-suspend` 为 200 MHz；BSP devfreq 休眠设置目标时覆盖了
`resume_freq`，恢复仍取 200 MHz，而 performance governor 不处理恢复事件。
Panthor 没有 `get_cur_freq`，`cur_freq` 仅返回 devfreq 保存值。
相关源码：`rk3588s.dtsi` 的 GPU OPP、`drivers/devfreq/devfreq.c` 的
`devfreq_set_target()` / `devfreq_suspend_device()` / `devfreq_resume_device()`，
以及 `governor_performance.c`。

正式测量临时设置 GPU `power/control=on`、`min_freq=1000000000`，
CPU4 performance / 2.304 GHz。运行采样为 1 GHz，并用
`debugfs/clk/scmi_clk_gpu/clk_rate=1000000000`（`CLK_GET_RATE_NOCACHE`）交叉验证。
测试结束恢复 GPU simple_ondemand、min 200 MHz、power auto。
温度各区起点约 34.2–35.2°C，最终核对 zone0 约 34.2°C，无新增内核日志。

**NanoVG 字形 stride**：全局 `LV_DRAW_BUF_STRIDE_ALIGN=64` 会触发本版本越界：
`lv_nanovg_reshape_global_image()` 分配紧凑 `w*h` A8，
`letter_create_cb()` 将它交给 `lv_font_get_bitmap_fmt_txt()`，后者按全局对齐 stride 写入。
ASan 在原配置实证 `heap-buffer-overflow`（`lv_font_fmt_txt.c:146`）。
本实验不改上游源码，三组统一使用默认 stride alignment 1；800 / 1920 的
32 位缓冲仍满足 RGA 对齐。两个场景修后 ASan 复测均退出 0；ASan 数据不计性能。

构建与运行入口见 实验 README（镜像源码：`packages/lvgl-rga-gpu/README.md`）。
结束后已确认 HDMI 调度恢复、kiosk / 上位机 / socket active、GDM inactive、HTTP 200；
没有残留实验任务或临时阻止图形启动的 drop-in。
