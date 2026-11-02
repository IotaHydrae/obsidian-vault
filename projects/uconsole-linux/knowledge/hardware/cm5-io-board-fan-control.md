# CM5 IO Board 第三方风扇的低速控制

## TL;DR

CM5 默认温控曲线不识别风扇型号，也不区分 CPU 是否超频；第三方风扇可能无法在默认最低 PWM 档稳定转动。应先验证停转后的起转和持续运转，再设置适合该风扇的最低档。

2026-10-28，官方 CM5 IO Board 上一只第三方 Pi 5 样式主动散热器在 PWM `75` 时转速为零，PWM `175` 能从停转起转并稳定约 3000 RPM。设备已持久化更早介入、最低 PWM 更高的专属曲线；这不是本仓库镜像的通用默认值。

## 已采用的设备专属配置

在该设备 `/boot/firmware/config.txt` 的 `[cm5]` 段设置：

```ini
[cm5]
dtparam=fan_temp0=40000,fan_temp0_hyst=5000,fan_temp0_speed=175
dtparam=fan_temp1=50000,fan_temp1_hyst=5000,fan_temp1_speed=200
dtparam=fan_temp2=60000,fan_temp2_hyst=5000,fan_temp2_speed=230
dtparam=fan_temp3=70000,fan_temp3_hyst=5000,fan_temp3_speed=255
[all]
```

温度和滞回单位为毫摄氏度；PWM 范围为 `0–255`。四档升温阈值分别为 40/50/60/70°C，滞回均为 5°C。`175/255` 是控制值比例，不代表额定转速的相同比例。

重启后已从运行中的设备树核对 `cooling-levels=<0 175 200 230 255>`、四个温度及滞回值，用户确认风扇持续转动。配置仍允许低温停转，不是恒速或全速策略。更换风扇、外壳或安装方式后需要重新核对。

## 默认曲线的适用边界

固定上游源码中的 CM5 曲线为：

| 升温阈值 | PWM 控制值 | 滞回 |
| --- | --- | --- |
| 50°C | 75 | 5°C |
| 60°C | 125 | 5°C |
| 67.5°C | 175 | 5°C |
| 75°C | 250 | 5°C |

CM5 风扇节点与 thermal trips（工程源码：`upstream/linux/arch/arm64/boot/dts/broadcom/bcm2712-rpi-cm5.dtsi`） 定义档位和温度；同文件 `cooling_maps` 将温度映射到各档。公共参数映射（工程源码：`upstream/linux/arch/arm64/boot/dts/broadcom/bcm2712-rpi.dtsi`） 中的 `fan_temp*` 将启动配置覆盖到这些属性。

pwm-fan 驱动（工程源码：`upstream/linux/drivers/hwmon/pwm-fan.c`） 的 `pwm_fan_set_cur_state()` 直接采用对应 `cooling-levels` 值，没有检测风扇型号或 CPU 超频状态。超频增加发热、可能需要更积极的曲线，但不能据此说默认曲线“仅适用于未超频”；第三方风扇在默认 CPU 频率也可能与低档 PWM 不匹配。

## 实机观察与验证依据

范围为 2026-10-28 的单台 CM5、4GB RAM、官方 CM5 IO Board、连接板载风扇座的第三方 Pi 5 样式主动散热器。系统为 Ubuntu Base 24.04.5，内核 `6.18.57-v8-16k-uconsole`；散热器具体型号及电源规格未知。

| 条件 | 观察或结果 |
| --- | --- |
| 原曲线，CPU 温度 47.4°C | PWM 为 75，转速为 0；用户观察到风扇间歇转动 |
| 手动保持 PWM 255 | 约数秒后稳定至 6400 RPM；较长保持窗口中用户确认连续转动、风量明显 |
| 先以 PWM 0 等待停稳，再保持 PWM 175 | 能起转，15 秒观察窗口内稳定约 3000 RPM |
| 新曲线，默认 CPU 2.4GHz | 180 秒 CPU/内存混合校验通过，最高 55.1°C，采样的保护状态均为 `0x0` |

混合负载为四个 CPU worker 与一个请求 1GiB 的 VM worker，使用 `stress-ng --cpu-method all --vm-method all --verify --timeout 180s`。通过判据来自校验要求：进程退出 0、无校验失败、最终回执 `passed=true` 且清理无错误。温度和转速本身是观察值，不单独构成通过判据。

该次对照回执为 `stock2400mixed`，运行约 181 秒；无错误报告、`cleanup_errors=[]`，负载采样实际 CPU 频率全部处于目标范围。现场证据另存为 `oc-fan-probe.json`、`oc-fan-fullspeed.json`、`oc-fan-minimum.json` 和 `oc-fan-stock-control-result.json`，不随知识正文发布。

## 推断与仍未知的事项

- **推断：** 默认低档在这只风扇上不足以可靠维持运转；提高最低 PWM 后，停转现象消失。现有证据不支持将其认定为 Linux 驱动故障。
- **未知：** 精确最低起转和持续运转 PWM。两者可能不同；本次验证了 175，没有完整扫描更低档，也没有验证 125。
- **未知：** 不同冷启动、环境温度、积尘和外壳条件下的长期表现，以及其他同类第三方风扇的兼容性。
- **边界：** 散热改善不等于超频稳定。短测温度低、无欠压或无节流，也不能替代计算与内存校验。

驱动支持 `fan-stop-to-start-percent` 与 `fan-stop-to-start-us` 起转辅助，见 `pwm-fan.c` 的 `__set_pwm()` 和属性读取逻辑；当前 CM5 源设备树未设置这两个属性。本次未试验起转辅助，也未确定“短时强起转后降低维持 PWM”能否适用于该风扇。

## 复核方法

定位 `name=pwmfan` 的 hwmon 节点，再同时记录 `pwm1`、`fan1_input`、CPU 温度和 thermal cooling state；不要固定假定它总是 `hwmon2`。判定起转要先等待风扇停稳，判定维持运转要持续观察，不能只取切档瞬间或上一轮转动惯性的转速。

固定 PWM 观察需覆盖 thermal governor 可能重新写入的情况；短时命令结束后的用户反馈不能当作命令生效期间的观察。完成后恢复自动温控，并在普通启动中核对运行设备树和实际转速。先验证默认频率对照，再独立评价超频，保留可恢复的启动配置。

源码基线为 Raspberry Pi Linux 提交 `234188315037848e5b07d65b85d38edc64095caa`。上述设备观察只覆盖本机配置；仓库板型说明见 CM5 IO Board（工程源码：`board/cm5-io-board/README.md`）。
