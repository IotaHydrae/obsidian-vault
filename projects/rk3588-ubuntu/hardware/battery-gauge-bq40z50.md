# 电量计 bq40z50（SBS 智能电池）接入与读数验证

> 2026-09-21；平台：**5io 开发板**（`<HOST_10>`，内核 6.1.118）；电量计挂在 **i2c7@0x0b**。
> 目的：**这颗芯片以后要用在自研底板上**，所以先把"接线能不能通、主线驱动能不能读、读数对不对、
> 要不要用 TI 的参考驱动"这四件事一次查清。

## TL;DR

- 结论：**主线 `sbs-battery` 驱动够用**（读数与寄存器对得上）⇒ **不需要** TI 的参考驱动（§5）。
- 接线与地址：**i2c7 @ 0x0b**（5io 开发板，§1）；驱动与 DT 见 §2。
- ⚠️ 字段语义容易踩（`capacity` / `cycle_count` 的刷新节奏与定义），见 §4。
- 没有 DT 节点时可以用 `new_device` **临时绑定**（§6 复跑、§7 实时看数据）。
- ⚠️ 副作用：接了电量计后 **GNOME 会在空闲 15 分钟后自动休眠** —— 已按"永不休眠"策略处理（§8）。

## 0. 一句话结论

| 问题 | 结论 |
| --- | --- |
| 硬件通不通 | **通**。i2c7 上 0x0b 正常应答，与同总线的音频 codec `es8388@0x11` **地址不冲突** |
| 用什么驱动 | **主线 `sbs-battery` 就够**（`CONFIG_BATTERY_SBS=y` 已在镜像里，是 builtin）；TI 的参考驱动**不要用**（见 §5） |
| 读数准不准 | **准**。驱动报的每一项都与**绕开驱动直读的原始 SBS 寄存器**逐条对上（稳定量完全相等；最终一轮 `通过 23 / 失败 0`，见 §3） |
| 还差什么 | DT 里还没有节点（现在是运行时 `new_device` 临时绑的），见 §2 |
| 绑上之后会不会"自己睡" | **2026-09-16 之前的旧镜像会**（空闲 900 s 自动 suspend，那块 5io 板就这样）；**当前仓库镜像不会**（两层默认值把 `sleep-inactive-{ac,battery}-timeout` 都写成 0），见 §8 |
| 要不要读**单节**电压/电流 | ✅ **不要**（2026-09-21 用户确认）：pack 级 SBS 数据就够用；单节只在"均衡诊断/安全事件取证"时才需要，**主线驱动也不暴露**，读一次要解绑驱动直读 `0x3C-0x3F`（做法见 §4）。脚本里那个数只是**平均**（标签已写成 `单节(均)`） |

## 1. 硬件与接线

- 总线：**i2c7 = `i2c@fec90000`**。5iob 的 DTS 里已经是 `status = "okay"` + `pinctrl-0 = <&i2c7m0_xfer>`
  （`kernel-6.1/arch/arm64/boot/dts/rockchip/rk3588-lubancat-5iob.dts:1083`），上面挂着音频 codec
  `es8388@11` —— 也就是说**电量计和 codec 共总线**（`i2cdetect -y 7` 里 0x11 显示 `UU` 就是 codec 被占用）。
- 电量计地址 **0x0b**（bq40z50 的 SBS 默认地址）。
- 实测到的是**成品 3S 锂电包**：`manufacturer=RR`、`model=XD-BQ3S12A`、序号 0001、2026-09-08 生产、
  `cycle_count=1`；设计 11.6 Ah / 128.76 Wh / 11.1 V，充电上限 12.6 V / 6 A。

## 2. 驱动与 DT

- 驱动：**主线 `drivers/power/supply/sbs-battery.c`**，`of_device_id` 是 `sbs,sbs-battery`，
  `i2c_device_id` 里除了 `sbs-battery` 还有 `bq20z65`/`bq20z75`（**没有 `bq40z50` 这个名字**，
  所以 DT `compatible` 必须写 `sbs,sbs-battery`，别写 `ti,bq40z50`）。
