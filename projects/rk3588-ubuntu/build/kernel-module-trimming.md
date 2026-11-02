# 内核模块裁剪（**已立单项，尚未动手**）

> 起因（2026-09-26 用户原话）："现在内核编译会把很多用不到的驱动编译成模块，在我本机编译得半个小时。"
>
> 本文件只记录**已核实的数据 + 拟采用的方案 + 验收判据**，方便以后单独立项时直接开工。
> 相关：构建耗时那一段已经先修掉了（见 §1），本文的重点是**减少要编/要装的东西**。

## TL;DR

- 状态：**已立单项，尚未动手**。先修掉的是"每次都全量重编"（已做，§1）。
- 现状量化（服务器 `-k` 产物）见 §2；候选裁剪清单**都要逐类确认**（不是"看着没用就删"，§3）。
- 动手前先做 §4 的几条确认；⛔ **不要直接改内核源码里的 defconfig**（§5 说明拟采用的做法）。

## 1. 先修掉的是"每次都全量重编"（已做）

`scripts/build-kernel.sh` 原来是 `rm -rf linux-rockchip && cp -r ../kernel-6.1 linux-rockchip`
⇒ 每次都把编译产物删光，**改一行也要全量重编**（1995 个 `.ko` + 2360 个 `=y`）。
改成 `rsync -a`（保 mtime、不加 `--delete`）后，24 核服务器实测：

| 场景 | 改前 | 改后 |
| --- | --- | --- |
| 无改动重跑 `-k` | 3.7 分钟（全量） | **41 秒** |
| 改一个 `.c` 后重跑 `-k` | 3.7 分钟（全量） | **32 秒**（只编那一个模块 + 重链 vmlinux + 打包） |
| 单模块增量编译 | — | **0.95 秒** |

⇒ "本机 30 分钟"的**主因是这个**，不是模块多。裁剪仍然值得做（体积/全量构建/开机探测），但优先级低于这条。

## 2. 现状量化（2026-09-26，服务器上 `-k` 产物）

- `.config`：**`=m` 2002 个、`=y` 2360 个**（2026-10-03 在开发板的 `/boot/config-6.1.172` 复查：
  `=m 2002 / =y **2361**` —— 差 1，属统计口径/小版本微差，量级不变）
- `build/linux-image-6.1.172_*.deb` 里：**1995 个 `.ko`，未压缩共 108.6 MB**
  （2026-10-03 板上 `find /lib/modules/6.1.172 -name '*.ko*' | wc -l` 复核 = **1995** ✓）
- 按目录（前 18，括号内为模块数）：

```
163 netfilter        145 hwmon             111 net/wireless
 92 media/usb         88 media/dvb-frontends 84 input/touchscreen
 62 gpu/drm           55 ipv4                55 input/sensors
 54 iio/accel         51 usb/serial          47 sched
 36 usb/gadget        36 ipv6                34 md
 32 staging/fbtft     31 input/keyboard      30 net/usb
```

**旁证（2026-09-26 装内核时看到）**：`dpkg -i linux-image` 触发 `update-initramfs` 时刷出
一串 `Possible missing firmware /lib/firmware/dvb-fe-xc*.fw for built-in driver xc2028/xc4000/xc5000`
—— 这些 DVB 前端是 **`=y`（内建）**，所以裁 `=m` 模块并不能消掉这些告警；要连 `=y` 的
无关 DVB 一起裁才行（风险更高，单独评估）。真正要确认"内建里哪些与两块板无关"，
可以对着 §4 的白名单逐条 `grep CONFIG_ ... .config` 反查。

## 3. 候选裁剪清单（**都要逐类确认**，不是"看着没用就删"）

