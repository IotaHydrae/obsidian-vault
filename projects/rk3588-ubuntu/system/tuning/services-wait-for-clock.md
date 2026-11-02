# 服务要不要"等时钟就绪"再启动

> `network.target` / `network-online.target` **都不代表时钟已准**；表示"时钟已同步"的原语是
> `time-sync.target`（由 `chrony-wait.service` 喂）。等它必须**有界 + 超时放行**：
> 无网且 RTC 时间不准时，等待本身无法校准，等满只会推迟启动。

## TL;DR

- `network.target` 用于网络服务启动 / 停止排序，不保证连接完成；`network-online.target` 使用网络管理器的就绪判据，
  不保证互联网或 NTP 可达。两者都不保证时钟已经同步。
- 本镜像里 `time-sync.target` 是**空过**的：`chrony-wait.service` 处于 `disabled`。
- ⛔ **不要 `systemctl enable chrony-wait.service`**：它 `WantedBy=multi-user.target`，而 target 会对
  `Wants=` 隐式补 `After=` ⇒ 无网时**整个 `multi-user.target` 最多被拖 180 s**。
- 要给某个服务加"等时钟"，用下面两种**有界**写法之一。
- 离线快速启动并维持可用日期，优先使用已校准且有电池的 RTC；初始误差与长期漂移仍需考虑。
  fake-hwclock 只提供存档下限，不能补算断电时长，见 [time-sync.md](time-sync.md)。

## 怎么等（两种有界写法）

**写法一：给服务加 drop-in 拉 `chrony-wait`（不改 ROS 的 Exec）**

```ini
# /etc/systemd/system/<svc>.service.d/10-wait-clock.conf
[Unit]
Wants=chrony-wait.service
After=chrony-wait.service

# /etc/systemd/system/chrony-wait.service.d/10-timeout.conf
[Service]
# 将默认的 180 秒上限收紧到 30 秒。
TimeoutStartSec=30
```

- `Wants=` 是**弱依赖**：在这里只配 `Wants/After`，`chrony-wait` 超时失败不会阻止后续服务启动。
- 等待上限是 `chrony-wait` 自己的 `TimeoutStartSec`（默认 **180 s**）——
  `chronyc waitsync 0 …` 里的 `0` = 无限次尝试、不自己返回。
- 这个 drop-in 把等待限定在选定服务；若该服务位于开机关键链，仍会推迟相应目标，不能视为完全没有启动影响。

**写法二：服务自己等，不依赖那个单元**

```ini
[Service]
ExecStartPre=-/usr/bin/timeout 30 /usr/bin/chronyc waitsync 0 0.1 0.0 1
```

前置的 `-` 让等待失败后仍执行 `ExecStart`。缺少它时，前置命令超时会使本次启动失败；
若配了 `Restart=on-failure`，服务可能反复等待、失败并重启，无法实现超时放行。

## 已有记录与适用边界

以下旧记录只注明 chrony 4.5，缺少测试日期、板型、内核版本与网络条件，不能推广为启动耗时保证：

```
[    9.852705] chronyd[1034]: chronyd version 4.5 starting
[   24.841155] chronyd[1034]: Selected source 106.55.184.199 (ntp.tencent.com)
```

两条日志相隔约 15 秒，只能说明该次启动的 chronyd 启动与选源时刻；
不能单凭选源日志证明校准完成，也不能据此计算业务服务额外等待了多久。
无网时等待可能耗尽上限；若 RTC 时间也不准，等待不会自行校准。

## 结论

- 有界等待可以给 NTP 留出校准机会，但超时放行后不保证已同步；无网时应检查 RTC / 存档状态。
- 需要时间准的地方（如采集服务日志）优先考虑**接 RTC 电池**，而不是靠等待。

## 相关

- systemd.unit(5) 的 `Wants/After`、systemd.service(5) 的 `ExecStartPre` 与 `TimeoutStartSec`；
  systemd.syntax(7) 的注释规则：注释单独成行，不能附在超时值后。
- [time-sync.md](time-sync.md) —— 时间从哪里来（内核 RTC → fake-hwclock → chrony）
- [boot-network-gating.md](boot-network-gating.md) —— 开机不要被"等网络"门控（另一件事：NM 的 `NM_ONLINE_TIMEOUT`）
