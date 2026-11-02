# RK3588 的 RGA：三个核、能力差异、什么时候用哪个

> 一句话：**两个 RGA3（一模一样）+ 一个 RGA2（上一代设计）**；RGA3 走 SoC IOMMU、散页随便，
> **RGA2 自带 MMU、要求整块物理地址 < 4 GB**。常规 2D 交给驱动自动选核最稳（>4G 的 buffer 根本不会落到 RGA2）。
> 相关：基准脚本 `tests/rga-bench.sh`（数字见 §4）；
> **给 RGA2 分配 <4G 缓冲的完整可编译示例** → [rga-dma32-buffer-demo.md](rga-dma32-buffer-demo.md)；
> **相机/编码链路上到底用不用 RGA、RGA2 隐患在哪** → [rga-gstreamer-camera-pipeline.md](rga-gstreamer-camera-pipeline.md)。
> wlroots 0.17.1 的实验 renderer 与独立 DMA-BUF 对照 → [wlroots-rga-renderer.md](wlroots-rga-renderer.md)。
> 逐帧 SHM 上传到真实 DRM 输出的成本 → [wlroots-rga-replay.md](wlroots-rga-replay.md)。
> LVGL 的 RGA 混合 / NEON / 真 GPU 绘制对照 → [lvgl-rga-gpu.md](lvgl-rga-gpu.md)。

## TL;DR

- 三个核：**两个 RGA3（彼此一模一样）** + **一个 RGA2**（不是"同款弱一点"，是上一代设计）。
  能干什么**以 `/sys/kernel/debug/rkrga/hardware` 的能力位为准**，别按型号猜（§1/§2）。
- ⛔ **RGA2 的硬约束是"物理地址必须 < 4 GB"**（不是"必须连续"；散页也能用它自带的 MMU）。
  从 V4L2/DRM 拿的普通帧缓冲（8 G 板上常在 4G 以上）**只能给 RGA3**（§2.1~§2.3）。
- **散页 vs 连续没有可测性能差**（4K 上逐位相同，§2.4）—— 别为"IOMMU 会拖慢"做额外拷贝。
- `auto` 不是"随机 + 事后回退"：它是**确定性的负载均衡**，且 >4G 的作业在挑核前就把 RGA2 剔除了（§5）。
  ⚠️ 但有一条**驱动漏洞**：走 legacy `c_RkRgaBlit`（MPP 插件那条路）时标志查不到，
  auto 仍可能把 >4G 的活派给 RGA2 ⇒ 偶发失败（§5）。
- 性能（单发同步口径，§4）：**copy / cvt 上 RGA3 是 RGA2 的 ~2.1 倍**；**resize 反而 RGA2 快 ~13%**。

## 1. 有三个核：两个 RGA3（一模一样）+ 一个 RGA2（另一个东西）

板上 `/sys/kernel/debug/rkrga/hardware` 是权威口径（要 root）：

```
rga3, core 1: version: 3.0.76831     input 68x2 ~ 8176x8176    output 68x2 ~ 8128x8128    mmu: RK_IOMMU
rga3, core 2: version: 3.0.76831     input 68x2 ~ 8176x8176    output 68x2 ~ 8128x8128    mmu: RK_IOMMU
rga2, core 4: version: 3.2.63318     input 2x2 ~ 8192x8192     output 2x2 ~ 4096x4096     mmu: RGA_MMU
```

内核 dmesg（`rga` 驱动 v1.3.9 = 仓库里的 `kernel-6.1/drivers/video/rockchip/rga3/`）：

```
rga3 fdb60000.rga: probe successfully, irq = 45, hw_version:3.0.76831
rga3 fdb70000.rga: probe successfully, irq = 46, hw_version:3.0.76831
rga2 fdb80000.rga: probe successfully, irq = 127, hw_version:3.2.63318
rga: IOMMU binding successfully, default mapping core[0x1]
```

