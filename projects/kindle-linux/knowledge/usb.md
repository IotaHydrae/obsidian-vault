# 用 USB 开发 Kindle

USB 自动启动已回滚。用户改名 ENABLE_USBNET 并重启后恢复正常界面；随后主机识别到
原 Kindle USB 存储卷，读回确认启用标记不存在、停用标记保留，并核对 emergency.sh
与部署 SHA256 一致后移除。再次核对两个活动入口均不存在，完成同步并安全卸载。
自动启用命令保持禁用。下次物理拔线后的界面退出仍需用户观察，不能由主机卸载替代。
原始清理证据保存在忽略的 work/usb-autostart-rollback.json。

恢复路径：保持 USB 拔下，长按电源 40 秒重启。若可进入 KOReader，在文件浏览器的
/mnt/us 下将 ENABLE_USBNET 改名为 ENABLE_USBNET.off，使现有启动钩子不再执行。
完成后再正常重启、保持不接 USB。恢复连接后移除本工程 emergency.sh 并验证拔插行为。
若文件被过滤，需要在文件浏览器显示不支持的文件。

## 已验证的连接

| 项目 | 实测结果 |
| --- | --- |
| Kindle USB 地址 | <HOST_02>/24 |
| SSH | root，端口 2223，仅公钥认证 |
| 本机 USB 地址 | <HOST_01>/24 |
| 主机网卡 | 已识别的 Kindle USB 接口，cdc_ether |
| USB 枚举 | 0525:a4a2，Ethernet/RNDIS Gadget |
| 主机连接配置 | kindle-usb，自动连接，不接管默认路由和 DNS |
| 身份确认 | 服务端 ED25519 与此前已信任的同一 Kindle 密钥一致 |

```sh
ssh -p 2223 root@<HOST_02>
```

本机已保存新地址对应的同一主机密钥，保持严格检查。旧 Wi-Fi 配置保存在忽略的
`work/device-wifi.toml`，没有混入版本控制。另一块 10.55.0.x USB 开发设备没有改动。

## 日常开发

Kindle 保持 USB 网络模式并连接数据线后，在本机工程目录执行：

```sh
make probe
make app-install
make app-check
make alpine-shell
```

这些命令使用工程默认的 USB SSH 配置。更新插件后，需要在界面正常退出并重新打开
KOReader，才能加载新代码。`make app-preview` 仅在 KOReader 尚未运行时启动预览。
当前已打开真实插件面板，截图在 `work/koreader-plugin.png`。

SSH、插件上传和 chroot 命令不依赖手机热点。没有配置 USB 共享上网，
这不等于 Alpine 或 Kindle 已通过电脑获得互联网连接。

USB 网络模式下不出现 Kindle U 盘是正常现象，用户区文件通过 SSH 工具访问。
此状态下可以运行 KOReader；实际验证了阅读器启动后 USB SSH 继续可用。

## 自动启动和手动恢复

```sh
make usb-autostart          # 当前暂停，拒绝执行
make usb-autostart-status   # 读取配置，不能当作实际重启验证
make usb-autostart-disable  # 删除本工程的启动标记/钩子，下次重启生效
```

配置复用原生 ENABLE_USBNET 和 KMC 已有的 emergency.sh 入口。脚本后台运行，
不改厂商根分区、内核或 bootloader；SSH 仍使用现有公钥。部署器拒绝覆盖外部脚本。
首次失败的原因是原生存储导出卸载用户区，等待 usb0 的旧脚本随即退出。
修正后，钩子先将代码复制到 /var/tmp/kindle-usb，worker 通过原生 volumd 属性及
HAL 事件请求网络，并在用户区重新挂载后启动 SSH。日志始终写入临时目录，结束时再
复制至 kindle-usb/boot.log。第二次重启网卡已出现，但 SSH 拒绝连接，需恢复调试后读日志。
仅看到网卡、配置标记或手动运行成功不算完整开机验证通过。

若重启后没有 SSH，可用已安装插件手动恢复：

1. 安全弹出存储，拔下数据线，再打开 KOReader。
2. 顶部工具图标 → 更多工具 → Kindle Linux 示例 → USB 有线连接。
3. 点击“启用并退出阅读器”。
4. 出现“USB 有线连接已开启”后接上数据线。

本机已保存自动连接配置；换电脑或接口识别异常时，显式检查和配置：

```sh
python3 tools/usb.py remember  # 存储模式下记录物理端口
make usb-probe
make usb-configure
```

