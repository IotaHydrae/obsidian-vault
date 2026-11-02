# GPU：RK3588 Mali-G610 / panthor

> 覆盖：内核里有哪些 GPU 驱动、板子**实际**绑的是哪个、固件与用户态（OpenGL / Vulkan）从哪来、
> 怎么量 GPU 占用、已知告警。
> 相关但不同：实时占用率看 [rk3588-hw-monitor.md](rk3588-hw-monitor.md)；硬解/编码在 **VPU** 上不在 GPU 上，看
> [../system/chromium-rkmpp.md](../system/chromium-rkmpp.md)；2D 加速是 **RGA**（另一个硬件），看 [rga.md](rga.md)。

---

## TL;DR

- 板子实际绑的是 **panthor**（mainline backport）；另两个 Mali 驱动虽已配置为内建或模块，但没有绑定设备（§2）。
- 用户态：OpenGL 走 Mesa、Vulkan 走 **PanVK**，都来自官方 noble-updates（§3）。
- 量 GPU 占用**现在只有一条口径**（§4）；常用命令速查见 §5。
- ⚠️ 已知告警与坑见 §6 —— dmesg 里那几条看着吓人的告警是**预期**的。

## 0. 现状速查（开发板实测，2026-09-16，内核 6.1.172）

> 2026-10-03 在开发板上**逐项复核过**：Mesa 5 个包都是 `25.2.8-0ubuntu0.24.04.2`、
> devfreq `simple_ondemand` 空闲 200 MHz、`panthor` 绑在 `fb000000.gpu-panthor`、
> `mali` 驱动目录下无设备、`card0=rockchip-drm / card1=panthor / card2=RKNPU`、
> `board` 在 `video(44)` + `render(999)` 组 —— 下表全部对得上。

| 项 | 值 |
| --- | --- |
| SoC GPU | RK3588 Mali-G610（厂商资料为 MP4，见 §6 待核） |
| 内核驱动 | **`drm/panthor`，内建**（`CONFIG_DRM_PANTHOR=y`），绑定在 `fb000000.gpu-panthor` |
| 驱动版本 | `panthor 1.5.0 20230801`；CSF 固件 interface **v1.5.0**，FW git sha `95a25d71030715381f33105394285e1dcc860a65` |
| 固件文件 | `arm/mali/arch10.8/mali_csffw.bin.zst` → 软链到 `../arch10.10/mali_csffw.bin.zst`（zstd 压缩，内核自己解压） |
| DRM 节点 | **`card1` / `renderD129`**（`DRIVER=panthor`）；别和 `card0`+`renderD128`（`rockchip-drm`，即 VOP 显示）、`card2`/`renderD130`（`RKNPU`）混 |
| 调频 | devfreq `simple_ondemand`，**200 MHz ~ 1 GHz（9 档）**，空闲停在 200 MHz |
| 温控 | `thermal_zone5` = `gpu-thermal`（空闲 37 °C） |
| 用户态 GL | Mesa 25.2.8 的 panfrost gallium：`Mali-G610 (Panfrost)`，**GL 3.1 core / GLSL 1.40**，EGL 1.5 |
| 用户态 Vulkan | **可用**：`driverName=panvk`、`deviceName=Mali-G610`、`apiVersion=1.4.318`（另有 llvmpipe 兜底） |
| 包来源 | Mesa 全部来自 **Ubuntu 官方 `noble-updates`**（不是任何 PPA，见 §3） |

---

## 1. 硬件侧：设备树里的 GPU

`rk3588s.dtsi` 里**同一个地址有两个互斥节点**（默认都 `disabled`，由板卡 dts 选一个）：

| DT 节点 | compatible | 会被哪个驱动接 |
| --- | --- | --- |
| `gpu: gpu@fb000000` | `arm,mali-valhall` | ARM 厂商 kbase（`drivers/gpu/arm/bifrost`） |
| `gpu_panthor: gpu-panthor@fb000000` | `rockchip,rk3588-mali`, `arm,mali-valhall-csf` | **panthor** ← 我们用的 |

