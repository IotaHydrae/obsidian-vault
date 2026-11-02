# FUSB302 source 连接的 CC/VBUS 去抖与插拔恢复

> 补丁从本工程 Rockchip BSP 6.1.172 的原始驱动重新实现：仅过滤已建立 source 连接的短时
> CC/VBUS 丢失，保留持续丢失和重新插入检测；不支持的 Ra/Ra 组合重新检测，避免进入音频附件状态。

## TL;DR

- 默认 `detach_debounce_ms=100` 是板级抗瞬态策略，**不是** Type-C 规范要求。
  BSP 的 `PD_T_CC_DEBOUNCE=200`（100–200 ms）用于 attach，不能拿来证明 source detach 必须等 100 ms。
- 两类事件各有截止时间，共用一个 delayed work；恢复采样取消对应事件。
  旧回调即使已经开始，也不能提前消费新脉冲。
- 重读 I²C 失败保留事件，每 10 ms 重试；连续 3 次复检失败退出去抖并请求 TCPM 重置。
  这是恢复策略，不是协议定时。BSP 的重置是同步函数，必须释放驱动锁后调用。
- 调试接口只保留只读 `stats`（`version 2`）；删除生产驱动中的合成 `inject`。
  板测直接写 MEASURE，走真实比较器和 IRQ。
- 本地回归：`python3 tests/fusb302-debounce.py`；板测：
  `sudo bash tests/usb-typec-detach-debounce.sh --cycles 20 --keep`。

## 行为与边界

原始基线是 `4740254cd8fc` 的 `drivers/usb/typec/tcpm/fusb302.c`，不是 mainline 驱动。
固定 source 与 DWC3 默认 host 的 DT 配置保持现行方案。

只有 `set_roles(attached=true, source)`、VBUS 已开启、COMP 检测已启用且硬件不在 toggling 时
才过滤丢失事件。初次检测、sink、主动关 VBUS 沿用 BSP 的立即处理。
`set_cc`、源电流/测量阈值变化、重新 toggling、初始化和角色退出使旧事件失效。
若 `set_cc` 前还有待确认事件且新配置仍为已连接 source，则按新测量上下文重新采样，
不依赖芯片再发一个 IRQ。IRQ 锁存已读清但 STATUS0 读取失败时，也保留一次重采样机会。

| 输入 | 行为 |
| --- | --- |
| COMP=1 或 VBUSOK=0 | 开始对应的独立计时，重复丢失 IRQ 不延长持续丢失窗口 |
| COMP=0 或 VBUSOK=1 | 取消对应事件，即使已上报缓存尚未改变 |
| 截止时间前的旧回调 | 检查最新截止时间，继续等待 |
| 到期且重读仍丢失 | 上报 TCPM，完成事件 |
| 到期且恢复 | 丢弃事件并计数 |
| 重读失败 | 保留事件、有界重试；耗尽后重置端口 |
| 卸载/休眠 | 阻止调度并等待 worker；恢复后重采样尚未完成的事件 |

参数范围是 0–1000 ms，0 对新的 IRQ 立即处理；调整值作用于新事件，不重新计算已有截止时间。
1000 ms 是防止误配置导致无界等待的策略上限，不是 Type-C 允许的延迟。

`handle_togdone_src()` 原来接受 Ra/Ra，而 `TOGSS_AA` 分支本来就声明不支持音频附件。
现在两者一致：Ra/Ra 不上报 TCPM，重新 toggling；Rd/Open 与 Rd/Ra 正常建立连接。
这个驱动不增加音频附件支持，也不改变 TCPM 全局状态机。

## 测试判据与限制

Oracle 为 **REQUIREMENT / INVARIANT**：短暂丢失不能拆口，持续丢失仍能拆口、重新插入能恢复；
禁用过滤必须仍能触发断开。恢复超时只是可配置观察窗口，不冒充协议限值。

`tests/fusb302-debounce.py` 提取实际 C 函数，编译进确定性 fake I²C/时间/调度测试：
重复独立脉冲、持续丢失、CC/VBUS 独立截止时间、已启动旧回调、读取失败、上下文变化、
暂停/停止、jiffies 回绕、Ra/Ra 拒绝后 Rd/Ra 恢复。它不模拟 Linux 完整线程调度或物理硬件。