- 内核配置：`rockchip_linux_defconfig:1278 CONFIG_BATTERY_SBS=y` ⇒ 板上 `modinfo sbs_battery` 显示
  `filename: (builtin)`，**驱动已经在内核里，只缺一个 DT 节点**。
- **临时绑定 / 解绑（运行时实例化，不改 DT、不重启、不需要重编内核）** —— 本次就是靠它测的，
  现场调试、新板子第一次点灯、或者内核里驱动已 builtin 但 DT 还没加节点时都很好用：

  ```bash
  # 绑（第二个字段是 **i2c_device_id**「sbs-battery」，不是 DT compatible，两者同名但概念不同）
  echo sbs-battery 0x0b | sudo tee /sys/bus/i2c/devices/i2c-7/new_device

  # 确认：多出一个 power_supply 节点，名字是 sbs-<bus>-<addr>
  ls /sys/class/power_supply/                  # → sbs-7-000b
  cat /sys/class/power_supply/sbs-7-000b/uevent
  dmesg | tail -3                              # → sbs-battery 7-000b: battery gas gauge device registered

  # 撤（地址用十六进制；撤完节点立刻消失）
  echo 0x0b | sudo tee /sys/bus/i2c/devices/i2c-7/delete_device
  ```

  | 要点 | 说明 |
  | --- | --- |
  | ⛔ **重启就没了** | 这是内核的"运行时设备实例化"，DT 里没有节点 ⇒ 下次开机不会自动出现。要永久就用下面两条路 |
  | ⛔ **不能对同一总线同一个地址重复 `new_device`** | 第二次会返回 `-EEXIST`（`Device or resource busy`），先 `delete_device` 再绑 |
  | ⛔ **绑定期间 `i2ctransfer` 读不了原始寄存器** | i2c-dev 会直接拒绝已被驱动占用的地址：`Could not set address to 0x0b: Device or resource busy`。要绕开驱动直读原始 SBS 寄存器，**必须先 `delete_device`**，读完再 `new_device` 绑回来（`.scratch-apps/battery-gauge-test.sh` 就是这么做的，且用 `trap` 保证退出前一定绑回来） |
  | 生效条件 | ① 内核里得有对应的 i2c 控制器（`/dev/i2c-7` 存在）；② 驱动得在（`CONFIG_BATTERY_SBS=y` 或模块已 `modprobe`）；③ 器件得真在总线上应答（先 `i2cdetect -y 7`，注意 `i2cdetect` 需要 root） |
  | 名字怎么来的 | `sbs-<bus>-<addr>` 是驱动 `devm_kasprintf(..., "sbs-%s", ...)` 拼的，看 `sbs-battery.c` 里 `sbs_desc->name` |

- 永久生效两条路：
  1. **dtbo 叠加**（不动内核、也不用重编 DTS）—— ✅ **已落**：
     `overlay/boot/firmware/dtbs/rockchip/overlay/rk3588-lubancat-5io-bq40z50-overlay.{dts,dtbo}`
     （`dtc -@` 编好，并用 `fdtoverlay` 在本地叠到 5io 基树上验证过：`i2c@fec90000` 下
     `es8388@11` 与 `battery@b` 并存）。
     构建脚本会自动把这个 `*.dtbo` 拷进启动分区，并把**基名**追加到 `ubuntuEnv.txt` 的 `overlays=`
     —— **不需要**改 `config/uEnv/lubancat-5io.uEnv`（那是用户手写文件，构建脚本本来就自动追加）。
     ⚠️ 该目录下所有 `*.dtbo` 都会被追加到**所有板卡**的 `overlays=` ⇒ 没装电量计的板子上会多一条
     `sbs-battery: probe of 7-000b failed`（**无害、不影响启动**）；不想要就把这个 `.dtbo`
     移出该目录再构建（`.dts` 留着不影响）。
     ⚠️ 自研底板 `rk3588-lsp-evb.dts` 已经**直接写了同一节点** ⇒ 那条路**不要**再叠这个 overlay
     （会变成同地址两个节点）。
     ⏳ 5io 开发板当前仍是运行时 `new_device`（要新镜像/重刷之后才会走 dtbo 这条路）。
     前置条件板上都满足：DTB 里有 `__symbols__`、`dtc`/`fdtoverlay`/`fdtget` 都在。
  2. 直接改板级 DTS 后重编内核 —— **自研底板已落**：`rk3588-lsp-evb.dts` 末尾的
     `&i2c7 { battery@b {...} }`（2026-09-21，`cpp`+`dtc` 编过并反编译核对），
     清单同步在 `notes/build/derived-board-bringup.md` §2.2。
     ⛔ 节点里带 `sbs,disable-charger-broadcasts`：自研底板和 5io 一样**没有 SBS 充电器**，
     而电量计默认可以当 SMBus 主机往 0x12/0x14 发充电器/报警广播（与音频 codec 共享的 i2c7 上
     有干扰风险）—— 这条属性让驱动在检测到电池时把 `BatteryMode` 的 CHARGER_MODE(bit14) 置 1
     （**只会关、不会开**）。机制细节见下面 §4。
