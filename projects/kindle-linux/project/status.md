# 开发状态

记录日期：2026-10-29（Asia/Shanghai）。

## 已观察到的设备状态

- 用户确认：2019 款青春版、中国大陆销售；设备平台为 KT4 / Jaeger。
- 已通过 KOReader 的 2222 端口验证 root SSH；保留原公钥并添加了本机公钥。
- SoC：i.MX6SLL，单核 ARMv7；系统可见内存 504412 KiB。
- 固件：5.18.1.1.1；内核：4.1.15-lab126。
- 屏幕模式：600×800 / 800×600；帧缓冲 mxc_epdc_fb。
- eMMC：3909091328 字节；boot0 / boot1 各 4 MiB；GPT 表头和分区项 CRC 通过只读解析。
- system 为 p8，snapshot 为 p7；不能套用旧型号的 diags 分区图。
- 启动参数仍为 secure_cpu=1、androidboot.prod=1、androidboot.unlocked_kernel=false。
- 厂商系统根挂载始终观察为 ext3、只读。

## 已验证的手动 Alpine chroot

- 已用固定 SHA256 / PGP 签名的 Alpine minirootfs 3.24.3 ARMv7 安装独立镜像。
- 镜像是用户区的 512 MiB 普通文件；没有替换厂商 rootfs、内核或启动链。
- 原生 busybox、shell 和 apk-tools 3.0.8-r0 在原 4.1.15-lab126 内核上运行。
- 已验证跨会话文件持久性、失败命令退出码传递、互斥锁拒绝并发，以及正常/失败退出后的挂载和 loop 清理。
- 再次安装被拒绝，既有镜像的大小和 mtime 保持一致。
- chroot 的最小 dev 中无 eMMC 块设备节点；proc 与宿主共享进程视图，不是安全沙箱。
- 已安装 ca-certificates、curl、Python；Python 3.14.10 返回 armv7l、数学结果 5050。
- OpenSSL 3.5.9；curl 的 HTTPS 响应正文验证通过，未跳过证书或包签名检查。
- dl-cdn 的部分大文件在 Kindle 和本机都提前 EOF；切换官方列表中的 TUNA 镜像后成功安装。
- 当前只启用 v3.24/main；附加软件包 revision 未锁定，不宣称逐字节可复现。
- 安装、开发环境及运行验证的本机回执分别在忽略的 work/chroot-*.json。

## 工程与研究

- 人类 Makefile / 机器 JSON handoff、板型策略、只读探测、输入验证及手动 chroot 命令已实现。
- 具体命令见 [使用方法](../knowledge/chroot.md)；设备操作不属于离线 make check。
- 已运行 16 项离线测试、ShellCheck、真实 bootstrap / HTTPS 和独立实机 chroot 检查。
- Amazon 对应 GPL 源码的版本、大小、SHA256 在 manifest/amazon-source.toml。
- 安全启动与离线恢复边界见 [刷写调查](../knowledge/flashing.md)。

## 未验证与保留限制

- 未创建整机备份，未刷写分区、修改 IDME / fuse / 启动变量、重启或切换恢复模式。
- 没有已验证的离线写回通道；没有验证任意替换 system 后能否启动。
- 手动运行可信前台程序，不启用 OpenRC init，不接管 Kindle PID 1，没有开机自动启动。
- 没有 USB 存储导出联锁；必须退出 chroot 后再进入 USB 存储模式。
- 已验证单次中文文字绘制；触摸应用、后台长期服务及其他软件与旧内核的兼容性尚未验证。

## 显示与应用示例

- 实测 FBInk 92e1270：600×800、8-bit Y8、608 字节行跨度；可用 OpenType，当前构建 Image=No。
- Alpine Python 生成 UTF-8 内容，由 Kindle 宿主 FBInk 使用现有 CJK 字体绘制，全部 15 行成功。
- 已等待刷新完成，读回并查看 600×800 framebuffer 截图，中文布局正确；未做光学测量。
- 原根挂载及启动参数一致，显示后的 chroot 挂载与 loop 清理检查通过。
- 设备已有 glibc 版 FBInk，未向 musl chroot 安装不兼容二进制或暴露 framebuffer。
- KOReader 触摸计数 / 归零 / 返回插件已安装，实际 Lua 语法检查通过；UI 操作尚未实测。
- `make display-demo`、`make app-install` 是手动设备操作；使用与限制见 [应用开发](../knowledge/apps.md)。

