# 野火 6.1.172 与 Armbian 的差异审查

> 已修复 CMA fault 尾界，并在 V2 内核 `#11` 验证越界返回 SIGBUS、合法读写正常；STMMAC 两项撤回也已纳入该测试包。
> 仍有条件触发的 RS485、挂起恢复和 MIPI 外设问题。不能将这棵树视为原样 Armbian，也不能整体撤回板级适配。

## 基线与覆盖范围

2026-10-16 按完整 Git tree 的路径、mode、blob 哈希比对，不统计构建产物和未跟踪文件：

| 对象 | 精确快照 |
| --- | --- |
| Armbian `linux-rockchip` / `rk-6.1-rkr7.2` | [`a794e888f8eb3feafcf4020f77e968e965426600`](https://github.com/armbian/linux-rockchip/commit/a794e888f8eb3feafcf4020f77e968e965426600)，2026-09-09 |
| 野火 `ubuntu-ebf-rk3588` | [`0c8d06b4965436aa0584f69e42be8f995339fd4c`](https://github.com/erd-yunmu/ubuntu-ebf-rk3588/commit/0c8d06b4965436aa0584f69e42be8f995339fd4c)，2026-09-11 的 6.1.172 导入 |
| 本工程导入 | `e4e9c64cb`；驱动 blob 与上述野火快照完全一致，仅另有 clangd 配置、自研板 DTS 和 DTB 注册 |

Armbian 共 84,799 个跟踪文件，野火共 91,444 个；共同路径 84,431 个，其中 84,380 个完全相同、
**51 个不同**。野火独有 7,013 个，Armbian 独有 368 个；这不是与 Linux mainline 的差异统计。

- 野火独有：6,615 个无线驱动文件、390 个 DTS/overlay、4 个 DMA heap helper、3 个 JLSemi PHY 文件和 1 个 sensor 文件。
- Armbian 独有：350 个 DTS/overlay，其余为 AIC UART 蓝牙驱动、工作流、图像、链接脚本与 selftest 文件。
- 51 个共同修改文件已逐组审查，下载的 Armbian 文件重新计算 Git blob SHA-1，与清单一致。
  新增无线驱动整包只核对集成范围，没有逐行审完 6,615 个文件；板级 DTS 也未逐个做硬件验证。
- 本工程的 PWM remote 已与 Armbian 对齐，另有本工程 FUSB302、VOP2 与本次 CMA 修复。
  本工程的 STMMAC 补丁已删除预等待和四次 PHY 私有寄存器裸写，已纳入 `-11` 测试包；其它差异仍在。

下面未标注板测的项目仍是源码审查结论，不代表已复现。2026-10-20 补充的 PHY 寄存器与通信验证见网口小节。

## 已修复：CMA fault 尾界越界

`drivers/dma-buf/heaps/Makefile` 将 SYSTEM/CMA exporter 切为 `rk_system_heap.o` / `rk_cma_heap.o`。
当前配置启用这两个实现，`DMABUF_HEAPS_ROCKCHIP=n`。

原 `heaps/rk_cma_heap.c:cma_heap_vm_fault()` 使用 `pgoff > pagecount`，
但下一步访问 `pages[pgoff]` 并 `get_page()`；等于尾界时越界。现已改为 `>=`，在访问数组前返回 `VM_FAULT_SIGBUS`。
Armbian 实际编译的 `heaps/cma_heap.c` 已使用 `>=`；野火不仅激活仍含旧错误的 rk 实现，
还将 generic 实现的同一检查改回 `>`。本次同步修复两份实现；当前配置只编译 `rk_cma_heap.o`，板测不覆盖 generic 实现。

上游出处（2026-10-20 核对）：CVE-2024-46852；mainline 提交
[`ea5ff5d351b5`](https://github.com/torvalds/linux/commit/ea5ff5d351b520524019f7ff7f9ce418de2dad87)
（"dma-buf: heaps: Fix off-by-one in CMA heap fault handler"，T.J. Mercier），`Fixes: a5d2d29e24be`。
stable 对应提交 [`e79050882b85`](https://github.com/gregkh/linux/commit/e79050882b857c37634baedbdcf7c2047c24cbff)
也将相同检查改为 `>=`，提交信息注明上述 upstream 来源。

初次 `mmap` 有 `dma_buf_mmap_internal()` 尺寸检查，但不能保护后续扩大映射：
该 exporter 没设 `VM_DONTEXPAND`，`mm/mremap.c:vma_to_resize()` 不会因此拒绝增长，
扩展一页后的首次访问可进入尾界 fault。具备相应 CMA buffer FD 的进程可能触发内核越界访问或崩溃。
已有旧内核板测记录为 `NULL pointer dereference @0x48`、Oops 并重启；越界数组内容不确定，不能将必然 panic 作为判据。

2026-10-20，LubanCat-5 V2 / eMMC / `6.1.172-11`，运行 `#11 SMP Tue Oct 20 16:31:10 CST 2026`，
页大小为 4096。用户提供 `/tmp/cma_tail_repro3.c`，本轮严格编译并独立复测：

| 用例 | 预期与结果 |
| --- | --- |
| `cma`，4 页，extra=1 | 扩展前首末页写入成功；扩展后 `pgoff == pagecount`，收到 SIGBUS |
| `cma`，4 页，extra=2 | `pgoff > pagecount`，保持原有 SIGBUS 行为 |
| `cma`，1 页，extra=1 | `pgoff == pagecount`，收到 SIGBUS |
| `cma-uncached`，4 页，extra=1 | 同一尾界返回 SIGBUS |
| `cma` / `cma-uncached` 各 64 页 | 独立用例写入并读回全部 256 KiB，内容一致 |

Oracle 为数组有效索引 `0..pagecount-1`；越界返回 `VM_FAULT_SIGBUS`，不进入 `pages[pgoff]` 访问。
四个越界子进程均由 SIGBUS 终止（Python returncode `-7`，shell 对应 135），不是启动失败或 `mremap` 失败。
本轮 journal 游标之后无新增 Oops / BUG / panic，boot ID 不变，taint 前后均为 0。
本轮未再次启动旧内核复现；未验证非 4 KiB 页、generic exporter 或长期压力。

复现器源码 SHA256 为 `c9e585e1a3e6ae227c83698d3a369328649502339eb2bd8fcc1093667f5f43ec`；
固定 4096 字节和受控正整数参数只适用于此次板测，源码不自带信号或日志断言。
核验入口是 `cc -O2 -std=gnu11 -Wall -Wextra -Werror /tmp/cma_tail_repro3.c -o /tmp/cma_tail_repro3`，
随后分别运行 `/tmp/cma_tail_repro3 /dev/dma_heap/{cma,cma-uncached} N extra`，从外层检查信号和新增内核日志。
本机原始结果保留在 `/tmp/lsp-cma-commit-20261020-0xuilbcl/board-review.log`，板端为 `/tmp/lsp-cma-commit-check-0xuilbcl/results.json`。

来源为野火 `551b3d038bef8fabe9934585149c999a2804415e`（DMA-BUF Android/RK 功能，2026-04-08），
不是 6.1.172 bump 或本工程后加。本次仅修复边界，没有更改映射策略或其它 heap 功能。

### UVC 使用 CMA 不等于触发该 fault

uvc_queue.c（镜像源码：`kernel-6.1/drivers/media/usb/uvc/uvc_queue.c`） 的普通 MMAP 帧缓冲使用
`vb2_vmalloc_memops`，经 videobuf2-vmalloc.c（镜像源码：`kernel-6.1/drivers/media/common/videobuf2/videobuf2-vmalloc.c`）
的 `vmalloc_user()` 分配；DMABUF 导入则走 `dma_buf_vmap()`，来源取决于调用方。
uvc_video.c（镜像源码：`kernel-6.1/drivers/media/usb/uvc/uvc_video.c`） 的 URB 缓冲使用
`dma_alloc_noncontiguous()`；没有适用 allocator 时，经
mapping.c（镜像源码：`kernel-6.1/kernel/dma/mapping.c`） 的 `alloc_single_sgt()` 回退到
direct.c（镜像源码：`kernel-6.1/kernel/dma/direct.c`） 的直接分配，可尝试系统 CMA。
这些路径不能证明进入了 `rk_cma_heap` 的用户映射 fault；正常开流、CMA 用量变化和导入 fd
均不能替代上述 `mremap` 尾界复现，最小复现也不需要摄像头。

## RS485：中断忙等与 GPIO 获取失败

`drivers/tty/serial/8250/8250_port.c:serial8250_tx_chars()` 在 DE GPIO 分支新增
`5000 × udelay(10)`，最多忙等约 50 ms。调用方 `serial8250_handle_irq()` 此时持有
`spin_lock_irqsave()`，会拖长本 CPU 的关中断时间；TEMT 超时后仍无条件撤 DE，可能截断尚未发完的帧。
例如剩余 32 字节、4800 波特率、8N1 需约 66.7 ms，已超过该固定上限。

`drivers/tty/serial/serial_core.c:uart_get_rs485_mode()`（第 3479 行）又将 DE GPIO
获取的任意错误吞成 NULL，包括 `-EPROBE_DEFER`。GPIO 尚未就绪或已被占用时，UART 仍注册成功，
但丢失 DE 控制；紧邻的 termination GPIO 路径则正确返回 `dev_err_probe()`。

这些代码从野火首次导入 `e01918e5c` 已存在。5io 的两个 `rs485-{1,2}` overlay 明确使用此 GPIO；
默认 uEnv 未启它们，V2 默认也不进入该分支，不能将 50 ms 当成当前每次串口操作的固定成本。
原样 overlay 缺 `dma-names`，实际回退 PIO；若以后启用 UART DMA，还须处理 DMA 绕过此 DE 逻辑的问题。
修复应纳入 8250 的发送完成/RS485 生命周期，不能直接删除等待后立即撤 DE。

## 网口：私有寄存器裸写与缺失的恢复修复

### 通用 MDIO 注册中的 PHY 私有操作

野火的 `drivers/net/ethernet/stmicro/stmmac/stmmac_mdio.c:stmmac_mdio_register()` 在 MDIO 注册完成后无条件对地址 0：
写页寄存器 31 为 `2627`（`0xa43`）、写寄存器 25 为 `0x1801`、回页 0，再写 BMCR `0x8000` 复位。
没有限定 PHY ID，不使用总线锁，不检查四次写操作的结果，也不等待复位完成。
选页失败后仍会继续写，可能作用于错误的寄存器页。

这组代码由野火 `0c8d06b49` 新增，Armbian 没有；V2/5io 的 PHY 地址均为 0，会进入该路径。
完全相同的四次写入与 `msleep(1500)` 可追溯至
[2022 年 JLSemi JL2xx1 workaround](https://github.com/coolsnowwolf/lede/commit/b889ab8789155707f5a73f69d87f33d18e33bb78)，
另见 [JLSemi FAE 说明](https://github.com/CoreELEC/linux-amlogic/pull/13#issuecomment-1370202774)。
本树 `jlsemi-core.h` 将这些位定义为 CLKOUT 使能、125 MHz 和恢复时钟选择；
`jl2xxx_clk_static_op_set()` 已能按 `jl2xxx,clk-enable` 配置。不能把这些 JLSemi 位定义套到 Realtek。

2026-10-20，LubanCat-5 V2 / eMMC / `6.1.172 #10` 实测两个有线 PHY 均为
RTL8211F（`0x001cc916`），两个设备树节点均有 `realtek,clkout-disable`。
RTL8822CE 是独立的 PCIe Wi-Fi，不是这两个 STMMAC 网口的 PHY。
`realtek.c:rtl821x_probe()` 缓存 CLKOUT bit0，`rtl8211f_config_init()` 也只掩码修改 bit0，
并不会恢复被整字写入覆盖的其它位。板测直接观察到最终寄存器差异：

| 模块 | 两个 PHY 的 `0xa43:0x19` | 完成接线数据测试的端口 |
| --- | --- | --- |
| 正式驱动 / 等价重建 A | `0x1800` | eth0；恢复正式驱动后 eth1 |
| B：仅删四次写入，保留 1.5 秒等待 | `0x0862` | eth0、eth1 |
| C：当前源码，同时删除等待与四次写入 | `0x0862` | eth1 |

上述数据测试均协商为 1000 Mbps / full，20 次 ping 无丢包，双向各 32 MiB 的 SHA256 一致，
RX/TX errors、dropped、CRC、length、carrier error 增量均为 0。A 的 `.text` 与正式模块一致；
B/C 在匹配 #10 的构建树编译，导入符号 CRC 匹配。两组 PHY 的 BMCR 均为 `0x1040`，PHYCR1 均为 `0x2118`。
这验证了本板通信不依赖这四行，并证明写入影响会保留到初始化之后；不是线速、长稳或时钟波形测试。
没有据此解释 RTL8211F 其它位的具体作用，也没有证明原值已导致丢包。

当前源码已最小化删除四次裸写，保留 GPIO reset、标准 PHY 初始化和各型号专用配置。
当次临时模块测试结束后恢复正式模块，未替换 `/lib/modules` 或启动文件；后续两项 STMMAC 撤回已纳入 `-11` 测试包并安装启动。
本补丁的冷上电、系统 resume、5io 及 JLSemi 实物尚未验证。

### 模块重载前必须处理 LED fixup 悬空回调

仓库已将四种全局 LED fixup 移到 `stmmac_init()` 注册，每个模块实例只注册一组；
任一注册失败逆序撤销已成功的项，`stmmac_exit()` 统一注销。四个回调的函数体与写入值不变，
删除本文件始终启用的 `LED_FIX` 宏和条件编译。目标 PHY 核心的扫描与注销共用
`phy_fixup_lock`，注销会等待正在执行的回调结束。

旧 `stmmac_dvr_probe()` 每成功 probe 一个网口就注册四项，而 remove / exit 不注销；
双口留下每种两条，模块卸载后回调指针失效。保留原代码的 A 组首次热重载即触发
`phy_scan_fixups()` 跳转到不可执行地址，发生在删除私有写入的 B 组加载之前。
同一 LED 实现可追溯到 Armbian `rk-6.1-rkr7.2` 的
[`69de70aa518c`](https://github.com/armbian/linux-rockchip/commit/69de70aa518c01bb3c12a3313aa263385d077be4)
（`mekotronics: stmmac: hack LED_FIX`）；本工程最早的厂商快照已含该代码。

2026-10-20，LubanCat-5 V2 / 两个 RTL8211F / eMMC / `6.1.172 #11` 临时模块板测：

- 匹配构建树编译成功，78 个源文件哈希一致，新模块 254 个导入符号 CRC 与板上 headers 匹配。
- 新模块完成 **10 轮热重载**；每轮卸载后用独立清理模块核对，四种 UID 均为
  `removed=0 empty`，两口均重新创建并 open。原正式模块的阳性对照为每种删除两条。
- 接线的 eth0 为千兆全双工，重载前后双向各 **32 MiB** SHA256 一致，20 次 ping 成功，
  RX/TX 错误与丢包增量均为 0；eth1 无网线，只验证创建与 open。
- 同一热重载条件下，原版 → 补丁版 → 原版的两个 PHY LED 页 `0xd04` 寄存器
  `0x10/0x11` 均为 `0x6251/0x0008`，采样后恢复原页。初次启动样本的 `0xc160`
  不作为热重载对照；未据此声称冷启动 LED 行为相同。
- boot ID 不变，测试区间无新增内核 Oops / BUG / WARNING / panic；taint 仅新增外部模块的 O 位。
  测试后恢复正式模块，磁盘模块哈希不变，未替换启动文件。

旧 `#11` 是未修复实现；从已加载的旧模块切换时，须先 `rmmod dwmac_rockchip` 移除网口而保留
stmmac，再由临时模块逐项调用 `phy_unregister_fixup_for_uid()` 到 `-ENODEV`，确认清空后才卸载
stmmac。该 API 每次只删一条，不能用会连带卸载依赖的 `modprobe -r dwmac_rockchip` 代替第一步；
临时清理仅适用于已核对无其它同 UID 注册者的本次环境。新模块正常退出不依赖这项清理。
注册前移会使首个 PHY 在 MDIO 注册阶段执行回调；本次未验证冷上电、resume、5io 和其它三种 PHY。

2026-10-21，修复已纳入完整 `6.1.172-12` image / headers 包并在同一开发板启动；
加载的正式 STMMAC 与包内模块一致，接线 eth0 的千兆通信验证通过。
完整包启动与核验口径见 [内核变更记录](kernel-changelog.md#stmmac-led-fixup-生命周期修复已纳入--12-测试包)。
本轮未重复模块热卸载，不将此前临时模块测试写成完整包的重测。

### 没有带入 Armbian 的 suspend/resume 修复

当前 `stmmac_resume()` 先 `phylink_resume()`，再 `stmmac_hw_setup()`；前者启动 PHY/resolve 异步工作，
可与 MAC DMA reset 争跑。Armbian 已通过
[`6a0a08dbd2dee8ebcf0b2434ba20bea3e5df1b44`](https://github.com/armbian/linux-rockchip/commit/6a0a08dbd2dee8ebcf0b2434ba20bea3e5df1b44)
回移 `ef43e5132895ad59b45e38855d32e966bb7434d9`：先恢复所需 PHY 时钟，MAC 初始化完成后才启动 phylink。
前置提交 [`8c6bb402939b37ea121b08ff548f955dde2db211`](https://github.com/armbian/linux-rockchip/commit/8c6bb402939b37ea121b08ff548f955dde2db211)
回移 `367f1854d442b33c4a0305b068ae40d67ccd7d6a`，提供对应 `mac_requires_rxc`、
`phylink_prepare_resume()` 及声明；两项提交均未进入本工程，`-12` 测试包也未包含。

这是沿用旧顺序、未带入该修复，不能推定为野火有意回退。上游故障记录是 Orange Pi 5B + YT8531
挂起恢复后自动协商寄存器被错误 RMW，不能算作本工程 RTL8211F 已复现。
本工程默认不自动休眠，日常触发概率较低；移除固定 1.5 秒等待不等于修复这组竞争。
`motorcomm.c` 还缺基线头提交的 YT8531 resume 后重配，当前两种板的 RTL8211F 不适用。

2026-10-21，LubanCat-5 V2 / RTL8211F / eMMC / `#12` 的修复前检查
**尚未复现目标故障，结论为 INCONCLUSIVE**。判据是完成恢复后，ANAR selector 从 1 变为 0，
同时失去链路或通信；系统未唤醒、其它设备恢复失败或一次无异常都不能替代该签名。

| 路径 | 实测结果与边界 |
| --- | --- |
| 默认 deep，95 秒 RTC 预检 | 挂起前 ANAR=`0x0de1`、千兆 full、ping 3/3；首轮未返回，人工复位。串口在失联后才接入，无恢复后快照，其余 14 轮停止。 |
| deep，持续串口和 PM 回调计时 | 进入 BL31 深睡，RTC 期限后未见返回 Linux；短按无效后人工复位。不能归因于 STMMAC；`ext-32k invalid` 后固件仍继续执行，不能将该行定为根因。 |
| `pm_test=devices` | 不进入 BL31；两 GMAC 恢复回调返回 0，eth0 恢复千兆、ping 3/3。PCIe Wi-Fi `0004:41:00.0` 恢复失败；PHY 停在页 `0xa42`，原探针跳过 ANAR，不能判目标是否复现。 |
| `mem_sleep=s2idle`，单轮 95 秒 | RTC 自动返回，实际挂起 95.3266 秒；PM success 增 1、failed_resume 增 0、boot ID 不变。两 GMAC 回调返回 0，ANAR 前/即时/稳定后均为 `0x0de1`。不覆盖 BL31 深睡。 |

s2idle 用匹配 `#12` 构建树的临时模块采样，19 个导入符号 CRC 与板上 headers 一致。
仅匹配 RTL8211F，在设备/PHY/MDIO 锁保护下选标准页读寄存器，再恢复原页；
挂起前页为 0，恢复后页为 `0xa42`，各次选页均恢复成功，不写 ANAR 或复位配置。
即时快照 ping 未通，随后 eth0 恢复 1000 Mbps / full、ping 3/3；独立复核双向各
32 MiB SHA256 一致、双向 ping 各 10/10，RX/TX errors 与 dropped 增量为 0。
未见新增 Oops / BUG / WARNING / panic；这不是所有外设恢复或长期稳定性验证。
临时 PM/控制台设置已逐项恢复，RTC alarm 为空，探针已卸载，taint 仅新增外部模块 O 位。
Wi-Fi 通过原保存配置重新连接；未改启动文件、磁盘驱动或两项 resume 修复。

原始预检为开发板工程 `build/kernel12-20261021-vkxuc5g1/recovery.tar.gz`；
后续原件在 `build/lsp-resume-diag-20261021-mxzhmt83/` 的 `recovery.tar.gz`、
`devices-evidence.tar.gz`、`devices-analysis.json` 与 `s2idle-evidence.tar.gz`；
本机 `/tmp/lsp-resume-diag-20261021-mxzhmt83/` 有副本、持续串口和 `s2idle-transfer.json`。
设备级原 runner 因回调名称匹配错误返回 3，单独分析保留 INCONCLUSIVE，不覆盖原件；
s2idle runner 返回 5。新启动的 PM 计数不替代旧轮次，未获得故障不代表竞争不存在。

## 非默认 MIPI 外设的条件问题

| 文件 / 入口 | 问题与触发条件 |
| --- | --- |
| `imx415.c:3880`，probe 模式选择 | 无条件用 `supported_modes[capture_mode]` 覆盖先前 HDR 选择；capture-mode 也没有 `< cfg_num` 校验。启用 IMX415 并配置 HDR 或越界 DT 索引时，模式错误或越界解引用。 |
| `imx415.c:2835`，`RKMODULE_SET_HDR_CFG` | 新 capture-mode 分支未给局部变量 `i` 赋值，后面仍用它判断搜索结果；且忽略请求的 HDR 模式，返回结果不可靠。 |
| `goodix.c:1303`，EEPROM 方向覆盖 | 吞掉解析函数所有错误，包括 EEPROM provider 未就绪的 `-EPROBE_DEFER`；先注册后不重读，可能一直使用错误方向。generic DSI overlay 配置了此 EEPROM，需在可重试的 probe 阶段处理依赖。 |
| `ov8858.c`，OTP/AWB/LSC | 删除 OTP 读取和起流应用，AWB/LSC ioctl 仍接受配置但不再应用原校准。需确认 ISP 是否另有替代，不能断言已测得画质退化。 |

默认 HDMI 和 UVC 摄像头不走这些 sensor/触摸路径，本轮未启相应 overlay 或连接相关外设。

## 其它差异与处理边界

- DMA heap 的 `dma_heap_release()` 未配对释放新增 `get_device()` 引用，初始化失败后 `dma_heap_put()` 可泄漏 device；
  新 `dma_buf_get_flags()` 在 exporter 无 callback 时返回成功但不写输出参数。前者是失败路径，后者未找到当前相关消费者调用。
- `rk_system_heap.c` 把 deferred free 改为 release 同步逐页清零并入 pool；大缓冲批量释放可能增加调用线程延迟，
  需量测后判断，不能由源码差异直接宣称退化或删掉清零。
- generic `system_heap.c` / `rk_heaps/` 删除 uncached 支持不代表当前镜像丢失该能力：活动 rk exporter 仍提供
  `system-uncached`、`system-uncached-dma32`、`cma-uncached`，本轮开发板只读确认节点存在。
- `dwmac-rk.c` 按 PHY ID 选择 TX/RX delay 是有目的的板级适配；当前未找到撤销依据。
- `panel-simple` 无 nvmem 时永久 defer、PWM remote IRQ index 问题，已由本工程 `72c2805d0` 修复，不重复算未处理项。
- `dw-dp.c` 只删首次 AUX 恢复 warning 并修正格式类型；等待、恢复和错误处理未变。
  AIC UART 蓝牙成套移除、未启用的 VK2C21 亮度扩展删除和 PCIe EP demo 兼容属于功能范围差异。
- `/proc/cpuinfo` 型号追加、root bootargs 调整、DTB/overlay 清单和 desktop defconfig 变化不能直接当错误回退。
  本轮未发现另一处对当前默认所有设备无条件执行的新增秒级等待。

本工程的 FUSB302、VOP2 gamma、以太网模块化和 BTRFS 裁剪应按
[内核变更账](kernel-changelog.md) 单独维护；不能拿 Armbian 文件整体覆盖它们。
STMMAC 固定等待的 A/B 与冷上电边界见 [Chromium 冷启动](../system/chromium-cold-start.md)。

## 复现比对

对照端用独立 bare 仓库，只取精确提交的 commit/tree；不改本工程 refs：

```sh
git init --bare /tmp/armbian-baseline.git
git --git-dir=/tmp/armbian-baseline.git fetch --filter=blob:none --depth=1 \
  https://github.com/armbian/linux-rockchip.git a794e888f8eb3feafcf4020f77e968e965426600
git --git-dir=/tmp/armbian-baseline.git ls-tree -rz FETCH_HEAD > /tmp/armbian-tree.z
git -C <embedfire-repo> ls-tree -rz 0c8d06b4965436aa0584f69e42be8f995339fd4c:kernel-6.1 > /tmp/embedfire-tree.z
```

解析 NUL 分隔的路径、mode、blob 哈希，先划分相同/修改/单侧独有，再只读取差异文件。
不能把 GitHub recursive tree 的截断结果当完整清单，也不能把当前分支 HEAD 当作历史导入基线。
本轮清单、逐文件 diff 和校验结果保存在本机 `/tmp/lsp-armbian-audit-20261016/`；原始大树与差异不入库。