我们的板子都使能后者：

- `rk3588-lubancat-5.dts:406` `&gpu_panthor { … status = "okay"; }`（**v2 就是 include 这个文件**）
- `rk3588-lubancat-5iob.dts:508` 同样使能 `&gpu_panthor`（**5io 走的就是这个文件**）
- ⚠️ 例外：**未编译**的 `rk3588-lubancat-5iof.dts:488` 使能的是老的 `&gpu`（kbase）—— 将来派生板卡别抄错

资源（来自 `gpu-panthor@fb000000` 节点）：

- 寄存器 `0xfb000000` + `0x200000`；三个中断（`job` / `mmu` / `gpu`，SPI 92/93/94，与老节点顺序相反）
- `power-domains = <&power RK3588_PD_GPU>`；时钟 `clk_mali`(scmi) + `clk_gpu_coregroup` / `clk_gpu_stacks` / `clk_gpu`
- 供电：`mali-supply = <&vdd_gpu_s0>`、`mem-supply = <&vdd_gpu_mem_s0>`（**没有** `sram-supply`，见 §6 告警）
- 散热：`#cooling-cells = <2>`、`dynamic-power-coefficient = <2982>`
- **OPP 表（panthor 内联，非老节点那张）**：

| 频率 | 电压 |
| --- | --- |
| 200 / 300 / 400 / 500 / 600 MHz | 675 mV（200 MHz 带 `opp-suspend`） |
| 700 MHz | 700 mV |
| 800 MHz | 750 mV |
| 900 MHz | 800 mV |
| **1 GHz** | **850 mV** |

驱动启动时打印的识别信息（实测原文）：

```
[    6.550992] panthor fb000000.gpu-panthor: [drm] mali-g610 id 0xa867 major 0x0 minor 0x0 status 0x5
[    6.551001] panthor fb000000.gpu-panthor: [drm] Features: L2:0x7120306 Tiler:0x809 Mem:0x301 MMU:0x2830 AS:0xff
[    6.551007] panthor fb000000.gpu-panthor: [drm] shader_present=0x50005 l2_present=0x1 tiler_present=0x1
```

⚠️ `l2_present=0x1` ⇒ **只有 1 个 core group**，所以驱动不会打那条 `using only 1st core group (%lu cores from %lu)`
（`panthor_gpu.c:320-333` 只在 `l2_present != 1` 时打印）。也就是说"几个 shader 核"没有被解码打印出来，
掩码与核数的对应关系见 §6。

---

## 2. 内核侧：树里其实有**三个** Mali 驱动

| 驱动 | 源码位置 | 配置（运行内核实测） | 匹配的 compatible | 现状 |
| --- | --- | --- | --- | --- |
| **panthor** | `drivers/gpu/drm/panthor/` | `CONFIG_DRM_PANTHOR=y`（**内建**） | `arm,mali-valhall-csf` / `rockchip,rk3588-mali` | ✅ 绑定 `fb000000.gpu-panthor` |
| panfrost | `drivers/gpu/drm/panfrost/` | `CONFIG_DRM_PANFROST=m` | `arm,mali-bifrost` / `arm,mali-valhall-jm` | 编了模块，RK3588 的 DT 没有匹配节点 ⇒ 不会加载 |
| ARM kbase | `drivers/gpu/arm/{bifrost,midgard,valhall,mali400}/` | `CONFIG_MALI_BIFROST=y`、`CONFIG_MALI_MIDGARD=y`、`CONFIG_MALI_CSF_SUPPORT=y` | `arm,mali-valhall` 等 | 编进内核、驱动已注册（`/sys/bus/platform/drivers/mali`），但**没有绑定任何设备** |

判据（不用猜）：

```bash
ls -l /sys/bus/platform/drivers/panthor/ /sys/bus/platform/drivers/mali/   # 谁下面有设备软链
for d in /sys/class/drm/card[0-9]*; do printf '%-8s %s\n' "$(basename $d)" "$(grep ^DRIVER= $d/device/uevent)"; done
```

