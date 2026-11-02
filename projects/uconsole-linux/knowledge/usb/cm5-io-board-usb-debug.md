# CM5 IO Board：USB 串口、固定地址网络与可选 ADB

## TL;DR

- 官方 CM5 IO Board 的唯一 USB-C 口 J11 同时承担供电和 USB 2.0 数据；正常 Linux 可将它用作设备口，`rpiboot` 则使用启动 ROM 的独立恢复协议。
- `cm5-io-board` 预置 ACM 登录串口 + NCM 网络配置，镜像默认不加载 `dwc2`；按需取消启动 Overlay 注释并重启。
- 板端固定 `<HOST_01>/24`，只向电脑分配 USB 链路地址，不发布网关或 DNS；电脑使用普通 DHCP 即可。
- ADB 是独立开关，默认关闭；当前 Linux 移植没有 Android 密钥授权，启用后以镜像普通用户运行，仅开放 USB。
- 2026-10-28 已在 Linux 主机验证项目载荷的三功能并发；其他主机系统、长时间稳定性和最终镜像启动需分别验证。

## 启用与使用

前提是已安装本仓库的 CM5IO USB 载荷、匹配内核及 `dwc2.dtbo`，nRPIBOOT 跳线已拔掉。
用支持数据传输的线将 J11 连到电脑；这一根线同时供电，电脑端供电能力仍须满足板子负载。
在板子的 `/boot/firmware/config.txt` 的 `[cm5]` 段取消以下行的注释，随后重启：

```ini
dtoverlay=dwc2,dr_mode=peripheral
```

只有 USB 线的部署，应在首次正常启动前离线启用这行配置。镜像预置了载荷，
不代表设备口已经开启；不能假设还能先用 SSH 修改它。若已启动但没有其他访问入口，
可重新进入 nRPIBOOT 存储模式，只修改 eMMC 的 FAT 启动分区，再拔跳线正常重启。
这属于设备部署选择，不改变通用镜像的默认关闭策略。

启动后，`cm5io-usb-gadget.service` 创建复合 USB 设备，板端接口为 `ttyGS0` 和 `cm5usb0`。
电脑侧通常出现 `/dev/ttyACM0` 与以 `enx` 开头的网卡；编号、名称由电脑现有设备和命名策略决定，不保证叫 `usb0`。
串口使用普通账户密码登录，没有自动登录。电脑的 USB 网卡使用自动获取 IPv4，然后执行：

```sh
ssh pi@<HOST_01>
```

这里的 `pi` 是默认镜像账户；自定义用户名时使用实际名称。
板端 dnsmasq 仅绑定 `cm5usb0`，租约池为 `<HOST_02>–<HOST_03>`。
`port=0` 关闭 DNS 服务，空 router / dns-server DHCP 选项避免修改电脑的默认网关与 DNS。
这条链路用于访问板子；没有启用桥接、NAT 或电脑到板子的互联网共享。
若电脑已有相同网段的连接，应先调整地址规划，固定私网地址无法避免所有现场冲突。

### Arch：允许 NetworkManager 管理板端 gadget 网卡

Arch 的 `/usr/lib/udev/rules.d/85-nm-unmanaged.rules` 默认将
`DEVTYPE=gadget` 设为 `NM_UNMANAGED=1`。仅安装静态地址 keyfile 不会覆盖此策略。
2026-10-29 实机出现 ACM 正常、主机 NCM 网卡存在但无 carrier、板端
`cm5usb0` 为 down/unmanaged，NM 原因为 77。

Arch 后端现在安装局部覆盖 `/etc/udev/rules.d/99-cm5io-usb-network.rules`：

```udev
SUBSYSTEM=="net", KERNEL=="cm5usb0", ENV{DEVTYPE}=="gadget", ENV{NM_UNMANAGED}="0"
```