## KOReader 插件开发起点

- 新版 0.1.0：中文面板、计数/归零、设备信息和返回；使用 LuaSettings 按需保存计数。
- 状态逻辑独立于控件，可离线运行同一组 Lua 行为契约；菜单支持 Dispatcher 绑定。
- 本机 25 项测试通过，其中包含 27 条纯 Lua 行为断言；更新目录保留和失败回退已在临时文件夹验证。
- 新版安装器先验证 staging 文件再替换，记录源码哈希，不自动重启阅读器。
- 本轮部署在首次 SSH 预检失败，返回 No route to host；未上传新版、未启动预览。
- 原设备只有此前初版插件的语法验证记录，不能视作新版部署或实际触摸验证。
- 开发命令和验证边界见 [KOReader 插件](../knowledge/koreader-plugin.md)。

## USB 开发入口

- 已通过 USB 存储更新当前插件，三个 Lua 文件 SHA256 读回一致，原插件目录已保留。
- 对 USB staging 源码执行了本机 Lua 行为契约，回执记录 transport=usb-mass-storage。
- 当前 make app-status 的源码与安装回执匹配，真实预览回执仍缺失。
- 已安全卸载并弹出存储区；尚未出现 Kindle USB 网卡，尚未实际配置 USB IP/SSH。
- 工具按 Amazon 身份或已记录的 Kindle 物理端口识别，不选择另一块 USB 网络设备。
- 已保存的命令表包含 ;un / ;uns，原生 USB 网络切换行为仍待实测。
- 本机 30 项测试通过；没有安装第三方 USBNetwork 包或设备端启动服务。
- 后续步骤见 [USB 开发](../knowledge/usb.md)。

## USB 原生入口失败后的诊断路径

- 用户执行 ;un 后仍为存储模式，主机针对 Kindle 的 USB reset 和物理重新插拔均未产生网卡。
- 已确认 KOReader 设备实现和 wrapper 会暂停 volumd，无法把阅读器内的无响应当作驱动缺失。
- 已通过 USB 存储部署 0.2.0，包含只读诊断按钮和手动 USB 连接 helper，四个运行文件哈希读回一致。
- helper 预检 root / i.MX6SLL / 原根只读 / 现有密钥 / 无 Alpine 挂载 / 线缆已拔出。
- 只有预检通过才让阅读器正常退出，等待 volumd 后请求 USB 网络；不修改原生脚本或开机服务。
- 目标为 USB IP <HOST_02>:2223、仅公钥的独立 Dropbear；尚未执行，不能报告连通。
- 本机 32 项测试及 ShellCheck 通过；纯 Lua 35 条断言覆盖 USB 回调及失败不退出。
- 日志在用户区 koreader/settings/kindledev-usb.log；真实 USB/SSH 结果仍待设备端启动。

## 设备 USB 诊断结果

- 已读回用户通过插件导出的日志，确认插件 v0.2.0 成功打开，导出过程以 root 运行。
- 原生 /usr/local/bin/usbnetwork.sh 缺失；保存的 ;un 命令映射没有实际脚本可执行。
- 诊断时 volumd 处于 T（stopped），原系统根分区仍只读，无 usb0；当前加载存储 gadget。
- KDB 配置 modprobe g_ether，但模块能否加载、实际 USB 网络及专用 SSH 仍未验证。
- 已验证真实触摸路径“打开插件→导出诊断”；没有把这次观察扩展为所有按钮均已验证。
- 已安装四个运行文件与当前源码哈希匹配；独立用户区启动器无需改代码，可进入设备端尝试。

## 线缆状态检查修正已部署并实测

- 用户在 0.2.0 点击启用时遇到 USB cable-state path is unavailable，前置检查阻止了切换。
- 已定位旧实现只读取 KDB 路径；未得到该设备该键的实际返回值，不能把缺失路径当作未插线。
- 对应固件官方 BD71827 驱动提供 bd71827_ac (Mains) 的 ONLINE，来自 DCIN_DET；电池 ONLINE 不等价。
- 0.2.1 新实现接受有效旧字段并读取标准外部电源状态，任一接入信号优先，未知仍拒绝。
- 已添加缺失路径、满电插线、冲突信号与无状态来源等 10 项测试；总计 42 项测试和 ShellCheck 通过。
- 已在用户接回 USB 后部署 0.2.1，核对版本字段及四个运行文件的读回 SHA256；升级后已在设备启用 USB，并完成实际 root SSH 验证。