### panthor 是 mainline 的 backport，厂商几乎没改

- 整个 `drivers/gpu/drm/panthor/` 目录里**唯一的 Rockchip 痕迹**是 of_match 里多一条
  `{ .compatible = "rockchip,rk3588-mali" }`（`panthor_drv.c:1658`）——DT 的第一个 compatible 就是它。
- uapi 已含 `DRM_IOCTL_PANTHOR_BO_SET_LABEL`（1.4）与 `DRM_IOCTL_PANTHOR_SET_USER_MMIO_OFFSET`（1.5），
  与 in-tree 的版本注释（`panthor_drv.c:1569-1578`）一致；**上游到 2025-10 才 bump 到 1.6**
  ⇒ 这个 backport 停在 **1.5 那一档**，之后没跟着 mainline 走（要坐实得上游逐文件对 tag，见 §7）。
- 固件：`panthor_fw.c:28` `#define CSF_FW_NAME "mali_csffw.bin"`，`MODULE_FIRMWARE("arm/mali/arch10.8/mali_csffw.bin")`；
  仓库 `prebuilt/firmware.tgz` 里带 `arch10.8 / 10.10 / 11.8 / 12.8 / 13.8`（都是 `.zst`，给不同代 Mali 用）。
- 内存模型：**UMA**，没有独立显存；panthor 走 `DRM_GPUVM` + `IOMMU_IO_PGTABLE_LPAE`（GPU 用自己的页表访问系统内存）。

---

## 3. 用户态：GL 与 Vulkan

- 包与来源（实测）：`libgl1-mesa-dri` / `mesa-libgallium` / `libglx-mesa0` / `libegl-mesa0` / `mesa-vulkan-drivers`
  全是 **`25.2.8-0ubuntu0.24.04.2`**，来自 `repo.huaweicloud.com/ubuntu-ports noble-updates/main`。
- ⚠️ **别被 `overlay/etc/apt/preferences.d/panfork-mesa-ppa` 误导**：那是个 pin 1001 的优先级文件，
  但**panfork PPA 并不在镜像的 apt 源里**（构建脚本只 `add-apt-repository ppa:liujianfeng1994/rockchip-multimedia`），
  所以这条 pin 现在**不生效**，Mesa 不是来自 panfork。
- OpenGL / GLES：

```
OpenGL core profile renderer: Mali-G610 (Panfrost)
OpenGL core profile version: 3.1 (Core Profile) Mesa 25.2.8-0ubuntu0.24.04.2
```

  "Panfrost" 是 **Mesa 那个 gallium 驱动的名字**（它同时支持 panfrost 和 panthor 两个内核驱动），
  内核侧仍然是 panthor，两边不矛盾。
- **Vulkan 可用（PanVK）**：`/usr/share/vulkan/icd.d/` 里有 `panfrost_icd.json`，实测

```
deviceName = Mali-G610
driverName = panvk
driverInfo = Mesa 25.2.8-0ubuntu0.24.04.2
apiVersion = 1.4.318
```

  （同一个 `vulkaninfo` 里还有 `llvmpipe (LLVM 20.1.2, 128 bits)` 软件实现兜底。）
  ⚠️ PanVK 在这个后端上还算新东西，**稳定性没验过**（见 §7）。
- 板上现成的工具（2026-10-03 开发板复查）：`eglinfo`、`es2_info`、`glxinfo`（`mesa-utils`）。
  ⚠️ **`vulkaninfo` 现在没有**（要测得装 `vulkan-tools`）；`glmark2` / `vkmark` 也没有
  —— [gpu-vkmark-benchmark.md](gpu-vkmark-benchmark.md) 那两组数字是当时**手工装在开发板上**的，不在镜像里。改板子环境需用户同意。

---

## 4. 怎么量 GPU 占用（现在只有这一条口径）

首选 `tools/lsp-rk-monitor.sh`（GPU 行 = devfreq `load` + 频率 + panthor 中断/秒）：

