# 相机 → NV12 → MP4：这条链路用不用 RGA，RGA2 隐患在哪

> 一句话：**MJPEG→NV12 是 MPP 硬解，不经过 RGA**（本镜像也没有 `rgaconvert`）；
> 真正会碰 RGA 的是 **wrists 那两路 `mpph264enc` 内部随机做的一次 `NV16→aligned NV12`**（`auto` 选核）
> —— 那正是 [rga.md](rga.md) §5.1 驱动漏洞（legacy API 查不到 `mm_flag`）唯一的真实触发点；
> chest 那路则是**纯性能问题**（白花 0.35 核/路 CPU），与 RGA2 无关。
> RGA 本身的能力/内存约束/API 见 [rga.md](rga.md)。

## TL;DR

- **没人在用 `rgaconvert`**：镜像里 `gst-inspect-1.0 | grep -i rga` 只有 replaygain 的 `rganalysis`。
  但 **MPP 插件自己链接 `librga.so.2`**、用老 API `c_RkRgaBlit`，会在编码前转一次格式。
- **wrists（`software_csc=false`）**：`mpph264enc` 内部**随机**触发 `NV16→aligned NV12` 的 RGA 转换
  （`core_mask=0x0` = auto）⇒ 三路并发时可能踩到 §5.1 的漏洞、偶发 `unsupported mm_flag` 失败。
- **chest（`software_csc=true`）**：`format=NV16 ! videoconvert ! mpph264enc`，RGA job 稳定 0（不碰 RGA），
  代价是 CPU 空转 ≈11.5 ms/帧 ≈ **0.35 核/路 @720p30**。
- **相机帧给 RGA2 必失败**（UVC 用普通匿名页 → `mm_flag=0x2`，有页 >4G），所以纪律是
  **别把相机/解码帧的活钉到 RGA2**（见 [rga.md](rga.md) §2.3）。
- 这条链路**不消耗 CMA**（MPP 走 `system*` heap）；连续录制的真问题是**容量/码率**与**异常终止丢 moov**。

## 1. 链路上谁在做转换

`triple_camera_mjpeg_publisher` 的录制链路是
`appsrc(JPEG) ! jpegparse ! mppjpegdec ! […] ! mpph264enc ! h264parse ! mp4mux`，
`software_csc` 决定中间那一段。GStreamer 侧的事实（1.24.2 + `gstreamer1.0-rockchip1 1.14-4+git240423`）：

- `mppjpegdec` sink = `image/jpeg, parsed=true`（**前面要接 `jpegparse`**），src caps 里**原生就有 NV12**
  （还有 NV21/I420/NV12_10LE40/RGB/BGR… 以及 `video/x-raw(memory:DMABuf)` 变体）；
  `mpph264enc` sink 收 NV12（`width≥96, height≥64`）。
- ⛔ **零拷贝走不通**：`mppjpegdec ! "video/x-raw(memory:DMABuf),format=NV12" ! mpph264enc` 会报
  `mpph264enc0 can't handle caps …` —— 这个版本的编码器只吃普通 `video/x-raw`（中间有一份拷贝）。
- 实测可用（1080p30 × 60 帧 → 1.87 MB Quicktime）：

  ```bash
  # 单帧 MJPEG → NV12（硬解）
  gst-launch-1.0 filesrc location=/tmp/t.jpg ! jpegparse ! mppjpegdec \
    ! video/x-raw,format=NV12 ! filesink location=/tmp/out.nv12
  # 输出 3133440 字节 = 1920×**1088**×1.5 —— 解码器把高度补到 16 对齐，读原始字节要按 stride 处理

  # 完整链路 + 相机三路（每路一条 pipeline）
  gst-launch-1.0 v4l2src device=/dev/video1 ! image/jpeg,width=1920,height=1080,framerate=30/1 \
    ! jpegparse ! mppjpegdec ! video/x-raw,format=NV12 \
    ! mpph264enc ! h264parse ! mp4mux ! filesink location=/tmp/cam1.mp4
  ```

## 2. 三路节点的实际路径（以第三次更正为准）

| 相机 | 参数 | 实际链路（代码决定） | 实测 RGA job（真实 4:2:2 输入） |
| --- | --- | --- | --- |
| chest | `software_csc=true` | `mppjpegdec format=NV16` `! videoconvert`（CPU）`! mpph264enc` | **0**（5/5 稳定；代价 CPU ≈11.5 ms/帧） |
| wrist_left / right | `software_csc=false` | `mppjpegdec`（原生 NV16）`! mpph264enc`（内部**随机**用 RGA 转 aligned NV12） | **0 或 ＝帧数（每次跑随机）** |