板测用 Python 标准库 i2c-dev ioctl 写 `MEASURE(0x04)=0x20`，还原保存的阈值：

1. 100 ms 下单个 20 ms 脉冲、间隔 70 ms 的两个 20 ms 脉冲：设备号不变、不上报 detach。
2. 0 ms 下同样的 20 ms 脉冲：必须上报 detach 并重新枚举。
3. 100 ms 下持续 250 ms 的变化：循环断开/重连，恢复后必须 USB 已配置、source、VBUS GPIO 高、accessory 为 none。

测试保存并恢复原参数和 MEASURE，不读会清 IRQ 的 `0x3e/0x3f/0x42`，不改 MASK。
没有确认真实 COMP IRQ 时返回 INCONCLUSIVE；失败/超时保留寄存器、stats、FUSB/TCPM 日志。
VBUSOK 不能用 MEASURE 伪造，VBUS 瞬态与 I²C 错误路径只在确定性回归中验证。
`--rebind-cycles 10` 可增加 pending 事件未到期时的 unbind/bind，期间并发读 stats。
实际休眠/唤醒与长期自然故障仍需另做现场验证。

## 已完成的验证

2026-10-12，LubanCat-5 V2 / BSP 6.1.172：匹配内核构建树交叉编译临时外部模块，
仅将测试副本的驱动名改为 `fusb302-review`，通过 unbind/insmod 测试实际驱动，结束后恢复内建驱动。
该临时模块测试未安装内核、未改启动介质。

```text
本地 C 回归: [RESULT] passed=32 failed=0
板上寄存器 A/B + 生命周期: [RESULT] passed=66 failed=0 exit=0
checkpatch: 0 errors, 0 warnings
```

板测包含 20 轮持续 CC 变化/USB 重新配置、10 轮 pending 事件中的 unbind/bind 与并发 stats 读取。
每轮恢复后均为 source、accessory none、VBUS GPIO 高；整段测试的 kernel journal 未见
BUG/Oops/WARNING/Call Trace。恢复内建驱动后确认 USB 配置为 1、MEASURE 为 0x3d、参数为 100。

被测生产源码 SHA-256（测试副本仅改驱动名）：
`414b8b03a4f393331efefb34ed0a57ec6901754ee16ecd482520ae1c13b11a42`。

正式部署：开发板和工程机的 image / headers 均已安装 `6.1.172-9`。
开发板重启后 `uname -v` 为 `#9`，只读 stats version 2、参数 100、无 inject。
工程机按用户要求未重启，仍运行 `#6`，新补丁尚未生效；Quest 的 USB/ADB 连接保持在线。
两台升级前启动文件和旧包均保留在 `/var/backups/lsp-kernel-20261012/`，未刷写整盘镜像。

## 现场 analog_audio 的含义

Quest 拔掉后 partner 停在 `analog_audio`、VBUS GPIO 低，说明 TCPM 处于音频附件状态，
不能仅凭 `STATUS0=0x10` 推断 CC 完全没有电平：COMP=0 只代表低于当前 MDAC 阈值。
Ra 和 Rd 在 Rd 阈值下都可能得到 COMP=0。

故障发生前的日志已随 unbind 丢失，因此**不能确认旧补丁导致了那次现场故障**。
旧补丁的计时/读取失败问题可由回归复现；Ra/Ra 的恢复行为由本次回归直接验证。

## 依据

- `kernel-6.1/drivers/usb/typec/tcpm/fusb302.c`：驱动与 Ra/Ra 检测。
- `kernel-6.1/drivers/usb/typec/tcpm/tcpm.c`：`SRC_ATTACH_WAIT`、`tcpm_tcpc_reset()`。
- `kernel-6.1/include/linux/usb/pd.h`：attach 定时。
- [typec-fault-inject-and-verify.md](typec-fault-inject-and-verify.md)：寄存器注入与现场观测。
