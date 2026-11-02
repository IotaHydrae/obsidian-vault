# CM4：Raspberry Pi Desktop 电量与横屏集成

> 空电池图标来自插件未读取有效 capacity；横屏属于会话输出配置，均应与启动耗时分开验证。

## TL;DR

- PMIC 已导出百分比时，修复插件读取严格的 0–100 `capacity`；不要伪造 charge/energy 数据。
- CM4 的 AC 输入上报另需 `CONFIG_CHARGER_AXP20X`；它与电量图标缺陷是两个问题。
- 原生面板保持 720×1280，桌面用 `transform 270`；不能用旋转后的逻辑尺寸修改面板时序。
- 本文是 2026-10-18 Trixie/Raspberry Pi Desktop 的热重启验证；Ubuntu/Noctalia 当前策略见[电源管理](../power/desktop-power-policy.md)。

## 方法：分别验证数据、输入电源和输出变换

先读真实 sysfs/UPower 数据，再检查插件选用了哪种字段；缺少累计能量时保留“未知”。
AC 驱动、百分比显示与横屏是独立变量，分别检查驱动上报、已知图标内容和会话输出设置。
重启后核对包/模块身份与截图，并将首次初始化耗时排除出优化比较。

## 电量图标的源码缺陷

树莓派 `pplug-batt/wfplug-batt` 1.11 后端使用 charge 或 energy 比值，AXP20x 可以
只导出 `capacity`。所以 UPower 有电量而面板空白并不矛盾；缺失测量值也不能解释为零电量。

补丁（工程源码：`image/packages/wfplug-batt/0001-batt-read-capacity.patch`）接受严格整数 0–100，
保留 charge/energy 回退；`present=0` 与不可用数据为未知，没有真实累计数据时不估算剩余时间。
先保留电源类型/作用域分类，避免把离线 AC 当作电池，不改 PMIC 充电寄存器或假设电芯容量。

构建与判据（工程源码：`image/packages/wfplug-batt/README.md`）仅编译三个插件源文件并复用官方资源。
24 个实际后端合成 sysfs 用例覆盖边界、状态更新与多电源选择；这是软件合同检查，不是电池校准。

## AC 输入与百分比是独立问题

固定内核的 `CONFIG_CHARGER_AXP20X` 对应 `axp20x_ac_power`，不同于已经启用的
`CONFIG_AXP20X_POWER` USB 输入驱动。只需重编匹配的目标模块，无需全部 modules。
后续 CM4/CM5 配置启用该驱动；CM5 硬件结果仍需单独验证。

`axp22x-ac` 的 present/online 只描述该输入，不能根据一次 0 值就断言整机没供电，
也不能把驱动报告的 Charging 或编程电流当作实测电芯电流。

## 横屏输出策略

面板时序保持 720×1280，会话 Kanshi 对 DSI-1/DSI-2 使用 transform 270，
逻辑桌面为 1280×720。此处是 Raspberry Pi Desktop 的会话策略，不用于通用载板。
部署和集成后热重启均返回 transform 270，审阅截图为完整横屏壁纸与面板。
登录界面、物理首帧和冷启动重复性没有由此证明。Ubuntu 的缩放另见[外壳对比](labwc-shell-comparison.md)。

## 设备观察与验证条件

2026-10-18，CM4 Rev 1.1、SD、`6.18.57-v8-uconsole`。初次桌面竖放、图标为空，
UPower 报 62%；后续 sysfs 为 present=1、capacity=57、Discharging。
修补的 `wfplug-batt 1.11+uconsole1` 在重启 `00000000-0000-4000-8000-000000000026`
后保持正常填充图标和横屏，当时报告 capacity=54、Charging，AC present/online=1。百分比未校准。

| 采样条件 | systemd 总耗时 | 边界 |
| --- | ---: | --- |
| SD 首次启动 | 28.167 s | 扩容、主机密钥、ldconfig/trim 初始化 |
| 无配置变化的第二次启动 | 20.833 s | 不应将首次初始化消失记为优化 |
| 电量/横屏集成后重启 | 20.328 s | 集成持久性检查，不是单变量计时 |

图形目标和面板进程已存在时，仍曾取得全黑 PNG。集成后完整桌面采样结束于内核后
23.71 s、主机重启后 41.206 s；这是软件合成边界，不能解释约两分钟的用户冷启动观察。
早期 journal 收录时钟、固件时钟、内核单调时钟与物理电源键必须分开。

PackageKit 事务与黑帧重叠，但 updater 异步路径未发现桌面等待更新完成的依赖，
没有证据支持禁用更新器作为修复。LightDM DRM 等待和 NM wait-online 也未达到配置超时。
后续受控结果见[启动时间](../boot/cm4-boot-time.md)，不要混算本轮基线。

## 证据与局限

包 SHA-256 为 `31cfed0a3995b447111f5d3697d2d77350905b113dcb7b74c8e762ac7f882aee`；
已安装 libbatt.so 为 `0fae925438055b701de609a4f917f40dfb5809d5f65c31cccf1317b8caa77c78`。
原始截图、部署与基线在 `~/.cache/uconsole-cm4-postflash-20261018/`。
本轮不证明电量准确性、实际充电、完整外设或最初间歇性黑屏根因，也不自动改写既有镜像；
硬件恢复问题见[启动排查](../boot/cm4-uconsole-boot-debug.md)。
