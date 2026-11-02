# 相机"节点还在、但 ROS 里帧率 0，只有拔插才恢复"（UVC 流半死）

> 用户 2026-09-26 描述："有时候相机会挂掉，节点还在，但是 ros2 里一直读不到数据，帧率 0，
> 但是拔下来插到电脑上是正常的，这时候再插回去数据帧率又都正常了。"
>
> 本文是**代码 + 板级取证**的排查记录（只读，没改厂商代码、没改内核）。

## TL;DR

- 现象：相机**节点还在、ROS 里帧率 0**，只有拔插才恢复。
- 结论：坏在**主机侧**（uvcvideo / USB 控制器 / hub / 供电），不是相机本体（§1）。
- 三个可疑成因按可能性排序，而且**用日志就能区分**（§3）。
- ⛔ **下次发生时先别拔** —— 按 §4 抓那几条证据（拔掉就没了）。
- "软件重新枚举"能不能替代拔插：已实测（§5）；可落地的修法两条见 §6（要先定再动）。

## 1. 结论：坏在**主机侧**（内核 uvcvideo / USB 控制器 / hub / 供电），不是相机

两条判据把锅分得很清楚：

**① 相机节点自己会重试，而且重试的是"重开设备"，不是"重启进程"**

`triple_camera_mjpeg_publisher/mjpeg_capture.py` 的采集线程（GStreamer `v4l2src ! image/jpeg ! appsink`）：

```python
sample = self._sink.emit('try-pull-sample', 500 * Gst.MSECOND)
if sample is None:
    self._check_bus()
    if time.time() - self._last_publish_time > 3.0:
        self._warn_throttled(f'[{self.config.name}] 连续 3 秒未收到 JPEG 帧，准备重连')
        self._close_pipeline()          # pipeline -> NULL，释放 /dev/videoN
    continue
```

下轮循环进 `_ensure_pipeline()` 重新 `Gst.parse_launch(...)` + `set_state(PLAYING)`，
即**每 ~3 s 就会 STREAMOFF/关设备再重开一次**；`GStreamer ERROR/EOS` 也会触发 `_close_pipeline()`。
⇒ 如果"重开设备"能救回来，节点早就自愈了。**它救不回来，说明坏的状态不在应用层**。

**② 拔插 = USB **重新枚举**（新地址/新 endpoint/重投 URB、重新分配 isoc 带宽、重新上电），
而"重开 /dev/videoN"只是 **STREAMOFF→STREAMON**，清不掉主机侧那个半死状态**

⇒ 现场只有拔插能好，正是这条的必然结果。同样的道理：设备在 PC 上正常，也不代表板子这一侧没问题
（PC 的端口供电、控制器、驱动版本都不一样）。

## 2. 板级取证（工程机 `<HOST_09>`，2026-09-26 14:2x）

```
$ lsusb -t
/:  Bus 003 ... xhci-hcd/1p, 480M      |__ Port 001: Dev 002, Class=Video, Driver=uvcvideo, 480M   ← 相机 A 独占 xHCI
/:  Bus 004 ... ehci-platform/1p, 480M |__ Port 001: Dev 002, Class=Video, Driver=uvcvideo, 480M   ← 相机 B 独占 EHCI
/:  Bus 006 ... ehci-platform/1p, 480M
    |__ Port 001: Dev 002, Class=Hub, 480M            ← CH334F hub（1a86:8091）
        |__ Port 001: Dev 003, Wireless, btusb, 12M
        |__ Port 003: Dev 004, Class=Video, Driver=uvcvideo, 480M    ← 相机 C 与 btusb/HID/CDC 共用同一条 EHCI
        |__ Port 004: Dev 005, Class=Hub, 480M         ← 05e3:0610（再套一层）
            |__ Port 001: Dev 006, cdc_acm, 12M        ← 手套/LCD
            |__ Port 003: Dev 008, cdc_acm, 12M        ← 手套/LCD
            |__ Port 002: Dev 007, Class=Hub → HID x2

# 三台相机是同型号同参数
3-1 / 4-1 / 6-1.3 :  0c45:636b  "USB 2.0 Camera"  speed=480  bMaxPower=500mA  bmAttributes=0x80（总线供电、不支持远程唤醒）

# uvcvideo 是内建（=y），可调参数：
quirks=4294967295（全开）  timeout=5000  nodrop=0  hwtimestamps=0  clock=CLOCK_MONOTONIC
# 防自动挂起已经在跑，且所有设备都是禁止挂起状态：
$ systemctl is-active lsp-usb-no-autosuspend   → active
$ cat /sys/bus/usb/devices/6-1/power/control   → on     （runtime_status=active）
```

## 3. 三个可疑成因（按可能性排序，都能用日志区分）