| 类别 | 数量 | 判断依据 / 风险 |
| --- | --- | --- |
| `hwmon` | ~145 | 各种电压/温度传感器芯片；我们只需要 RK3588 自带的 tsadc（通常 `=y`）。✅ 最安全的一批 |
| `netfilter` + ipv4/ipv6 netfilter | ~250 | 我们不做路由/NAT/防火墙（没有 docker）。⚠️ 若将来用 `iptables/nft` 或容器要留一部分 |
| `media/*`（DVB tuner / frontend / 其它 USB 摄像头芯片） | ~250 | ⛔ **必须保住 UVC 相关**：`uvcvideo`、`videobuf2-*`、`v4l2-*`、`videodev`（三台相机就是 UVC）；`media/usb` 这 92 个里混着它们，别一刀切 |
| `input/*`（触摸屏/传感器/摇杆/异形键盘） | ~200 | 保留 USB HID（`usbhid`）与 `rockchip_pwm_remotectl`（IR 遥控，虽然现在没在用） |
| `iio/*` | ~100 | 本板没有 I2C/SPI 传感器；⚠️ 但 `st_accel` 之类若被别的驱动依赖要小心 |
| `net/wireless` | 111 | 只留我们板子实际用的那颗（见 §4 待确认）+ `cfg80211`/`mac80211` 依赖 |
| `staging/fbtft` | 32 | SPI 小屏，与本板无关 |
| `usb/serial` | 51 | FTDI/CP210x/CH34x/PL2303… 我们用的是 **CDC-ACM**（`usb/class`，不是这一类）；⚠️ 调试用的 USB 串口线会被砍掉 |
| `net/can`、`md`、`sched`、`ipv6` | ~150 | CAN 我们不用；`md`（软 RAID）不用；`sched`/`ipv6` 里有一部分是**必须**的，只能挑 |

**粗估：1995 → 800~1000 个模块**，deb 里模块体积大概能砍掉一半。

## 4. 动手前的待确认（先做这几条，再改配置）

1. **两块板子（`lubancat-5-v2` / `5io`）实际用哪些驱动**：WiFi/BT 芯片型号（Mini-PCIe 还是 SDIO、Realtek 哪一款）、
   以太网 PHY、SATA/PCIe、USB2 hub、DSI/eDP 面板、ES8388 音频 —— 逐条对着 `lsmod`/`lspci`/`dmesg` 记下来，
   作为"必须保留"的白名单。
2. **构建时间归因**：清一次构建树分别编"全量 / 裁掉 hwmon / 裁掉 media"几版，量每一类的耗时与 `.ko` 体积
   （判据同 `notes/build/apps.md` §8 的口径：同一台机器、同一 `-j`、至少两次取中位数）。
3. **验收**：两块板子各跑一次
   `tools/verify-image.sh` + 现场功能回归（相机 3 路 / 手套 2 只 / Quest / LCD 语音桥 / HDMI / DP / 以太网 /
   WiFi / 蓝牙 / SATA / PCIe / `lsp-usb` 全子命令 / kiosk / 采集栈 `collect_system`）。
   ⛔ 少一项就不算过。

## 5. 拟采用的做法（不要直接改内核源码里的 defconfig）

- 把裁剪写成**独立配置片段**：`config/kernel-trim.config`（`CONFIG_X=n` 一行一条），
  在 `scripts/build-kernel.sh` 里 `make rockchip_linux_defconfig` **之后**合并进去
  （`merge_config.sh` 或 `scripts/config --file .config -d ...`），再 `make olddefconfig`。
  理由：① `rockchip_linux_defconfig` 是内核源码（BSP）里 git 跟踪的文件，改了将来 rebase 上游会冲突；
  ② 片段自带注释，一处开关、随时整批回退；③ 便于把"每个类别为什么要裁"写清楚。
- 先只裁 §3 里**最安全的一类（`hwmon`）**跑一遍全流程（编 → 装 → 两块板回归），确认机制与收益，
  再按类别逐批加。
- 记录口径：每批裁剪都要在本文补一行"裁了什么 / 少了多少个 .ko / deb 小了多少 / 全量构建快了多少 / 回归结果"。
