# 关机 / 重启走哪条路（PSCI → BL31 → PMIC）

> 关掉或重启时，**内核之后还发生了什么**：`reboot: Power down` / `Restarting system` 落到 PSCI，
> 由 BL31 决定是写 IOC/GPIO0 还是交给 PMIC。
> 电源键那半边（PWRON 引脚、长按定时器、`DEV_OFF`）见 [rk806-pwron.md](rk806-pwron.md)；
> BL31 的 SIP/系统调用全表见 [bl31-mechanism.md](bl31-mechanism.md) §4。
> ⚠️ 正文**原样搬自 [rk3588-boot-flow.md](rk3588-boot-flow.md) §3.8**（2026-09-18 反汇编 + 真机核实）。

## TL;DR

- `reboot: Power down` → PSCI `SYSTEM_OFF` → **BL31 写 IOC/GPIO0**；重启同理走 PSCI `SYSTEM_RESET`。
- 这条路上**内核侧没有 PMIC 代码参与**（与电源键的硬长按断电是两套机制）。


### 3.8 关机 / 重启走哪条路（2026-09-18 反汇编 + 真机核实）

**结论**：`reboot` 与 `poweroff` 都是 **normal world 先经 SMC 交给 EL3（BL31）执行**，
跟 Linux 的 watchdog 没关系（watchdog 只是"PSCI 那条路失败"的兜底 + 关机卡死时的安全网）。

**重启（`reboot`）**：

1. 内核里挂着**两个** restart handler：`psci_sys_reset`（优先级 **129**，`drivers/firmware/psci/psci.c:325`）
   和 `dw_wdt`（优先级 **128**，`drivers/watchdog/dw_wdt.c:674` + `watchdog_core.c:320`）。
   `register_restart_handler()` 走 `atomic_notifier_chain`，**按优先级降序**调用 ⇒ **PSCI 先跑**。
2. 板子 DT：`psci/method = "smc"`、`PSCIv1.1`；默认 `reboot_mode = COLD` ⇒ 发的是
   **`PSCI_0_2_FN_SYSTEM_RESET` = 0x84000009**（只有 `reboot=w` 那类 WARM/SOFT 才会走 `SYSTEM_RESET2`）。
3. EL3 —— 反汇编 `rk3588_bl31_v1.53.elf` 里引用 `(system reset......` 的那个函数（`0x50d28`）：
   一堆 PMU/CRU 预置寄存器 → 打印 `(system reset......` → `CRU(0xfd7c0000) + 0xc08 ← 0xfdb9`
   → `dsb sy` → WFI 循环（**不返回**）。
   `0xc08` = `RK3588_GLB_SRST_FST`（`u-boot/.../cru_rk3588.h:118`），`0xfdb9` 就是 U-Boot
   `sysreset_rockchip.c:27` 用的同一个**全局软复位魔数** ⇒ **走 CRU，不是 watchdog**。
4. 镜像里**确实有** `wdt_config` 函数（`0x52e04`，写 `0xfd586000`，一个 DT 里都没有的 PMU 域 WDT），
   但**全镜像没有一处调用它**（`bl 0x52e04` 命中 0）⇒ 这段是死代码。
   注意别混：Linux 那个 watchdog 在 `0xfeaf0000`（`dw_wdt`，`/dev/watchdog0`），不是同一个块。
5. 兜底/安全网：PSCI 的 SMC 万一返回了，才会轮到 `dw_wdt`（把超时设到最小再踢一脚）；
   systemd 另有 `RebootWatchdogUSec=10min`（关机过程卡死的兜底，`RuntimeWatchdogUSec=0`
   ⇒ 平时没人喂狗，它也不是"重启的发起者"）。
6. 实测旁证：板上 `sudo reboot` 那次**下一轮启动没有 ext4 日志恢复**（干净卸载后复位）。

**关机（`poweroff`）**：**是 BL31（EL3）执行的**，真正切电的是 PMIC。