- ⚠️ **rk3x 的 I2C 控制器不支持 SMBus 块读**，驱动会打印：
  `sbs-battery 7-000b: I2C adapter does not support I2C_FUNC_SMBUS_READ_BLOCK_DATA. Fallback method does not support PEC.`
  ⇒ 读**字符串/块数据**（厂商名、型号）走的是"自己拼 I2C 传输"的兜底路径，**没有 PEC 校验**。
  功能和读数都正常（实测厂商名/型号都对），但**自研底板若有长走线/干扰，要考虑 PEC 或换支持块读的控制器**。

## 3. 实测数据（2026-09-21，放电中）

驱动视角与原始寄存器（解绑驱动后 `i2ctransfer` 直读）对照，**稳定量逐条相等**：

| 量 | 原始寄存器（SBS） | 驱动属性 | 一致性 |
| --- | --- | --- | --- |
| 电压 | `0x09` = 11868 mV | `voltage_now` = 11869000 µV | ✓（差 1 mV，读取时刻差） |
| 电流 | `0x0a` = −506 mA | `current_now` = −435000 µA | 时刻差（负荷在 −400~−500 mA 之间抖） |
| 平均电流 | `0x0b` = −427 mA | `current_avg` = −419000 µA | ✓ |
| 温度 | `0x08` = 3026 (0.1K) = 29.5 ℃ | `temp` = 295 (0.1℃) | ✓ |
| 相对电量 | `0x0d` = 80 % | `capacity` = 80 % | ✓ |
| 自估误差 | `0x0c` = 1 % | `capacity_error_margin` = 1 % | ✓ |
| 剩余容量 | `0x0f` = 9263 mAh | `charge_now` = 9263000 µAh | ✓ |
| 满充容量 | `0x10` = 11584 mAh | `charge_full` = 11584000 µAh | ✓ |
| 设计容量 | `0x18` = 11600 mAh | `charge_full_design` = 11600000 µAh | ✓ |
| 剩余能量 | `0x0f`(WATTS) = 9797 cWh = 97.97 Wh | `energy_now` = 97980000 µWh | ✓ |
| 设计电压 | `0x19` = 11100 mV | `voltage_min_design` = 11100000 µV | ✓ |
| 循环次数 | `0x17` = 1 | `cycle_count` = 1 | ✓ |
| 剩余时间 | `0x11` = 1098 min | `time_to_empty_now` = 77040 s | 时刻差（电流抖动 ⇒ TTE 跟着变） |
| 状态位 | `0x16` = 0x00c0 | `status = Discharging`（`POWER_SUPPLY_STATUS`） | ✓（bit6 = DSG） |

自洽性判据（全过）：

- 端电压 11868 mV ÷ 3 节 = **3.956 V/节**，落在 3.0–4.2 V ⇒ SOC 80% 合理；
- `剩余/满充 = 9263/11584 = 80.0%` ≈ 上报 SOC 80% ✓；
- 健康度 = 满充/设计 = `11584/11600 = 99.9%`，循环 1 次 ⇒ 新电池 ✓；
- 电流为负 ⇒ 放电，符号方向正确 ✓；`MaxError = 1%` ⇒ 电量计自估误差很小 ✓；
- 充电电压上限 12.60 V = 3×4.2 V ✓。

