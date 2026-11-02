# Noctalia 桌面的低电量与熄屏策略

> UPower 负责临界关机，Noctalia 负责提醒/空闲，专用程序处理电源键手势。

## TL;DR

- 低电量提醒 20%，空闲锁屏 300 秒、熄屏 600 秒；实机临时禁锁例外单独保留。
- 短按显示 off/on、2 秒菜单已获 CM4 实体确认；必须先持有 logind inhibitor 再接管。
- PMIC 硬件长按独立存在；显示电源切换不等于系统睡眠，其他核心板单独验证。

## 方法：按不同电源动作分别验证

提醒测试只调整通知阈值，熄屏测试临时缩短 idle 超时并等待真实回调；结束后恢复原配置。
同时检查协议、DRM、背光与实体画面，保留自动恢复发生在备用开屏之前的证据。
电源键先核对图形会话 inhibitor 和独占输入，再分别观察短按与长按菜单；不把这些结果当作挂起或硬断电验证。

## 策略与实现

| 功能 | 当前 Noctalia 默认策略 | 实现与边界 |
| --- | --- | --- |
| 低电量提醒 | 20%，之后 5%、2% 逐级提醒 | 外壳读取 UPower；后两级由固定上游实现 |
| 临界关机 | 沿用发行版 UPower 策略 | 当前实机百分比为 20/5/2，有效临界动作 `PowerOff` |
| 空闲锁屏 | 300 秒 | 镜像默认保留；当前实机按用户要求临时禁用 |
| 空闲熄屏 | 600 秒，输入后恢复 | `wlopm --off '*'` 与 `wlopm --on '*'` 配对 |
| 过渡 | 关闭 idle 淡出与锁屏动画 | `pre_action_fade_seconds=0`、`transition=[]` |
| 自动睡眠 | 不配置 | 当前 CM4 `/sys/power/state` 为空，logind `CanSuspend=na` |

600 秒是工程选择的空闲超时，排在正常的 300 秒锁屏之后，不是硬件限制。
20% 提醒与当前 Noble UPower 的低电量阈值一致，也不是电池校准结果。
UPower 的配置和有效动作仍应在其他发行版、CM5 或其他厂商核心板上重新核对。

Noctalia 5.3.0 将 labwc 接入通用输出后端，该后端执行缺少输出选择器的
`wlr-randr --off/--on`。实机 `wlr-randr --dryrun --off` 返回
`no --output specified before --off`，因此不能直接启用原生 `screen_off`。
工程改用上游支持的自定义 idle command/resume_command，调用
`zwlr_output_power_management_v1` 的 `wlopm`，保留输出布局和缩放。
这是自动熄屏策略的修复；上游菜单和 `noctalia msg dpms-off/on` 的原生命令仍有该限制。
两种 labwc 桌面都安装此工具，目前只为 Noctalia 配置自动熄屏。

## 熄屏与通知的设备证据（2026-10-25）

同一 CM4 Ubuntu SD 启动 `00000000-0000-4000-8000-000000000035`：

- `wlopm 0.1.0-1` 能查询 `DSI-1 on`，证实当前合成器提供所需电源协议。
- 临时将提醒阈值调为 100%，真实电量仍为 64%。配置重载后，通知接口出现
  `Battery low` 和 `Battery: 64% remaining`；截图也包含该通知。仅关闭本次生成的通知，
  随后恢复 20%。没有伪造 UPower 电量或触发关机。
- 将空闲超时临时改为 3 秒，不直接执行熄屏命令。空闲后协议返回 `DSI-1 off`、
  DRM `dpms=Off`，背光 `bl_power` 从 0 变为 4。
- 注入按键后，恢复命令自动运行：协议和 DRM 回到 on/On，`bl_power=0`，
  原亮度 4/9 保留。恢复截图包含正常横屏桌面。
- 测试后恢复 600 秒，保留实机临时的 `lock.enabled=false`。最终备用开屏命令
  位于上述自动恢复观察之后，不能用它代替自动唤醒证据。

