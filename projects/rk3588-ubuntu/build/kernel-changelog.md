# 内核源码变更与部署验证

> 本文记录本工程对 Rockchip BSP `kernel-6.1/` 的修改，按源码提交由新到旧排列。
> 仓库源码、已构建包和正在运行的内核分别核对；镜像体检不能替代新内核的运行验证。
> 完整测试口径见各项出处；U-Boot 见 [独立变更账](uboot-changelog.md)。

## 当前源码与部署状态

| 对象 | 已核实状态 / 记录日期 |
| --- | --- |
| 仓库源码 | Rockchip 6.1.172；RTL8822CE TXBD 两项修改已纳入 `6.1.172-14` / `#14`，2026-10-23 构建、包核验与工程机启动核对通过。 |
| 开发板 | 2026-10-23 仍运行 `6.1.172-13` / `#13`，含 V2 保电与 GPIO-only 规避；已有 RTC / 电源键 deep 往返验证通过。 |
| 最新镜像 | `20261023`，构建指纹 `2026-10-23T20:23:51+08:00`，含 `6.1.172-14` / `#14`、OEM `e5953e65`；离线体检 752 项通过、0 失败，双分区 fsck、ATF 及内核载荷核验通过。尚未烧录启动。 |
| 工程机 | 2026-10-23 已从 eMMC 启动当日 18:06 版镜像，运行 `#14`，内核包校验无差异、taint=0；RTL8822CE 已加载，Wi-Fi SSH 正常。仍有三项启动服务异常，见下文；未重跑休眠、外设与吞吐测试。 |

## 变更总表

日期为源码提交的作者日期；FUSB302 另注明重写日期。“已验证”仅适用于对应硬件、版本和测试条件。

| 源码提交 | 日期 | 变更 | 已有验证与部署 |
| --- | --- | --- | --- |
| `0deed3f24` | 2026-10-23 | RTL8822CE 关闭 TXBD 调试备份及保护调用 | 已纳入 `-14`，构建与包核验通过；工程机已加载，Wi-Fi SSH 通信正常。 |
| `b4c0c97f4` | 2026-10-21 | V2 保持休眠供电，GPIO-only 临时隔离 UART2 早醒 | 已纳入 `-13`；RTC 两轮、电源键一轮通过。 |
| `d2453796a` | 2026-10-20 | STMMAC LED fixup 注册 / 注销配对到模块生命周期 | 临时模块热重载 10 轮通过；已纳入并启动 `-12`。 |
| `c5f7b0e80` | 2026-10-20 | CMA heap fault 尾界检查 `>` → `>=` | `#11` 四个越界用例均为 SIGBUS，合法读写通过。 |
| `537f88aa1` | 2026-10-19 | 撤回 STMMAC 通用路径中的 JLSemi workaround | 临时模块两口数据校验通过；已纳入并启动 `-11`。 |
| `b5b7ea6d2` | 2026-10-14 | VOP2 停用时缓存 gamma，保护硬件访问 | 开发板 `#10` 停用 ioctl 20/20；工程机已运行 `#10`。 |
| `1cc10fd0a` | 2026-10-03 | 以太网改模块，关闭 Btrfs | 相同启动条件下首帧 3.996 → 3.101 s；保留有线能力。 |
| `e2071dc3f` | 2026-10-02；10-12 重写 | FUSB302 source 去抖与插拔恢复 | 实际 C 函数回归 32 项、真实 CC IRQ 与生命周期板测 66 项通过。 |
| `680e2883c` | 2026-09-30 | 启用 `CONFIG_CAN_RK3568` | 装板启动后驱动注册；CAN overlay 默认未启用。 |
| `72c2805d0` | 2026-09-29 | panel-simple nvmem 条件与 remotectl IRQ 回退 | 启动核验通过，原四行 IRQ / 唤醒报错消失。 |
| `e4e9c64cb` | 2026-09-16 | BSP 6.1.118 → 6.1.172，加入 EVB DTS 骨架 | LubanCat 实板正常启动；EVB 无实物。 |

## RTL8822CE TXBD 调试备份

