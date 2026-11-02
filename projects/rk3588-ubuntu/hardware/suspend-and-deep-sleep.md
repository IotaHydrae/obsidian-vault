# RK3588 的 deep 挂起与 BL31 唤醒

## 结论

> 本工程 RK3588 的 deep 经 PSCI 进入 BL31，由固件配置 PMU 并恢复系统；能进入挂起不等于唤醒及外设恢复已经通过。

- `s2idle` 不走 BL31 的系统挂起；`deep` 的电源与唤醒策略取决于板级 DT 和目标固件。
- V2 已修复 `vdd_log_s0` 休眠关断导致的无法唤醒；UART2 早醒另以 GPIO-only 临时规避。
- RTC / 电源键已有真实 deep 往返；网络 / 无线唤醒、功耗、长期可靠性及全部外设恢复仍需分别验证。

镜像仍采用[不自动休眠策略](../system/tuning/no-idle-suspend.md)；本文的人工板测不改变该策略。
UART2 诊断见[早醒问题](rk3588-cpu0-immediate-wake.md)，无线卡接线见 [RTL8822CE 唤醒](rtl8822ce-wakeup.md)。

## 方法

先在目标板读取实际状态，不能用旧镜像记录或 `/sys/power/state` 中出现 `mem` 代替确认：

```sh
cat /sys/power/state
cat /sys/power/mem_sleep
cat /sys/power/pm_test
cat /sys/power/suspend_stats/success
cat /sys/power/suspend_stats/fail
```

真实 deep 验证需同时满足 `mem_sleep=[deep]`、`pm_test=none`，并预先准备 RTC 兜底与串口观察。
记录内核 / 有效 DT / BL31 版本、接线、唤醒刺激及挂起计数，再判定结果：

1. BL31 确认进入固件挂起；`PM: suspend entry` 或 `pm_test=core` 返回只说明到达了部分流程。
2. 按 RTC / 电源键的实际事件与 IRQ 增量判定唤回，核对 success 增加、fail 不增。
3. 恢复后分别验证有线传输、USB、PCIe、无线等受影响功能；`PM: suspend exit` 不能替代这些验证。

恢复日志若含 xHCI `Reinit` 或 PCIe Link up，应检查重枚举与链路恢复。
`/dev/videoN`、`eventN`、USB device number 可能变化；稳定路径绑定仍须核对设备是否回到预期拓扑。

## 原因

### s2idle 与 deep 的边界

| 项目 | s2idle | deep |
| --- | --- | --- |
| 入口 | 内核 `s2idle_loop()` / cpuidle | PSCI `SYSTEM_SUSPEND` → BL31 |
| 电源策略 | 设备仍执行 suspend，具体关电由驱动决定 | 板级 `sleep-mode-config` 与 regulator 策略交给固件协同处理 |
| 唤醒约束 | 内核唤醒事件 / 中断，不由 BL31 PMU 位图决定 | 受 PMU 唤醒配置、供电、时钟及中断路径限制 |
| 恢复要求 | 依设备驱动行为 | 固件恢复 DDR / 电源域后，驱动还须完成外设恢复 |

当前 defconfig 未启用 `CONFIG_ROCKCHIP_LITE_ULTRA_SUSPEND`，列表只有 s2idle / deep。
PSCI 提供系统挂起且没有显式指定默认模式时，本 BSP 选择 deep；仍以运行时方括号为准。

### 板级模式与唤醒位图

基础 DTS 的 sleep config 为 `0x01000604`：`RKPM_SLP_ARMOFF_DDRPD`、
`RKPM_SLP_PMU_PMUALIVE_32K`、`RKPM_SLP_PMU_DIS_OSC`、`RKPM_SLP_32K_EXT`。
这组位决定低功耗模式，不是唤醒源清单；固件的低功耗入口 / 恢复代码不能依赖已经挂起的 DRAM。

基础 DTS 的 wakeup config 为 GPIO + CPU0（`0x101`）；V2 当前覆盖为 GPIO-only（`0x100`）。
`RKPM_GPIO_WKUP_EN=BIT(8)`、`RKPM_CPU0_WKUP_EN=BIT(0)` 是唤醒类别，
不能把 GPIO 类等同于仅 RTC / 电源键，也不能把 CPU0 类等同于单一设备。
`pm_config_prepare()` 每轮通过 `sip_smc_set_suspend_mode()` 把所选配置交给 BL31。

### V2：`vdd_log_s0` 休眠策略与唤醒失败