## 4. SBS 语义要点 → 见 [battery-gauge-semantics.md](battery-gauge-semantics.md)

那些"名字像但意思不一样"的字段/单位/符号与踩过的坑**已搬成单独一篇** →
**[battery-gauge-semantics.md](battery-gauge-semantics.md)**。


## 5.~7. TI 参考驱动的取舍 / 怎么复跑 / 实时看数据 → 见 [battery-gauge-live-monitoring.md](battery-gauge-live-monitoring.md)

不用 TI 参考驱动的理由、复跑方法、实时看数据（含 5io 上的 board-local 落地）**已搬成单独一篇** →
**[battery-gauge-live-monitoring.md](battery-gauge-live-monitoring.md)**。


## 8. ⚠️ 副作用：接了电量计后，**GNOME 会在空闲 15 分钟后自动休眠**（2026-09-21 实测）

**因果链（板上实测，时间戳严丝合缝）：**

1. `sbs-battery` 一绑定，系统里就多了一个 `Battery` 电源节点（`sbs-7-000b`，状态 `Discharging`）：
   `18:07:58 kernel: sbs-battery 7-000b: battery gas gauge device registered`。
2. **UPower 立刻判定"在用电池"**：`busctl get-property ... UPower OnBattery` → `b true`。
   原因很直接 —— 系统里唯一的 line-power 设备是 **Type-C 的 PD 口**（`line_power_tcpm_source_psy_1_0022`，
   `online: no`），而板子是 **DC 圆孔供电**、内核里**没有对应的 AC/line-power 节点**
   ⇒ 有电池 + 没有交流 ⇒ `OnBattery = true`。
3. GNOME 于是用**电池策略**：`sleep-inactive-battery-type='suspend'`、
   `sleep-inactive-battery-timeout=900`（**这两个就是 schema 默认值**，不是谁配的；
   对照 AC 策略是 `type='suspend'` + `timeout=0`，而 `gsettings describe` 明确写 **0 = 永远不睡眠**，
   所以**接电量计之前**这台板子从来不会自动睡）。
4. 机器上**唯一的图形会话是 GDM 登录界面**（`gdm` 用户的 `gsd-power`，seat0，**一个输入设备都没有**）
   ⇒ 它一直处于"空闲" ⇒ 到点就 `Reached target sleep.target` → `systemd-suspend.service`。
   实测：`18:07:58 + 900 s = 18:22:58`，日志里 suspend 正是 **18:22:58**。
5. 按板上 ON 键唤醒：BL31 日志 `wake up status: 0x101` / `GPIO0 interrupt wakeup` / `CPU0 interrupt wakeup`
   —— 电源键走 PMU 唤醒，是**正常唤醒路径**，不是崩溃。深睡入口那几行
   （`GPIO POWER INFO:` / `IRQ_EN:` / `IRQ_PED:` / `gpio0_a7`）的读法见
   `notes/hardware/suspend-and-deep-sleep.md` —— **`gpio0_a7` 就是 RK806 PMIC 的中断线**，也就是 ON 键。
   （顺带：这块 5io 开发板跑的是**旧镜像**，日志里 `BL31 ... fwver: v1.47`；仓库/prebuilt 现在是 **v1.53**。）

**为什么恰好是 900 s：那块板子跑的是 2026-09-16 之前的旧镜像**（本次补上的最后一环）：

- 它**既没有** `/usr/share/glib-2.0/schemas/99-lsp-desktop.gschema.override`（`99-lsp-branding.*` 也没有），
  **也没有** `/etc/dconf/db/local.d/`（`/etc/dconf/profile/user` 只有 `user-db:user` + `system-db:ibus`）
  ⇒ 只剩 stock 默认值。Ubuntu 的 `10_ubuntu-settings.gschema.override` **只把 AC 设成 0**，
  **电池那一组仍是 schema 默认 `900` / `'suspend'`**。
