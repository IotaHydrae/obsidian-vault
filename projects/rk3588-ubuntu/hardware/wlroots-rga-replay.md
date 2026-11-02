# wlroots RGA：逐帧 SHM 上传是否划算

计入逐帧上传、同步和纹理导入后，当前 RGA 候选路径在全帧更新中慢于 Pixman。
uncached staging 改善了局部更新：4K、每帧更新 1/16 面积时，合成耗时约 2.23 ms，
同轮 Pixman 为 2.39 ms，约 1.07×。这项收益限于所测负载，尚不足以证明真实 kiosk 加速。

本篇是第一层可控重放；独立 DMA-BUF 微基准与 renderer 实现见
[wlroots-rga-renderer.md](wlroots-rga-renderer.md)。代码与构建见
实验入口（镜像源码：`packages/wlroots-rga/README.md`），逐轮数据见
replay-results.json（镜像源码：`packages/wlroots-rga/replay-results.json`）。

## 测量链路

```text
Pixman：CPU 每帧更新 cached SHM → Pixman → DRM dumb 输出
RGA：   CPU 每帧更新 cached SHM → DMA staging 上传 → RGA3 → DRM dumb 输出
```

2026-10-14，LubanCat-5 V2、8 GiB、eMMC 启动，Rockchip 6.1.172 #10；
wlroots 0.17.1-2.1build1 实验库、Pixman 0.42.2、librga 2.2.0-1、RGA driver 1.3.13。
基线是同一实验库中的纯 Pixman 构造器；两个引擎共用测试缓冲的 CPU / DMA 同步契约。
固定 A76 CPU4、performance governor，初始频率 2.304 GHz。
图形栈与 HDMI 自动调度暂停后原生编译和测量，端点各温区为 34.2–37.0℃。

- 源是普通 cached `MAP_SHARED | MAP_ANONYMOUS` 内存，CPU 每帧写入新图案。
  两个 renderer 各有独立源、独立三个输出；不共享 RGA attachment。
- 输出由 `/dev/dri/card0` 的 `rockchip` 驱动真实分配，dumb flags=0、XRGB8888、linear。
  本项目 BSP 源码将其 CPU mmap 设为 WC；这不是对任意 DRM 驱动的保证。
  使用导出的 PRIME fd 执行 CPU START / END，同步规则与 system-uncached heap 不同。
- RGA 复制场景持久分配一个 staging buffer，比较 cached `system-dma32` 与
  WC `system-uncached-dma32`。仍执行同步调用；不因 uncached 跳过设备完成等待。
- 固定 RGA3 `3_0`。staging 在 DMA32 heap，不代表 DRM 输出也在 4 GiB 以下，因此不用 RGA2。
- 默认每帧创建 / 导入 / 销毁纹理；另外测持久纹理对照，后者初次导入发生在预热期。
- 每项 5 轮 × 60 帧，预热 5 帧，每轮交换 renderer 顺序。
  cached 与 uncached 分组依次运行，每组都有交错的 Pixman 对照。

`composite` 从上传开始，到同步 render-pass 和纹理清理返回；包含 CPU / DMA 交接、
纹理创建 / 导入 / 销毁、fence 等待、RGA job 及实际回退。
`producer_to_submit` 从 CPU 生成开始，到 composite 返回，包含 submit 后的纹理销毁。
两者分别逐帧采样；分配 / 初始化单列 setup，预热及像素回读不计入稳态样本。

## 完整更新与局部更新

下表单位 ms，为五轮各自 p50 的中位数；不是合并 300 帧后的全局 p50。
加速比为同组 Pixman / RGA，大于 1 才表示更快。

| 场景 | Pixman | RGA cached staging | 加速比 | 同组 Pixman | RGA uncached staging | 加速比 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1080p 全帧 | 1.516 | 2.783 | 0.54× | 1.516 | 2.572 | 0.59× |
| 4K 全帧 | 6.155 | 9.553 | 0.64× | 6.150 | 8.873 | 0.69× |
| 1080p 局部 | 0.687 | 1.373 | 0.50× | 0.688 | 0.947 | 0.73× |
| 4K 局部 | 2.394 | 3.623 | 0.66× | 2.388 | 2.233 | 1.07× |