| # | 成因 | 机制 | 内核日志特征 |
| --- | --- | --- | --- |
| 1 | **uvcvideo 放弃重投 URB**（最典型） | 一次传输错误后 `uvc_queue` 不再重投，`STREAMON` 仍返回成功但**永远没有帧** | `uvcvideo: Non-zero status (-71 / -110) in video completion handler` + `Failed to resubmit video URB` |
| 2 | **EHCI 周期表/带宽被挤** | 相机 C 与 btusb + 2×CDC + 2×HID 共用同一条 EHCI（还套了 3 层 hub）。EHCI 的 isoc 调度比 xHCI 脆，丢一个包/带宽峰值就可能让主机停投 | `usb 6-1.3: Not enough bandwidth for new device state` / `ehci-pci/ehci-platform ... periodic ...` / `xhci: ERROR Transfer event` |
| 3 | **供电跌落**（三台都是 500 mA 总线供电） | 局部电压跌落→相机固件卡在"停止流"，软件 STREAMOFF 无效，断电才恢复 | `over-current change` / `device descriptor read/64, error -110` / 重新枚举记录 |

⚠️ 不是 autosuspend（`control=on` + `lsp-usb-no-autosuspend` 在跑）；也不是相机/线（同型号、PC 上正常）。

## 4. 现场取证（下一次发生**别拔**，先抓这几条）

```bash
# ① 内核：本次 + 上一次开机都要看（journalctl -k 默认只看本次）
journalctl -k | grep -Ei 'uvcvideo|Non-zero status|resubmit|Not enough bandwidth|error -7[01]|error -110|over-current'
journalctl -k -b -1 | grep -Ei 'uvcvideo|resubmit|error -7[01]|error -110|over-current'
# ② 设备还在不在、能不能自己吐帧
v4l2-ctl --list-devices
v4l2-ctl -d /dev/chest_cam_video --stream-mmap --stream-count=10 --stream-timeout=2000   # 0 帧 = 主机侧卡死
ls -l /dev/camera_left /dev/camera_right /dev/chest_cam_video
# ③ 相机节点日志：这三行就能把锅分清
grep -a '连续 3 秒未收到 JPEG 帧\|已打开 MJPEG 视频源\|GStreamer ERROR\|GStreamer WARNING' \
     ~/.lsp/log/camera/$(ls -t ~/.lsp/log/camera | head -1)
#   反复出现"准备重连 + 已打开"却始终没帧  ⇒ 内核/主机侧卡死（重开设备没用）
#   只有"准备重连"没有"已打开"          ⇒ 打开设备就失败（带宽/设备状态/权限）
# ④ 运行时电源/挂起状态
cat /sys/bus/usb/devices/<dev>/power/{control,runtime_status}
```

## 5. 先验证一件事：**软件重新枚举**能不能替代拔插

### 5.1 工具：`lsp-usb reenumerate`（2026-09-26 已加到 `overlay/usr/local/bin/lsp-usb`）

```bash
sudo lsp-usb reenumerate                    # 只列可重新枚举的设备（相机/串口 + 各自的 /dev 链接）
sudo lsp-usb reenumerate /dev/camera_left   # 也接受 sysfs 名（4-1）或 VID:PID（0c45:636b）
```

底层就是 `echo 0 > /sys/bus/usb/devices/<dev>/authorized; sleep 1; echo 1 > …`，
工具会打印**前后对照**（video/tty 节点 + `/dev` 符号链接 + devnum）。
不带参数只列设备、不做动作（动作必须显式点名设备）。

`lsp-usb reenumerate` 在工程机上的设备解析结果（2026-09-26 14:49）：

```
3-1      0c45:636b  USB 2.0 Camera   link: /dev/camera_right -> video0
4-1      0c45:636b  USB 2.0 Camera   link: /dev/camera_left  -> video2
6-1.3    0c45:636b  USB 2.0 Camera   link: /dev/chest_cam_video -> video4
6-1.4.1  303a:1001  USB JTAG/serial  link: /dev/lcd_voice_bridge -> ttyACM0
6-1.4.3  303a:1001  USB JTAG/serial  link: /dev/glove_right -> ttyACM1
6-1.4.4  303a:1001  USB JTAG/serial  link: /dev/glove_left  -> ttyACM2
```

### 5.2 实测（2026-09-26 14:50，工程机；**单独跑相机节点**，没启动 collect_system，
所以整个过程不碰 LCD）

基线：三路都稳在 `chest 29.6 / wrist_left 30.0 / wrist_right 30.0` fps。然后
`sudo lsp-usb reenumerate /dev/camera_left`：