随后重复测试，保持熄屏约 15 秒；用户确认“背光完全熄灭，恢复正常”。
这补充了实体背光和恢复画面的观察；唤醒按键仍由软件注入，实体键盘/轨迹球唤醒未单独测试。
电量百分比准确性、真实临界关机、充电/拔电、物理睡眠恢复均未由这些结果证明。
UPower 以 root 运行，其 logind `CanPowerOff=yes`；这证明动作条件具备，不能替代实际关机测试。

非发布证据保存在开发机 `~/.cache/uconsole-desktop-20261025/power/`，
设备同名缓存中保留原配置、测试结果和截图。镜像构建输入已更新，旧导出镜像未更新。

## 电源键机制、手势证据与限制

当前内核将 AXP221 PEK 注册为输入设备 `axp20x-pek`，固定驱动分别上报
`KEY_POWER` 的按下与松开；不应固定依赖它本次分配到的 `event0` 编号。
实机 sysfs 读取到启动阈值 1000 ms、硬件长按关机阈值 6000 ms；没有改写寄存器或
按键实测，不能仅凭该读数证明硬件强制断电是否启用及其实际时刻。

两种 labwc 桌面现由 `uconsole-desktop-clients` 启动专用 evdev 程序：

- `systemd-inhibit --what=handle-power-key --mode=block` 在实际图形登录会话中获取 inhibitor，
  避免 logind 同时执行关机。SSH 即使补齐 GUI 环境变量，也不等于获得活动图形会话的授权。
- 只接管按键能力恰为 `KEY_POWER` 的设备，并使用 `EVIOCGRAB`。
  labwc 在处理绑定前通知 idle；若不独占，同一个唤醒按键可能先让 idle 恢复显示，
  然后又被短按 toggle 关掉。不能抓普通混合键盘来绕过这个竞态。
- 按下不足 2 秒松开：`wlopm --toggle '*'`。持续按住达到 2 秒：先开屏，再调用
  `uconsole-desktop-control power-menu`；Noctalia 使用幂等的 `panel-open session`，
  Waybar 使用现有 fuzzel 电源菜单。长按松开不执行短按。
- 时间来自 evdev 单调时钟；启动时已按住、丢事件、设备断开均丢弃未完成手势。
  VT 切换释放抓取，返回后重新读取状态；合成器退出释放资源。管理进程只回收自有进程组。

**设备观察：** 同一启动中，普通 UID 1000 从图形会话获得 block inhibitor，第二个
抓取请求返回 `EBUSY`；随后用户确认短按熄屏、再短按恢复，以及约 2 秒长按显示菜单。
日志顺序为 `short/short/menu`，启动 ID 未变，最终 `DSI-1 on`、`bl_power=0`，
没有新增 failed units。临时用于启动的 labwc 绑定已移除，原 rc.xml 字节保持一致。
软件及会话清理测试通过；普通键盘、音频用户服务和临时禁用锁屏的用户设置保持。
后续菜单进程组和 inhibitor 包装进程提前退出的清理修订已通过本机父子进程测试，
尚未重新部署实机。

**边界：** 当前 logind 全局 `HandlePowerKey=poweroff` 保留，接管仅在桌面程序存活时有效。
inhibitor 不会取消 PMIC 独立的硬件强制断电；本轮没有测试该阈值。Waybar 的菜单映射
通过软件测试，尚未做实体按键验证。CM5 和其他厂商核心板也尚未验证。
显示熄灭不等于系统睡眠；当前 CM4 内核和唤醒链的障碍见[睡眠研究](cm4-system-sleep.md)。

## 源码依据

- [Noctalia 5.3.0 电量提醒](https://github.com/noctalia-dev/noctalia/blob/v5.3.0/src/system/battery_warning_monitor.cpp)
- [通用输出电源后端](https://github.com/noctalia-dev/noctalia/blob/v5.3.0/src/compositors/ext_workspace/ext_workspace_output_backend.cpp)
- [空闲与恢复回调](https://github.com/noctalia-dev/noctalia/blob/v5.3.0/src/idle/idle_manager.cpp)
- [wlopm](https://git.sr.ht/~leon_plickat/wlopm)
