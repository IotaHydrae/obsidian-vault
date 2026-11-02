# CM4 底板：SD 启动验证方法

> 工程内核在通用 CM4 载板启动，只能验证共用链路；uConsole 专用外设须在目标底板单独验证。

## TL;DR

- 载板镜像使用独立配置，移除 uConsole PMIC/DSI 和 GPIO 音频控制，保留匹配的内核与 initramfs。
- 构建与刷写分开；对指定卡核对身份、容量和占用，写后同步并回读全部写入范围。
- 启动后核对实际产物哈希、根分区扩容和服务状态；SSH 可达或桌面进程存在不足以确认全部功能。

## 准备独立载板输入

在原镜像副本上使用载板验证配置（工程源码：`image/validation/cm4-carrier/config.txt`）：
通用 CM4 DTB、工程 `kernel8.img`、显式匹配的 initramfs、HDMI KMS 和 `otg_mode=1`。
省略 uConsole overlay、音频重映射和 DWC2 overlay，不沿用原设备的超频设置。
删除 `uconsole-audio.service` 及其启用链接和辅助程序，防止在其他底板上驱动 GPIO10/11。

预置项目账户和 SSH；密码只通过本地受控输入提供，不写入来源元数据。
保留发行版首次启动的根分区扩容及 SSH 主机密钥生成，不把临时检查用的主机密钥留进镜像。
只读检查使用对应变体：

```sh
make image-check BOARD=uconsole-cm4 IMAGE_VARIANT=cm4-carrier IMAGE_USER=pi \
  IMAGE=<载板镜像路径>
```

## 刷写目标与回读判据

1. 先确认用户指定的可覆盖卡，记录块设备身份、容量、可移除属性及磁盘序号，排除系统盘。
   设备名可能复用；开始写入前重新检查身份、挂载点和其他块设备占用。
2. 写入程序持续持有独占设备描述符，核对容量和内核磁盘序号；准确写入镜像字节数并 `fsync`。
3. 使块缓存失效后，回读**整个已写入范围**，SHA-256 必须与源镜像一致。
   耗时只作为观察；没有预先定义的性能阈值就不判定速度合格。
4. 使用 `fsck.fat -n`、`e2fsck -f -n` 检查文件系统。只读挂载复核时，ext4 使用 `noload`。
5. 核对启动输入、模块、initramfs、账户及 SSH 策略；全部卸载后安全弹出读卡器。

回读一致证明写入范围与源文件相同，不证明后续启动或载板外设工作。

## 启动后的验收方法

- **运行输入：** 核对硬件 revision、`uname -r`、页大小及实际 Image、DTB、配置、initramfs 哈希；
  预期值来自本次构建产物与配置，不能只比较版本字符串。
- **存储：** 检查根分区大小、读写挂载、首次启动扩容和 SSH 主机密钥生成结果，以及 SD/ext4 I/O 错误。
- **服务和链路：** 读取 failed units、网络接口/路由、USB 枚举和延迟探测列表。
  链路速率、温度、服务运行仅是观察，不替代吞吐量、稳定性或真实输出测试。
- **下一次启动：** 首次启动可能修改磁盘 ID 和 PARTUUID。
  `/proc/cmdline` 表示本次启动最初的输入；另检查保存的 cmdline 与 `/etc/fstab` 是否使用新标识。
- **日志保留：** 核对合并后的 journald 配置和实际日志文件，见下文。

## journald drop-in 的排序陷阱

**本次观察：** `/etc/systemd/journald.conf.d/10-boot-test.conf` 请求 `Storage=persistent`，
但厂商 `/usr/lib/systemd/journald.conf.d/40-rpi-volatile-storage.conf` 随后设置 `Storage=volatile`；
日志实际位于 `/run/log/journal`，持久目录中没有日志文件。

不同目录的优先级不能覆盖不同文件名之间的字典排序。工程配置应选排序更靠后的文件名
（如 `90-boot-test.conf`），并用 `systemd-analyze cat-config systemd/journald.conf` 检查生效值。
再确认 `/var/log/journal/<machine-id>/system.journal` 实际存在；仅创建目录或 drop-in 不足以证明持久化。
后续修正后的验证见[Wi-Fi 访问准备](../network/cm4-uconsole-wifi.md#检查实际持久化输入)。

## 历史设备观察与边界（2026-10-17）

2026-10-17，CM4 Rev 1.1（revision `c03141`、4 GB）在微雪载板上从 SD 启动
`6.18.57-v8-uconsole`，aarch64 页大小为 4096 字节；五个所查启动产物哈希与准备输入一致。
首次启动完成了根分区扩容与 SSH 主机密钥生成，ext4 以读写挂载；systemd 为 `running`、无失败单元。
密码 SSH、以太网链路和 USB 集线器枚举已验证；采集日志中无 panic、Oops 或 SD/ext4 I/O 故障。

当时 Wi-Fi 与蓝牙固件已初始化，但均被 rfkill 软件阻止；该状态不证明缺少驱动。
HDMI 未接显示器，桌面进程存在没有对应可见画面验证。
外部 USB 传输、无线连接与配对、显示/音频输出、挂起、关机、持续负载及恢复原卡均不在此次验证范围内。
uConsole 的显示、背光、PMIC、充电及 GPIO 音频行为必须在实际 uConsole 底板上另行测试。
后续无线与 uConsole 结果见[Wi-Fi 访问准备](../network/cm4-uconsole-wifi.md)和
[启动排查](../boot/cm4-uconsole-boot-debug.md)；它们不改变本次载板样本的验证范围。

## 证据

本地忽略目录 `image/work/cm4-carrier-test/` 保存准备脚本，以及
`uconsole-cm4-project-boot-20261017.json`、`uconsole-cm4-project-boot-followup-20261017.json`
和 `journal-storage-20261017.txt`。镜像相邻的 `.img.sha256`、`.json` 与
`image/deploy/2026-10-17-cm4-carrier-card-check.json` 保存完整性和卡上检查结果；
它们是该次输入与观测的原始证据，不作为通用产物路径契约。