- **改动**：`0deed3f24` 采纳野火 `3a28689bd`、`409968622`，关闭 `include/autoconf.h` 的
  `DBG_TXBD_DESC_DUMP`，并以同宏保护 `rtl8822c_ops.c` 中的 `core_get_tx_ring_ext()` 调用。
- **原因**：旧配置默认在正常发送路径备份描述符，增加 MMIO 读取、复制与记账。
  关闭宏时必须同时保护调试调用，正常错误计数与自复位路径保留。
- **验证**：`6.1.172-14` 完整内核包构建与核验通过，配置同 `#13`，TXBD 三个调试符号均不存在；
  原版 / 补丁模块对照 `.bss` 减少 **8968 B**、strip 后减少 **10640 B**；旧模块运行态 A/B 差值约 **5.41 µs / 1200 B UDP 报**。
- **运行核对**：2026-10-23 工程机从 eMMC 启动 `#14`，已加载的 `8822ce` 与安装文件的
  `srcversion` 均为 `EB08C10553079BD3C5EBB27`；`dpkg --verify linux-image-6.1.172` 无差异。
  管理连接使用该驱动的 `p2p0` Wi-Fi 接口，内核 taint=0；本轮只读核验，不重载模块。
- **边界与出处**：补丁版仅验证加载与既有 SSH 通信，未重测吞吐或功耗。上述 A/B 是固定负载下的系统级观察，不能视为函数耗时、
  包延迟或补丁版最大吞吐收益；不涉及网络唤醒配置。源码、上游链接与数据见
  [TXBD 调试成本](../hardware/rtl8822ce-txbd-cost.md)。

### `20261023` 18:06 版工程机启动验收边界

运行指纹为 `2026-10-23T18:06:57+08:00`、OEM `3ada7b80c65a44c63fe061f0e0e6ef7d774b9426`。
eMMC 两段 bootloader 与安装文件逐字节摘要相符，启动内核 / initrd 与 `/boot/` 原件相符；
OEM 六个恢复原件、九项系统摘要、103 个运行文件及三个采集准备组件校验通过。
Data1T 实际挂在 SD 的 `/run/lsp_data_1T`，含 `nosuid,nodev`，发布链接正确。
kiosk、上位机及采集服务在运行；采集为现场 `enabled / active` 状态，不据此反推镜像默认启用。
本镜像尚不含随后加入的 GNOME NumLock、任意新 4K 屏默认 200% 与 MCAP 菜单隐藏改动。