`/sys/kernel/debug/rkrga/load` 也印证是 3 个 scheduler（`scheduler[0]/[1]` = rga3、`[2]` = rga2）。

**结论：core 1 / core 2 两个 RGA3 完全一样**（同 hw 版本、同能力、同 `RK_IOMMU`）；第四个 core 是 RGA2。

## 2. RGA2 与 RGA3 的能力差异（debugfs 逐项对照）

| 项目 | rga3 ×2 | rga2 |
| --- | --- | --- |
| hw 版本 | `3.0.76831` | `3.2.63318` |
| 输入尺寸 | 68x2 ~ 8176x8176 | 2x2 ~ 8192x8192 |
| **输出尺寸** | 68x2 ~ **8128x8128** | 2x2 ~ **4096x4096** |
| 缩放范围 | 1/8 ~ 8 | 1/16 ~ 16 |
| byte_stride 对齐 | 16 | 4 |
| max byte_stride | 32768 | 32768 |
| CSC（RGB↔YUV） | `0xf / 0xf` | `0x7 / 0x7`（能力更少） |
| feature 位 | `0x4` | `0x205f` |
| **MMU** | **RK_IOMMU**（SoC 的 IOMMU 翻译，散页随便） | **RGA_MMU**（自带页表，**物理地址必须 < 4 GB**） |

### 2.1 ⛔ RGA2 的物理地址必须在 4 GB 以内（不是"必须连续"）

2026-09-15 更正：RGA2 能用散页，只要这些页都在 4 GB 以下。它在 `system` heap 上失败的真实原因是地址超了 4G：

```
rga: RGA_MMU unsupported memory larger than 4G!
rga: scheduler core[4] unsupported mm_flag[0x0]!
```

实测（1080p copy，同一份程序只换 heap）：

| heap | 物理连续性 | rga3 | rga2 |
| --- | --- | --- | --- |
| `cma` | 连续 | OK | OK（连续 → 直接给物理地址，**不走它的 MMU**） |
| `system-dma32` | **散页，但 < 4G** | OK | **OK**（走 RGA_MMU + 驱动编的软件页表） |
| `system-uncached-dma32` | 散页，< 4G | OK | **OK** |
| `system` / `system-uncached` | 散页，8 G 板上会落到 4G 以上 | OK（RK_IOMMU） | **EINVAL** |

驱动里的四条判据（`kernel-6.1/drivers/video/rockchip/rga3/`）：

- `rga_mm.c:400` `map_dev = scheduler->iommu_info ? iommu_info->default_dev : scheduler->dev;`
  → **RGA3 有 iommu_info，走 RK_IOMMU**；RGA2 没有，只能靠自己的 MMU 或物理地址。
- `rga_mm.c:320` `rga_mm_check_range_sgt()`：所有 sg 页都 < `0xffffffff` 才算 `RGA_MEM_UNDER_4G`。
- `rga_mm.c:304` `rga_mm_check_memory_limit()`：`mmu == RGA_MMU && !UNDER_4G` → `-EINVAL`。
- `rga_mm.c:1041` `rga_mm_is_need_mmu()`：RGA3（`RGA_IOMMU`）直接 `return false`；
  RGA2 则连续内存不用 MMU（直接物理地址）、散页用它的 MMU（`rga_iommu.c:48` `rga_set_mmu_base()`）。

⇒ 想用 RGA2，缓冲区要么 CMA（连续，最省事），要么保证**整块都在 4 GB 以下**（`system-dma32` 等）。

### 2.2 用户态怎么保证给 RGA2 的内存在 4 GB 以内

用户态**没有 API 能查一块 dma-buf 的物理地址**（`/proc/self/pagemap` 对 dma-buf 映射返回 PFN=0，实测），
所以**不能"检测"，只能"从正确的池子里分配"**：自己分配就用 `/dev/dma_heap/system-dma32`
（散页、整块 <4G、不占 CMA）；`cma` 只在需要连续内存时用（本板只有 256 MB）；
别的进程给的 fd（V4L2/DRM/GBM）地址未知 ⇒ 交给 RGA3，或"试 RGA2、失败回退 RGA3"（失败是干净的 EINVAL）。

