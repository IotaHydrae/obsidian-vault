# CM4 启动固件与 EEPROM 更新

> CM4 的 FAT 启动固件与 SPI EEPROM 必须分别备份、升级和验证。

## TL;DR

- `start4.elf/fixup4.dat` 与 EEPROM 是两套输入；替换前分别保存字节、版本和配置。
- 更新固件时保留自定义内核、DT、overlay 和 initramfs，升级后核对实际生效输入。
- 本板新固件通过 2.2 GHz 有限负载回归，不代表其他板或长期稳定性已验证。

## 方法与适用范围

以下保留本板 2026-10-18 已执行升级的输入与判据，供同类诊断复用；不是跨板升级配方。
先区分 FAT 固件与 EEPROM，保存各自可恢复输入；控制替换范围，重启后分别核对运行版本。
版本、下载、暂存、实际运行和回归结果是不同证据层次，不能相互代替。

## 备份的范围

**设备观察：** 旧启动文件和 EEPROM live 配置保存在 `/var/lib/uconsole-firmware/20261018/before/`，
已归档到本地 `~/.cache/uconsole-fw-20261018/device-before.tar.gz`。

- 启动文件备份用于成对恢复 `start4.elf` / `fixup4.dat`，并保存它们的哈希和原配置。
- EEPROM live 配置保留启动策略，不是完整 EEPROM 二进制读回。
- 本轮以官方 `old/stable/pieeprom-2023-01-11.bin` 加保存的 live 配置，重建了 524288 字节的回退镜像。

重建镜像 SHA-256 为 `197de22501bebb9f4de09b4645a9540227cb2bdacfdfc9c19d1f7d463895cb7f`。
它是已知旧版本与配置的组合，不是整片 SPI dump；实际回退写入或 USB 恢复路径尚未验证。

## 重启与回归判据

1. 记录新 boot ID，核对正在运行的 VideoCore 版本；不能仅凭 FAT 文件哈希认定新固件已生效。
2. 单独读取 EEPROM 运行版本和 live 配置；目标镜像下载或暂存成功不代表 EEPROM 已升级。
3. 对比内核、DTB、overlay、initramfs 和正常配置哈希，确认板级适配仍为预定输入。
4. 复核启动、网络、显示与必要外设，区分服务启动、驱动绑定和真实可用的证据。
5. 按[CM4 超频试验判据](../hardware/cm4-overclock-testing.md)重测 2200 MHz / OV6，检查计算日志、退出码、温度、实际频率与固件标志。

新固件可能改变频率或电压管理行为；旧版本下的短时通过不能替代升级后的回归，也不构成长期稳定性保证。
实际显示是否有画面需要对应的可见证据，SSH 可达或 DSI 日志正常不足以确认。

## 两层固件与本次输入

| 层次 | 作用及存储位置 | 升级前观察 | 本轮生效输入 |
| --- | --- | --- | --- |
| 启动分区固件 | FAT 分区中的 VideoCore 固件与配套 fixup 数据，随后加载 Linux | 2024-02-29，版本前缀 `f4e2138c` | 官方 `firmware` tag `1.20260915` |
| Bootloader EEPROM | CM4 板上 SPI EEPROM，负责早期启动和启动介质选择 | 2023-01-11，版本前缀 `8ba17717` | 官方 BCM2711 `default` 2026-09-23，版本前缀 `456428dc` |

**固定来源：** 本轮 `start4.elf` 与 `fixup4.dat` 同取自官方仓库提交
`6f0881cba8bea8ec24956a5718714730e9936ec5`，对应 tag `1.20260915`。
重启后固件自报构建时间为 `2026-09-14 14:49:30`，运行版本为
`a86983925695a7e63166327d7c002d64040ed31d`；仓库提交、tag 与运行版本是不同标识，需分别保存。
这里记录的是固定升级目标，不代表任何时间点的“最新版本”。

**源码依据：** 官方文档要求 `start_file` 与 `fixup_file` 配套，混用会导致无法启动。
EEPROM 使用另一套镜像与升级流程，复制启动分区的两个文件并不会重写 EEPROM。

## 保留自定义内核与板级适配

启动分区固件升级阶段仅替换上述两个启动文件，保留 `6.18.57-v8-uconsole` 内核、uConsole DTB、overlay 和 initramfs。
这些已适配产物的哈希作为升级前后比较依据，不用官方 firmware 仓库中的同名内核或设备树覆盖它们。
正常配置继续保留 `arm_freq=2200`、`over_voltage=6` 及 `fbcon=rotate:1`。

复用结论：更新 VideoCore 固件不要求同时换掉工程内核。控制输入范围后，才能把新变化与固件更新联系起来；
版本号未变仍须核对字节哈希，启动后的频率与电压也要重新测量。

## 与旧版本相比的相关变化

**上游记录：** 以下只比较本轮实际安装的固件，不包括同一发布包里的内核、模块或 DTB 更新。

| 范围 | 安装版本已包含的变化 | 对当前设备的边界 |
| --- | --- | --- |
| 启动恢复 | EEPROM 启动看门狗、恢复分区、分区回退与致命启动错误复位 | 新能力需要对应配置；本轮未启用固件阶段看门狗 |
| SD 与 FAT | 修复分区尾部文件读取、避免启动固件修改 SD 卡并接受 `0xf` 扩展分区 | 属于正确性与兼容性修复，没有吞吐量收益测量 |
| 内存初始化 | BCM2711 DDR 初始化代码更新到 2.35 | 不能从版本号推导本机内存性能提高 |
| 时钟与电压 | 修复 BCM2711 节流时 ARM 提频行为，并将 Pi 4 的 `SET_VOLTAGE` 限定为核心电压 | 升级后需重测频率、电压和保护标志 |
| initramfs | 包含 2026-07-01 的 `Fix auto_initramfs take 3` | 本工程使用显式 initramfs，不能据此宣称启动加速 |

