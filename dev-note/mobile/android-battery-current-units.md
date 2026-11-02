# Android 电流差 1000 倍：定位 sysfs、Health HAL 与框架单位

> 沿原始节点到框架的读取链核对单位和符号；电池侧净功率与 USB 输入功率必须分别测量。

## TL;DR

- Android `BATTERY_PROPERTY_CURRENT_NOW` 的单位为 µA；契约规定充入电池为正、净放电为负。
- 约 1000 倍差异支持 mA／µA 不匹配，但需要连续、相近时刻的多层采样。
- 监测脚本换算正确不等于系统 HAL 已修复；正式补丁还要覆盖所有输出路径并完成实机验证。

## 从哪一层开始查

```text
电量计／驱动 → power_supply sysfs → BatteryMonitor／厂商 Health HAL
                                       → 属性查询
                                       → HealthInfo 更新回调
                                       → Android 框架／应用
```

先确认当前 ROM 使用的 Health 服务及实际节点，不能假设所有设备都从 `/sys/class/power_supply/battery/current_now` 读取。Linux power_supply 类的标准单位也为 µA，但厂商旧驱动可能违背约定。

同批采样的一个案例：

| 读取层 | 原始值 |
| --- | ---: |
| 框架 `current_now` | -315 |
| `battery/current_now` | -315 |
| `bms/current_now` | 约 -315429 |

框架与 battery 相同，而 BMS 约大 1000 倍，结合源码直接返回原值的路径，支持 battery 按 mA 提供、上层按 µA 使用的判断。滤波和采样时刻不同，不要求不同节点逐点相等。该组负号本身不能证明符号契约正确，还要做自然充电／放电对照。

## 功率换算

当电流和电压已确认分别以 µA、µV 表示：

```text
I_A = I_uA / 1_000_000
V_V = V_uV / 1_000_000
P_battery_W = I_uA × V_uV / 1_000_000_000_000
```

例如 500000 µA × 4000000 µV = 2 W，代表该时刻电池侧净功率。手机自身耗电、转换损耗及不同采样位置，使它不等于充电器输出；USB 输入侧需用相应遥测或功率计。参见 [USB-C 能力、合约与实际功率](../linux/usb-c-power-observability.md)。

先固定单位再确认符号。不能为了让充电显示为正而盲目取绝对值或翻转所有节点；边充边用时也可能实际净放电。

## 正式修复需要覆盖什么

- 对齐运行构建的源码、设备树、厂商依赖和 Health 接口，审阅时分支头不等于设备精确 manifest。
- 若属性查询和 HealthInfo 更新分别取数，两条路径都要统一；只改一处会让不同调用者继续不一致。
- 从每次原始采样产生换算值，避免在已换算的缓存结果上重复乘 1000。
- 按已核验的设备属性限制补丁作用范围；设备代号和厂商产品名可能不同。
- 分别验证量级、充放电方向、应用属性与回调结果，确认没有改变充电控制和温控逻辑。

2026-10-20 案例只完成只读监测与 HAL 候选补丁审阅，补丁未针对匹配构建编译和部署。不能把脚本读数正常、补丁能应用或本地检查通过写成系统问题已经修复。

## 参考

- [Android BatteryManager：CURRENT_NOW 契约](https://developer.android.com/reference/android/os/BatteryManager#BATTERY_PROPERTY_CURRENT_NOW)
- [Linux：power_supply 类与单位](https://docs.kernel.org/power/power_supply_class.html)
- [LineageOS：Health 服务入口，案例固定提交](https://github.com/LineageOS/android_vendor_qcom_opensource_healthd-ext/blob/9bdd090e6b5797d339e96b060c03412f36993554/aidl/main.cpp)