池子对比、`mm_flag` 判据（`bit0 = RGA_MEM_UNDER_4G`）、`GFP_DMA32`↔ZONE_DMA 的机制、
以及**完整可编译示例**（含 cache 同步与试-回退）都在
→ [rga-dma32-buffer-demo.md](rga-dma32-buffer-demo.md)。

### 2.3 实测：真实 V4L2（相机）的 buffer 能不能给 RGA2

2026-09-15 在**工程机（三路 UVC）**与**开发板**上实测（`VIDIOC_EXPBUF` → `importbuffer_fd` →
`imconfig` 钉核 → `imcopy`，同时读 `mm_session` 的 `mm_flag`）：

| 来源 | vb2 分配器 | 实测 `mm_flag` | RGA2 | RGA3 |
| --- | --- | --- | --- | --- |
| **UVC 相机**（`/dev/video1/3/5`，MJPG 1920x1080 / 3.96 MB） | `vb2_vmalloc_memops`（**普通匿名页**） | **0x2**（没有 UNDER_4G） | **FAIL（3/3）** | OK |
| HDMI RX（开发板 `/dev/video0`，`rk_hdmirx`） | `vb2_dma_contig_memops`（CMA，连续） | 0x7（`pa=0x10900000`） | **OK** | OK |
| MIPI 相机（`rkisp` / `rkcif`） | `vb2_cma_sg_memops`（CMA 散页） | 未实测（没有 MIPI 相机接入） | 预期 OK | OK |

**是内存问题，不是格式/尺寸问题**（同尺寸 1920x1080、同一份程序，只换 buffer 来源/格式）：

```
UVC(0x2) + NV12      RGA2 → FAIL      RGA3 → OK
CMA(0x7) + NV12      RGA2 → OK        ← 对照：<4G 就没问题
CMA      + YUYV422   RGA2 → OK        ← 说明 RGA2 支持 YUYV422（前面 UVC 的失败不是格式）
CMA      + RGBA8888  RGA2 → OK
```

两种 dmesg 报错形态（都表示"RGA2 用不了这块内存"）：

```
rga: ID[19]: no core match          ← 策略层（rga_policy.c:475）挑不到能用的核
rga: ID[19]: job assign failed
rga: RGA_MMU unsupported memory larger than 4G!     ← 映射层（rga_mm.c:304）
rga: scheduler core[4] unsupported mm_flag[0x0]!
```

⇒ **相机帧（UVC）基本没法和 RGA2 搭配**；`rkisp/rkcif/hdmirx` 这类 CMA 分配的才可以。
**一键复现**：`sudo bash tests/rga-v4l2-probe.sh`（扫描所有 capture 节点，逐台判"是不是 >4G 引起的"，
再跑三组对照排除格式因素）。用法与参考值见 `tests/README.md`。

### 2.4 散页会不会掉性能？—— 没有可测差异

（2026-09-15 用户追问"IOMMU 对不连续的物理内存会不会影响性能"）实测 `rga3_0`，交错 5 轮 × 50 次取中位数（MP/s）：

| 尺寸/算子 | cma（连续） | system（散页） | system-uncached（散页） |
| --- | --- | --- | --- |
| 4K copy | 2699.2 | 2700.2 | 2700.3 |
| 4K cvt | 2135.2 | 2133.9 | 2133.0 |
| 4K resize | 1165.2 | 1164.9 | 1164.9 |
| 1080p copy | 2551.8 | 2237.4 | 2182.3 |
| 1080p resize | 1053.8 | 1052.1 | 1054.4 |