局部场景按 4×4 网格每帧更新一个 tile，即 1/16 面积。
单 staging 保存完整最新源，只上传本帧脏区；输出环每次复用三帧前的目标，
需绘制三次变化的并集，即本次正式尺寸的 3/16 面积、三个大矩形。
4K 每轮 producer 更新 31,104,000 像素，目标重绘 93,312,000 像素；
两个 renderer 工作量相同，RGA 每轮 180 jobs、0 fallback。

uncached 4K 局部的五轮 p50 范围为 Pixman 2.377–2.392 ms、RGA 2.229–2.237 ms，
五轮均同方向。按各轮平均时间的中位数，Pixman / RGA 为 2.497 / 2.218 ms，
约下降 11.2%；这与 p50 约下降 6.5% 是不同统计量。
含 producer 的逐帧总耗时 p50 为 3.084 / 2.926 ms。

## 纹理复用、CPU 与回退

| 全帧 cached staging | Pixman p50 ms | RGA 每帧纹理 p50 ms | RGA 持久纹理 p50 ms | 持久模式 Pixman / RGA 进程 CPU ms |
| --- | ---: | ---: | ---: | ---: |
| 1080p | 1.511 | 2.783 | 2.247 | 1.522 / 1.021 |
| 4K | 6.148 | 9.553 | 8.376 | 6.275 / 4.075 |

复用纹理减少了导入 / 释放等开销，但仍不能让本次全帧上传比 Pixman 更快。
CPU 一栏是五轮进程 CPU 平均时间的中位数；不包含所有 IRQ / kworker，
不能等同于整机 CPU 或功耗下降。uncached 4K 局部的合成进程 CPU 为 2.498 / 0.756 ms，
约下降 69.7%；cached 1080p 局部中，RGA 的进程 CPU 反而更高。

alpha / scale 直接提交 SHM 并回退 Pixman，不分配 staging。
每轮 0 RGA jobs、120 fallback（背景填充和纹理操作），两条路径耗时接近。
4K 合成 p50 分别约 36.96 ms / 9.23 ms，这些场景没有 RGA 加速。

## 正确性与适用边界

十次运行退出 0，含 1919 宽 padding 冒烟、8 组正式对照：
396 帧独立正确性检查、8656 帧计时、912 次输出回读，RGB、padding、背景及 RGA errors 均为 0。
先在每个场景 / renderer 连续九帧回读，覆盖三个目标首次使用与两轮复用；
计时阶段结束后再回读三个目标各自的最新帧，不逐帧回读干扰测量。
全帧 / 局部的 oracle 独立重放 producer 内容，不沿用 renderer 的 damage / age；
alpha / scale 用同库 Pixman 对照及背景、padding 不变量，不能证明两者共有的缺陷不存在。

临时变体在首帧之后故意不更新 staging：板上检测出 4,665,600 处 RGB 差异并退出 1。
本机 stock wlroots + SHM 的辅助负控还识别了遗漏 age 累积与一个 padding 字节破坏；
本机结果不作为 DMA / DRM / RGA 硬件证据。

重放未做 KMS 设置、scanout 或 page-flip，不测呈现 FPS、丢帧和输入延迟。
当前实验 renderer 仍不发布客户端 DMA-BUF 协议；SHM 上传是测试程序中的候选路径，
没有变成真实 Chromium 的加速实现。实际客户端多缓冲、格式 / modifier、GPU producer、
fence 并发与显示节拍仍需第二层 Cage + Chromium 验证。
本次复制判据覆盖 XRGB8888；alpha 的 ARGB8888 是 Pixman 回退，不证明 RGA ARGB 复制路径。

测量结束后已核对 kiosk / oem-host / socket active、GDM inactive、HTTP 200，
Cage 加载系统 wlroots，临时 HDMI drop-in 已删除，无实验进程残留。
结果文件保存源码、补丁、库和程序 SHA256，原始日志保留在
`build/wlroots-rga/replay-board-v1/`；开发板对应 `build/wlroots-rga-final-x11/replay-v1/`。