基础 DTS 的 DCDC_REG3 / `vdd_log_s0` 由 `regulator-off-in-suspend` 恢复为 `regulator-on-in-suspend`，
保留 750000 µV 休眠电压和运行电压范围。V2 继承该 rail，当前消费者包括 DMC 的 `mem-supply` 和 VOP 的 `vop-supply`。
原关断策略随 `e4e9c64cb` 引入；修复仅恢复休眠供电，不回退整个 regulator 节点。
5io 使用独立 DTS，本次补丁未改变其 DTB。

RK806 `BUCK3_SLP_EN` 为 `0x06` bit2；保电实验的 regmap trace 有成功写入 `reg=6 val=e4`。
本 BSP 的 `suspend_mem_state/enabled` 显示不能区分 `DISABLE_IN_SUSPEND=1` 与 `ENABLE_IN_SUSPEND=2`，
诊断须交叉核对有效 DT、regulator 约束和实际写入。
停止电源键服务、仅改变 PMIC 长按配置不足以消除该故障；寄存器读取限制见 [RK806 寄存器](rk806-pmic-registers.md)。

保电对照支持恢复该 rail，但尚未确定具体电路失效机制或功耗变化。
它与 [UART2 / CPU0 早醒](rk3588-cpu0-immediate-wake.md)是两个问题：保电解决无法唤回，不能保证不早醒。

### lite / ultra 不等于新增硬件睡眠机制

该 BSP 的 lite / ultra 标签仍进入同一 deep 路径，差别来自按标签选择的 DT 配置和少数驱动分支。
`parse_sleep_config()` 解析各状态属性，`pm_config_prepare()` 缺配置时回落到 mem；
`pwm_bl.c` 的 lite 分支不关背光，`mh248.c` 的 ultra 分支改变霍尔 IRQ wake。
本工程没有相应板级配置，不应仅为增加模式名称而开启此 Kconfig。

## 证据

2026-10-21 已有 V2 / eMMC / Linux `6.1.172 #12` / BL31 v1.53 对照；临时模块只改变 rail 的休眠开关约束，
休眠电压不变，未在整理本文时重跑。

| 对照 | 实际结果 | 判定 |
| --- | --- | --- |
| 原 suspend-off 及撤回对照 | RTC 到期、短按均未唤回，需 Reset | 原休眠关断策略存在故障 |
| 保电，750000 µV 不变 | RTC 请求 35 秒，往返 35.140 秒，IRQ79 +1，BL31 GPIO0=`0x100` | 保电后可完成 RTC 往返 |
| 保电、仍允许 CPU0 类唤醒 | 出现 status=`0x1` 的提前返回 | 另有独立早醒问题 |

原始 rail / DTB 对照在开发板与编译服务器工程 `build/lsp-mem-wake-20261021-dB50Ey/` 的 `evidence/`、`dtb-verify/`。
补丁已纳入 `6.1.172-13` 并确认运行 FDT 生效；RTC / 电源键的部署验证见 [UART2 专题的证据](rk3588-cpu0-immediate-wake.md#证据)，
包、安装与回退材料统一见[内核变更账](../build/kernel-changelog.md)。

BL31 v1.53 反汇编已核对以下输出含义；5io 的旧串口样本不替代当前 V2 的功能验证。

| 输出 | 含义与限制 |
| --- | --- |
| `enter: cfg=...` 与 `armoff_ddrpd` 等模式名 | 模式位图及其解码 |
| `IRQ_EN` / `IRQ_PED` | GIC SPI 使能 / pending 快照，编号为 INTID；pending 不等于源头 |
| `wake up status` | 实际命中状态，不是 wakeup config 的回显；`0x101` 可同时报告 GPIO / CPU0 |
| `GPIO0: ...`、`gpio0_a7` | GPIO 状态 / 唤醒配置线索；GPIO0_A7 为 RK806 INT，电源键通过 PMIC 中断唤醒 |

## 参考

- PSCI 系统挂起入口（镜像源码：`kernel-6.1/drivers/firmware/psci/psci.c`）、s2idle / deep 状态选择（镜像源码：`kernel-6.1/kernel/power/suspend.c`）。
- 板级 DTS（镜像源码：`kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588-lubancat-5.dts`）、V2 覆盖（镜像源码：`kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588-lubancat-5-v2.dts`）。
- 模式与唤醒位定义（镜像源码：`kernel-6.1/include/dt-bindings/suspend/rockchip-rk3588.h`）、配置解析与 SMC 下发（镜像源码：`kernel-6.1/drivers/soc/rockchip/rockchip_pm_config.c`）。
- [BL31 机制](bl31-mechanism.md)、[RK806 电源键](rk806-pwron.md)、[UART2 中断判读](rk3588-cpu0-immediate-wake.md#原因)。
