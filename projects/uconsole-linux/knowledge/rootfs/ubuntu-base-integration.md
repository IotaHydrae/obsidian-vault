# Ubuntu Base 接入与内核兼容性

> Ubuntu Base 后端原生构建 ARM64 根文件系统，内核与 initramfs 兼容性仍独立校验。

## TL;DR

- 使用独立 TOML、私有挂载命名空间与普通文件导出，不把 pi-gen 配置混入此后端。
- 固定历史内核时显式提供报告及 SHA-256，不能放松默认严格产物校验。
- 固件解压、initramfs hook 与包版本均需核对；构建成功不代表实机已验证。

适用于本工程 Ubuntu Base 24.04.5 ARM64 后端；操作入口见 构建说明（工程源码：`image/ubuntu-base/README.md`）。

## 接入与验证方法

1. 显式选择板型、Ubuntu Base 配置档和其独立 TOML，先校验账户、固定基础包与内核输入。
2. 根文件系统组装在私有挂载命名空间中执行；TLS/签名验证和软件包原有许可证保持有效。
3. 核对启动固件、无线固件完整链接链和内核加载能力，再生成所选 release 的 initramfs。
4. 导出普通 `.img` 后离线检查分区引用、产物字节、账户和服务；刷写与实机验收另行进行。

## 后端职责与源码依据

**源码依据：** 原生构建器（工程源码：`scripts/image_backends/ubuntu_base.py`） 使用独立 TOML 配置，
在私有挂载命名空间中组装 ARM64 chroot，不依赖 pi-gen。镜像导出器使用普通文件生成 FAT32、ext4
和 MBR；构建、镜像检查、设备刷写是分开的操作。

复用结论：根文件系统后端负责发行版与账户、软件包、服务的集成；板型和内核选择保持显式。
Ubuntu Desktop、XFCE、KDE 的安装与会话策略仍待实现。

网络与 DNS 由 NetworkManager 管理。Ubuntu Base 未安装 systemd-resolved，
构建器显式覆盖发行版 DNS 默认值；定位方法和配置依据见
[DNS 所有权](../network/ubuntu-networkmanager-dns.md)。

## HTTPS 软件源的 CA 引导

**源码依据：** Ubuntu Base 未预装 `ca-certificates`。选择 HTTPS APT 源时，构建器先复制构建机已有的
CA bundle 到 chroot，再安装经 Ubuntu 仓库签名校验的 `ca-certificates`，由该软件包生成发行版证书集合。
APT 的 `Signed-By` 始终使用 Ubuntu archive keyring。

复用结论：首次安装 CA 包之前也需要 HTTPS 信任链；可用宿主机证书引导，无须关闭 TLS 或 APT 签名校验。
固定输入（工程源码：`image/ubuntu-base/recipe.json`）中的基础包 SHA-256 已与 Canonical 签名清单核对，
日常构建校验固定哈希；APT 仓库的软件包仍可能更新，不能据此声称整个镜像逐字节可复现。

## 启动固件与压缩无线固件

**源码依据：** `linux-firmware-raspi` 的启动文件位于 `/usr/lib/linux-firmware-raspi/`，
构建器检查 `start4.elf`、`fixup4.dat` 并复制到 `/boot/firmware/`。
无线固件位于 `/usr/lib/firmware/`，其中包含 `.zst` 文件。

保留的 `6.18.57-v8-uconsole` 内核未启用压缩固件加载支持。构建器保留软件包原有压缩文件，
同时生成未压缩文件及对应的符号链接，使内核请求的文件名能够解析到可加载字节；许可证文件随包保留。
CM4 的 `brcmfmac43455-sdio` 链还经过 `/etc/alternatives`，其中包含绝对路径；必须按目标根文件系统
解析完整链，再链接到选中的未压缩文件。只去掉第一层链接末尾的 `.zst` 会产生断链。

复用结论：使用发行版固件包时，还要核对所选内核的固件加载能力、文件名和链接目标。
软件包安装成功本身不能证明 Wi-Fi 可用。

## 显式固定历史内核与当前 initramfs 集成

**源码依据：** 历史内核校验器（工程源码：`tools/retained_kernel.py`）要求同时提供 `kernel_report` 和
`kernel_report_sha256`，只接受 `kernel/build/` 内指定报告；检查报告哈希、板型、上游提交、补丁序列、
受限路径及保存的配置、Image 和内核包字节。

历史报告保留当时的补丁与配置哈希，不表示它与当前源码相同。
默认的 `tools/kernel_artifact.py` 严格校验流程仍要求产物与当前输入匹配。
这样可以固定已知内核试验新用户空间，并保留输入来源，而不把历史产物冒充为当前构建结果。

