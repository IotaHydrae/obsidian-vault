# CM4：缩短 labwc 启动前的等待

> CM4 可用 initial_turbo 缩短早期初始化；Netplan 应保留语义后再优化。

## TL;DR

- 当前板保留 `initial_turbo=10`，不提高最大频率或电压；其他核心板先独立测量。
- 受控重启的 labwc 创建时间从 7.66 s 到 5.85 s，不是电源键到画面计时。
- 关闭 Netplan 集成仅为对照实验；生产采用专用 generate 入口，不能照搬实验二进制。


## 适用范围

2026-10-23，本机为 Ubuntu Base 24.04.5、`6.18.57-v8-uconsole`、SD 根文件系统、
labwc + Noctalia，两者均显式选择私有 Mesa。CPU 上限 2200 MHz、OV6 已由
[独立负载试验](../hardware/cm4-overclock-testing.md)验证；本次没有提高这两个值。

## 方法：核对运行输入后再做配对比较

先固定内核、存储、输出、网络配置和采样方法；一次只改变早期频率或 NM 集成。
进程创建时间取 `/proc/<pid>/stat`，NM 耗时取 unit 的 Starting→Started；物理首帧另设判据。
每次记录 boot ID、实际 ELF/模块哈希、保护标志和网络健康；不以包版本或 SSH 回连代替这些检查。

### 服务私有替换与回退

用 root 拥有的私有文件，通过 `BindReadOnlyPaths` 只在 NM 服务命名空间中覆盖 daemon
和模块，保留 dpkg 原件及系统 `libnm0`。回退 timer 使用 `OnActiveSec=180s`，不依赖
网络；把激活与重启放进同一自动化流程，中间不等待人工操作，避免回退已先执行。
采样前必须核对 boot ID、`/proc/<pid>/exe` 与命名空间内十个文件的哈希，再记录耗时。
版本号和路径相同不足以证明候选在运行。

初次 `nm-off-1` 因前一轮回退已删除 drop-in，实际运行原程序，已在
`excluded-samples.json` 排除；其时间不参与比较。结束后恢复原程序并重启核对，
移除临时绑定、timer 和回退 unit。只测试 daemon、不保留匹配模块或必要 RUNPATH，
会引入功能变化，不能作为同变量对照。

## 原因：早期频率与异步初始化等待

原配置的 `init_kprobe_trace` 耗时 1.465433 s。源码表明，即使没有启动探针，
它仍获取 `event_mutex`；异步 trace eval-map 更新也持有这把锁。相邻 printk 只能
标出空档，不能证明 CPU 时间由 kprobe 自身消耗，方法见[initcall 定位](cm4-initcall-profile.md)。

一次诊断屏蔽该 initcall 后，等待转移到 `init_blk_tracer`（1.418594 s）。因此没有
保留黑名单，也没有删除 tracing 或晚期同步；只修补第一个等待者不足以消除等待。

固件原配置显式使用 `initial_turbo=0`，实际的 Raspberry Pi CPUFreq 驱动直到
2.291839 s 才开始初始化。设置 `initial_turbo=10` 后，kprobe initcall 降为
0.393786 s，CPUFreq 首次记录在 0.866863 s，随后仍能降到 600 MHz。
这支持早期频率影响初始化开销的结论；锁拥有者的完整调度栈未采集。