- **4K 上几乎没有差别**（三档逐位相同，偏差 < 0.1%）—— 4K 最能暴露 IOMMU 页表/TLB 开销，所以结论可信。
- 1080p 表里那 ~10% 是**噪声**：同样配置重跑会出现两个离散档位（copy ≈0.81 ms 与 ≈0.96 ms 交替，
  与 heap 无关）。20 轮**配对**测量（每轮 200 次）：cma 中位 **0.8140 ms**、`system-dma32` 中位 **0.8190 ms**，
  配对差值中位 **+0.001 ms**、比值中位 **1.001**。RGA2 也一样（`cma` vs `system-dma32` 比值 0.991~1.008）。
- 为什么没影响：RGA 一次搬运是**大块线性**读写，页表遍历/TLB miss 被几十 KB~几 MB 的连续突发摊薄；
  算子本身受内存带宽限制（1080p copy ≈2.5 GPix/s ≈7.8 GB/s 读写合计）。
  ⚠️ 边界：本测试是**单客户端、单发同步**；多客户端（VPU/ISP/PCIe）同时压 IOVMM 时没测过，别外推。

## 3. 怎么用（im2d API，librga 2.2.0-1 / im2d 1.10.1）

```c
imconfig(IM_CONFIG_SCHEDULER_CORE, IM_SCHEDULER_RGA3_CORE0);   /* 或 RGA3_CORE1 / RGA2_CORE0 */
rga_buffer_t a = wrapbuffer_fd(src_fd, W, H, RK_FORMAT_YCbCr_420_SP);
rga_buffer_t b = wrapbuffer_fd(dst_fd, W, H, RK_FORMAT_YCbCr_420_SP);
imcopy(a, b);            /* imresize / imcvtcolor 同理，默认同步等完成 */
```

- `IM_SCHEDULER_DEFAULT`（= 0）**不能**传给 `imconfig`（会被拒；不传就是让驱动自己挑）。
- 头文件在 **`/usr/include/rga/`**（`librga-dev` 装的），编译 `cc -O2 -I/usr/include/rga … -lrga`。
- ⚠️ **dma-buf 的 cache 同步常量别记错**（第一版就写错过，结果 memcmp 全是不一致）：
  内核 uapi 是 `DMA_BUF_SYNC_READ=1`、`WRITE=2`、`START=0`、`END=4` ——
  "CPU 写完给设备看" = `START|WRITE` → 写 → `END|WRITE`；"设备写完给 CPU 看" = `START|READ` → 读 → `END|READ`。
  `*-uncached*` heap 的 CPU 映射是 Write Combine，exporter 跳过 attachment 缓存维护，
  但仍保留 START / END 契约及设备完成等待；不因 uncached 删除同步调用。
  CPU 读取会变慢，RGA 侧旧测量吞吐接近，见 §2.4；wlroots 对照见
  [render-pass 测量](wlroots-rga-renderer.md)。

## 4. 性能实测（`tests/rga-bench.sh`，单发同步口径，1080p 20 次 / 4K 15 次，warmup 3）

**1080p（1920x1080）**

| 核 | copy | resize（→960x540） | cvt（NV12→RGB888） |
| --- | --- | --- | --- |
| rga3_0 | 0.81 ms / **2551 MP/s** | 1.89 ms / 1099 MP/s | 1.03 ms / **2023 MP/s** |
| rga3_1 | 0.80 ms / **2578 MP/s** | 1.87 ms / 1112 MP/s | 1.01 ms / **2049 MP/s** |
| rga2 | 1.77 ms / 1174 MP/s | 1.67 ms / **1246 MP/s** | 1.65 ms / 1259 MP/s |
| auto（驱动挑）| 0.83 ms / 2487 MP/s | 1.89 ms / 1096 MP/s | 1.02 ms / 2025 MP/s |

**4K（3840x2160）**

| 核 | copy | resize（→1920x1080） | cvt |
| --- | --- | --- | --- |
| rga3_0 | 3.06 ms / 2710 MP/s | 7.11 ms / 1166 MP/s | 3.88 ms / 2137 MP/s |
| rga3_1 | 3.00 ms / 2761 MP/s | 7.04 ms / 1178 MP/s | 3.79 ms / 2191 MP/s |
| rga2 | 6.55 ms / 1267 MP/s | 6.15 ms / **1348 MP/s** | 6.09 ms / 1363 MP/s |