Ubuntu 后端先解包内核包，再安装当前 initramfs 集成 hook（工程源码：`scripts/kernel_backends/raspberry_pi.py`），
随后配置软件包并生成匹配的 initramfs。内核驱动字节沿用所选包，根文件系统集成 hook 则有意更新；
构建回执分别记录原内核包哈希、历史输入标志和 `initramfs_hook_sha256`。
镜像始终保留 initramfs，不安装 pi-gen 的 CM4 direct-root 定时策略。

## Noble 的可选 pam_lastlog 日志

**根因依据：** Noble 的 `greetd 0.9.0-4ubuntu1` 包含 `@include login`；
发行版 `login 1:4.13+dfsg1-4ubuntu3.2` 的会话栈仍声明
`session optional pam_lastlog.so`，但 `libpam-modules 1.5.3-5ubuntu5.7`
未提供该模块。原配置与 dpkg conffile 校验值一致，缺失警告并非工程误删模块。

Linux-PAM 1.5.3 的 `configure.ac` 默认关闭 lastlog，Ubuntu 的构建参数未启用它；
同版本 [pam.conf 语法](https://github.com/linux-pam/linux-pam/blob/v1.5.3/doc/man/pam.conf-syntax.xml)
规定：类型前加 `-`，可以抑制模块不存在时的加载日志，控制语义不变。
因此镜像后端在全部软件包安装完成后，只把这一条无附加参数的 optional 规则改为：

```text
-session optional pam_lastlog.so
```

若模块存在或 login 配置不存在则不改；自定义参数、注释、required 规则和认证规则均保持原样。
文件权限、换行和空白保留，重复执行不再改写。此策略属于 Ubuntu 后端，不放入共享桌面载荷。
模块缺失并不需要安装旧版 PAM，也不应通过重写整个登录栈来消除警告。

**CM4 实机观察（2026-10-24）：** 独立临时 PAM 服务以同一可选 lastlog 规则和
`pam_permit.so` 验证 libpam 行为。修改前后 `pam_start`、`pam_open_session`、
`pam_close_session`、`pam_end` 均返回 0；原规则记录两条缺失/无效模块日志，
`-session` 规则没有这些日志。探针没有调用认证接口，临时服务与可执行文件已删除。
实机 `/etc/pam.d/login` 已通过同一构建器函数更新，差异仅为一个 `-`，
权限仍为 root:root 0644，第二次执行未修改内容或 mtime。原文件备份在设备
`/root/uconsole-pam-lastlog-20261024/login`。
随后经正常 greetd 初始会话入口，在 19:47:24 打开 Waybar 会话 122，19:54:59
打开 Noctalia 会话 147。两次均有 `pam_unix(greetd:session)` 打开 UID 1000 的记录，
该区间没有 lastlog 缺失日志。这补足真实桌面会话栈的验证，不代表新的交互式口令认证或冷启动。

非发布证据：`~/.cache/uconsole-desktop-20261024/functional/pam-lastlog-runtime-result.json`，
以及同目录的 `pam-lastlog-*` 软件包来源、构建规则和上游文档摘录。
真实会话日志在 `functional/waybar/final-restored-health.log`。

## 历史验证证据与适用边界

- **离线验证（2026-10-18）：** `.img` 导出及只读镜像检查通过；所选内核、initramfs、分区引用和预制账户符合约定。该轮 212 项主机测试通过，不作为当前测试数量。
- **固件内容检查：** 全部 74 个压缩固件路径对应的未压缩文件与所选源字节匹配，CM4 的板型及通用 Wi-Fi 别名均可解析。此结果不代表无线硬件已工作。
- **CM4 初始上板观察：** 用户确认 Ubuntu 启动并显示文本控制台；SSH 确认 Wi-Fi、首次扩容、machine-id 和 SSH 主机密钥生成，未报告失败的 systemd 服务。
- **uConsole CM4 控制台方向：** 初始 `fbcon=rotate:3` 横屏上下颠倒，改为 `1` 后用户确认方向正常；后续默认频率恢复启动的命令行和 `fbcon/rotate` 均为 `1`。生成器已修正；旧镜像是否包含修正须按其构建回执确认，不能套用于 CM5 IO Board。
- **后续验证入口：** CM4 后续[启动测量](../boot/cm4-boot-time.md)与[桌面/外设结果](../desktop/labwc-shell-comparison.md)独立维护；初始文本控制台观察不能代表其全部状态。CM5 各板型的真实构建与上板状态分别见[开发状态](../../project/status.md)，不能由 CM4 结果推定。

后续检查与设备结果以[开发状态](../../project/status.md)为准。