EEPROM 发布记录中 2026-02-06 的约 250 ms 提升仅适用于带 eMMC 的非 Lite 模组，不能套用到本机 SD 启动。
升级时本机显式设置 `initial_turbo=0`，新版默认值变化不构成该轮加速依据；工程 DSI 驱动未随本次固件更新而改变。
后续单独调整此参数的受控观察见[labwc 前的等待与早期提频](cm4-early-desktop.md)。

## 已验证的 CM4 self-update 方法

**升级前观察：** 旧 EEPROM 的 `ENABLE_SELF_UPDATE=1`；设备没有 `flashrom`、spidev 节点或预装的 EEPROM 更新工具。
本轮使用固定官方 `rpi-eeprom` 工具（提交前缀 `7ed1cc2b`），关键配置为：

```sh
CM4_ENABLE_RPI_EEPROM_UPDATE=1
RPI_EEPROM_SELF_UPDATE=1
RPI_EEPROM_IMMEDIATE_UPDATE=0
ENABLE_VL805_UPDATES=0
```

**源码依据：** 此工具要求当前 EEPROM 时间戳不低于 `1650968668`；本机旧版本为 `1673458852`，满足条件。
本轮保留 live 配置，不传会改用镜像默认配置的 `-d`。CM4 的默认工具门控不等于 bootloader 不具备自更新能力。

**暂存观察：** 更新器输出 `Using self-update`，暂存镜像与 `.sig` 校验通过，未安装 `recovery.bin`。
**重启后观察：** bootloader 自报 `2026/09/23 12:02:14`，版本为
`456428dcbedc313131e5dc8be59a80f1f55cd9ba`，时间戳为 `1790164934`；EEPROM 更新已实际生效。
live 配置全文与升级前一致，包含 `BOOT_ORDER=0xf25641`。本机因此验证了无需 `flashrom` / spidev 的官方自更新路径。
随后核对 `pieeprom.upd` 与 `.sig` 仍匹配本轮暂存哈希，再移除这两个暂存文件。

## 本轮结果与证据（2026-10-18）

- **常规启动已确认：** 运行目标 VideoCore 版本，保留原 `6.18.57-v8-uconsole`；内核、DTB、overlay、initramfs、config 和 cmdline 哈希均与升级前一致。
- **系统观察：** `tryboot=0`、`ondemand`、600–2200 MHz、旋转 1，`fb0` 为 `vc4drmfb`，固件标志为 0，failed units 为空；实际可见画面尚无物理确认。
- **EEPROM 生效已确认：** 运行目标版本，live 配置与原文一致；VideoCore 固件、工程内核及上述保留产物仍为预定输入。
- **回归已完成：** 复用原试验程序完成四核 int64 20 s、CPU 120 s、1 GiB VM 60 s、锁定 256 MiB 的 memtester 1 轮及 CPU＋VM 混合 600 s。
  全套 1014.136 s，982 条遥测的最高温度为 62.809 °C，保护标志全为 0；退出码、校验日志和内存覆盖判据均通过。
  未发现 BUG、Oops、panic、OOM 或存储 I/O 错误；启动日志仍保留 Wi-Fi 信道设置 `-52`，不概括为日志完全无错误。
- **日常配置已复核：** 移除临时 15 秒运行中看门狗后，常规启动再次确认两层固件、原输入哈希、
  `tryboot=0`、2200 MHz / OV6 和 `ondemand`；PID 1 看门狗为 0，设备看门狗 inactive，升级暂存文件已清理。

**性能观察：** 相同 2.2 GHz 的 int64 为 5968.04 bogo ops/s，旧固件单次结果为 5966.24，差约 +0.03%；这不足以证明性能提高。
完整回归与测量边界见[超频笔记](../hardware/cm4-overclock-testing.md)，最终状态见[开发状态](../../project/status.md)。
设备证据位于 `/var/lib/uconsole-firmware/20261018/`；完整本地归档为
`~/.cache/uconsole-fw-20261018/device-evidence.tar.gz`，包含回归日志、独立判据复核与最终启动记录。

- [固定的官方 firmware 输入](https://github.com/raspberrypi/firmware/tree/6f0881cba8bea8ec24956a5718714730e9936ec5/boot)
- [官方 start_file / fixup_file 配套要求](https://www.raspberrypi.com/documentation/computers/config_txt.html#start_file-fixup_file)
- [固定的官方 EEPROM 更新器源码](https://github.com/raspberrypi/rpi-eeprom/blob/7ed1cc2b/rpi-eeprom-update)
- [本次 EEPROM 输入的发布记录](https://github.com/raspberrypi/rpi-eeprom/blob/7ed1cc2b210053b5d6537b2beb542663cab7e10b/firmware-2711/release-notes.md)
- [SD 与扩展分区修复](https://github.com/raspberrypi/firmware/commit/458df3adc11fccc3d26d3d6d8864738459290416)、[ARM 节流修复](https://github.com/raspberrypi/firmware/commit/de0f03dd9c1893785d0303797bac63fdded8b17a)
- [核心电压接口修正](https://github.com/raspberrypi/firmware/commit/980e91e3a64dc2e4de251150e0076e276b5f447e)、[initramfs 后续修复](https://github.com/raspberrypi/firmware/commit/ea3bc5a63dc7feb867fedbce1e87594bf36fe0f6)
