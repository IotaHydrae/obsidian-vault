# bq40z50 的 SBS 语义要点（容易踩的那些）

> 本文是 [battery-gauge-bq40z50.md](battery-gauge-bq40z50.md) 的**语义篇**：SBS/智能电池规范里
> 那些"名字像但意思不一样"的字段、单位、符号、以及踩过的坑。
> 硬件接线、驱动与 DT、实测数据、怎么复跑/实时看数据、以及"接了电量计后 GNOME 会自己休眠"的副作用
> 见主篇及 [battery-gauge-live-monitoring.md](battery-gauge-live-monitoring.md)。
> ⚠️ 正文**原样搬自主篇的 §4**。

## TL;DR

- ⚠️ 这些字段**别按名字猜**（`RemainingCapacity` vs `FullChargeCapacity`、`AverageCurrent` 的符号、
  以及 `DesignCapacity` 与实际可用容量的差别都在这一节）。
- 判据一律以 `power_supply` 暴露的属性与实测为准，不照抄 TI 文档的措辞。


## 4. 语义要点（容易踩）

- ⛔ **"充电器广播"（charger broadcasts）是什么、`sbs,disable-charger-broadcasts` 到底做了什么** ——
  这是 SBS 里一个**主从反转**机制，很容易理解反：
  - **正常情况**：SoC（SMBus host）当主，读写电量计（从，0x0b）。**但广播是反过来的**：
    电量计自己当 **SMBus 主机**，主动往总线上写：
    | 发什么 | 发给谁（地址） | 多久一次 | 作用 |
    | --- | --- | --- | --- |
    | `ChargingVoltage()`(0x15) + `ChargingCurrent()`(0x14) | **智能充电器 0x12** | **10~60 s** | 告诉充电器"给我多少 V / 多少 A" |
    | `AlarmWarning()`(0x16) | 主机 **0x14** | 10 s（有 OCA/TCA/OTA/TDA/RCA/RTA 标志时）| 报警 |
    | `AlarmWarning()` | 智能充电器 0x12 | 10 s（有 OCA/TCA/OTA/TDA 时）| 报警 |
    （TRM §4.11「Charge Control SMBus Broadcasts」；地址 0x12/0x14 是 TI 的实现值。）
  - **总开关有两层**：① DF `Settings→Configuration→Power Config` 的 **`[BCAST]` 位**（TRM §14.2.1.3，
    **默认 0 = 关**）—— 它才是"允许广播"的总闸；② `BatteryMode(0x03)` 的
    **`CHARGER_MODE(bit14)`**（1 = 关充电广播，**默认 1**）与 **`AM(bit13)`**（1 = 关报警广播，**默认 1**）。
    ⇒ **出厂默认其实是"不广播"**，只有 DF 里把 BCAST 打开、又清了 CHGM/AM 才会有广播。
  - **主线驱动只做一件事**：DT 里有 `sbs,disable-charger-broadcasts` 时
    `chip->charger_broadcasts = false`，于是**检测到电池的那一刻**
    `sbs_disable_charger_broadcasts()` 把 bit14 **置 1**（`val |= BATTERY_MODE_CHARGER_MASK`）。
    ⛔ **它只会"置 1"、从不"清 0"**，也就是**只会关、不会开**；没写这个属性时驱动**根本不碰**这一位。
  - **本板的实测值**：`BatteryMode(0x03) = 0x6001` = `CHGM(bit14)=1` + `AM(bit13)=1` + `ICC(bit0)=1`
    （ICC 是只读能力位："支持内部充电控制器"）⇒ **这颗包的广播本来就是关着的**。
    所以这条属性在本包上是**双保险**，真正的作用是"换一颗 DF 里 BCAST=1/CHGM=0 的电池时兜住"。
  - 为什么值得防：本板**没有 SBS 充电器**（0x12 上没人应答），广播只会打空 NAK；而 i2c7 是**与音频
    codec `es8388@11` 共享**的，电量计当主设备时可能与 SoC 的传输抢总线（Linux 绑定文档原话：
    *some SBS battery fuel gauges are known to have a buggy multi-master implementation*）。
  - ⚠️ DF 里的 `[BCAST]` **读不到也不用读**：TI 的 DF 访问要先把电量计 **unseal**，而 unseal 需要
    写密钥、有把配置写坏的风险 ⇒ **不要去解封**，按 TRM 的默认值（0 = 关）理解即可。
- ⛔ **`0x0F/0x10/0x18` 的单位由 `BatteryMode(0x03)` 的 bit15 `CAPACITY_MODE` 决定**
  （0 = mA/mAh，1 = cW/cWh）。主线驱动读 `energy_*` 时会**临时把电量计切到 WATTS 模式、读完再切回**
  （`sbs-battery.c` 的 `sbs_get_battery_capacity()`/`sbs_set_capacity_mode()`：`CAPACITY_MODE_WATTS`/`_AMPS`）。
  所以 `energy_now`(97.98 Wh) 与 `charge_now`(9.263 Ah) 数值不同但**是同一份能量**：
  换算电压 = 9797 cWh ÷ 9263 mAh ×10 = **10.58 V**（表内平均电压，**不是端电压** 11.87 V）。别把这两个数直接相除当电压。