[固定版本官方说明](https://github.com/raspberrypi/documentation/blob/ddea98e98dafe9419feb4fd82710c2fbdef7dc44/documentation/asciidoc/computers/config_txt/overclocking.adoc#L62)
规定：`initial_turbo` 持续指定秒数，或到 CPUFreq 首次设置频率，取先发生者，
上限 60 s。它不等于 `force_turbo`，也不提高配置的最大频率。2024 年 11 月起
固件默认已是 60；本机的显式零值覆盖了新版默认行为。

## 原因：NetworkManager 同步执行 Netplan

greetd 等 `systemd-user-sessions`，后者等 `network.target`。这不是已禁用的
`NetworkManager-wait-online`，也不等 DHCP 成功。该顺序还参与反向关机依赖，
本次没有删除它。

Ubuntu `network-manager 1.46.0-1ubuntu2.8` 的 Netplan 补丁让 keyfile 插件在
`reload_connections()` 中同步执行 `netplan generate`，后者同步重新加载 udev/systemd。
仅为 NM 设置私有 PATH、原样委托真实 Netplan 的临时 wrapper 测得调用从
6.07 s 到 6.89 s，返回零，约 0.82 s。wrapper 随后移除。这与
[先前 Trixie 试验](cm4-boot-time.md#networkmanager移除同步-netplan-生成)属于同类编译期集成；
删 YAML 或调整插件配置不会跳过该调用。

2026-10-24 在 CM4 原生编译匹配的 Ubuntu 源码与 13 个补丁，仅构建 daemon 和
九个匹配模块，`-j3`，不安装开发包。关闭集成使用 24 个私有开发/工具包，启用对照
使用 26 个；输入均按签名 InRelease 的 Sources/Packages 哈希认证。原 21 包压缩档
是早期配方，完整复现应使用更新后的认证清单及 `nm-source/README-trial.md`。

Ubuntu 原包使用 autotools，私有构建使用 Meson，故不能只比较原包与候选。
启用对照补齐 Ubuntu 补丁遗漏的 `config.h.meson` 中 `WITH_NETPLAN` 占位符，
实际生成 `WITH_NETPLAN=1`；规范化后其余构建选项、环境及额外配置相同。
九个模块的 `.text` 均相同，daemon 则按开关选择是否链接系统 `libnetplan`。
Wi-Fi、WWAN、Bluetooth、PPP 等构建功能保持；清除私有构建 RPATH 后，Bluetooth
和 WWAN 工厂仍保留正式模块目录的 RUNPATH，避免运行期找不到匹配模块。

在相同 `initial_turbo=10`、网络配置和采样方法下，关闭→启用→关闭的重启观察如下。
NM 耗时取 journal 中 unit 的 Starting→Started 单调时间差，舍入至毫秒；
进程时间仍以内核单调时钟为起点。

| 重启样本 | Netplan 集成 | NM 耗时 | labwc 进程 | Noctalia 进程 |
| --- | --- | ---: | ---: | ---: |
| `nm-disabled-1` | 关闭 | 0.667 s | 5.32 s | 6.90 s |
| `nm-enabled-control` | 启用 | 1.428 s | 5.95 s | 7.57 s |
| `nm-disabled-2` | 关闭 | 0.668 s | 5.27 s | 6.87 s |
| `stock-control` | Ubuntu 原包，启用 | 1.404 s | 6.02 s | 7.67 s |

两次关闭样本均比同配方启用对照少约 0.76 s，桌面进程相应提前。这支持 Netplan
同步集成位于当前启动关键路径；样本数量有限，且未测物理首帧或按键到桌面时间。
四组 Wi-Fi、路由和网关可用，网络配置哈希一致，无失败 unit、保护标志为零，电池
26–28% 且 Charging。采样时四组 DNS 检查均失败，后续解析器修复不计入此次对照；
恢复原包后修正解析器配置，另一次重启确认 DNS 正常。不能把 SSH 回连等同于完整
网络验证。设备没有 `pppd`，也没有声称完成 PPP 功能验证。

`netplan=false` 关闭整个集成，也会改变连接保存、删除与 YAML 的同步语义，并非只跳过
一次生成。实验保留 Netplan 包、YAML 和启动 generator，仅验证配置不变时的启动。
没有将实验二进制纳入镜像。后续采用保留原包和 YAML 语义的
[专用生成入口](../network/ubuntu-netplan-generate.md)，减少 CLI 分派开销；
它仍执行生成和重载，收益与关闭整个集成不同。

## 设备观察：早期频率的配对重启

| 重启样本 | `initial_turbo` | systemd 前阶段¹ | greetd 进程 | labwc 进程 | Noctalia 进程 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 原配置 | 0 | 3.309 s | 6.87 s | 7.56 s | 9.34 s |
| 提频诊断 | 10 | 1.831 s | 5.26 s | 5.92 s | 7.55 s |
| 恢复原配置 | 0 | 3.339 s | 6.98 s | 7.66 s | 9.49 s |
| 无跟踪参数 | 10 | 1.842 s | 5.21 s | 5.85 s | 7.50 s |
| 恢复供电后 | 10 | 1.865 s | 5.33 s | 5.99 s | 7.73 s |

¹ `systemd-analyze` 标为 kernel，但没有独立的 InitRD 时间戳，包含 initramfs 用户态。
其余列来自 `/proc/<pid>/stat` 的单调启动时间，分辨率 10 ms。
提频诊断带 initcall/CPUFreq trace；正常性能对照不带这些参数。

无跟踪参数组在 uptime 21.22 s 采集，DSI-1 启用、transform 270、scale 1.5，
无失败 unit，固件保护标志为零。随后 SSH、ARP 均失联。2026-10-24 用户确认
是电池耗尽；这属于用户报告，设备没有持久 journal，无法追溯验证关机过程。
恢复供电后相同设置连续在线超过十分钟，电池从 5% 升至 7%、状态 Charging，
UPower 报告非电池供电，保护标志仍为零。该观察支持保留设置，不能替代完整稳定性试验。

## 证据身份、已恢复输入与未知项

原始 JSON、trace 与 NM 输入认证材料在开发机
`~/.cache/uconsole-desktop-20261018/boot-early-desktop/`。样本身份：

- 原配置：`00000000-0000-4000-8000-000000000003`，`baseline-1.json`。
- 提频诊断：`00000000-0000-4000-8000-000000000015`，`turbo-10-diagnostic.json`。
- 恢复原配置：`00000000-0000-4000-8000-000000000025`，`turbo-0-control.json`。
- 无跟踪参数：`00000000-0000-4000-8000-000000000001`，`turbo-10-normal.json`。
- NM 调用计时：`00000000-0000-4000-8000-000000000036`，`nm-timed-baseline.json`。
- 恢复供电后：`00000000-0000-4000-8000-000000000007`，
  `~/.cache/uconsole-desktop-20261024/boot-resume/baseline.json`，uptime 407.78 s。

NM 配对结果、身份和健康采样在 `~/.cache/uconsole-desktop-20261024/boot-resume/`，
汇总为 `paired-reboot-results.json`；构建控制记录在原证据目录
`nm-source/paired-control-comparison.json`。有效配对样本：

- 关闭第一次：`00000000-0000-4000-8000-000000000018`。
- 启用对照：`00000000-0000-4000-8000-000000000027`。
- 关闭第二次：`00000000-0000-4000-8000-000000000011`。
- 原包复核：`00000000-0000-4000-8000-000000000020`。

DNS 修复后的原包重启为 `00000000-0000-4000-8000-000000000022`，
见 `dns-fixed-reboot-health.json`；不与修复前的配对时间混算。

设备原始 cmdline/config 在 `/root/uconsole-early-boot-20261022/`。最后启动不带黑名单
或 trace 参数；计时 wrapper、drop-in、恢复 unit 已删除。导出的 SD 镜像未变。

**未验证候选：** `driver_async_probe=bcmgenet`；当轮同步以太网探测约 198 ms，位于 SD
初始化前，固定内核支持该选项并保留末尾异步同步点；收益尚未实测。initramfs 已用
zstd，解包约 118 ms，不能仅凭体积断言更换压缩格式更快。物理冷启动、首张可见画面、
CM5 及其他厂商模块都需独立验证。