- 实测判据（`GSETTINGS_BACKEND=memory` = 强制只读编译期默认值、绕开 dconf）：

  ```
  # 旧镜像（cat@<HOST_10>，kernel 6.1.118）   # 我们的镜像（board@<HOST_07>）
  sleep-inactive-ac-timeout       0               sleep-inactive-ac-timeout       0
  sleep-inactive-battery-timeout  900             sleep-inactive-battery-timeout  0
  sleep-inactive-battery-type     'suspend'       sleep-inactive-battery-type     'suspend'
  ```

**✅ 当前仓库的镜像已经免疫，不需要为此改任何东西**（2026-09-16 提交 `087981515` 起）：

- 两层默认值都写的 0：`overlay/usr/share/glib-2.0/schemas/99-lsp-desktop.gschema.override:14-15`、
  `overlay/etc/dconf/db/local.d/01-desktop-behavior:15-16`（`[org/gnome/settings-daemon/plugins/power]`）。
- 源码判据（gnome-46 `plugins/power/gsd-power-manager.c` 的 `idle_set_mode()`）：

  ```c
  if (timeout_sleep != 0) {                     /* timeout=0 ⇒ 这个 idle watch 根本不注册 */
          if (action_type != GSD_POWER_ACTION_NOTHING) {
                  manager->idle_sleep_id = gnome_idle_monitor_add_idle_watch (...);
  ```

  ⇒ **`timeout=0` 与 `type='nothing'` 两条路都能达到"永不自动睡"**；我们的 `type` 仍是 `'suspend'`，
  但在 `timeout=0` 前提下是死代码，无害。
- 复现/体检：

  ```bash
  GSETTINGS_BACKEND=memory gsettings get org.gnome.settings-daemon.plugins.power sleep-inactive-battery-timeout   # 期望 0
  GSETTINGS_BACKEND=memory gsettings get org.gnome.settings-daemon.plugins.power sleep-inactive-ac-timeout        # 期望 0
  ```

**旧镜像上要当场止血（这块 5io 板就是）**，二选一：

| 做法 | 命令 / 说明 |
| --- | --- |
| 不绑电量计 | `echo 0x0b \| sudo tee /sys/bus/i2c/devices/i2c-7/delete_device` —— 立刻生效，但电量计数据也没了 |
| 补上我们的默认值 | 把上面那两个文件装进去，再 `sudo glib-compile-schemas /usr/share/glib-2.0/schemas` + `sudo dconf update`（重启后仍有效） |

**✅ 2026-09-21 已在 5io 板（`cat@<HOST_10>`）实施**，三个坑都踩到了、记下来：

1. 那块旧镜像**没有** `/etc/dconf/profile/user`（只有 ibus 包带的 `system-db:ibus`）⇒ 只放
   `/etc/dconf/db/local.d/` 是**死的**，必须同时建 `profile/user`（`user-db:user` + `system-db:local`）。
2. override 用的是**只含电源切片**的版本 —— 那块旧镜像**没有 kitty**，整份照拷会把
   `default-applications.terminal` 指到不存在的 kitty，反而弄坏 Ctrl+Alt+T。完整那份只进我们的镜像。
3. 装完必须 `systemctl restart gdm`：运行中的 `gsd-power` 不会自己重读编译后的 schema（值是在
   `idle_set_mode()` 里算的）。

实测（重启 gdm 之后）：greeter 会话里 `sleep-inactive-{ac,battery}-timeout=0`、
`{ac,battery}-type='nothing'`；`GSETTINGS_BACKEND=memory` 读编译期默认值同样是 `0`/`'nothing'`；
电量计仍绑在 `7-000b`。`tools/verify-image.sh`（live，普通用户跑）新增的 4 条断言全过。

⛔ **不要只改自己用户的 dconf**：会睡的往往是 **GDM greeter 会话**（那块板子上跑 `gsd-power` 的就是 `gdm`），
只改普通用户的值对它无效；要改就改**系统级**（gschema override 或 `/etc/dconf/db/local.d` + `dconf update`）。

⚠️ 若哪天真要"电池供电时自动睡"，那才需要方案 B：给 DTS/驱动补一个真实反映 DC 适配器的 AC/line-power
节点（让 `OnBattery` 反映真实供电来源），GNOME 策略侧不用动。
