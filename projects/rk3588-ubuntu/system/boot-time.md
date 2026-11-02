# 当前启动时间与测量口径

> 2026-10-10 开发板实测：热重启后，后端约 **8.9 s** 完成启动，浏览器约 **12.5–13.0 s** 收到上位机主页响应；一次 Chromium trace 的主页首次内容绘制约 **12.9 s**。
> 这些是复位到事件的时间；没有测物理冷上电或 HDMI 光学首帧。

2026-10-16 的极简引导页、Chromium 冷缓存测量及 STMMAC/RTNL 等待定位见
[Chromium 启动评估](chromium-cold-start.md)。其中驱动补丁通过临时模块验证，尚未安装正式内核包，
不能把它与本页旧基线直接拼成新的复位到显示时间。

## TL;DR

- eMMC、HDMI 已连接、Cage + Chromium + 本机上位机；三次热重启，实际板上软件，未重编/刷镜像。
- `systemd-analyze` 合计中位数 **8.754 s**，加上完整复位前缀后约 **10.4 s**；它不是页面显示时间。
- 后端启动和 Chromium 并行；后端就绪后，浏览器还需约 **3.8–4.1 s** 才收到主页响应。
- `U-Boot Total`、Cage 进程开始、HTTP 200 和浏览器 FCP 是不同指标，不能互相替代。
- 无 HDMI 时，显示管理服务保持 Cage、Chromium、上位机及其 socket 停止；本次数据只适用于有屏场景。

## 1. 时间基准

用开发机串口 `/dev/ttyUSB0`、1500000 8N1，从 `reboot: Restarting system` 的接收时间起算。
串口行时间戳来自主机 `CLOCK_MONOTONIC`，包含 UART/USB/调度延迟。

启动后向板侧 `/dev/ttyFIQ0` 写入五个 `time.monotonic_ns()` 标记，把板侧 `CLOCK_MONOTONIC`
与主机接收时间对齐。三个样本的锚点偏移极差为 0.15 / 0.14 / 0.28 ms；这是重复性，
不是绝对精度。第二次因 SSH 恢复延迟，锚点在启动约 243 s 后补采；其换算也包含期间时钟速率调整的影响。

```text
板侧 mono=0 的主机时刻 ≈ median(标记主机接收 ns − 标记板侧 mono ns)
复位→事件 ≈ 事件板侧 mono + (板侧 mono=0 的主机时刻 − 复位行主机接收时刻)
```

关键区别：

- 本 BSP `CONFIG_PRINTK_TIME_FROM_ARM_ARCH_TIMER=y`，printk 使用 ARM arch counter。
  不应把 printk 方括号、journal 接收时间 `__MONOTONIC_TIMESTAMP`、内核来源时间
  `_SOURCE_MONOTONIC_TIMESTAMP` 混为一套时间。
- `fake-hwclock` 修改墙钟，不会令 `CLOCK_MONOTONIC` 跳变；NTP 校频可以影响其速率。
- `critical-chain` 的 `@` 相对 PID1 启动；`systemctl show` 的 `*TimestampMonotonic` 相对板侧 mono=0。
- `Run /init as init process` 是开始执行 initramfs 用户态，不是它执行完毕；前面的区间不能记成 initramfs 用户态耗时。
- `systemd-analyze` 在本板不含 firmware/loader。`U-Boot Total` 从 `gd->sys_start_tick` 起算，
  也不覆盖完整复位前缀。历史上 `Total=3.979580 s`、串口复位到 `Starting kernel=4.385 s` 已证明两者不同。

## 2. 三次实际值

环境：Ubuntu 24.04.4、Rockchip BSP 6.1.172、Chromium 151.0.7922.169；
根分区 `/dev/mmcblk0p2`，SPL/U-Boot banner 为 2017.09、构建于 2026-10-03。
镜像之外已部署 HDMI 启停配置，不能把本次结果当作仓库新镜像的验收。

| 事件 / 秒 | 第 1 次 | 第 2 次 | 第 3 次 |
| --- | ---: | ---: | ---: |
| 复位 → `Starting kernel` | 1.018 | 1.064 | 1.064 |
| U-Boot 自报 `Total`，起点不同 | 0.640 | 0.658 | 0.659 |
| 复位 → 板侧 mono=0（锚点换算） | 1.583 | 1.865 | 1.624 |
| `systemd-analyze` kernel + userspace | 8.754 | 8.564 | 8.763 |
| 复位 → systemd 启动完成（换算） | 10.337 | 10.429 | 10.387 |
| 复位 → Cage 主进程开始（换算） | 7.166 | 7.198 | 7.048 |
| 复位 → 后端 `Uvicorn running`（换算） | 8.905 | 8.909 | 8.600 |
| 复位 → 浏览器主页 GET 200（换算） | 13.019 | 12.719 | 12.500 |

