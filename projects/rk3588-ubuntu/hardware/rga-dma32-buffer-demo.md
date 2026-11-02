# 给 RGA2 分配 <4G 缓冲：可直接编译的完整示例

> 背景与判据在 [rga.md](rga.md) §2（RGA2 的 `RGA_MMU` 要求整块物理地址 < 4 GB）。
> 本文只给**代码**：从 `system-dma32` 拿一块保证 <4G 的缓冲 → CPU 填数据（带 cache 同步）→
> RGA2 做一次 NV12→RGB888 转换 + 缩放 → **失败就回退 RGA3** → CPU 读回校验。
> 板子上编译：`cc -O2 -I/usr/include/rga -o demo demo.c -lrga`。
> 现成的复现工具是 `tests/rga-v4l2-probe.sh`（扫真实 V4L2 设备）。

## 1. 该从哪个池子分配

用户态**没有 API 能查一块 dma-buf 的物理地址**（`/proc/self/pagemap` 对 dma-buf 映射返回 PFN=0，实测），
所以**不能"检测"，只能"从正确的池子里分配"**：

| 做法 | 是否 <4G | 代价 / 限制 |
| --- | --- | --- |
| ✅ **`/dev/dma_heap/system-dma32`**（推荐） | **是**（驱动 `mm_flag=0x3`，`UNDER_4G` 置位） | 散页无所谓（RGA2 有自己的页表）；容量是整个低 4G 区（本板 ZONE_DMA 约 3.65 GB） |
| ✅ `cma` / `cma-uncached` | 是（实测 `pa=0x10900000`） | 连续；但**本板 CMA 只有 256 MB**（`CmaTotal 262144 kB` / 空闲 240 MB），放不下多路大缓冲 |
| ❌ `system` / `system-uncached` | **否**（8 G 板实测 `mm_flag=0x2`） | 给 RGA3 没问题，给 RGA2 必然 EINVAL |
| ⚠️ V4L2 / DRM / GBM / 别的进程给的 fd | **不知道** | 用户态查不到 → 这类 buffer 交给 RGA3，或先试 RGA2、失败再回退 |

**判据**：import 一块 buffer 后读 `/sys/kernel/debug/rkrga/mm_session`（root，buffer 存活期间才有），
`mm_flag` 的 **bit0 = `RGA_MEM_UNDER_4G`**：

```
handle = 1 ... mm_flag = 0x7     ← cma：UNDER_4G | NEED_USE_IOMMU | PHYSICAL_CONTIGUOUS, pa = 0x10900000
handle = 2 ... mm_flag = 0x2     ← system：只有 NEED_USE_IOMMU，**没有 UNDER_4G**
handle = 3 ... mm_flag = 0x3     ← system-dma32：UNDER_4G | NEED_USE_IOMMU（散页，RGA2 能用）
```

**机制**：`rk_system_heap.c:755` 给 `*-dma32` 的页池加了 **`GFP_DMA32`**（`:531` 按堆名选池）。
本板 `CONFIG_ZONE_DMA32=y` 但 `/proc/zoneinfo` 里 **ZONE_DMA32 是空的（managed 0）**，
32 位可寻址内存全在 **ZONE_DMA**（约 3.65 GB）—— `GFP_DMA32` 按 zonelist 回退到 ZONE_DMA，
所以拿到的一定是 <4G 的页；`cma` 则直接落在 CMA 保留区。

⚠️ 用 `system-dma32` 做的事要**显式**：默认（`auto`/不 pin）驱动会把活派给 RGA3，`system` heap 也能跑；
只有"想把 RGA2 当第三份算力"时才需要换池子。

**回退策略**（buffer 不是自己分配时）：>4G 的失败是**干净**的 —— 驱动在 map 阶段就返回 EINVAL、
不会画坏数据，librga 这边是 `IM_STATUS_FAILED`：

```c
imconfig(IM_CONFIG_SCHEDULER_CORE, IM_SCHEDULER_RGA2_CORE0);
if (imcopy(src, dst) != IM_STATUS_SUCCESS)
    { imconfig(IM_CONFIG_SCHEDULER_CORE, IM_SCHEDULER_RGA3_CORE0); imcopy(src, dst); }
```

## 2. 完整示例

