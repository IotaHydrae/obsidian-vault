# CM4 系统睡眠与电源键唤醒链

> CM4 的 s2idle 需要独立内核和 GPIO 唤醒修正；候选已离线构建，尚未启动或验证恢复。

## TL;DR

- 熄屏、系统挂起恢复和省电幅度分别验证，不把黑屏称为休眠成功。
- 启用 SUSPEND 时同步修正/审查 GPIO2 wake 链，并重编匹配模块 ABI。
- 独立 release、tryboot 与恢复手段先备好；先自动返回测试，再验证真正 s2idle。
- 没有硬件 RTC；软件 `rtc-test` 仅作为 s2idle 定时验证候选，不代表 deep 唤醒能力。

适用于 Raspberry Pi Linux `234188315037848e5b07d65b85d38edc64095caa` 与 CM4 spin-table 启动；CM5、Rockchip 等须重新核对平台入口与唤醒链。

## 试验方法与恢复边界

1. 原生工具链、独立源码与对象目录、独立 kernel release；保留正常 Image、模块、
   initramfs、config/cmdline，不改默认启动。源码副本不能保留指向生产工作树的 `.git`。
2. 只增加 SUSPEND 及其自动依赖，保留 HIBERNATION 关闭。先构建 Image.gz，再在一次
   Kbuild/modpost 中构建实际模块闭包，避免编译旧配置的 1913 个模块或无关 DTB。
   `PM_SLEEP` 会改变驱动 ABI，必须用新内核生成的符号版本重编匹配模块，禁止混用旧模块。
3. 单独生成匹配 release 的 initramfs，核对 root 所需内建驱动、hook 和真实模块清单。
   tryboot 保留正常配置；它只保证下次复位选回正常启动，不能自动复位已挂起的设备。
   看门狗恢复需在本轮内核/固件验证，并保留物理断电恢复手段。
4. 分别运行 `pm_test=freezer`、`devices`、`platform`，每层都写 `freeze`，检查返回、
   内核日志、suspend_stats 和设备健康，失败即停。前两层与经过 noirq 的 platform
   都会自动返回，不能作为真实睡眠成功证据；s2idle 不使用 processors/core 测试。
5. 真正 s2idle 才使用 `pm_test=none`。保持 logind inhibitor，并用无动作的 evdev
   reader 接管电源键；AXP221 不自动清掉唤醒按键，恢复后的积压事件不能再次触发熄屏或关机。
   记录真实进入/退出、相同 boot ID，分别检查桌面、输入、Wi-Fi、SD 和音频恢复。
6. 分别测量正常桌面、DPMS off、s2idle 的整机功耗。成功日志、黑屏或 CPU 空闲都不能
   替代功率/可解释电池数据；没有测量就不声明续航改善。

当前没有 `/dev/rtc`，`rtcwake -m show` 返回 `unable to find device`。Linux 的
`drivers/rtc/rtc-test.c` 可提供软件 RTC，通过内核定时器与 `rtc_update_irq()`
报告 alarm。它是验证 rtcwake/s2idle 往返的实验输入，不是硬件 RTC，不能用于声明
deep、掉电计时或 suspend-to-RAM 唤醒。先核对 timer/alarm 的真实行为，再安排短时
`rtcwake -m freeze`；只有日志、相同 boot ID 和设备恢复均满足判据才记录成功。

## 源码结论

### s2idle 有通用入口，但电源键唤醒存在具体缺口

`kernel/power/suspend.c` 的通用 s2idle 不要求平台 `suspend_ops`；没有 cpuidle
驱动时，ARM64 的默认 idle 仍使用 WFI。当前配置关闭睡眠，不足以证明硬件永远不能睡眠。

已定位的唤醒链为：

```text
AXP221 PEK -> regmap IRQ -> BCM2711 GPIO2 -> chained GIC IRQ -> PM core
```

PEK suspend callback 对两个虚拟按键 IRQ 调用 `enable_irq_wake`，regmap 将请求
传到 GPIO 父 IRQ。但 `pinctrl-bcm2835.c` 的 `bcm2835_gpio_irq_set_wake()` 在没有
独立 `wake_irq` 数组时返回 `-EINVAL`，该数组只为 BCM7211 分配，当前 BCM2711 不满足。
regmap 忽略父错误。noirq 阶段 `suspend_device_irqs()` 会屏蔽未标记 wake 的 GPIO2；
该 irq_chip 带 `IRQCHIP_MASK_ON_SUSPEND`，会实际关闭对应 GPIO 检测。
因此只打开 `CONFIG_SUSPEND` 仍不能承诺按电源键恢复。

候选修正仅允许 BCM2711 在 `PM_SUSPEND_TO_IDLE` 的挂起阶段接受 wake enable，
并允许恢复/中止时的 disable 清理；保留其他状态的拒绝和 BCM7211 原路径。
这样 IRQ core 可保留并 arm 该 GPIO。s2idle 使用正常中断路由，chained GIC IRQ
不作为普通设备 IRQ 屏蔽；GPIO 的 ARMED IRQ 可先触发 `irq_pm_handle_wakeup()`，
无需在 noirq 阶段通过 I2C 读取 PMIC 即唤醒 PM core。

