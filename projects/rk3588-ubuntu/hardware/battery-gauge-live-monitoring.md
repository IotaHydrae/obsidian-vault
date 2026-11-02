# bq40z50：TI 参考驱动的取舍、怎么复跑、实时看数据

> 本文是 [battery-gauge-bq40z50.md](battery-gauge-bq40z50.md) 的**操作篇**：
> 为什么不用 TI 那份参考驱动、怎么复跑一次测量、实时看数据的两条路（sysfs / 脚本），
> 以及 5io 开发板上的临时落地（**board-local，不进仓库/镜像**）。
> 硬件接线、驱动与 DT、实测数据、SBS 语义要点见主篇与
> [battery-gauge-semantics.md](battery-gauge-semantics.md)。
> ⚠️ 正文**原样搬自主篇的 §5~§7**（节号保留）。

## TL;DR

- 结论：**不用** TI 的参考驱动（`hardware-docs/modules/bq40z50_fg.rar`）—— 内核自带的 SBS 驱动够用。
- 复跑与实时看数据：`/sys/class/power_supply/…` + 一个临时脚本；5io 上的落地是 **board-local**。


## 5. TI 参考驱动（`hardware-docs/modules/bq40z50_fg.rar`）—— 结论：不用

解出来是 `bq40z50_fg.c`（1137 行）+ `Kconfig.txt`（`FUEL_GAUGE_BQ40Z50`）+ `Makefile.txt`，
**2017 年 TI 的 out-of-tree 驱动**（GPL-2，`Copyright (C) 2017 TI`）。

- 接口代次不算太老：用的是 `power_supply_desc` + `devm_power_supply_register`（≥4.14 那套），**不是** 4.4 的老接口。
- ⛔ **但它在 6.1 上原样编不过**：属性表里用了
  `POWER_SUPPLY_PROP_RESISTANCE_ID` 与 `POWER_SUPPLY_PROP_UPDATE_NOW`，而这两个
  **主线 6.1 的 `include/linux/power_supply.h` 里都没有**（已 grep 确认）⇒ 至少得删/改这两项才能编译。
- 它比主线**多**的东西其实很少：
  - 两个自定义 sysfs：`RaTable`、`Qmax`（通过 ManufacturerBlockAccess `0x44` 读出来，**量产学习/QA 校验用**）；
  - `client->irq` 的 BTP/ALERT **中断**路径（需 DT 里接中断线，本板没接，`client->irq == 0` 时会跳过）。
- 它比主线**少**的东西：只暴露 11 个属性（status/present/voltage/current/capacity/level/temp/FCC/technology/
  resistance_id/update_now），**没有** `energy_*`、`charge_now/full/design`、`time_to_*`、`manufacturer/model`
  等主线都有的东西；`POWER_SUPPLY_PROP_HEALTH` 被注释掉留给了"battery power_supply"。
- 两处"看着吓人其实没调用"的：`BQ_FG_REG_SOH`(0x4F) **只定义未使用**（死代码，所以它也**不提供 SOH**）；
  `FG_MAC_CMD_SEAL`/`FG_MAC_CMD_DEV_RESET` **只定义未调用**（没有会对电量计加封/复位的危险写操作）。
- 它的块读写走 `i2c_smbus_read_block_data()` —— 在我们这块板上由 i2c 核心**仿真**实现（同样没有 PEC）。

**建议**：自研底板继续用主线 `sbs-battery`（零维护、已在硬件上验证）。
真要给产线看 `RaTable`/`Qmax`，把那两个 sysfs（约 30 行，读 MBA `0x44`）单独移植过来即可，
不值得为此背 1137 行厂商代码。

## 6. 怎么复跑

```bash
# 板上需要 root（要开 /dev/i2c-7）；脚本会临时解绑驱动读原始寄存器，结束前绑回来
ssh <板子> 'bash -s' < .scratch-apps/battery-gauge-test.sh        # 先传进板子再 sudo 跑
```
（脚本：`.scratch-apps/battery-gauge-test.sh`；它会打印驱动视角、原始寄存器视角、逐项对照、
自洽性判据、5 次连续采样。）

## 7. 实时看数据

`tools/battery-monitor.sh`（**临时版**：零依赖、只读，只读 `/sys/class/power_supply/` 的现成属性）：

```bash
tools/battery-monitor.sh            # TUI：1 s 原地刷新，q 退出（Ctrl-C 也行）
tools/battery-monitor.sh -i 0.5 -n 20
tools/battery-monitor.sh --once     # 一份纯文本快照（ssh 一行取数 / 贴日志）
tools/battery-monitor.sh --json     # 一份 JSON（给脚本、上位机）
tools/battery-monitor.sh --node=/sys/class/power_supply/sbs-7-000b --cells=3
```

显示：SOC 进度条 + 状态、电压（端电压与单节）、电流（瞬时/平均）、功率、温度、循环、MaxError、
剩余/满充/设计电量（Ah）与能量（Wh）、健康度、剩余时间（瞬时/平均）、充满时间、充电上限，
以及身份字段（厂商/型号/序号/化学）。**两列表格、列对齐**：

```
══ 电量计 sbs-7-000b      2026-09-21 19:22:59
  SOC      [##################------]  76%  ↓ 放电中
  电压     11.750 V          单节     3.917 V ×3
  电流     -503.0 mA         平均电流 -416.0 mA
  功率     -5.91 W           温度     29.2 ℃
  循环     1                 自估误差 ±1%
  剩余电量 8.743 Ah          满充电量 11.584 Ah
  剩余能量 91.86 Wh          满充能量 126.21 Wh
  设计容量 11.600 Ah         健康度   99.9%
  剩余时间 18h27m            均值     21h01m
  充满时间 --                充电上限 12.60 V / 6.00 A
  厂商     RR                型号     XD-BQ3S12A
  序号     0001              化学     Li-ion
```