代码位置（以后要改就看这几处）：

- `triple_camera_mjpeg_publisher/mjpeg_capture.py:18-22` —— `os.environ['GST_VIDEO_CONVERT_USE_RGA'] = '0'`
  （**只对 `videoconvert` 有效**，管不到插件内部的 RGA；而实测 `videoconvert` 这个场景本来就是 0 job）
- `…/mjpeg_capture.py:66-69` —— `software_csc: bool = False` 字段与注释
- `…/mjpeg_capture.py:111-129` —— 两个分支拼管线（chest 走 `format=NV16 + videoconvert` 那支）
- `…/triple_camera_mjpeg_publisher/triple_camera_mjpeg_node.py:66-71`（chest）、`:82`、`:94`（wrists）—— 默认值
- 节点代码里**没有**直接调 librga、也**没有**任何 `IM_SCHEDULER_*` / 选核设置。

⚠️ **一条更正**：早先据"ffmpeg 从封口 MP4 抽的预览图是 `yuvj420p`"判断"chest 注释（YUV422）不成立"，
**已撤回** —— 用 `v4l2-ctl --stream-to=` 直接从 UVC dump 的原始帧是 **`yuvj422p`、SOF0 采样因子 `(2,1)`**
（原生 4:2:2），节点注释是对的；预览图的 4:2:0 是 ffmpeg 抽帧造成的。

## 3. 隐患 A（唯一真实的 RGA2 触发点）：wrists 随机用 RGA

- **现象**：真实 4:2:2 流下 `jpegparse ! mppjpegdec ! mpph264enc`（= wrists 写法）会**随机**在
  `mpph264enc` 内部触发一次 RGA `NV16→aligned NV12`：插件日志
  `gstmppenc.c:675 converting to aligned NV12` → `gstmpp.c:255 converted with RGA`；
  内核侧同一 job：`src YCbCr422SP → dst YCbCr420SP`、**`core_mask=0x0`（auto，不指定核）**。
  同一命令重复跑 8 次（各 3 帧）得到 `0/3/0/3/0/3/0/3`；30 帧跑法 12 次里 5 次非 0。
  触发的轮次比不触发的多一行 `gstmpp.c:351 gst_mpp_video_info_update_format: update format to NV16`。
  **根因未定位**（怀疑与首个 buffer 是否带 `GstVideoMeta`／是否 dmabuf 有关，未验证）。
- **风险**：这块 buffer 是 MPP 的 dma-buf（`system*` heap，8 G 板上可能 >4G），而 legacy `c_RkRgaBlit`
  请求里带 fd/地址 ⇒ 驱动查不到 flags ⇒ 两个 RGA3 都忙时可能被 auto 派给 RGA2 并映射失败
  （[rga.md](rga.md) §5.1 的漏洞）。
- **留意方式**：录制时 `dmesg | grep -i rga` 看有没有 `unsupported mm_flag` / `map failed` / `no core match`；
  或 `GST_DEBUG=mppenc:6` 看有没有 `using RGA converted buffer`（有 ≈ 这一轮在用 RGA）。
- **可选处置（都未实施）**：① 不动（现状 `auto` 大概率落 RGA3）；② 内核侧保守置位
  `RGA_JOB_UNSUPPORT_RGA_MMU`（最彻底，要重编内核）；③ `GST_MPP_NO_RGA=1` 关掉编码器内的 RGA ——
  ⚠️ 实测它会让 `format=NV16` 那条路径**直接不工作**（0 字节 MP4 + `Internal data stream error`），
  要关得先把 chest 那条 `format=NV16` 去掉。
- **待办**（用户 2026-09-16："先记录，不着急"）：在**工程机**上用真实节点录 30 s，确认
  ① wrists 是否随机走 RGA、② `dmesg | grep -i rga` 有无上面的报错、③ 三路并发时两个 RGA3 是否会同时忙到
  把活甩给 RGA2。开发板这边只是文件源复现，**概率不代表生产**。

## 4. 问题 B：chest 白花 0.35 核/路 CPU