`systemctl --failed` 仍有 `boot_init.service`、`fake-hwclock-load.service`、`rkaiq_3A.service`，
均在约 4.20 秒命中 `start-limit-hit`，早于 `sysinit.target` 的 4.41 秒；三项 `Restart=no`、
`NRestarts=0`。具体新启动事务发起方未唯一确定，不能归因于内核补丁。
fake-hwclock 每次正常拒绝用过去的存档倒调时间，chrony 后续已同步并完成本次启动存档；
已知完成状态问题见[既有隔离复现](image-size-trimming.md#新-sd-的真实断电冷启动验收)。
rkaiq 停止脚本另报告 `killall: command not found`。本轮未修改配置、清除失败状态或重启。

## V2 deep：休眠供电与 GPIO-only 临时规避

- **改动**：`b4c0c97f4` 恢复基础 DTS 的 `vdd_log_s0` 休眠供电，保持 **750000 µV**；
  V2 单独将唤醒配置从 GPIO + CPU0 `0x101` 改为 GPIO-only `0x100`。
- **原因**：原 rail 关断策略导致 RTC / 电源键无法唤回；CPU0 类另有 UART2 早醒，先作临时隔离。
- **验证**：定向 DTB 编译无新增警告，5io DTB 不变。`6.1.172-13` 构建、安装并启动后，运行 FDT
  两项策略正确；RTC 请求 35 / 90 秒及用户短按共 **success=3、fail=0**。
  eth0 恢复千兆全双工，双向各 10 次 ping 和各 4 MiB SHA256 校验通过，错误计数增量为 0。
- **边界与出处**：GPIO-only 是临时规避；UART2 根因、功耗、长期可靠性及以太网 / Wi-Fi / 蓝牙唤醒未完成。
  见 [rail 供电条件](../hardware/suspend-and-deep-sleep.md#v2vdd_log_s0-休眠策略与唤醒失败)、
  [UART2 诊断](../hardware/rk3588-cpu0-immediate-wake.md)与 [RTL8822CE 唤醒](../hardware/rtl8822ce-wakeup.md)。

## STMMAC LED fixup 生命周期修复（已纳入 `-12` 测试包）

- **改动**：`d2453796a` 将四种全局 PHY LED fixup 集中到模块初始化注册；注册失败逆序回滚，
  模块退出统一注销，保留回调函数体及寄存器写入值。
- **原因**：旧代码每次网口 probe 重复注册，卸载未注销，重载后可能调用已释放的模块代码。
- **验证**：匹配 `#11` 的临时模块完成 **10 轮热重载**，每轮退出后四种 UID 残留均为 0；
  eth1 无线缆，仅验证创建 / open。后续 `6.1.172-12` 完整包已构建、安装并启动，eth0 千兆全双工，
  双向各 32 MiB 校验、各 20 次 ping 通过，错误与丢包增量为 0，taint=0。
- **边界与出处**：完整 `#12` 未重复热卸载，5io 与另外三种 PHY 未验证；未纳入 Armbian 的两个 resume 修复。
  `#12` 单轮 s2idle 往返不能证明 deep 路径或目标竞争已验证。机制与验证见
  [LED fixup 悬空回调](armbian-kernel-delta.md#模块重载前必须处理-led-fixup-悬空回调)。

## CMA heap fault 尾界检查

- **改动**：`c5f7b0e80` 将两份 CMA exporter 的页索引检查由 `>` 改为 `>=`，
  在访问 `pages[]` 前返回 `VM_FAULT_SIGBUS`；当前配置实际编入 `rk_cma_heap.o`。
- **原因**：扩大 DMA-BUF 映射后，`pgoff == pagecount` 原本仍被放行，可能越界取页并导致崩溃。
- **验证**：`6.1.172-11` 干净构建并在 V2 / eMMC 启动；四个越界用例均收 SIGBUS，
  cached / uncached 各 **64 页**全量读写一致，新增日志无 Oops / BUG / panic，boot ID 与 taint 不变。
- **边界与出处**：仅板测当前活动的 rk exporter，generic 实现为同步修复；没有用普通 UVC 开流替代 fault 验证。
  mainline 来源、复现器及判据见 [CMA 尾界越界](armbian-kernel-delta.md#已修复cma-fault-尾界越界)。

## STMMAC：撤回通用路径中的 JLSemi workaround

- **改动**：`537f88aa1` 删除 `stmmac_init_dma_engine()` 的无条件 `msleep(1500)`，以及
  `stmmac_mdio_register()` 对 PHY 地址 0 的四次私有写入；保留 DMA reset 轮询、GPIO reset 与型号专用配置。
- **原因**：固定等待在持有 RTNL 时阻塞 Chromium 初始网络快照；无 PHY ID 限定的私有写入会改变 V2 的 RTL8211F 配置。
  两处均见于 [LEDE JLSemi 补丁](https://github.com/coolsnowwolf/lede/commit/b889ab8789155707f5a73f69d87f33d18e33bb78)，
  随野火 BSP 导入；未证实野火直接从 LEDE 移植。
- **验证**：V2 / `#10` 匹配临时模块的重启冷缓存 A/B 中，Cage → FCP 中位数 **5.618 → 4.744 s**。
  仅删私有写入的 B 模块两口、合并删除的 C 模块 eth1 均通过千兆通信、双向各 32 MiB 校验，错误 / 丢包增量为 0；
  两 PHY 的 `0xa43:0x19` 从 `0x1800` 变为 `0x0862`。后续联合补丁已纳入并启动 `-11`。
- **边界与出处**：无原驱动 DC 冷上电对照，不计算冷上电提速；5io / JLSemi 实物未验证，目标 resume 竞争仍未复现。
  见 [Chromium 冷启动](../system/chromium-cold-start.md)与
  [PHY 私有写入及恢复边界](armbian-kernel-delta.md#网口私有寄存器裸写与缺失的恢复修复)。

## VOP2 停用 gamma 缓存与硬件访问保护

- **改动**：`b5b7ea6d2` 在 `vop2_crtc_legacy_gamma_set()` 等待已有提交完成，并持有 enable/disable 共用锁；
  始终缓存 LUT，仅在 VOP、CRTC、VP 和 LUT 映射有效时访问硬件，下次 enable 使用原恢复路径。
- **原因**：Plymouth 可在 CRTC 停用时设置 gamma；旧代码仍向断电 VOP 写 CFG_DONE，触发 `0xbe000411` SError。
- **验证**：两台机器的旧 `#9` 首次停用 ioctl 均复现；开发板修复 `#10` 的停用 ioctl **20/20** 通过，
  在线 gamma、CRTC 重新启用、按键开机及 DC 上电样本通过。工程机新镜像已运行 `#10`，kiosk / 上位机正常。
- **边界与出处**：工程机未重跑停用注入；缓存回读不证明硬件 LUT 内容，多 VP 并发及长期稳定性未验证。
  见 [VOP2 gamma 启动崩溃](../hardware/vop2-inactive-gamma-panic.md)与 复现工具（镜像源码：`tests/vop2-legacy-gamma.py`）。

## 以太网改模块与关闭 Btrfs

- **改动**：`1cc10fd0a` 将 `STMMAC_ETH`、`STMMAC_PLATFORM`、`DWMAC_ROCKCHIP` 一并改为 `=m`，
  关闭 `DWMAC_GENERIC` 和 `BTRFS_FS`；md/dm-raid 的 RAID6 模块能力保留。
- **原因**：将两路 GMAC 的同步 probe 移出内核启动关键路径，去掉 Btrfs 引入的 **510 ms** RAID6 开机基准。
  三个 STMMAC 配置须同为模块，避免内建代码引用模块符号。
- **验证**：V2 相同 floor 单元、无 Plymouth、串口计时条件下，两项修改使首帧 **3.996 → 3.101 s**，
  kernel 阶段 **3.425 → 2.556 s**；启动 RAID6 基准日志为 0。udev 自动加载三模块，eth0 / eth1 及插线联网能力保留。
- **边界与出处**：这些是该轮启动条件下的结果，不代表当前镜像首帧承诺；镜像不再提供 Btrfs。
  见 [initcall 分析](../hardware/kernel-initcall-profiling.md)与 [启动优化第 14、15 项](../system/boot-time-optimizations.md)。

## FUSB302 source 去抖与插拔恢复

- **改动**：`e2071dc3f` 从 BSP 原始驱动重写 source CC/VBUS 丢失过滤，使用独立截止时间与共用工作项；
  处理重采样、有限 I²C 重试及锁外 TCPM 重置，补齐失败 / 卸载 / 休眠清理；拒绝不支持的 Ra/Ra 附件组合。
  默认 **100 ms**，仅保留只读 `stats` v2，不提供合成 `inject`。
- **原因**：短脉冲不应拆掉正常 source 连接，持续丢失仍须断开并能重插恢复；旧回调不能延长或误用过期窗口。
  100 ms 是板级策略，不是 Type-C 协议要求。
- **验证**：实际 C 函数回归 **32/0**；工程机 V2 / BSP 6.1.172 匹配内核的临时模块通过真实 MEASURE IRQ A/B，
  以及持续丢失、重枚举、pending 时 unbind/bind 和并发 stats 板测，共 **66/0**，未见新增内核异常。
- **边界与出处**：真实 IRQ 只覆盖 CC；VBUS 瞬态与 I²C 错误仅做逻辑回归，实际休眠和长期自然故障未验证。
  现场 `analog_audio` 故障缺少完整日志，不能归因于旧补丁。实现及测试见 [FUSB302 专题](../usb/fusb302-debounce.md)。

## 启用 RK3588 CAN 驱动

- **改动**：`680e2883c` 在 defconfig 启用 `CONFIG_CAN_RK3568=y`，与上游 `3b064db4b` 一致。
- **原因**：RK3588 的 `rockchip,can-2.0` 由 `rk3568_can.c` 匹配，原 `CONFIG_CAN_ROCKCHIP` 只覆盖 can-1.0。
- **验证**：`6.1.172-5` 装板启动后配置为 `=y`，`/sys/bus/platform/drivers/rk3568_can` 存在。
- **边界与出处**：基树 CAN 节点仍为 `disabled`，默认不启 CAN overlay，因此没有 `can0` 属于预期；未做 CAN 总线收发验证。
  来源与采纳理由见 [上游台账](upstream-sync-commits.md) §3。

## panel-simple nvmem 条件与 remotectl IRQ 回退

- **改动**：`72c2805d0` 使 `panel_simple_dsi_of_get_desc_data()` 仅在 DT 存在 `nvmem` 时进入对应分支；
  remotectl 撤回本地 PWM v4 channel-3 独立 IRQ 改动，与上游 `93cdf2c7c` 对齐。
- **原因**：原 nvmem 路径可使无此节点的 DSI 面板永久 `-EPROBE_DEFER`；IRQ 改动使同一 IRQ 被以不同 flags 申请两次。
- **验证**：目标源码与上游一致，安装后模块与包一致、启动核验通过；原四行 IRQ / 唤醒报错全部消失。
- **边界与出处**：未做 DSI 面板与 IR 遥控实物功能测试；现有 deferred 列表不变不能证明所有面板均正常。
  见 [上游台账](upstream-sync-commits.md)的 panel-simple 与 remotectl 条目。

## BSP 6.1.118 → 6.1.172 与 EVB DTS 骨架

- **改动**：`e4e9c64cb` 同步厂商 BSP，共 **7915 个文件**，为单亲内容同步；
  加入 `rk3588-lsp-evb.dts` 及 DTB 构建项，以 5io 为基础覆盖网口、Combo PHY 与 PCIe 配置，走线参数保留 TODO。
- **原因**：跟进 Rockchip BSP 与板级 SDK，并为自研底板保留独立 DTS。
- **验证**：LubanCat 实板能启动 6.1.172；当次产物核对有 **1995 个模块、7 个基础 DTB、264 个 overlay DTBO**。
- **边界与出处**：`rk3588-lsp-evb` 构建入口复用 5io U-Boot 包；EVB 暂无实物，
  走线 TODO、启动及外设适配未验证。见 [派生板边界](derived-board-bringup.md)与 [内核升级评估](kernel-upgrade-feasibility.md)。

## 已部署测试包的材料记录

以下为截至 2026-10-21 保存的工程内相对路径；构建缓存可能被清理，回退前须核对实际文件。

| 测试包 | 编译服务器 / 开发板工程目录 | 已记录材料 |
| --- | --- | --- |
| `6.1.172-13` | `build/kernel13-sleep-20261021-7k25ol6_/` | 构建与包核验；开发板保留原 `-12` 两包、boot / modules / headers 备份及 `preflight-verification.json`。 |
| `6.1.172-12` | `build/kernel12-20261021-vkxuc5g1/` | 构建、安装与运行核验原件。 |
| `6.1.172-11` | `build/kernel-cma-20261020-ornhomxr/` | 构建日志、源码清单与新包；开发板 `backup/` 保留原 `-10` 两包及 boot / modules / DTB / headers。 |

## 核对源码、包与运行版本

```sh
git log --format='%h %ad %s' --date=short -- kernel-6.1/
git show --stat b4c0c97f4
git status --short -- kernel-6.1/
uname -r
uname -v
dpkg-query -W linux-image-6.1.172 linux-headers-6.1.172
```

- `git log` 也包含厂商历史，不能将所有提交都视作本工程新增；分析使用受跟踪源码，不使用可能过期的 `build/` 副本。
- 包安装后还需核对运行内核、启动 Image / initrd 和运行 FDT；内核包钩子负责 DTB / overlay 发布，见 [DTB 更新](../system/dtb-update.md)。
- overlay 文件和构建生成的 `ubuntuEnv.txt` 同样影响行为；本页的内核提交不能代替其核对，见 [Combo PHY](../hardware/combphy-mux.md)。