**对齐是怎么做到的（别再踩）**：`printf '%-8s'` 是按**字节**补齐的，汉字 3 字节却只占 **2 列**
⇒ 直接拿汉字标签对齐必然错位。脚本里的 `pad8()` 在**启动时**把标签补到 8 个**显示列**
（宽度 = 字符数 + (字节数−字符数)/2，纯 bash，无外部依赖），值列定宽 18 列
（**值必须是纯 ASCII**，`×`/`℃`/`±` 这类变宽字符只允许出现在每行**最后一个**字段）。
`--json` 里对应的键是 `manufacturer` / `model` / `serial` / `technology`；⚠️ `serial` 是**十六进制**，
见 [battery-gauge-semantics.md](battery-gauge-semantics.md) 的身份字段那条。非 TTY（管道/日志）时不打转义码。

**TUI 刷新怎么才不闪（2026-09-21 用户报"刷新时会闪烁"，已改）**：

* ⛔ **不能逐行 `printf`** —— 终端会一行一行地重画，看着就是闪。改成把整帧先拼进一个变量，
  每帧**只写一次**（`printf '\033[?2026h\033[H%s\033[J\033[?2026l' "$F"`）。
* TTY 下进**备用屏幕**（`\033[?1049h`）并在退出/`Ctrl-C`/`q` 时还原（`\033[?25h\033[?1049l`，
  走 `trap cleanup INT TERM`）：不污染滚动回看，也不会把上一帧留在终端里。
* `\033[?2026h/l` 是**同步刷新**（synchronized update）标记：认识的终端会"整帧画完再刷屏"，
  不认识的老实忽略 —— 所以可以无条件带。
* 清行用**行尾** `\033[K`（写在换行之前）；⛔ 写在 `\n` 之后清的是**下一行**，是错的。
* 非 TTY 一个 ESC 都不能有（判据：`battery-monitor --once | tr -cd '\033' | wc -c` → `0`）。
* 串口控制台（`ttyFIQ0` 115200）本身带宽就低，整帧 ~700 B ≈ 60 ms；嫌慢就 `-i 2`。

⏳ **以后正式做**：并进 `apps/lsp-rk-monitor`（Rust TUI）当一栏 —— 那边已经有 CPU/GPU/NPU/RGA/VPU/DDR
的采集（`apps/lsp-rk-monitor/src/collect.rs`）与绘图，电量计只是再加一个外设条目；
那时 bash 这份就降级成"没编 Rust 时的兜底"（与 `tools/lsp-rk-monitor.sh` 的角色一致）。

### 7.1 5io 开发板上的落地（2026-09-21，**board-local，不进仓库/镜像**）

给同事看数据用的两件东西，都装在 `cat@<HOST_10>` 上（旧镜像、没有 DT 节点）：

1. **脚本进了 PATH**：`/usr/local/bin/battery-monitor`（755，root:root，内容 = 仓库
   `tools/battery-monitor.sh` 逐字节相同），另外 `/home/board/battery-monitor.sh` 也留着。
   任何用户、任何目录直接敲：

   ```bash
   battery-monitor            # TUI
   battery-monitor --once     # 快照
   battery-monitor --json
   ```

   ⚠️ 帮助里的用法按**调用名**显示（`basename "$0"`），所以板上看到的是 `battery-monitor`，
   不是仓库里的 `tools/battery-monitor.sh`。
2. **开机自动绑电量计**：`/etc/systemd/system/bq40z50-bind.service`（enabled）。
   旧镜像里 i2c7 上没有 `battery@b` 节点，**运行时绑的 `new_device` 一重启就没了**；
   正式做法是把 `overlay/boot/firmware/dtbs/rockchip/overlay/rk3588-lubancat-5io-bq40z50-overlay.dtbo`
   放进启动分区（见 `overlay/.../overlay/README.md`），但那是**我们镜像**的路径，
   这块旧板子走的是这个单元。内容（幂等 + 冷启动重试，描述全 ASCII）：

   ```ini
   [Unit]
   Description=Bind bq40z50 fuel gauge on i2c7 (board-local workaround; formal fix is the device-tree overlay)
   After=sysinit.target
   ConditionPathExists=/sys/bus/i2c/devices

   [Service]
   Type=oneshot
   RemainAfterExit=yes
   TimeoutStartSec=60
   ExecStart=/bin/sh -c 'for i in $(seq 1 30); do [ -e /sys/bus/i2c/devices/i2c-7 ] && break; sleep 1; done; \
     if [ -e /sys/bus/i2c/devices/7-000b ]; then echo "already bound"; exit 0; fi; \
     echo sbs-battery 0x0b > /sys/bus/i2c/devices/i2c-7/new_device && echo "bound sbs-battery at 0x0b on i2c-7"'

   [Install]
   WantedBy=multi-user.target
   ```

   实测：`enable --now` 后 `already bound`（因为节点已在）；`delete_device` 解绑后
   `systemctl restart` → 节点自己回 `7-000b`；再 restart → `already bound`（幂等）。
   ⏳ **真重启没试**（不想打断演示），但开机路径与 `restart` 走的是同一条 ExecStart。
3. 演示期间不会被"空闲 15 分钟自动挂起"打断 —— 那块板子的
   "永不休眠"策略已装好（见 [../system/tuning/no-idle-suspend.md](../system/tuning/no-idle-suspend.md)）。