读法：

- **两个 RGA3 一样快**（逐项差 2~6%，是轮次噪声）。
- **copy / cvt：RGA3 是 RGA2 的 ~2.1 倍**（1080p copy 2578 vs 1174 MP/s）。
- **resize：RGA2 反而略快 ~13%**（1246 vs 1099）—— "RGA3 一定全面更快"是错的。
- 口径提醒：这里是**一次调用一个 job、同步等完成**。~0.8 ms 里很大一块是 per-job 底座，
  **别把这些数字当硬件吞吐上限**；要比并发/流水线得用 `im_commit`+`imsync`（本测试没做）。
- 4K 时 RGA2 已接近它的输出上限 4096x4096；RGA3 可以到 8K。

## 5. 选核与内核兜底：`auto` 到底安不安全

内核侧的机制（`rga_job.c` / `rga_policy.c`）：

- `rga_job.c:66-103`：作业准备时逐个查 src/dst/pat 的 `mm_flag`，**任何一块没有 `RGA_MEM_UNDER_4G`
  就置 `RGA_JOB_UNSUPPORT_RGA_MMU`**；`rga_policy.c:344` 挑核时**跳过所有 `RGA_MMU` 核（= RGA2）**。
- 选核**不是随机**（`rga_policy.c:478-502`）：候选核按 `[rga3#1, rga3#2, rga2]` 顺序扫，
  哪个当前空闲（`running_job == NULL`）就选哪个，都不空闲时选 `job_count` 最小的。
- **过滤发生在选核之前**，不是"先给 RGA2 再退回" ⇒ 不显式指定核时，>4G 的作业从来不会成为 RGA2 的候选
  （两个 RGA3 都忙时它排队等 RGA3）。MPP 的 buffer 池是多块轮转的，每块 buffer 的 `mm_flag` 在
  import/map 时算一次、之后不变 ⇒ 不同帧可能走不同核，**看起来像随机**。

实测（用 librga 老 API `c_RkRgaBlit`，即 MPP 插件走的那条路，`core` 留 0 = auto）：

```
heap=system        （mm_flag 无 UNDER_4G） core=auto → OK    core=RGA2 → EINVAL   core=RGA3 → OK
heap=system-dma32  （<4G）                core=auto → OK    core=RGA2 → OK       core=RGA3 → OK
heap=cma           （<4G）                core=auto → OK    core=RGA2 → OK       core=RGA3 → OK
```

### 5.1 ⛔ 但有一条驱动漏洞：legacy API 查不到 flags

`rga_job_judgment_support_core()`（`rga_job.c:48`）用 `req->src.yrgb_addr` 去 mm session 里
**按 handle 查 flags**（`rga_mm_lookup_flag()`），**查不到时直接 `goto out_finish` —— 没有置
`RGA_JOB_UNSUPPORT_RGA_MMU`**。而：

| 提交方式 | 候选核掩码（0x7 = RGA3×2+RGA2，0x3 = 仅 RGA3） | 说明 |
| --- | --- | --- |
| **legacy `c_RkRgaBlit`**（MPP 插件用的）+ >4G buffer，连续两次 | **0x7 / 0x7** | 请求里带的是 fd/地址、不是 session handle；internal buffer 要到 commit 映射时才建 ⇒ **永远查不到，不是"首次"问题** |
| im2d（`importbuffer_fd` → handle）+ >4G buffer | **0x3** | import 时就算好 `mm_flag` ⇒ RGA2 被正确剔除 |
| legacy + `system-dma32`（<4G） | 0x7 | <4G 时 RGA2 本来就该是候选（能跑） |

打开这个日志：`echo msg > /sys/kernel/debug/rkrga/debug`，然后看 `dmesg | grep "matched cores"`。

