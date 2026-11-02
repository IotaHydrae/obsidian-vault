# Quest 3 的供电角色与实际充电判据

> 当前镜像将 Type-C 固定为 source/host，硬件提供 5V/3A；是否净充电要看 Quest 电池电流和一段时间的电量变化。

## TL;DR

- v2、5io 默认一起启用 `-usbcon-fixedsrc` 与 `-dwc3-rolehost`，防止对端反向给板子供电。
- 该口不能再作为 USB device 接 PC；固定 source 仍可能瞬态断连重枚举。
- `AC powered` 与 `Max charging current` 表示电源在位和合约额度，不能证明净充电。
- 无 root 可从 healthd 日志读取 `c=current_now`，单位 µA，负值为放电。
- 同机实测醒着时净电流接近零，睡下后稳定约 +1.94 A；5V 档能给 Quest 电池充电。

## 当前角色策略与服务

默认 overlay 清单：

```text
lubancat-5-v2.overlays: -usbcon-fixedsrc + -dwc3-rolehost
lubancat-5io.overlays:  -usbcon-fixedsrc + -dwc3-rolehost
```

两者一起使用；固定 source 防止 sink 状态，rolehost 保持数据控制器的 host 角色。
固定角色不等于去抖，CC/VBUS 瞬态仍可能使 tcpm 关 VBUS、设备掉线。
决定与验证见 [typec-rollout-and-decision.md](typec-rollout-and-decision.md)，
故障注入见 [typec-fault-inject-and-verify.md](typec-fault-inject-and-verify.md)。

镜像保留 `lsp-usb-powerfix` 兼容旧角色配置；默认固定 source 正常供电不依赖该服务。
typec 的 data_role、power_role 变化发
`KOBJ_CHANGE`，`99-lsp-typec-powerfix.rules` 用 `systemctl --no-block start` 请求 oneshot 服务。
不使用 `SYSTEMD_WANTS`：它仅在设备首次变为 active 时触发，不能处理已激活端口的连续角色变化
（`systemd.device(5)`）。服务运行中重复 start 合并，不打断正在进行的 swap；退出后的事件可再次启动。
脚本仅在 data host/power sink 时尝试切 source，其它状态退出。
写 power_role 会等待 PD role swap，故由服务执行，不能同步阻塞 udev worker。
`StartLimitIntervalSec=0` 防止连续事件撞启动限流，`TimeoutStartSec=20` 限制 swap 等待。

```sh
cat /sys/class/typec/port0/data_role
cat /sys/class/typec/port0/power_role
journalctl -b -u lsp-usb-powerfix
```

默认固定角色下应为 host/source。发现 sink 时先核对实际加载的 overlay 和镜像版本。

## 电池电流的观测方法

```sh
adb shell dumpsys battery
adb logcat -d -b all | grep 'healthd.*battery l='
```

healthd 示例：

```text
battery l=14 v=3887 t=45.0 h=2 st=2 c=1913865 fc=4986000 cc=1 chg=a
```

| 字段 | 含义 |
| --- | --- |
| l、v、t | 电量百分比、电池电压 mV、电池温度 °C |
| st、h | Android 报告的 battery status、health |
| c | 电池 current_now，µA，带符号；正为充电，负为放电 |
| fc、chg | 满充容量 µAh、充电器类型（a 为 AC） |

`c` 是瞬时电池电流，需结合采样窗口和电量趋势判断净充/净放。
`Charge counter` 是累计容量，短窗口差值的粒度较粗。
电池电流不等于 USB VBUS 电流，板子内核没有 source 输出电流采样接口；
要测输入功率或线缆压降须用外部 USB-C 功率计或测量 VBUS 电压。

## 已核实的充电现象与边界

2026-10-02，同一台 Quest、同一块板、同一次会话的对照：

| 工况 | 电池 c | 电量变化 |
| --- | --- | --- |
| 醒着、屏幕常亮且运行任务 | 约 +250/-240 mA 来回变化，净接近零 | level 保持 13% |
| 按电源键睡下 | 连续样本约 +1.94 A | level 13→14，电池电压 3712→3887 mV |

睡眠窗口 12.5 分钟，Charge counter 增加 392897 µAh，平均约 +1.89 A，
与瞬时样本吻合。同期板侧为 host/source，未观察到 tcpm detach 或 xHCI remove。
这证明该工况下 5V 可充电；不能把一个工况推广为所有任务的功耗或 USB 实际输入电流。

若 AC 在位但电量不升，可在同一供电连接下比较醒着与睡眠的电池电流；
睡下转为稳定正值，说明运行功耗消耗了充电余量。仍需独立检查角色、合约与断连。

detach 后还可能遇到合约未建立：Source_Capabilities 发出后对端未及时回 Request，
tcpm 进入无 explicit contract 的 SRC_READY，Quest 会按普通 USB 电源档处理。
此时即使仍为 source，也须检查协商日志，不能从角色推定已经拿到 3A 合约。
合约上限与 Quest 实际取流之间的差异涉及头显充电策略，现有资料不足以解释全部工况。

## 硬件限制与依据

VBUS 由 5V 电源轨提供，没有升压节点；硬件供电上限为 5V/3A=15W，
仅改 source-pdos 不能提供 9V。Type-C 角色变化依据 BSP 的
`drivers/usb/typec/class.c`，服务行为以 `lsp-usb-powerfix` 与对应 unit/udev 规则为准。
主机查询与外设重新枚举见 [usb-gadget.md](usb-gadget.md)，
Quest ADB 授权见 [usb-gadget-power-and-adb.md](usb-gadget-power-and-adb.md)。