```
GPU  Mali panthor   ················   0%  200MHz          中断 0/s
DDR  dmc            █···············   9%  1848MHz
── 温度
soc=37.9°C  bigcore0=37.9°C  bigcore1=37.9°C  littlecore=37.9°C  center=37.0°C  gpu=37.0°C  npu=37.0°C
```

- 数据源是 `/sys/class/devfreq/fb000000.gpu-panthor/load`，格式 `<百分比>@<频率>Hz`（实测空闲 `2@200000000Hz`）。
- panthor 的 load 是**真 busy 百分比**（驱动用 GPU 时间戳算 busy/idle），不是"有活就 100%"。
- **硬解不在 GPU 上**：4K30 硬解时 GPU 只有 2%，两个 VPU 核各 ~15%（见 `../system/chromium-rkmpp.md`）。
- 目前**还没有**基于同版本工具的横比数字（`glmark2` / `vkmark` 都是**手工装在开发板上**的，不在镜像里；
  已测数据见 [gpu-vkmark-benchmark.md](gpu-vkmark-benchmark.md)），这是 §7 的待办。

---

### 4.1 基准横比：为什么"别人的 vkmark 6666、我们的 1123"

`glmark2` / `vkmark` 都**不在镜像里**（§3/§4），那组对照是当时手工装在开发板上做的，已单独成篇 →
**[gpu-vkmark-benchmark.md](gpu-vkmark-benchmark.md)**。
结论一句话：**这两个 vkmark 数字不能直接比**（差在工具版本、呈现与分辨率、PanVK 三层）；
同板同参数下 GL 6120 vs Vulkan 1334 ⇒ 差距全在 Vulkan 栈，硬件/频率/散热都没问题。

---

## 5. 常用命令速查

```bash
# 谁绑定了 GPU（看哪个驱动目录下有设备软链）
ls -l /sys/bus/platform/drivers/panthor/ | tail -n +2

# 全部 DRM 设备分别是哪个驱动（card0=VOP, card1=panthor, card2=RKNPU）
for d in /sys/class/drm/card[0-9]*; do printf '%-8s %s\n' "$(basename $d)" "$(grep ^DRIVER= $d/device/uevent)"; done

# 驱动/固件版本（启动日志）
sudo dmesg | grep -i panthor

# 频率与负载
cat /sys/class/devfreq/fb000000.gpu-panthor/{cur_freq,min_freq,max_freq,available_frequencies,governor,load}

# 温度
grep . /sys/class/thermal/thermal_zone5/{type,temp}

# 用户态能力
eglinfo -B | grep -E 'vendor|renderer|version'
vulkaninfo --summary | grep -E 'deviceName|driverName|apiVersion'

# 实时占用（GPU/NPU/RGA/VPU/DDR/CPU/温度）
sudo tools/lsp-rk-monitor.sh            # --once 只打一次
```

### 5.1 权限：`video` 与 `render` 分别管什么（2026-09-21 实测）

⛔ **`board` 不进 `render` 组时，GPU 在 ssh / 无 seat 的服务里直接不可用**（镜像曾漏了这条，
现在构建脚本 `scripts/build-lsp-image.sh` 已显式 `gpasswd -a board render`）：

| 组 | 覆盖的设备（都是 `0660`） | 少了它 |
| --- | --- | --- |
| `video` | `/dev/dri/card*`（VOP 显示）、`/dev/rga`、`/dev/mpp_service`、`/dev/video-dec0` | **VPU 硬解 / RGA 全部 Permission denied** |
| `render` | `/dev/dri/renderD128/129/130` | **GPU（EGL/GL/Vulkan）全废**：`libEGL warning: failed to open /dev/dri/cardN: Permission denied` → Mesa 退回 **llvmpipe 软渲染**；`vulkaninfo` 一个设备都枚举不到（`TU: error: tu_knl.cc:387: failed to open device … VK_ERROR_INCOMPATIBLE_DRIVER`）；`gpu_busy` 恒 0 |