```c
// demo.c —— RGA2 用 <4G 缓冲的正确姿势；RGA2 失败时自动回退 RGA3
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "im2d.h"
#include "RgaUtils.h"

/* dma-heap 的内核 uapi 头不一定装了，这里自己声明最小结构 */
struct dma_heap_allocation_data {
    unsigned long long len;
    unsigned int fd, fd_flags;
    unsigned long long heap_flags;
};
struct dma_buf_sync { unsigned long long flags; };
#define DMA_HEAP_IOCTL_ALLOC _IOWR('H', 0x0, struct dma_heap_allocation_data)
#define DMA_BUF_IOCTL_SYNC   _IOW('b', 0, struct dma_buf_sync)
/* ⚠️ 内核 uapi 的真实取值（写错 = 没同步，会读到脏 cache）：
 *    READ=1  WRITE=2  START=0  END=4 */
#define SYNC_START_WRITE (0u | 2u)     /* (START|WRITE)：CPU 写 → 给设备看 */
#define SYNC_END_WRITE   (4u | 2u)
#define SYNC_START_READ  (0u | 1u)     /* (START|READ) ：设备写 → 给 CPU 看 */
#define SYNC_END_READ    (4u | 1u)

typedef struct { int fd; void *cpu; size_t size; } buf_t;

/* heap 传 "system-dma32"（保证 <4G）或 "cma"（连续、<4G，但本板只有 256 MB） */
static int buf_alloc(buf_t *b, const char *heap, size_t size)
{
    char path[64];
    snprintf(path, sizeof path, "/dev/dma_heap/%s", heap);   /* crw-rw---- root:video */
    int h = open(path, O_RDWR | O_CLOEXEC);
    if (h < 0) { fprintf(stderr, "open %s: %s\n", path, strerror(errno)); return -1; }

    struct dma_heap_allocation_data d = { .len = size, .fd_flags = O_RDWR | O_CLOEXEC };
    if (ioctl(h, DMA_HEAP_IOCTL_ALLOC, &d) < 0) { perror("DMA_HEAP_IOCTL_ALLOC"); close(h); return -1; }
    close(h);

    b->fd = (int)d.fd;
    b->size = size;
    b->cpu = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, b->fd, 0);
    if (b->cpu == MAP_FAILED) { perror("mmap"); return -1; }
    return 0;
}

static void buf_sync(buf_t *b, unsigned long long flags)
{
    struct dma_buf_sync s = { .flags = flags };
    ioctl(b->fd, DMA_BUF_IOCTL_SYNC, &s);
    s.flags = flags ^ 4u;                    /* START(0) 与 END(4) 只差 bit2 */
    ioctl(b->fd, DMA_BUF_IOCTL_SYNC, &s);
}

/* 先用 RGA2；失败（多半是这块内存 >4G）就回退 RGA3。返回 0 成功 */
static int rga_run_with_fallback(int src_fd, int W, int H, int src_fmt,
                                 int dst_fd, int dw, int dh, int dst_fmt)
{
    const struct { const char *name; IM_SCHEDULER_CORE core; } plan[] = {
        { "RGA2", IM_SCHEDULER_RGA2_CORE0 },
        { "RGA3", IM_SCHEDULER_RGA3_CORE0 },   /* 兜底：它走 RK_IOMMU，散页/高地址都能用 */
    };

    for (int i = 0; i < 2; i++) {
        if (imconfig(IM_CONFIG_SCHEDULER_CORE, (uint64_t)plan[i].core) != IM_STATUS_SUCCESS)
            continue;
        IM_STATUS st = imresize(wrapbuffer_fd(src_fd, W, H, src_fmt),
                                wrapbuffer_fd(dst_fd, dw, dh, dst_fmt), 0, 0, 0);
        printf("%s: %s\n", plan[i].name, st == IM_STATUS_SUCCESS ? "OK" : "FAIL");
        if (st == IM_STATUS_SUCCESS)
            return 0;
    }
    return -1;
}

int main(void)
{
    const int W = 1920, H = 1080, DW = 960, DH = 540;
    const int src_fmt = RK_FORMAT_YCbCr_420_SP;      /* NV12 */
    const int dst_fmt = RK_FORMAT_RGB_888;
    buf_t src = { .fd = -1 }, dst = { .fd = -1 };

    /* ⚠️ 别用 heap="system"：8 G 板上它的页可能落到 4G 以上，RGA2 会 EINVAL */
    if (buf_alloc(&src, "system-dma32", (size_t)W * H * 3 / 2) < 0) return 1;
    if (buf_alloc(&dst, "system-dma32", (size_t)DW * DH * 3) < 0) return 1;

    memset(src.cpu, 0x40, src.size);                 /* NV12：Y=0x40 / UV=0x40 */
    buf_sync(&src, SYNC_START_WRITE | SYNC_END_WRITE);

    if (rga_run_with_fallback(src.fd, W, H, src_fmt, dst.fd, DW, DH, dst_fmt) != 0) {
        fprintf(stderr, "两个核都失败了（看 dmesg | grep rga）\n");
        return 1;
    }

    buf_sync(&dst, SYNC_START_READ | SYNC_END_READ);
    unsigned char *p = dst.cpu;                       /* Y=U=V=0x40 转 RGB 后应得到绿色 */
    printf("校验：dst[0..2] = %02x %02x %02x（应为 R≈0x00 G≈0x85 B≈0x00）\n", p[0], p[1], p[2]);
    return 0;
}
```

**源数据来自相机 / 解码器（不归你分配）时**，换成同一个套路即可：

```c
/* V4L2：拿到一帧后导出 dma-buf，再走上面的试-回退 */
struct v4l2_exportbuffer e = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .index = buf.index, .flags = O_RDWR };
ioctl(vfd, VIDIOC_EXPBUF, &e);                        /* e.fd 就是 dma-buf */
rga_run_with_fallback(e.fd, W, H, src_fmt, dst.fd, DW, DH, dst_fmt);
/* ⚠️ 实测：UVC 相机（uvcvideo，普通页分配）在 8 G 板上 mm_flag=0x2（>4G）→ RGA2 必失败，
 *    RGA3 正常；rkisp/rkcif/hdmirx（CMA）才可能 <4G。相机链路直接用 RGA3 最省事。 */
```

- 缓冲**留在 `system-dma32` 里**：同一块内存给 RGA 和 CPU（`mmap`）用，别来回拷。
- 想确认某块 buffer 到底算不算 <4G：buffer 存活期间
  `sudo cat /sys/kernel/debug/rkrga/mm_session`，看 `mm_flag` 的 **bit0**（`0x1` = `UNDER_4G`）。
- 想连 CPU 侧 cache 同步也省掉：用 `system-uncached-dma32`（同样 <4G），代价是 CPU 访问慢。
- `/dev/dma_heap/system-dma32` 是 `root:video 0660` ⇒ **进程要在 `video` 组**。