⇒ 这种情况下 RGA2 **仍是候选**；若两个 RGA3 都忙（多路并发就是），负载均衡会选 RGA2，然后映射失败：

```
rga: 4945 4959 : scheduler core[4] unsupported mm_flag[0x0]!    ← core4 = RGA2
rga: 4945 4959 : ID[22644]: job buffer map failed!
rga: 4945 4959 : ID[22644]: request commit failed! / submit failed!
```

2026-09-15 在开发板上**抓到过一次**（三路并发的 RGA 转换路径），随后 60 s 同样压测**没复现**
⇒ 偶发，取决于"那一刻两个 RGA3 是否都忙 + 那块 buffer 的 flags 是否还没建立"。

**规避（二选一）**：

1. **应用侧（立刻可用）**：别让 legacy 路径产生这块 >4G buffer 的 RGA 作业（例如把某步
   `NV16→NV12` 改到 CPU 里做，确定性最高），或者显式钉 RGA3（别钉 RGA2）。
   具体到相机链路见 [rga-gstreamer-camera-pipeline.md](rga-gstreamer-camera-pipeline.md)。
2. **内核侧（更彻底，需重编内核验证，未实施）**：把 `rga_job_judgment_support_core()` 里
   lookup 失败的分支改成保守置位 `job->flags |= RGA_JOB_UNSUPPORT_RGA_MMU`
   （"证明不了 <4G 就别用 RGA2"）。

## 6. 什么时候用哪个 + 怎么排查

- 常规 2D（拷贝 / 格式转换 / 缩放）：交给 `auto`（驱动派给 RGA3），**不要手工钉 RGA2**。
- 想把三个核都用起来：显式钉 `RGA3_CORE0/1` 做多线程分流；RGA2 作为补充算力也行，
  但它的缓冲区必须整块 < 4 GB（自己分配就用 `/dev/dma_heap/system-dma32`），
  而且 4K 以上直接超它的输出上限 4096x4096。
- 排查"RGA 没生效/报错"：先看 `/sys/kernel/debug/rkrga/load`（各核 load）与 `scheduler_status`，
  再看 `dmesg | grep rga`。`RGA_BLIT fail: Invalid argument`：
  对 **RGA2** 十有八九是地址超 4G（配 `RGA_MMU unsupported memory larger than 4G!`），其次是超尺寸/stride；
  对 **RGA3** 则往尺寸/stride/格式能力上找（散页内存没问题）。

## 7. 怎么复现

```bash
# 板子上（普通用户即可；前置检查与 debugfs 部分会尽量用 root）
bash tests/rga-bench.sh                  # 1080p + 4K 全核 + 内存能力/性能对照
bash tests/rga-bench.sh --resolution=1080p --iters=50 --keep

# 相机/V4L2 buffer 能不能给 RGA2（扫所有 capture 节点）
sudo bash tests/rga-v4l2-probe.sh
```

判定：每个"核×算子×分辨率"都必须 `status=OK` 且输出核对通过；此外断言 **RGA2 在 `<4G` 的散页
（`system-dma32`）上必须 OK**（证明限制是 4G 而不是连续性），并做一次"散页 vs 连续"的性能 A/B
（差异 > 20% 才告警）。手工换 heap 做更细对照的命令见脚本 `--help`（`rga-bench.c` 支持 `--heap=`）。

## 8. 相关

- **给 RGA2 分配 <4G 缓冲的完整示例**（可直接编译）：[rga-dma32-buffer-demo.md](rga-dma32-buffer-demo.md)
- **相机 → NV12 → MP4 链路用不用 RGA / RGA2 隐患在哪**：[rga-gstreamer-camera-pipeline.md](rga-gstreamer-camera-pipeline.md)
- GPU（另一套硬件）：[gpu.md](gpu.md)；实时占用率：[rk3588-hw-monitor.md](rk3588-hw-monitor.md)
- 测试约定与参考值：`tests/README.md`