它只接管项目创建的接口；不改变其他 gadget 或普通网卡。已有设备加载后，重载
udev 规则、触发该接口的 change 事件，再激活 `cm5io-usb` 配置。实机随后恢复
固定地址、DHCP 与 SSH，2 MiB 双向传输校验一致，冷启动后自动连接也已复核。
另一次软件重启中，系统与 gadget 服务已启动、DWC2 已绑定，但主机没有重新枚举；
日志没有后续 high-speed reset / address 记录，原因仍待隔离。不能把这个失败归于
已成功启动的 UDC 条件检查，也不能把冷启动成功推广为软件重启可靠。
实际设备状态见[开发状态](../../project/status.md)。

### 按需增加 ADB

在启用上述 Overlay 的板子上执行：

```sh
sudo touch /etc/cm5io-usb-adb.enabled
sudo reboot
```

电脑安装普通 `adb` 客户端后可执行：

```sh
adb devices -l
adb shell id
adb push local-file /home/pi/
adb pull /home/pi/local-file
```

`adb shell id` 应显示配置的普通用户；自定义用户需替换示例 home 路径。
关闭 ADB 时删除 `/etc/cm5io-usb-adb.enabled` 并重启。
关闭全部 USB 调试时重新注释 `dwc2` Overlay 并重启。
推荐在启动时一次创建所需组合；动态加载、卸载 Overlay 不作为已验证的日常切换方法。

此 adbd 的 Linux 移植明确关闭 Android framework authorization，电脑不会看到 Android 的信任确认界面。
项目使用普通账户、`NoNewPrivileges=yes`、`ADBD_USB=1`，并清空三个网络监听变量。
这限制了 daemon 及子进程的权限和传输入口；该账户本来可读取或修改的文件仍可通过 ADB 访问。
ADB 开启期间，能连接该 USB 数据口的电脑可以直接使用该账户的 ADB 会话。

### 可选的主机名访问

板子运行 Avahi、电脑支持 mDNS 且能收到广播时，可用 `ssh pi@cm5-io-board.local`。
这里使用板子的实际主机名与 `.local` 后缀，不要求修改成 `raspberrypi`。
2026-10-28 在用户配置 Wi-Fi、网线断开的实机上安装并启用 Avahi 后，Linux 主机
将此名称解析到 USB 固定地址，密码 SSH 成功；回执为 `avahi-ssh-test.json`。
用户选择先只在板上测试，当前镜像不预装 Avahi；其他主机的解析支持仍需确认。
名称可能对应多个活动接口，固定 USB 地址仍可用于明确选择 NCM 链路。

### Linux 主机的探测与权限

若 ModemManager 把 ACM 当成调制解调器探测，登录串口可能收到 AT 命令。
仓库提供按产品、VID/PID 限定的主机 udev 规则（工程源码：`tools/host/79-cm5io-usb-debug.rules`）：

