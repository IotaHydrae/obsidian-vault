# RK3588 deep 早醒：UART2 调试中断通路

## 结论

> LubanCat-5 V2 / Linux 6.1.172 / BL31 v1.53 的对照支持 UART2 调试中断参与 deep 早醒，具体字节来源尚未确定。

- 已确认：仅在挂起边界屏蔽 UART2 的 GIC INTID365，可等待 RTC；撤回屏蔽后早醒复现。
- 当前规避：`6.1.172-13` 保持 `vdd_log_s0` 供电，并以 GPIO-only `0x100` 代替 `0x101`；RTC / 电源键已验证。
- 尚未完成：UART2 根因修复及以太网、Wi-Fi、蓝牙唤醒。GPIO-only 会限制其他 CPU0 类唤醒，不能据此判定完整需求已满足。

无线卡 WAKE# 的接线与软件条件单独见 [RTL8822CE 深睡唤醒](rtl8822ce-wakeup.md)。

## 方法

本方法用于区分 RTC / 电源键唤回与无外部刺激的提前返回，适用于上述 V2 环境。
真实 deep 对照前须先满足 [vdd_log_s0 休眠供电条件](suspend-and-deep-sleep.md#v2vdd_log_s0-休眠策略与唤醒失败)，
并设置 RTC 兜底；板上操作另遵循工程的远程测试与串口占用约定。

1. 核对运行内核、有效 DT、`mem_sleep=[deep]` 和 `pm_test=none`，记录 RTC、PMIC、UART 中断基线。
2. 记录 BL31 的 `wake up status` / `GPIO0` 与 Linux IRQ 增量，保留原日志，不清空 dmesg。
3. 用匹配当前内核及符号 CRC 的可卸载探针，在 `machine_suspend` begin 屏蔽 INTID365，end 恢复原掩码；
   不清 pending、不读 RX、不改 UART 寄存器。做屏蔽 / 撤回的双向对照，避免把偶然正常当作修复。
4. 按实际事件判定：RTC 须有 IRQ79 增量；按键须有真实短按确认及 PMIC IRQ67/68/69 增量，且 RTC 未先触发。

`mem` 往返包括设备挂起 / 恢复，不能视为固件内驻留时间。
临时变更 wakeup config 时，`pm_config_prepare()` 每轮会重发 Linux 缓存值；提前只发一次 SMC 不足以覆盖本轮挂起。
探针必须保存并恢复原状态，退出后核对模块、IRQ 掩码、RTC alarm 和受影响服务。

## 原因

### 已定位的通路

```text
UART2 接收中断 → GIC SPI333 / INTID365 → CPU0 类唤醒请求 → PMU
```

目标源码将 UART2 交给 fiq-debugger，实际使用 IRQ 模式；`debug_port_init()` 仅启用 `UART_IER_RDI`，
发送使用轮询，调试 IRQ 带 `IRQF_NO_SUSPEND`。因此不能把本次 pending 直接解释为 TX-empty 中断。
`RKPM_CPU0_WKUP_EN=BIT(0)` 控制一类中断唤醒；`RKPM_GPIO_WKUP_EN=BIT(8)` 保留 GPIO 类入口。
关闭前者只限制 deep 唤醒许可，正常运行时 UART 中断仍可处理。

### 尚未确定的字节来源

挂起边界前 RX FIFO 为空，返回后重复观察到 RFL=1、IER=1、IIR 从 `0xc1` 变为 `0xc4`；
一次主动读取消费了 `0x00`。这是接收数据中断，不是 `0x0c` 的空 FIFO timeout。
板端 TX / RX / GND 全部断开、停止日志转发、`console_suspend=Y`、进入 PSCI 前排空 TX 均不足以消除早醒。

BL31 v1.53 在 `0x470ac`、`0x47214` 附近的挂起 / 恢复代码会临时环回、复位 UART 并重配 DLL / LCR / IER，
是内部字节来源的候选机制；尚未证明具体哪条指令生成字节，或字节发生于唤醒前还是恢复中。
当前也缺少 RX 电气波形，不能排除板端电平因素或归咎于外接转接线。

### 中断转储的判读限制

BL31 v1.53 的 `IRQ_EN` / `IRQ_PED` 分别转储 GICD enable / pending，编号为 GIC INTID，不是 Linux IRQ。
INTID365 对应 UART2；INTID321 为 rk_timer；345 尚未确认设备归属；468 对应 HDMI RX。
单次 pending 快照不能定位唤醒源，UART2 结论依赖屏蔽 / 撤回对照。
`wake up status` 为实际命中状态：旧配置下 RTC 可报告 `0x101`，早醒样本为 `0x1`；它不是配置回显。
PSCI 返回后的寄存器已经经过固件恢复，不能当作唤醒瞬间；恢复后首个 arch_timer IRQ 也不能直接认作源头。

## 证据

以下为 2026-10-21 已有板测，均为 LubanCat-5 V2 / eMMC / BL31 v1.53；没有在文档整理时重跑。
`#12` 对照使用临时保电模块、750000 µV 不变；`#13` 使用安装后的 DT 策略，无临时模块。

| 对照 | 关键结果 | 支持的结论 |
| --- | --- | --- |
| `#12` 原 `0x101` | BL31 enter→wake 约 0.204 秒，status=`0x1`，RTC / PMIC IRQ 不增 | 存在 CPU0 类早醒 |
| 仅屏蔽 INTID365，再撤回 | 接线与拔线条件下均可等到 RTC；撤回后早醒复现 | UART2 中断通路参与 |
| 板端整组断线、停日志、console suspend、排空 TX | 各条件仍有提前返回，RTC IRQ 不增 | 这些操作不是可靠规避 |
| UART 返回边界取样 | RFL=1、IIR=`0xc4`；主动读取样本为 `0x00` | 返回边界存在接收数据；来源与时刻未定 |
| `#12` `0x101→0x100→0x101` | 约 5.101→118.787→6.967 秒；仅中间轮 RTC IRQ +1 | GPIO-only 可隔离本次早醒 |
| `#13` RTC 请求 35 / 90 秒 | 往返 35.617 / 89.185 秒，IRQ79 各 +1 | 部署后的 RTC 唤醒通过 |
| `#13` 真实短按，RTC 240 秒兜底 | 往返 36.218 秒，IRQ67/68/69 各 +1，IRQ79 不增 | 部署后的电源键唤醒通过 |

`#13` 三轮 success=3、fail=0。eth0 恢复后的短时双向通信通过，但这不是 WoL 验证；
USB 仍有 xHCI `Reinit`，Wi-Fi connected / 蓝牙可枚举也不等于完整恢复验证。
功耗、长期可靠性和其他外设功能尚未覆盖。

原始记录保留在开发板 / 编译服务器工程的下列目录，不将逐轮日志复制进正文：

| 目录 | 证据入口 |
| --- | --- |
| `build/lsp-mem-wake-20261021-dB50Ey/` | `evidence/cpu0-summary.json`、`cable-summary.json`、`uart-summary.json`、`gpio-summary.json`；`evidence/` 下的 cable / uart / gpio 归档及探针版本映射 |
| `build/kernel13-sleep-20261021-7k25ol6_/` | `deep13-summary.json`、`deep13-evidence.tar`、`boot-verification.json`；构建、安装与回退材料见内核变更账 |

## 参考

- UART2 与 GIC SPI 定义（镜像源码：`kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588s.dtsi`）、fiq-debugger 设备配置（镜像源码：`kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588-linux.dtsi`）。
- UART 初始化与收发实现（镜像源码：`kernel-6.1/drivers/soc/rockchip/fiq_debugger/rk_fiq_debugger.c`）、调试 IRQ 注册（镜像源码：`kernel-6.1/drivers/soc/rockchip/fiq_debugger/fiq_debugger.c`）。
- 唤醒位定义（镜像源码：`kernel-6.1/include/dt-bindings/suspend/rockchip-rk3588.h`）、挂起配置下发（镜像源码：`kernel-6.1/drivers/soc/rockchip/rockchip_pm_config.c`）的 `pm_config_prepare()`。
- V2 当前唤醒配置（镜像源码：`kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588-lubancat-5-v2.dts`）、[测试包与验证范围](../build/kernel-changelog.md)。
