# 项目交接：当前状态与未完成事项（2026-10-21）

> STMMAC 的固定 1.5 秒等待与四次 PHY 私有写入已合并到 `537f88aa1` 一起撤回；
> LED fixup 生命周期已修复并纳入 `-12`；V2 的单轮 s2idle 与有线通信短测已有记录，
> 目标网口恢复竞争仍未复现，冷上电及其它板型仍未验证。
> 本文保留接手所需的状态、证据和待办；原理、操作契约与完整测量留在专题文档。

先读 AGENTS.md（镜像源码：`AGENTS.md`），再按 [知识索引](reference/source-index.md) 进入专题。
下文设备与实验记录来自 2026-10-20/21，不能替代操作前的现场检查；“已有板测”以注明的日期、板型和版本为限。
内核安装与运行版本以 [内核变更账](build/kernel-changelog.md) 为入口；“未验证”不写成通过。

## 1. Git 与提交

分支为 `24.04-panfrost`。与本页相关的三条提交及重写映射已核对：

| 提交 | 内容 | 重写前 |
| --- | --- | --- |
| `318489315` | 野火相对 Armbian 的差异审查及 PHY 板测说明 | `33599609a` |
| `568133c2b` | 启动事件与显示性能测量口径；补丁内容未变 | `29e154374` |
| `537f88aa1` | 同时删除 STMMAC 的 `msleep(1500)` 与四次私有写入 | `40af78b6d` |

本地 HEAD、远端 refs 与工作区状态在每次操作前重新查询，避免将本页当作同步状态依据。
commit、重写历史和 push 分别遵循 AGENTS.md（镜像源码：`AGENTS.md`） 的授权要求。

旧链保存在 `codex/backup-stmmac-jlsemi-20261020-b0kj8oit`，指向 `33599609a`；不要删除该备份。
重写映射及工作区快照在本机 `/tmp/lsp-stmmac-fold-20261020-b0kj8oit/`。

## 2. 开发板与连接记录

| 项目 | 已有记录与核对入口 |
| --- | --- |
| 板型 | Embedfire LubanCat-5 V2 |
| SSH | 网口测试曾通过 `board@<HOST_02>`、`board@<HOST_01>` 直连；地址可能变化，操作边界以 AGENTS.md（镜像源码：`AGENTS.md`） §8 为入口 |
| 内核安装与运行版本 | 见 [内核变更账](build/kernel-changelog.md) 的现状快照，操作前核对 `uname` 与已安装包 |
| 根分区 / 启动分区 | `/dev/mmcblk0p2` → `/`；`/dev/mmcblk0p1` → `/boot/firmware` |
| 网线测试方式 | 本机与板子直连，两个 RJ45 分别接线测试；不据此推定当前接线状态 |
| Wi-Fi | 已有设备识别结果是 PCIe RTL8822CE |
| 有线 PHY | 已有读数：eth0 / eth1 均为 RTL8211F，ID `0x001cc916` |
| 串口 | 本机 `/dev/ttyUSB0`，1500000 8N1；网口实验结束时 capture 已停止，操作前重新检查占用 |

网口实验结束时，`phy_fixup_cleanup`、`phy_snapshot` 已卸载，`lsp-phy*` 临时 systemd 任务和
`/run/lsp-phy-mdio-20261020` 已清理。工程机的 `#10` 状态属于
[VOP2 修复记录](hardware/vop2-inactive-gamma-panic.md) 中的已有核验；此次文档整理未连接设备。

## 3. STMMAC：完成项与证据

### 同时撤回的厂商 workaround