```
[lsp-usb] 设备 4-1：0c45:636b  USB 2.0 Camera  速度=480
  重新枚举前： v4l: /dev/video2, /dev/video3    link: /dev/camera_left -> video2
[lsp-usb] authorized 0（逻辑拔出）→ 1（重新枚举）…
[lsp-usb] 已回来：devnum=2 runtime=active
  重新枚举后： v4l: /dev/video3, /dev/video6    link: /dev/camera_left -> video3

# dmesg：
[14:50:46] usb 4-1: Found UVC 1.00 device USB 2.0 Camera (0c45:636b)
[14:50:46] usb 4-1: authorized to connect

# 相机节点日志（关键：它自己发现的，没人重启它）：
14:50:43.203 [ERROR] [wrist_left_0] GStreamer ERROR: gst-resource-error-quark: Could not read from resource (9);
             debug=… gst_v4l2_object_poll(): poll error 1: Invalid argument (22)
14:50:45.205 [INFO]  [wrist_left_0] 已打开 MJPEG 视频源: v4l2src device=/dev/camera_left …   ← 2 s 后自己重开
14:50:46.780 [INFO]  [wrist_left_0] fps=15.80        ← 恢复中
14:50:51.780 [INFO]  [wrist_left_0] fps=30.00        ← 完全恢复
# 同期另两路全程未受影响：
14:50:46.779 [chest_0] fps=29.40   14:50:51.779 [chest_0] fps=29.60
14:50:46.780 [wrist_right_0] fps=30.00   14:50:51.780 [wrist_right_0] fps=30.20
```

**结论**：

1. `authorized 0/1` 就是**逻辑拔插**：videoN 换号（video2→video3）、udev 符号链接自动跟上
   （`/dev/camera_left -> video3`），设备重新枚举（`Found UVC …`）。
2. **应用层能自己恢复**：节点 2 s 后重开设备、约 6 s 后回到 30 fps，**不需要重启进程**，
   另两路相机完全不受影响 ⇒ 这条路可以作为"卡死相机"的远程/现场恢复手段。
3. ⚠️ 但**断流实测约 3.2 s**（43.2 起断、46.4 左右恢复出帧），正好压在触发节点
   `health_topic_timeout_sec=3.0` 的门限附近 ⇒ **整机栈在跑时自动做这个动作，可能会顺带把
   采集流程判成 ERROR（进而在面板上锁存相机位）**。要做成自动恢复，得先把门限/机制对齐
   （见 §6）。
4. 不能恢复的那种（只有物理拔插才行）⇒ 需要断 VBUS，属相机固件卡死或 §3-3 供电问题。

## 6. 可落地的修法（两条，都要决定后再动）

1. **厂商侧（推荐）**：`mjpeg_capture.run()` 在"连续 N 次重开仍 0 帧"之后，
   不要只 `_close_pipeline()`，而是对这条 USB 路径做**软件重新枚举**（`authorized 0/1`
   或 `USBDEVFS_RESET` ioctl），再把 `CameraStatus.reason` 标成可诊断的字符串
   （例如 `UVCS_STALL`）—— 现在只会报 `ok=False/fps=0`，现场看不出是"卡死"还是"真掉线"。
2. **我方系统侧（快，但不治本）**：给 `lsp-usb` 加 `reenumerate <camera|dev>` 子命令
   （就是 §5 那两行），维护模式白名单放行；如要做成自动的，检测面用"`CameraStatus.fps==0` 且
   节点进程还在"持续 N 秒，比看话题更准。
   ⚠️ 自动重新枚举会把厂商节点的 pipeline 打断，靠它自己的 3 s 重连恢复 —— 这条要先和厂商确认。

## 7. 还没做/未验证

- [x] §5.2 软件重新枚举**已在工程机实测**：相机节点自己 2 s 后重开、6 s 后回到 30 fps，
      另两路不受影响；`lsp-usb reenumerate` 已加进 `overlay/usr/local/bin/lsp-usb`。
- [ ] **真故障（卡死态）下的恢复**没验证过 —— 工程机 13 小时日志里这个故障一次都没出现
      （`grep -c '连续 3 秒未收到 JPEG 帧'` = 0，也没有 `GStreamer ERROR`）。
      也就是说 §5.2 证明的是"这套动作本身有效且安全"，不是"它能救活卡死的相机"；
      那个要等现场/开发板复现时按 §4 取证、再按 §5 试。
- [ ] 相机 C 换到独立 xHCI 口（v2 板 USB3 口）做 A/B，看是否还犯 —— 用来区分 §3-1 与 §3-2。
- [ ] 三台相机 500 mA 总线供电的余量：整机上还有 Quest(USB)、2×CDC、btusb，需不需要外供电 hub。
- [ ] 要不要把 `lsp-usb reenumerate` 加进 kiosk / 维护模式的 sudo 白名单（要用户定），
      以及要不要在采集侧做"检测到 fps=0 且进程还在 → 自动重新枚举"（要厂商一起定，见 §6）。