- ⛔ **状态位**（`0x16` BatteryStatus）：`FC = bit5`、`FD = bit4`、**`DSG = bit6`**、`INIT = bit7`
  （依据：主线 `sbs-battery.c:80-83` 的 `BATTERY_INITIALIZED/DISCHARGING/FULL_CHARGED/FULL_DISCHARGED`，
  以及 TI 自己的驱动 `FG_FLAGS_DSG = BIT(6)`/`FC = BIT(5)`/`FD = BIT(4)` 完全同口径）。
  实测 `0x00c0` = DSG|INIT = 放电中 + 已初始化 —— 与事实吻合。
  ⚠️ 别按"SBS 表格里 CHG=b3"那套想当然（我第一版脚本就是这么写错的，`0x00c0` 会被误读成"正在充电"）。
  TRM §13.23 的原话：**`DSG=1` = "Battery is in DISCHARGE or RELAX mode"**（放电**或静置**）、
  `DSG=0` = CHARGE 模式；`FC=1` = GaugingStatus()[FC] 为 1 才算充满；`INIT=1` = 计量初始化完成。
- **驱动怎么把它变成 `status`（两级，`sbs-battery.c`）**：
  1. `sbs_get_battery_property()`（`:646-651`）：`FC` → `FULL`；否则 `DSG` → `DISCHARGING`；
     否则（DSG=0 = 充电模式）→ **`CHARGING`**；值超范围/I2C 失败 → `UNKNOWN`。
  2. `sbs_status_correct()`（`:480-504`）再用 `Current(0x11)` 修正：
     `status != FULL && Current == 0` → **`NOT_CHARGING`**（静置时 `DSG=1` 就是靠这一步纠过来的，
     因为 TRM 说 DSG 也代表 RELAX）；`status == FULL` 时再按电流符号改判
     （`>0`→CHARGING、`<0`→DISCHARGING）。电流按 **s16** 解释：负=放电、正=充电。
  3. 状态**变化**时驱动会 `power_supply_changed()` 立刻推事件（`:655-661`），不用等轮询。
  ⚠️ TI 的参考驱动（`bq40z50_fg.c:565-579`）少了第 2 步的"零电流"修正 ⇒ 静置的包它报 `DISCHARGING`，
  主线报 `NOT_CHARGING`（TRM 语义上前者更合理）。
  ⛔ **这条链里没有任何 GPIO/充电器参与**：sbs-battery 不信外部电路，只信电量计自己的 Flags
  ⇒ 板子上有没有插充电器、Type-C/DC 供电路径如何，都**不影响** `status`。
- ⚠️ 驱动里 `voltage_min_design` 与 `voltage_max_design` **读的是同一条 `0x19`**（驱动口径问题，两者必然相等）；
  真正的充电电压上限看 `constant_charge_voltage_max`（`0x15`）。
- ⛔ **时间类字段有"不可用"哨兵 `0xFFFF`，驱动不过滤**（2026-09-21 用户问"没充电为什么充满时间 1092h15m"）：
  SBS 的 `AverageTimeToFull(0x13)` / `TimeToEmptyNow(0x11)` / `AverageTimeToEmpty(0x12)` 单位是**分钟**，
  规范里 **`0xFFFF` = 未知/不支持**；而 `sbs-battery.c` 的 `sbs_unit_adjustment()` **只做 ×60 换算、不判哨兵**
  ⇒ sysfs 里就变成 **3932100 s**，直接换算就是荒唐的 **1092h15m（= 65535 分钟 = 45.5 天）**。
  实测（5io 板，放电中）：`time_to_full_avg = 3932100`，而 `time_to_empty_now/avg` 是正常的 73200/77340。
  ⇒ 读这三个字段时**必须自己把 ≥3932100 s（或 0）当"未知"**（`tools/battery-monitor.sh` 的 `sec()`）；
  `--json` 里未知统一给 **0**。
  **"预计充满耗时"完全由电量计固件算**：TRM §13.20 只说它是 *"predicted time-to-full charge based on
  AverageCurrent()"*（驱动不做任何估算，只换算单位；常见实现是 `(FullChargeCapacity−RemainingCapacity)/|AverageCurrent|`，
  但 TRM 没给公式，属推断）。⚠️ TRM 那一行的 NOTE 写的是 **"65535 = Battery is not being discharged."**
  —— 这句是 **TI 文档笔误**（照抄了 0x11/0x12 的说明），对 `0x13` 的语义应是"**没在充电**"。
  同级命令还有 `0x11 RunTimeToEmpty`（瞬时电流）/`0x12 AverageTimeToEmpty`（平均电流）/
  `0x05 AtRateTimeToFull`（按 `AtRate` 设定值而非实际电流）。TI 参考驱动对这类 0xFFFF 是
  `return -ENODATA`（`bq40z50_fg.c:560`）—— 比主线讲究，但主线这边我们自己在用户态滤即可。
