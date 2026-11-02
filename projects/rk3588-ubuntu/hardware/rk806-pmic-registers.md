# RK806 寄存器级操作：长按断电定时器 / `DEV_OFF` / 实测快照

> 本文回答**"想改变 RK806 的行为时，能动哪些寄存器、动了会怎样"**：
> 硬件长按断电定时器（`0x76` 的 `PWRON_LP_OFF_TIME`）、用户态可用的"立即断电"入口 `DEV_OFF`（`0x72` bit0）、
> 哪些位**绝对不能碰**、寄存器"读回值"为什么只能当参考，以及板上实测的寄存器快照。
> PWRON 引脚与"按下键用户态收到什么"见 [rk806-pwron.md](rk806-pwron.md) §1~§3；
> 落地成 app 的部分见 [lsp-powerkey-implementation.md](lsp-powerkey-implementation.md)。
> ⚠️ 正文**原样搬自该篇的 §4 / §6 / §7**（节号保留在标题里便于对照）。

## TL;DR

- 硬件长按断电的时长**只**由 `PWRON_LP_OFF_TIME`（`0x76` bit5:4）决定（4 档）。
- ⛔ **`0x76` 的 bit7 绝对不能动**（会毁掉"按键开机"）；`0x76` 的写入**跨软关机保留**。
- `DEV_OFF`（`0x72` bit0）是**用户态可用的"立即断电"入口**；这条路上 **Linux 侧完全没参与**。
- `r` 走 regmap：当前 BSP 的 `0x70–0x7a`（含 `0x76`）为 volatile，读取访问硬件；其他地址可能命中缓存。
  debug 节点忽略总线错误，命令成功或 dmesg 打印本身不能证明读写成功，见 §4.4。


## 4. 硬件长按断电：实测行为 + 怎么放宽（本工程的关键一手数据）

RK806 内部有一个"按键持续 N 秒 → **直接切断所有电源轨**"的定时器，
不通知 Linux、不产生任何事件。**这一段全是 2026-09-14 在开发板上一档一档试出来的**，
网上流传的说法和内核头文件里的字段名都不足以推断，照抄会翻车。

### 4.1 硬件长按定时器**只**受 `PWRON_LP_OFF_TIME`（0x76 bit5:4）影响

| 写入 `0x76` | bit7 (ON_TIME) | bit6 (LP_ACT) | bit5:4 (LP_OFF_TIME) | 实测断电时刻 |
| --- | --- | --- | --- | --- |
| `0x86` | 1 | 0 | `00` | **6 秒**（OTP 原值） |
| `0x96` | 1 | 0 | `01` | **~8 秒**（撑过了 6 秒，7 秒那级有日志） |
| `0xB6` | 1 | 0 | `11` | **~12 秒**（12 秒内的三级全部跑完） |
| `0x06` | **0** | 0 | `00` | 6 秒 + **关机后按键开不了机**（见 4.3） |

⚠️ **注意 OTP 原值 `0x86` 里 bit6 本来就是 0**，也就是说"`PWRON_LP_ACT` 管着这个定时器"
这个从字段名来的推断**不成立**：bit6 一直是 0，定时器照样在 6 秒工作。
能改它的**只有 bit5:4**，范围 6/8/10/12 秒，**12 秒是上限**。

⇒ 结论：**没有任何寄存器设置能"关掉"这个定时器**，软件只能在 12 秒内抢时间。
本工程正是这么做的：把长按处置交给用户态，在 **10 秒**时主动写 `DEV_OFF`
（见 [lsp-powerkey-implementation.md](lsp-powerkey-implementation.md)），把 PMIC 的 12 秒留作"内核/用户态都死了"的最后兜底。

### 4.2 `0x76` 的写入会**跨软关机保留**（不会随 OTP 重载）

实测把 `0x76` 写成 `0x96` 之后，正常关机→再开机，`0x76` 读回仍是 `0x96`；
只有**拔插电源线（VBAT 真掉）**才会重载 OTP 回到 `0x86`。
原因：PMIC 的"关机"并没有切断自己的 VBAT 供电。
⇒ 所以每次开机都要重设一遍（本工程由 `lsp-powerkey --setup` 在服务启动时做）。

### 4.3 ⛔ 绝对不能动 `0x76` 的 bit7（会毁掉"按键开机"）

`0x06`（把 bit7 清 0）那次实测的后果：**系统关机后按 PWR 完全没反应，
必须拔掉电源线重插才能开机**（拔插让 VBAT 掉电 → PMIC 重载 OTP → 恢复）。
bit7 是开机判定时长（0=500ms / 1=20ms），被清掉后按键开机那套判定就废了。

`0x96` / `0xB6`（bit7 保持 1）之后，按键开机都正常——**只改 bit5:4 是安全的**。
所以本工程写的是整字节 `0xB6`（不是读-改-写：debug 节点只写不可读，见 4.4），
并且在代码和注释里都写死了这条禁令。

### 4.4 区分 volatile 读取与 regmap 缓存

当前 6.1.172 的 rk806-core.c（镜像源码：`kernel-6.1/drivers/mfd/rk806-core.c`） 使用
`REGCACHE_RBTREE`，同时把 `RK806_POWER_EN0–5` 和 `0x70–0x7a` 列为 volatile。
因此 `r 76` 经 `regmap_read()` 实际访问硬件，不能称为缓存读；例如休眠使能寄存器 `0x06`
则可能命中缓存，需要另行核对。2026-10-21 的 regmap trace 已观测到成功的 `0x76` 硬件读写。

