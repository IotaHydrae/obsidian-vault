# CM4：移入 uConsole 前的 Wi-Fi 访问准备

> 无网口 uConsole 的远程诊断应先在可访问载板上验证独立无线链路，再迁移核心板和启动卡。

## TL;DR

- 仅在 Git 忽略的本地配置中提供 Wi-Fi 凭据；预置账户、SSH 和归 root 所有、权限为 `0600` 的 keyfile。
- 安排有线恢复后断开以太网，新建 SSH 会话并核对返回路由经 `wlan0`，避免误判访问路径。
- 检查实际启动命令行和 journald 生效配置；工具成功退出、配置文件存在都不足以证明生效。

## 准备与验证方法

1. 确认载板上运行预期工程内核，根分区来自将要迁移的 SD 卡；另存正常启动与网络配置。
2. 在 `/etc/NetworkManager/system-connections/` 安装 keyfile，使用 DHCP、自动连接和重试。
   同时检查 NetworkManager 无线电状态及 `rfkill`；有驱动而被软件阻止时无需先更换内核。
3. 关联后核对接口地址、默认路由及 `iw reg get`。国家代码配置值与运行时全局/PHY 监管域可能不同，分别记录。
4. 保留可自动恢复的有线连接措施，临时断开以太网，从无线地址新建 SSH 会话。
   在设备上用 `ip route get <客户端地址>` 核对返回路径的接口与源地址，再恢复有线连接。
5. 迁移前恢复 uConsole 板级配置，核对天线选择与实际接线匹配，并检查下述持久化输入。

无线成功的判据是独立会话经无线接口建立且路由一致；DHCP 地址、关联状态或有线 SSH 可达不能单独满足这个判据。
以上过程验证远程访问路径，不验证吞吐量、长期稳定性或机壳内的射频表现。

## 检查实际持久化输入

**命令行选择：** 本工程显式设置 `cmdline=/cmdline.txt`。当时安装的 `raspi-config` 中，
`do_wifi_country` 按 `/proc/device-tree/chosen/os_prefix` 选择文件，没有处理该覆盖项。
因此应沿固件实际选择路径核对 `/boot/firmware/cmdline.txt` 中的监管参数，不能只看工具退出码。
运行中的 `/proc/cmdline` 与下次启动保存的参数也应分开检查。

**日志持久化：** 核对合并配置和实际日志文件；drop-in 的排序规则及覆盖问题见
[载板日志验证](../storage/cm4-carrier-sd-test.md#journald-drop-in-的排序陷阱)。

```sh
systemd-analyze cat-config systemd/journald.conf
find /var/log/journal -name system.journal -type f
```

当时改为 `90-boot-test.conf`，重启 journald 并写入日志后，确认 `Storage=persistent` 生效且产生了持久日志。
仅创建 `/var/log/journal` 或配置文件不能证明日志已经落盘。
完整的载板验证边界见[CM4 底板 SD 启动测试](../storage/cm4-carrier-sd-test.md)。

## 设备观察与适用边界

2026-10-17，微雪载板上的 CM4、SD 根分区和 `6.18.57-v8-uconsole` 组合完成了上述独立无线 SSH 验证。
初始故障是 NetworkManager 无线电关闭且被 rfkill 软件阻止；解除后通过 2.4 GHz 接入点关联。
配置请求国家代码 `CN`，运行时观察到全局 `AU`、PHY `99`，因此没有把保存值当成实际监管域。

同一测试镜像的离线 `image-check` 核对了所选内核、模块、initramfs、账户、SSH 与板级输入；
另用镜像内的 NetworkManager 解析 keyfile，核对权限、自动连接、DHCP 及无线电状态。
这些检查只能证明该次镜像输入符合预期，不能单独证明移机后的重连与外设功能。后续 uConsole 的
Wi-Fi/SSH 启动观察见[启动排查记录](../boot/cm4-uconsole-boot-debug.md)；不要把本次载板结果
或早期未知项当作后续所有配置的结论。
凭据及私有镜像保存在 Git 忽略路径，不进入公共仓库。

## 证据与关联

- 板级输入：CM4 启动配置（工程源码：`board/uconsole-cm4/config.txt`）。
- 本地原始证据：忽略目录 `image/work/cm4-wifi-test/` 中的 `wifi-live-20261017.json`、
  `wifi-only-ssh-20261017.json`、`next-boot-20261017.json`；其中保留实际地址和完整运行时状态。
- 无线已关联而启动仍报 `-52` 时，另见[信道调查报错](cm4-brcmfmac-channel-survey.md)。