⚠️ **本地图形会话其实不需要这个组**：logind 会给**当前活跃会话**的用户加 uaccess ACL
（判据 `getfacl -p /dev/dri/renderD128` → 里面有 `user:<活跃会话的用户>:rw-`；
会话切走 `Active=no` 时那条 ACL 就没了）。所以"桌面上好好的、ssh 上去就掉软渲染"这个现象就是这么来的。
⇒ 结论：**要在 ssh / 无 seat 的 systemd 服务 / 容器里跑 GPU，就必须有 `render` 组**
（或让服务拿到 seat 会话，例如 kiosk 用 `PAMName=login`，见 [../system/kiosk/session-design.md](../system/kiosk/session-design.md)）。

`tools/verify-image.sh` 已把 `video`/`render` 两条做成断言（离线查 rootfs 的 `/etc/group`、上板查实际 `/etc/group`）。

---

## 6. 已知告警与坑

- `Looking up sram-supply property in node /gpu-panthor@fb000000 failed` —— **非致命**：DT 没给 `sram-supply`
  （老的那张 `gpu-opp-table` 带 pvtm/nvmem 校准，panthor 这张内联表不需要）。
- `EM: OPP:400000 is inefficient` / `OPP:300000 is inefficient` —— 能量模型（EM）的提示，不是错误。
- `Firmware protected mode entry not be supported, ignoring` —— 正常（该板没走 protected mode）。
- **两个 DT 节点别混**：`&gpu`（kbase）与 `&gpu_panthor`（panthor）同地址互斥；kbase 也编进了内核，
  哪天有人把 `&gpu` 打开就会换驱动、`renderD*` 编号和 ioctl 面都会变。
  - **sysfs 路径也跟着变**：devfreq 设备挂在平台设备自己下面（kbase 的
    `drivers/gpu/arm/valhall/backend/gpu/mali_kbase_devfreq.c:633` 是 `devfreq_add_device(kbdev->dev, …)`），
    所以 kbase 是 `/sys/devices/platform/fb000000.gpu/devfreq/fb000000.gpu/`、
    panthor 是 `/sys/devices/platform/fb000000.gpu-panthor/devfreq/fb000000.gpu-panthor/`
    （也可走 `/sys/class/devfreq/fb000000.gpu-panthor/`）。**按老路径写脚本会静默失效**。
- ⛔ **别指望用 `governor=performance` 把 GPU"钉最高频"**（2026-09-20 查清 + 已删服务）：
  上游 EmbedFire 带了一个 `gpu-governor-performance.service` 写 `…/governor`，**我们在 2026-09-20 把它删了**
  （`overlay/usr/lib/systemd/system/` 里已无此文件，`build-lsp-image.sh:1728` 的安装/enable 两行也去掉了），
  原因两条：
  1. 它写的是 **kbase 的路径** `fb000000.gpu/devfreq/fb000000.gpu`，panthor 上这个目录根本不存在
     ⇒ `|| true` 吞掉错误、退出码 0，**纯空转**（板上实测 GPU 一直是 `simple_ondemand`）；
  2. 就算把路径改对，**在 panthor 上它也会造成相反的效果**：devfreq 核心在 runtime-suspend 时走
     `if (devfreq->suspend_freq) devfreq_set_target(...)`，`suspend_freq` 取自 OPP 表的 `opp-suspend`
     —— panthor 这张表（`rk3588s.dtsi:2995`，第 3001 行）把它挂在 **200 MHz**，而 `performance` 只在
     `GOV_START` 设一次、没有 SUSPEND/RESUME handler ⇒ **第一次挂起后永久 200 MHz**。
     上游那套之所以"钉得住"，是因为 kbase 用的 `gpu_opp_table`（`rk3588s.dtsi:3038`）**没有 `opp-suspend`**，
     挂起时不改频率。**"同一句 performance，换驱动结果相反"**，这就是它过期失效的根因。
  - 想常驻高频只能动 `power/control`/`min_freq`（代价见 [rk3588-hw-monitor.md](rk3588-hw-monitor.md) §3.1）；
    CPU 那边不受影响 —— cpufreq 没有 `opp-suspend` 这套机制，`cpu-governor-performance.service` 照旧有效。
  - ⚠️ 上游遗留脚本 `scripts/build-rootfs.sh` 曾引用这个已删文件（该脚本 **2026-09-24 已从仓库删除**，
    此条为历史记录）。