`rk806_master_store()` 没有检查 `regmap_read()` / `regmap_write()` 返回值，
即使命令返回成功并打印数值，也不能排除 SPI 失败。核验总线操作可限定采集
`regmap_reg_read` / `regmap_reg_write` 成功事件；功能判据仍需看实际按键或断电行为。
读取只针对已确认安全的地址，不遍历可能清除中断状态的寄存器。

### 4.5 `DEV_OFF`（0x72 bit0）= 用户态可用的"立即断电"入口

实测：`echo "w 72 01" > /sys/rk806single/debug` → 板子**立即断电**，
连写日志都来不及（那条日志只写出 `写 D` 就被切了）。
这是"强制关机"最可靠的软件手段：**不依赖 systemd**，
内核还活着（哪怕用户态卡死）就能生效。
本工程用的就是它，且把日志顺序改成"先落盘日志、再触发断电"，
让那条关键记录不会再被截断。

### 4.6 Linux 侧完全没参与

`RK806_IRQ_PWRON_LP`（INT_STS0 bit3）虽然定义了，但 `rk805-pwrkey.c`
只 request 了 fall / rise 两个中断，`PWRON_LP` 从来没人接；
内核侧除 `rk806-core.c` 里给 `PWRON_ON_TIME` 写那一位之外，
**从不读写 0x76 的其它位**。

### 4.7 硬断电 vs 软关机的判据

* **硬断电**：`journalctl -b -1` 里日志**突然截断**，没有 `systemd-shutdown`
  （实测多见最后一条是普通用户态记录）。
* **软关机**：结尾一定有
  `systemd-shutdown[1]: Syncing filesystems and block devices.`
  → `Sending SIGTERM to remaining processes...` → `Journal stopped`。


## 6. 实测寄存器快照（板子 `lubancat-5-v2`）

读法（节点是 **write-only**，结果从 dmesg 读；总线错误与缓存边界见 §4.4）：

```bash
echo "r 76" | sudo tee /sys/rk806single/debug
sudo dmesg | tail -4
```

**OTP 原始值（冷启动、本工程未配置前）**：

| 寄存器 | 值 | 含义 |
| --- | --- | --- |
| `0x76` | `0x86` | ON_TIME=**1**(bit7，开机判定 20ms) / LP_ACT=**0** / LP_OFF_TIME=**00**(6s) / LP_TM_SEL=01 / DB_SEL=10 |
| `0x5D` | `0x80` | `PWRON_STS=1`（按 PWRON 开的机）/ `VB_LO_STS=1` |
| `0x72` | `0x42` | `RST_FUN=1`（与 DT `pmic-reset-func` 一致），`SLAVE_RESTART_FUN=1`，`DEV_OFF/DEV_RST=0` |
| `0x74` | `0x40` / `0x80` | 上电触发源记录（按键 = `0x40`，插电/VDC = `0x80`） |
| `0x75` | `0x00` / `0x04` | 掉电触发源记录 |
| `0x62` | `0x88` | `PWRCTRL2_POL=1`（PWRCTRL2 高有效，本板未用于掉电） |
| `0x63` | `0x08` | PWRCTRL3 配置 |
| `0x71` | `0x00` | PWRCTRL1/2 都是 NULL 功能（掉电状态只在关机那一刻切） |
| `0x73` | `0x00` | `WDT_EN=0` |

⚠️ 上电两行要注意：**`0x74` 的 `0x40` 表示"按键开机"，`0x80` 表示"插电开机"**。
排查"按键开不了机"时先看这一位。

**本工程运行时**：`0x76` 被 `lsp-powerkey --setup` 写成 `0xB6`（LP_OFF_TIME=11=12s）。

芯片识别：`rk806 spi2.0: chip id: RK806,ver:0x2, 0x1`。
`/sys/kernel/debug/regmap/spi2.0/registers` 只有一行（regmap 没设 `max_register`，不可用）。

## 7. 想改行为时，能动的地方

| 想干什么 | 动哪里 | 能不能持久 |
| --- | --- | --- |
| 改开机按住时长（20ms/500ms） | DT 加 `pwron-on-time-500ms` | 持久（进 DT） |
| **放宽**硬件长按断电（6/8/10/12 秒） | 写 `0x76` 的 **bit5:4**（`00/01/10/11`） | 易失：**拔插电源就回 OTP**，每次开机都要重设 |
| **关掉**硬件长按断电 | ⛔ **做不到**（见 §4.1：bit6 是 0，定时器照样工作） | — |
| ⛔ 清 `0x76` 的 bit7 | **绝对不要**：会让关机后按键开不了机（§4.3） | — |
| 立即断电（用户态） | `echo "w 72 01" > /sys/rk806single/debug`（`DEV_OFF` bit0） | 立即生效（§4.5） |
| "按住 N 秒触发正常关机/强制断电" | **必须写用户态守护进程**读 evdev 自己计时 | 见 [lsp-powerkey-implementation.md](lsp-powerkey-implementation.md) |

⚠️ **网上流传的 `pwron-lp-off-time = <12000>` DT 属性在本内核里不存在**：

```
$ rg -n "pwron-lp-off-time" kernel-6.1/
(空)
```

`rk806_core` 的 DT 解析里只有 `pwron-on-time-500ms`、`vdc-wakeup-enable`、
`pmic-reset-func`、`shutdown-sequence` 等（`rk806-core.c:743-830`）。别照抄那些教程
（那些文章里的"驱动实现解析"整段都是编的）。