1. 内核 `pm_power_off = psci_sys_poweroff`（PSCI ≥0.2 时无条件设置，
   `drivers/firmware/psci/psci.c:330`/`618`）；`do_kernel_power_off()` 会把 legacy `pm_power_off`
   挂进 sys-off 链（`kernel/reboot.c:640`）⇒ SMC **`SYSTEM_OFF` = 0x84000008**。
2. EL3 —— `0x50de4`（引用 `(system poweroff......`）：写 `IOC 0xfd5f0000 ← 0x0f000000`、
   `GPIO0 0xfd8a0000 / 0xfd8a0008 ← 0x00040004`（PMU 域引脚）→ WFI 循环。
   ⚠️ 这两个写如果没让 PMIC 断电，helper（`0x589a8`）的 WFI 循环最终会**跳回 `soc_system_reset`**
   ⇒ 这就是某些板子"关机变成重启"的来源。
3. **PMIC 侧不由内核驱动发起**（板子实测 + 源码）：RK806 走 SPI，
   DT 里有 `pinctrl-names = "default" "pmic-power-off"` + `pinctrl-1 = <&rk806_dvs1_pwrdn>`
   （`rk3588-lubancat-5.dts:640`），但**全内核只有 `rk806-core.c:632` 把它 lookup 出来，
   没有任何消费者**；`rk806-{core,spi,i2c}.c` 也没注册 `sys_off`/syscore 关机回调；
   运行 DT 里 **没有** `rockchip,system-power-controller`（那个属性只被 i2c 的 rk808 家族读）。
   ⇒ 断电来自 EL3 写 PMU 域寄存器/引脚 → RK806 掉电。
4. 另有第三条"虚拟关机"（`virtual poweroff`：保留唤醒源、只关大部分电源）——
   `rockchip_pm_config.c` + `sip_smc_virtual_poweroff()`，**我们的 DT 里没有
   `rockchip,virtual-poweroff`，没启用**。
5. ⚠️ **没在板子上真的 `poweroff` 验证**（断电后无法远程开机，得有人到现场按电源）；
   上面"实际断电"这层是代码证据，不是实测。

**串口实测（2026-09-18，用户抓的原始输出）**：

```console
$ sudo reboot
[ 3171.232690] watchdog: watchdog0: watchdog did not stop!
[ 3171.641613] reboot: Restarting system

$ sudo poweroff
[   30.676204] reboot: Power down
```

- ⚠️ `watchdog0: watchdog did not stop!` **不是**"重启由看门狗触发"：这是
  `watchdog_dev.c:946` 的 **release 路径** —— 关机时 systemd 按默认 `RebootWatchdogSec=10min`
  打开 `/dev/watchdog0` 做"关机卡死兜底"，关 fd 时没写 magic 字符 `V`（`dw_wdt` 有
  `WDIOF_MAGICCLOSE`、`nowayout=n`）⇒ 内核拒绝停狗、打这句 `pr_crit` 并**补一次 keepalive ping**。
  实证：`systemd[1]: Watchdog running with a timeout of 10min.` →
  `kernel: watchdog: watchdog0: watchdog did not stop!` → `systemd-shutdown[1]: Watchdog running …`。
- `reboot: Restarting system` → PSCI `SYSTEM_RESET` → BL31 写 CRU `GLB_SRST_FST`；
  `reboot: Power down` → PSCI `SYSTEM_OFF` → BL31 写 IOC/GPIO0（详见 [bl31-mechanism.md](bl31-mechanism.md) §4）。
- ⚠️ BL31 那两行 `(system reset......` / `(system poweroff......` 常常抓不到（打完立刻复位/断电，
  UART 未必发完）；抓到即是 EL3 执行的直接证据。**实测确认**：reboot 后除上面两条内核日志外没有
  任何其它输出，直接就是下一轮的 `DDR …` → SPL → BL31 banner。
- ⚠️ 复位后那段 BL31 banner **是"哪块介质上的 loader"决定的**：拔掉 SD 卡就从 eMMC 起，
  banner 会显示对应**那块介质上的 loader 版本**。开发板 eMMC 已于 2026-09-18 换成 **v1.53（BL31-only）**
  （只写 `u-boot.itb`，SPL/DDR 未动）；SD 卡上是 v1.53 + BL32。