第 1 / 3 次只临时启用后端访问日志；第 2 次还启用 Chromium startup trace，测量开销未做 A/B 定量。
后端事件及 GET 使用 journal 接收时间，可能晚于实际发出时间。GET 由随后同连接的 CSS 请求确认属于浏览器，
不使用 SSH 中的 `curl` 探测请求作为浏览器终点。

第 2 次 trace 按主页 `navigationId` 对应事件，排除 Chromium 内部页面及子框架：

| 浏览器事件 | 板侧 monotonic | 换算复位后 |
| --- | ---: | ---: |
| 主页 `navigationStart` | 10.838 s | 12.703 s |
| 主页 `firstContentfulPaint` | 11.054 s | 12.919 s |

FCP 表示浏览器记录了内容绘制；不证明 HDMI 扫描输出、页面所有资源加载完或业务可交互。
Linux Chromium 的 `TimeTicks` 使用 `CLOCK_MONOTONIC`，见 [time_now_posix.cc](https://chromium.googlesource.com/chromium/src/+/main/base/time/time_now_posix.cc)。

## 3. 当前启动链

`lsp-kiosk-display.service` 检查 HDMI 状态，再异步启动上位机 socket、服务和 kiosk。
`lsp-kiosk.service` 排在后端 **进程启动**之后，随后退出 Plymouth、启动 Cage；它不等上位机 HTTP 就绪。
测量时 Chromium 先打开复制 HMI 外观的本地着陆页，再导航到本机 HMI。2026-10-16 已改成极简 HEAD 引导页；
上述旧版热重启数据不能视为新版冷启动成绩。现行契约见 [kiosk/session-design.md](kiosk/session-design.md)。

Cage 的 `ExecMainStartTimestampMonotonic` 只说明进程开始；本次没有合成器提交底色的专门探针。
三次从 Cage 开始到主页 GET 约 5.30 / 5.52 / 5.45 s；后端启动完成到 GET 约 4.11 / 3.81 / 3.90 s。
浏览器阶段仍比后端晚；仅加快 Python 导入不能证明整个页面会同步提前。

`blame` 只用于定位候选，须结合事件时间线；动态创建的 mount 也会改变事后 `critical-chain`，
不能把列表耗时直接累加。本轮不做启动优化。

## 4. 其它实验的边界

[LVGL 直出 DRM](lvgl-drm-direct-boot.md) 保留应用日志的 monotonic 指标。
旧文的 4.5 / 6.1 / 8.2 / 12.6 s 混用了 U-Boot `Total`、估算前缀和不同终点，不能作为当前“上电时间”基线。
优化状态见 [boot-time-optimizations.md](boot-time-optimizations.md)，I/O 归因方法见 [boot-io-profiling.md](boot-io-profiling.md)。

## 5. 复现与现场状态

先用 [serial-exec](../hardware/serial-exec.md) 后台抓取，再触发重启；保留 raw、JSONL、boot ID、
`systemd-analyze time`、单元 monotonic 属性和 journal。读取关键内核事件时使用 `journalctl --grep`，
避免通过串口输送整个 kernel journal。命令输出会混入并发串口告警，解析 JSON 行时须识别并保留这些额外行。

```bash
tools/serial-exec capture start --output /tmp/boot-record --duration 90 --json
tools/serial-exec run --sudo --password-env LSP_SERIAL_PASSWORD -c 'systemctl --no-block reboot' --json
# 等 getty 出现后登录、取数
tools/serial-exec run --login board --password-env LSP_SERIAL_PASSWORD -c 'systemd-analyze time' --json
tools/serial-exec capture stop --json
```

本轮临时 drop-in 和 trace 目录已清理，kiosk / oem-host 恢复运行，HTTP 200，最后一次系统 failed unit 为 0。
第 1 次出现过 `boot_init.service` 失败；后两次未出现，未把它当作稳定正常现象。
数据卡未找到时仍有有界 device timeout 日志；它不等于图形启动被阻塞。

本轮原始记录在开发机 `/tmp/lsp-boot-measure-20261010/`：`run-1/2/3` 分别含串口、journal 与单元记录，
第 2 次含 `chromium-trace.json` 和补采的 `anchors/`，汇总为 `summary.json`。原始日志不进知识库正文。