- **待核**：`shader_present=0x50005` 的 popcount 是 3，而厂商资料说 RK3588 是 Mali-G610 **MP4**——
  要么这个寄存器的位布局不是"一核一位"（`panthor_gpu.c:328-329` 的 `~(l2_present-1) & (l2_present-2)`
  说明位是按 core group 分段的），要么核配置另有出处。**没核实前别在文档/对外材料里断言核数**（要么引用厂商资料，要么标注未验证）。
- 版本注释里的 `1.5` 只是**驱动接口版本**，不等于内核版本，也不等于 GPU 硬件版本。

---

## 7. 待办 / 未决

1. **把 backport pin 到上游 tag**：逐文件对 `drivers/gpu/drm/panthor/`（或对 `panthor_drv.c` 的接口版本注释），
   量化「我们树里的 panthor ↔ 6.6/6.12 BSP ↔ mainline」的差距（新特性、性能/稳定性修复、Mesa 要求的最低驱动版本）。
2. **同版本工具的横比**（[gpu-vkmark-benchmark.md](gpu-vkmark-benchmark.md) 的第 1 步）：在板上**从源码编 vkmark 2025.01**，用与对方完全相同的命令跑，
   把"工具"这一层对齐；再试 **Mesa 25.3+/26.x**（PPA 或自编译）看 1334 → 多少，并确认新 Mesa 对
   panthor uapi（我们停在 1.5）的要求。
3. **GPU 基准/回归脚本**（如 `tests/gpu-bench.sh`）：把那篇的口径固化下来（版本 + winsys + size +
   present-mode + Mesa 版本 + GPU 频率一起记录），照 `tests/rga-bench.sh` 的路子现场编译、自带通过/失败计数。
4. **验证 PanVK 稳定性**：确认上位机/ROS2 侧有没有组件走 Vulkan（Chromium? Qt?），再决定要不要把它算进支持面。
5. **自研底板派生时**（见 `../build/derived-board-bringup.md`）：GPU 节点/供电/OPP 可以直接照抄 5io，
   但**散热能力与 DDR 带宽**要在新底板上重测（`dynamic-power-coefficient`、`#cooling-cells` 与整机散热强相关）。
6. 大版本内核升级的 GPU/Mesa 影响面（`../build/kernel-upgrade-feasibility.md` §5）可以用本文的实测补齐。

---

## 8. 合成器在不在用 GPU（GPU 合成 vs CPU 合成）

见 [gpu-compositing.md](gpu-compositing.md)：`WLR_RENDERER=gles2|pixman` 的 A/B、硬判别式、
实测数字（7% CPU/99% GPU/1098 FPS vs 90% CPU/0% GPU/4 FPS）与四个坑；配套回归
`tests/wlroots-renderer-ab.sh`。

## 9. 相关文档

- 实时占用率工具与接口地图：[rk3588-hw-monitor.md](rk3588-hw-monitor.md)
- 合成器在不在用 GPU（wlroots 渲染器 A/B）：[gpu-compositing.md](gpu-compositing.md)
- 板级资源清单（GPU 那一行）：[lubancat-5-v2-hardware.md](lubancat-5-v2-hardware.md)
- Chromium 硬解（GPU vs VPU 的分工、实测占用）：[../system/chromium-rkmpp.md](../system/chromium-rkmpp.md)
- 2D 加速（RGA，和 GPU 是两套东西）：[rga.md](rga.md)
- 内核升级的影响面：[../build/kernel-upgrade-feasibility.md](../build/kernel-upgrade-feasibility.md)
- 自研底板派生清单：[../build/derived-board-bringup.md](../build/derived-board-bringup.md)