识别仅匹配 Amazon 设备或已记住端口的已知 Ethernet gadget，避免改到其他 USB 网卡。
配置器只更新带工程 stable-id 的 kindle-usb 连接；遇到同名无标记连接会拒绝修改。
主机自动连接与设备端自动启动分别验证。当前重启后网卡已出现，SSH 仍未就绪；长期休眠和反复插拔也待验证。

## USB 存储更新方式

尚未建立 USB 网络时，仍可通过已挂载的用户区更新插件：

```sh
make app-check-local
make app-install-usb
# 非默认挂载路径：
python3 tools/plugin.py install-usb --mount /path/to/Kindle
```

安装器检查挂载点、VFAT 类型、Kindle 卷标和 KOReader 目录；写入 staging 后 fsync、
读回哈希并运行本机行为检查，完整验证后才发布，保留旧版并同步。它不运行设备端代码。
进入 USB 存储前须退出 Alpine。当前没有自动 USB 导出联锁。

## 启动器和保护检查

手动 helper 先检查 root、i.MX6SLL、原根只读、现有密钥、无活动 Alpine 镜像和线缆未连接。
通过后，插件发送 KOReader 正常 Exit 事件，helper 等待 reader 进程结束、volumd 恢复响应，
再通过 useUsbForNetwork=1 请求系统切换，并配置 usb0。它不直接强制卸载模块。

专用 Dropbear 绑定 USB IP:2223，关闭密码认证，重用已有主机密钥和公钥授权文件；
手动 helper 不替换原 2222 服务，不写系统脚本，也不重启或刷写设备。
自动启动通过独立命令部署，并单独执行正常重启验证。

诊断与启动日志保存在用户区 `koreader/settings/kindledev-usb.log`。
“导出 USB 诊断”只读取模块、接口、原生脚本和守护进程状态，不切换模式。

## 本机问题与修正依据

- 保存的命令表包含 ;un，但实际 `/usr/local/bin/usbnetwork.sh` 缺失，原生命令无法执行。
- KOReader wrapper 暂停 volumd；阅读器内的 LIPC 超时不代表 USB 驱动缺失。
- 本机 KDB SYS_CONNECTED 返回路径
  `/sys/class/udc/ci_hdrc.0/subsystem/ci_hdrc.0/connected`，该路径不可读。
- 对应固件官方 BD71827 驱动声明 bd71827_ac 为 Mains；ONLINE 来自 DCIN_DET。
  电池 ONLINE 与外部供电不同，不用于判断 USB 线缆。
- 0.2.1 接受有效旧字段并读取 USB/Mains online。任一连接信号优先；未知仍停止。
  UDC not-attached 单独不作为物理拔线证据。切换前再次检查。
- 实机启动日志读到 bd71827_ac/online=0、imx_usb_charger/online=0，随后出现
  USB_LINK_READY=1；电脑枚举出 cdc_ether，实际 root SSH 连接成功。

## 已执行的验证

2026-10-29，Asia/Shanghai：

- 51 项本机工程测试、ShellCheck 通过，包含启动配置归属/发布检查、10 项线缆状态和 35 条 Lua 行为断言。
- 四个运行文件已通过 USB 更新为 0.2.1，读回哈希与版本核对一致。
- 实际 SSH_CONNECTION 是 <HOST_01> → <HOST_02>:2223，UID=0。
- 原根挂载保持 ro，secure_cpu=1、unlocked_kernel=false。
- 通过 USB 在设备 Lua 5.1 中执行 35 条行为断言；UI/存储使用测试替身。
- 通过 USB 的 Alpine 检查通过：原生程序、持久性、退出码、互斥和挂载/loop 清理。
- 通过 USB 启动真实 KOReader 插件，读回并查看 600×800 截图，布局正常。
- 用户实际打开插件、导出诊断、手动启用 USB 已有实机证据；未把所有按钮都算作手工验证。

本机回执和原始日志在忽略的 work/usb-check.json、work/usb-host-configuration.json、
work/device-usb-start.log、work/device-usb-probe.json、work/plugin-check.json、
work/plugin-preview.json 和 work/chroot-check.json。它们是本次观察，不是持续在线状态保证。

## 参考

USBNetLite 的公开实现提供了 volumd 控制的参考；没有运行其系统安装器。
来源：https://github.com/notmarek/kindle-usbnetlite 。官方内核研究来源固定在
manifest/amazon-source.toml，提取的驱动位于忽略的 .cache/amazon/kernel-usb-research/。