## USB SSH 和真实界面已验证

- 用户执行 0.2.1 启动器后出现 0525:a4a2 / cdc_ether，物理端口与原 Kindle 一致。
- 已为唯一匹配接口配置 <HOST_01>/24，kindle-usb 自动连接，不接管默认路由或 DNS。
- 服务端 ED25519 与既有 Kindle 身份匹配；严格主机密钥检查下，<HOST_02>:2223 公钥 root 登录成功。
- 实际 SSH_CONNECTION 的本机/对端地址确认走 USB；原根仍只读，安全启动参数保持。
- 默认 .device.toml 已改 USB，旧 Wi-Fi 配置保留在忽略的 work/device-wifi.toml。
- 设备 Lua 35 条行为断言、Alpine 原生/持久性/错误退出/互斥/挂载清理全部通过 USB 验证。
- 已通过 app-preview 启动真实 KOReader，读回 600×800 截图并查看，中文面板布局正常。
- 阅读器运行后 USB SSH 仍可用；自动启动和跨重启结果见下方。休眠长期稳定性尚未验证。
- 最新使用步骤和证据范围以 docs/usb.md 为准；本机原始日志、截图及回执均保存在忽略的 work/。

## USB 自动启动当前状态

- 用户区启动脚本复用 ENABLE_USBNET 和既有 KMC emergency.sh 入口；原根只读，启动链和系统服务文件保持。
- 首次重启的日志显示钩子已执行，但 volumd 随即卸载用户区，旧 worker 因标记消失退出。
- 新钩子把代码复制到 /var/tmp 后再后台执行；先通过原生 volumd/HAL 请求网络，等待用户区返回后启动 SSH。
- 用户区消失的回归用例已验证旧行为失败、新行为通过；51 项本机测试及 ShellCheck 通过。
- 第二次正常重启后网卡及主机连接已自动出现，但 TCP 2223 拒绝连接；SSH 自动启动尚未通过验证。
- 当前需通过 KOReader 手动恢复 SSH 后读取 /var/tmp/kindle-usb/boot.log，定位 SSH 的失败点。
- 本机日志及回执在忽略的 work/usb-autostart-*；使用与关闭命令见 docs/usb.md。

## 当前恢复优先

- 用户报告拔线后 USB 驱动器模式不退出；具体状态机原因尚未查明。
- 主机仍识别到 Kindle USB 网卡，但端口 2223 拒绝连接；此前 Wi-Fi 地址超时。
- 设备端回滚尚未完成。启用命令已在主机端禁止，防止继续部署或重启测试。
- 当前只推进恢复：保持不接 USB 重启，能进入 KOReader 后改名用户区 ENABLE_USBNET，再移除本工程 emergency.sh。
- 没有通过刷写、修改原根分区、IDME 或启动链进行恢复尝试。

## 用户已恢复正常界面

- 用户确认拔线重启后能进入正常界面，并已将 ENABLE_USBNET 改名、再次重启。
- 这停用了现有用户区启动钩子的条件；剩余 emergency.sh 待通过正常 USB 存储清理。
- 主机端启用命令继续拒绝运行，不恢复自动启动测试。
- 当前没有连接的 Kindle USB 设备；不能把用户确认扩展为已验证新的完整拔插周期。

## USB 自动启动入口清理已完成

- 接回 USB 后识别到原端口的 Amazon 存储设备，以及带 Kindle 卷标的 VFAT 用户区。
- 读回确认 ENABLE_USBNET 不存在，ENABLE_USBNET.off 保留。
- emergency.sh 的 SHA256 与该次部署回执一致；已保存主机副本后移除该钩子。
- 清理后再次确认活动标记和钩子均不存在，已同步并安全卸载 USB 卷。
- 回执 work/usb-autostart-rollback.json 记录实际清理结果；主机自动启用命令继续禁用。
- 用户已确认拔线重启能进入正常界面；清理后直接拔线的界面行为仍由用户观察。