[LEDE 提交 b889ab878915](https://github.com/coolsnowwolf/lede/commit/b889ab8789155707f5a73f69d87f33d18e33bb78)
在同一个 JLSemi JL2xx1 补丁中加入四次 MDIO 写入和 `msleep(1500)`。
两处代码随 [野火 6.1.172 导入](https://github.com/erd-yunmu/ubuntu-ebf-rk3588/commit/0c8d06b4965436aa0584f69e42be8f995339fd4c)
进入本工程；尚未证实野火直接从 LEDE 移植。

`537f88aa1` 只在代码中删除这两项通用路径操作，保留 DMA reset 轮询、错误处理、GPIO reset 和型号专用配置。
固定等待会在网口 open 持有 RTNL 时拖延 Chromium 初始 netlink 快照；私有写入则未限定 PHY ID、
未使用总线锁，也未检查结果。RTL8211F 后续初始化只恢复 CLKOUT bit0，不能恢复整字写入覆盖的其余位。

### 已有板测与边界

2026-10-20，V2 / eMMC / `#10` 匹配构建树的临时模块：

| 组别 | 代码差异 | 两 PHY 的 `0xa43:0x19` | 接线通信验证 |
| --- | --- | --- | --- |
| A | 与正式驱动等价，保留两项 workaround | `0x1800` | eth0 |
| B | 仅删除四次私有写入 | `0x0862` | eth0、eth1 |
| C | 同时删除等待和四次写入 | `0x0862` | eth1 |
| 恢复 | 重新加载已安装的正式模块 | `0x1800` | eth1 |

各组接线测试均为千兆全双工、20/20 ping，双向各 32 MiB 的 SHA256 一致，
RX/TX errors、dropped、CRC、length、carrier error 增量均为 0。
A 的 `.text` 与正式模块一致；B/C 构建通过且符号 CRC 匹配。
这些是通信完整性短测，不是线速、长稳、时钟波形或全部电源路径验证。

2026-10-16 的等待优化独立 A/B 中，Cage → HMI FCP 中位数为 5.618 → 4.744 秒；
该数据来自软件重启后冷缓存。另有仅删除等待的 DC 上电成功样本，不能用来证明联合补丁已做冷上电对照。
完整口径见 [内核变更账](build/kernel-changelog.md) 与 [Chromium 冷启动](system/chromium-cold-start.md)。

### 热重载发现的独立缺陷

保留原代码的 A 组首次重载就因 `phy_scan_fixups()` 调用失效地址而崩溃，发生在 B 加载之前。
`stmmac_dvr_probe()` 为每个网口注册四种全局 LED fixup，但 remove/exit 没有注销；
双口每种留下两条，stmmac 卸载后回调指针失效。
该缺陷已由 `d2453796a` 修复，并纳入 `6.1.172-12`；完整包验证与临时模块热重载的范围见
[内核变更账](build/kernel-changelog.md#stmmac-led-fixup-生命周期修复已纳入--12-测试包)。

后续实验先仅卸载 `dwmac_rockchip`，保留 stmmac 代码，再用临时 helper 清空已核对的四种回调，
确认每种均返回 `-ENODEV` 后才卸载 core。此清理只适用于已核对注册者的本次环境，
不能直接重跑普通的 `modprobe -r` 热替换流程。来源与 API 边界见
[网口差异审查](build/armbian-kernel-delta.md#网口私有寄存器裸写与缺失的恢复修复)。

## 4. 尚未完成

- **联合补丁验证**：冷上电、5io、JLSemi 实物未验证；完整包的恢复与通信短测见内核变更账，不能推广为长期可靠性结论。
- **目标网口恢复竞争**：单轮 s2idle 未见 ANAR selector 异常，目标故障仍未复现；不要将 INCONCLUSIVE 写成无需修复。
- **其它网口差异**：未带入的 Armbian suspend/resume 修复仍是独立事项，见 [差异审查](build/armbian-kernel-delta.md#没有带入-armbian-的-suspendresume-修复)。
- **其他专项**：WPE、内核模块裁剪、自研底板派生不属于本文已完成项，按下节入口重新核对后推进。
- **知识库审计**：2026-10-20 已修正 hardware 分类计数；仍有其他专题首屏的 5 项既有 FAIL，不能沿用旧交接的“全库 FAIL 0”。

不因文档整理重启工程机、重刷 eMMC、更新卡上 `/opt/lsp-flash/` 载荷或修改受保护现场文件。

## 5. 接续入口

| 任务 | 权威入口 |
| --- | --- |
| CMA 尾界、UVC 分配路径与复现判据 | [CMA 差异与验证](build/armbian-kernel-delta.md#已修复cma-fault-尾界越界) |
| 内核 / U-Boot 改动、验证与未测项 | [kernel-changelog](build/kernel-changelog.md)、[uboot-changelog](build/uboot-changelog.md) |
| kiosk / HDMI / 维护模式 | [会话契约](system/kiosk/session-design.md)、[维护模式](system/kiosk/maintenance-mode.md) |
| 当前启动时间和历史实验口径 | [boot-time](system/boot-time.md)、[boot-time-optimizations](system/boot-time-optimizations.md) |
| VOP2 gamma 修复与 `#10` 证据 | [VOP2 修复](hardware/vop2-inactive-gamma-panic.md) |
| 构建 / overlay 现场验证 | [构建内部](build/build-internals.md)、[overlay 部署](build/overlay-apply.md) |
| 现役测试与退出码 | tests/README（镜像源码：`tests/README.md`）；旧交接里的测试累计次数不能当作本轮结果 |
| WPE / 模块裁剪 / 派生底板 | [WPE](system/wpe-webkit.md)、[模块裁剪](build/kernel-module-trimming.md)、[派生清单](build/derived-board-bringup.md) |
| 板测串口接口 | [serial-exec](hardware/serial-exec.md)；不与其它串口程序抢占端口 |

本次可恢复材料仍在本机临时目录，不属于镜像内容：

- `/tmp/lsp-phy-mdio-20261020-iryn_2dp/`：串口原始日志、各组传输 JSON、切换与恢复日志。
- `/tmp/phy-mdio-20261020-5f41ab7c/`：A/B/C、只读探针、cleanup helper、构建与 ABI 核对记录。
- `/tmp/lsp-stmmac-fold-20261020-b0kj8oit/`：旧工作区快照及提交重写映射。

继续编辑时直接更新当前状态，不再把已完成历史逐轮追加回交接正文。