这是源码推导的试验候选，不是通用 GPIO 生产支持：提前在 probe 阶段请求 wake 的
其他消费者仍不满足此状态限制；过渡期间的快速按键、硬件中断及设备恢复尚需实测。
候选尚未列入默认 `kernel/patches/series` 或生产配置。

### deep 和写盘休眠各有独立阻碍

- 当前没有 PSCI `SYSTEM_SUSPEND` 的平台注册路径。spin-table 的 CPU operations
  没有 `cpu_die`，次级 CPU 下线被拒绝。不能靠开启 Kconfig 补出 deep 平台入口。
- ARM64 `hibernate.c` 对不能离线的多核 spin-table 明确返回 `-EBUSY`，提示
  `no mechanism to offline secondary CPUs`。添加 swap 并不能解决它。
- SD、SDIO Wi-Fi、DWC2 有 PM 回调；VC4 master/DSI 没有完整 system-sleep `.pm`。
  这些源码事实都不能替代显示、USB、网络和存储恢复的实机验证。

启用 SUSPEND 还会启用 `PM_GENERIC_DOMAINS_SLEEP`，noirq 阶段可能关闭 DSI 电源域。
因此 `pm_test=platform` 已须检查显示恢复，不能把显示风险推迟到真正睡眠。
V3D 通过 `DEFINE_RUNTIME_DEV_PM_OPS` 接入强制 runtime suspend/resume，和 VC4/DSI
的缺口不同，不应把两者统称为没有 PM 支持。

这描述当前固件/DT/内核接入方式，没有证明 BCM2711 硬件永远不能支持其他睡眠模式。

## 设备观察

2026-10-25，CM4 Ubuntu SD 启动 `00000000-0000-4000-8000-000000000035`：

- 内核 `6.18.57-v8-uconsole` 的 `CONFIG_SUSPEND`、`CONFIG_HIBERNATION` 均关闭；
  `/sys/power/state` 存在但为空，没有 `mem_sleep`。
- DT CPU 使用 spin-table，没有 PSCI 节点；没有发现 CPU cpuidle 状态。
- AXP221 PEK 的父中断接到 BCM2711 GPIO2，走 regmap IRQ；PMIC 使用 i2c-gpio 总线。
- SD/ext4、DWC2、HID、AXP221/PEK 已内建；76 个运行模块包含显示、Wi-Fi、音频等依赖。
- 没有 RTC 和 swap；当前看门狗 inactive。不能假定 rtcwake 可定时恢复。

以上只读观察没有执行挂起，不能证明恢复能力或待机功耗。

## 离线候选与当前验证边界

独立 release `6.18.57-v8-uconsole-s2idle-r1` 已用本机 GCC 16 构建 Image.gz 和
76 个实际运行模块，一次统一 modpost。ARM64/gzip 头、模块 vermagic、依赖闭包与
`depmod -e -F` / `-e -E` 检查通过；同 release 的 `rtc-test.ko` 已另行编译。
编译器能力变化还引入 RELR 等配置差异，故它并非只改变 SUSPEND 的单变量试验。

候选没有完成 initramfs、单次启动或任何 PM 测试。试验部署破坏 usrmerge 链接，
设备恢复前停止后续部署；故障与当前状态见[开发状态](../../project/status.md)，
预防方法见[保护 /lib 链接](../build/usrmerge-kernel-staging.md)。编译检查不能替代
启动、显示恢复、RTC 定时往返或功耗验证。

非发布候选、逐行源码审查与构建记录保存在开发机
`~/.cache/uconsole-desktop-20261025/powerkey/`。当前生产内核不包含上述实验。

## 主要源码依据

- `drivers/input/misc/axp20x-pek.c`：按键事件、suspend/resume IRQ wake。
- `drivers/base/regmap/regmap-irq.c`：虚拟 IRQ 到父 IRQ 的 wake_count 传递。
- `drivers/pinctrl/bcm/pinctrl-bcm2835.c`：GPIO wake、mask 及 chained handler。
- `kernel/irq/pm.c`、`kernel/irq/manage.c`：wake 标志、noirq 屏蔽和唤醒处理。
- `kernel/power/suspend.c`、`Documentation/power/suspend-and-interrupts.rst`：s2idle 契约。
- `arch/arm64/kernel/smp_spin_table.c`、`smp.c`、`hibernate.c`：CPU 下线与写盘休眠限制。
- `drivers/rtc/rtc-test.c`、`drivers/rtc/interface.c`：软件 alarm 与 RTC wake 事件。
- `drivers/base/power/domain.c`、VC4/DSI、V3D 的 PM operations：电源域与驱动恢复边界。