- ⛔ **"单节电压 ×N"是算出来的，不是量出来的**（2026-09-21 用户问"你怎么知道 3.916 V ×3"）：
  * **串数**：`voltage_min_design`（= SBS **0x19 DesignVoltage**，`sbs-battery.c:144`）÷ **3.7 V** 四舍五入
    （本包 11100 mV ÷ 3.7 = 3）。SBS 标准里**没有**"串数"命令 —— 真值在 DF 的 `DA Configuration` 里，
    要 unseal 才能读，所以我们不读。脚本可用 `--cells=N` 覆盖。
  * **单节**：`voltage_now`（端电压）÷ 串数 = **平均每节**，不反映各节差异。
  * **真值在 `CellVoltage1..4`（SBS 0x3F/0x3E/0x3D/0x3C，单位 mV，TRM §13.38-13.35）**：
    **主线 `sbs-battery` 根本不暴露这几个命令** ⇒ 只能解绑驱动后 `i2ctransfer` 直读
    （`.scratch-apps/cell-voltage-raw.sh`，带 trap 绑回）。2026-09-21 实测：

    ```
    Cell1(0x3F)=3919 mV   Cell2(0x3E)=3907 mV   Cell3(0x3D)=3917 mV   Cell4(0x3C)=0 mV  ← 没接 = 3S 实锤
    Voltage(0x09, 整包)=11741 mV      三节之和=11743 mV（差 2 mV）
    脚本估算=(11750)/3 = 3.916 V（平均 3916.7 mV）—— 与 11743/3=3914.3 只差 ~2 mV，但**掩盖了 12 mV 的压差**
    ```

    ⇒ **要看均衡就得读 0x3C-0x3F**；因此脚本里那个标签写的是 **`单节(均)`**（平均值，别当实测单节）。
  * 想"一次读全"用 `DAStatus1`（先写 `ManufacturerAccess 0x00 = 0x0071`，再读 `ManufacturerBlockAccess 0x44`
    的 32 字节）：Cell Voltage1-4 + BAT/PACK 电压 + 每节电流 + 每节功率 + 总功率 + 平均功率（TRM §13.1.52）。
    比读 4 次字省事，但要 MAC 握手（写命令 → 等不忙 → 读块）。
- **主线没有 SOH 属性**（`POWER_SUPPLY_PROP_SOH` 在 6.1 主线的 `power_supply.h` 里根本不存在）；
  健康度要用 `FCC/DesignCapacity`（`0x10/0x18`）自己算，或直接读 `0x4F`（SBS 可选命令）。
- **身份字段（`RR` / `XD-BQ3S12A` / `SN 0001` / `Li-ion`）来自哪、别读错**（2026-09-21 解绑直读核对过）：

  | 监控脚本里显示 | sysfs 文件 | SBS 命令 | 本包原始值 | 说明 |
  | --- | --- | --- | --- | --- |
  | `RR` | `manufacturer` | **0x20 ManufacturerName** | `52 52` | 电池包厂在 DF 里写的厂商代码 |
  | `XD-BQ3S12A` | `model_name` | **0x21 DeviceName** | `58 44 2d 42 51 33 53 31 32 41` | 包型号（"XD-" + 厂家自己的命名） |
  | `SN 0001` | `serial_number` | **0x1C SerialNumber**（字，小端） | `01 00` | ⚠️ **驱动用 `sprintf("%04x")` 打印 ⇒ 是十六进制**：`0010`=16、`00ff`=255；本包 = **1 号**（首件样品） |
  | `Li-ion` | `technology` | **0x22 BatteryType** | `4c 49 4f 4e` = `LION` | 驱动 `sbs_get_chemistry()` 把字符串映射成 Linux 枚举：`LION`→`Li-ion`、`LiP`→`Li-poly`、`NiMH`/`NiCd`、其它→`Unknown` |

  三条容易踩的：
  * `manufacturer`/`model_name`/`technology` 是 **constant string**，驱动**第一次读到时缓存**
    （`sbs-battery.c` 的 `sbs_get_constant_string()`：`if (!buf[0]) …read…`）⇒ 改了 DF 里的名字，
    要重新绑驱动（或重启）才看得到。
  * 直读原始值的姿势：`0x1C` 是**字**（`i2ctransfer -y 7 w1@0x0b 0x1C r2`）；
    `0x20/0x21/0x22` 是**块读**（先 `r1` 读长度字节，再 `r<len+1>`）。⚠️ 读之前必须解绑驱动
    （否则 EBUSY），读完立刻绑回来 —— `.scratch-apps/gauge-strings-raw2.sh` 就是这么做的（带 trap）。
  * `XD-BQ3S12A` 里的 "3S12A" 与实测参数自洽（3 串、设计 11.6 Ah、上限 12.6 V），但**型号命名规则是
    厂商定的**，别当规范解读。这些串只能靠 **TI bqStudio + EV2400 经 SMBus 改 DF**（要先 unseal）——
  ⛔ **我们不 unseal、不写**（见本文件 §4 第一条里同一条铁律）。