```sh
sudo install -m 0644 tools/host/79-cm5io-usb-debug.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

重新插拔数据连接或正常重启板子，使权限在重新枚举时应用。
规则跳过该 ACM 的 ModemManager 探测，并通过 `uaccess` / `plugdev` 为普通主机用户提供 ADB USB 访问权限。
串口访问仍遵循主机的终端设备权限策略。
若 adb server 已缓存 `no permissions`，在权限和枚举恢复后重启当前用户的 adb server 再检查。
无需为这项功能全局关闭 ModemManager。

## 组成与生命周期

`dwc2` Overlay 只负责设备树中的 USB 控制器角色；用户态服务负责配置复合设备。
ACM、NCM 和 ADB 分别由 `acm.GS0`、`ncm.usb0` 和可选的 `ffs.adb` 组成。
ADB 通过 FunctionFS 写好描述符和端点后，gadget 才绑定 UDC，避免发布不完整组合。
内核的 configfs FunctionFS 支持足够完成这条路径，无需启用旧式独立 `g_ffs` gadget。
服务停止时先解除 UDC 绑定，停止端点用户，再按依赖顺序卸载 FunctionFS、删除链接和目录；configfs 不使用递归删除。

USB 序列号来自硬件序列号，两个 MAC 根据硬件身份与角色分别派生，使用本地管理单播地址。
因此同一板子重启保持身份，不依赖克隆镜像中的 machine-id；不同角色使用不同地址。
内核 `gether_set_ifname()` 要求模板恰有一个 `%d`，项目使用 `cm5usb%d`，首个接口为 `cm5usb0`。

## ADB 输入与许可证

Ubuntu Noble 的普通 `adb` 包是主机客户端，不能替代设备端 adbd。
项目固定使用 [RoboParty adbd4linux v1.0.7](https://github.com/Roboparty/adbd4linux/releases/tag/v1.0.7)：

| 输入 | 固定值 |
| --- | --- |
| 源码提交 | `2116509f58fca2f53d4b029d9c123524a282cfc3` |
| 发布包 | `adbd_1.0.7-1_arm64.deb` |
| 包 SHA-256 | `a6c27dd1ea600badc788f36db9f241a1bc3cc56a4c5067e247965563e5a69cdf` |
| adbd SHA-256 | `d58818acb27325119cd9693c5f02722d1f55a46d7f5442ceeff9a7ec1ebcd92d` |

构建仅从校验后的包提取 `usr/bin/adbd` 和 copyright，安装到项目私有路径与文档目录。
不运行该包的安装脚本、不启用它自己的 gadget 服务，也不引入 scrcpy、SMB 等无关功能。
运行库来自 Ubuntu Noble；固定发布包及二进制哈希是输入完整性判据，不代替移植源码审查。
移植行为见固定提交的 [linux-port.patch](https://github.com/Roboparty/adbd4linux/blob/2116509f58fca2f53d4b029d9c123524a282cfc3/cmake/linux-port.patch)。

## 实测观察与未知项

2026-10-28，4GB CM5 / 官方 IO Board、Ubuntu Base 24.04.5、内核 `6.18.57-v8-16k-uconsole` 与本地 Linux 主机：

- 启动时加载 `dwc2,dr_mode=peripheral` 后，ACM 密码登录与 USB SSH 同时工作，2 MiB SSH 内容 SHA-256 一致。
- 板端 DHCP 模式下，电脑自动获取 USB 子网地址，无 DHCP 网关或 DNS，原有电脑默认路由保留。
- 项目最终载荷部署并重启后，ACM 密码会话、NCM 上的 2 MiB SSH 校验与 ADB 的 2 MiB push/pull 同时通过；ADB shell 是普通 `pi`，`NoNewPrivs=1`。
- 单独删除 ADB 标记并重启后 ACM/NCM/DHCP 仍可用；注释 Overlay 并重启后无 UDC/gadget，相关服务跳过，FunctionFS 已卸载。随后按用户要求恢复三功能开启。
- 用户另行配置 Wi-Fi 并拔掉网线后，外网路由走 `wlan0`，DNS 与证书校验后的 HTTPS 200 正常，USB 固定地址 SSH 仍可用；没有启用 USB 网络共享。
- 动态加载 Overlay 的早期原型曾出现主机到板子收包失败；运行中重绑还观察到 DWC2 stop-transfer 超时。启动时路径可用，根因未隔离。
- 电脑 USB 供电启动早期曾出现短暂欠压，随后当前保护标志清零；历史 `0x50000` 不能解释为持续欠压，也不能证明供电余量充分。

三功能并发回执为 `final-concurrent-test.json`，判据是 SSH 内容及 ADB 往返摘要一致且串口登录保持可用。
这些观察不建立 Windows/macOS 免驱保证、长期传输稳定性或新镜像冷启动结论。
ADB 权限规则须在新枚举后检查；主机型号、供电线和其他 USB 设备变化需要重测。
原始日志保存在忽略的 `image/work/cm5io-usb-20261028/`，现场地址与设备身份不进入本文。

## 参考

- [CM5 IO Board datasheet](https://datasheets.raspberrypi.com/cm5/cm5io-datasheet.pdf)：J11 USB-C 的电源与 USB2 接口。
- [Linux USB gadget configfs](https://docs.kernel.org/usb/gadget_configfs.html)：函数、配置与 UDC 绑定。
- 板型说明（工程源码：`board/cm5-io-board/README.md`）与[开发状态](../../project/status.md)：工程选择与最新验证边界。