- chest 分支强制 `format=NV16` 再 `videoconvert` 转回 NV12，而 `mpph264enc` 的 sink **本来就收 NV16**
  ⇒ 这次 `NV16→NV12` 是纯 CPU 空转。实测真实 4:2:2 帧 300 帧：chest 写法 CPU **3.45 s**
  （≈11.5 ms/帧 ≈0.35 核/路 @30fps），wrists 写法 **0.29 s**；两者 RGA job 都是 0。
  （用 ffmpeg 4:2:0 预览图跑时 chest 还会多出 1 job/帧 —— 那是预览图才有的现象。）
- **建议改法（未实施）**：`triple_camera_mjpeg_node.py` 里 chest 的 `software_csc` 由 `True` 改 `False`
  （= wrists 写法，省这 0.35 核/路）。⚠️ 这会顺带把 chest 也放进隐患 A 那条"可能用 RGA"的路径，
  所以要么接受，要么连同内核补丁一起做。
- **改之前必须确认**：当初给 chest 加 `format=NV16` 若是为了画面/颜色，要先在工程机上各录一段同场景比对。

## 5. 连续录制要盯的三件事

1. **不消耗 CMA**（2026-09-15 开发板实测，等价三路管线 2×2592x1944 + 1×1920x1080）：

   | 时刻 | MemFree | CmaFree |
   | --- | --- | --- |
   | 空闲 | 6071 MB | **234 MB** |
   | 单路 5MP 跑 12 s | 5991 MB | **234 MB** |
   | 三路并发 22 s | 5768 MB | **234 MB** |
   | 全停 5 s 后 | 5807 MB | **234 MB** |

   ⇒ MPP 的解码/编码缓冲走 `system*` heap（system 掉约 300 MB），**256 MB CMA 对这个场景够用**
   （它压根不用）；但别把 CMA 当通用池（一路 5MP NV16 就是 10 MB/帧）。要喂 RGA2 请用 `system-dma32`。
   ⚠️ **MPP 输出 buffer 到底用 `system` 还是 `system-dma32` 仍未区分**：解码进程初始化时把
   `/dev/dma_heap/` 下**六个节点全部打开**（数 fd 判断不出来），`mpp_dma_heap` / `dma_heap_debug`
   两个 env 试过都没生效。要定论得读 MPP 的 `os_allocator_dma_heap.c`，或写带 pad probe 的小程序把
   解码 dmabuf `importbuffer_fd` 后读 `mm_flag`。
2. **容量/码率**（默认码率 = 硬件编码器自选）：

   | 分辨率 | 60 s 的 MP4 | 码率 | 存储需求 |
   | --- | --- | --- | --- |
   | 2592x1944 | 134.6 MB | 2.24 MB/s | **≈ 8.1 GB/小时/路** |
   | 1920x1080 | 55.4 MB | 0.92 MB/s | ≈ 3.3 GB/小时/路 |
   | 1280x720 | 24.6 MB | 0.41 MB/s | ≈ 1.5 GB/小时/路 |

   三路（2×5MP + 1×1080p）≈ **19.5 GB/小时**（≈5.4 MB/s）；bag 输出落在数据卡上（SD 顺序写 ~77 MB/s
   折算 ≈277 GB/小时）⇒ **带宽够**，要盯的是卡容量与"先检查剩余空间"的策略。
3. **>4 GiB 的单个 MP4 没问题**（`libgstisomp4.so` 有 `co64` 支持 64 位 chunk offset）；
   ⛔ **异常终止 = MP4 不可播**：`mp4mux` 的 `moov` 在 EOS 才写，被 kill/掉电后文件没有 moov
   （`gst-discoverer-1.0` 直接解析不出时长）。现有设计已正确（`appsrc` 发 EOS → 等
   `_EOS_TIMEOUT_SEC = 10` → 才发 `CameraRecordingDone`）；**建议**把"没有 moov 的 MP4"也当成
   可恢复事件处理（同一 episode 的 bag 里还有 JPEG）。

## 6. 结论与纪律（在改任何用户代码之前）

- 不改代码不需要动任何东西（默认 `auto` 大概率走 RGA3）；**唯一要守的纪律是"别把相机/解码帧的活钉到 RGA2"**。
- 想用 RGA2 的算力：自己从 `system-dma32` 分配再拷一份（[rga-dma32-buffer-demo.md](rga-dma32-buffer-demo.md)）。
- 相关：[rga.md](rga.md)（能力/内存约束/选核机制）、`tests/rga-v4l2-probe.sh`（相机 buffer 能不能给 RGA2）。
