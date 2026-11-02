# kiosk 的 sudo 白名单与授权边界

> kiosk 不属于 sudo 组；免密提权仅允许固定入口和明确参数，运行时以 `sudo -l -U kiosk` 的结果核对。

## TL;DR

- 白名单在 `99-lsp-kiosk`、`99-lsp-maintenance`，镜像安装为 0440 并执行 visudo 校验。
- 关机、设备绑定、数据卡初始化、指定业务单元控制和维护模式各有固定入口。
- 参数通配符会跨空格和路径分隔符匹配，不能用它限制 unit 列表或目标路径。
- 生产规则写入走 `lsp-set-rule`，不授权原始 install/udevadm。
- 维护模式 on 还要验证脚本自己的口令；passwd 不向 kiosk 授权。

## 当前授权

| 文件 | 允许命令 |
| --- | --- |
| `99-lsp-kiosk` | `systemctl poweroff/reboot` 与 `/usr/sbin/poweroff`、`reboot` |
| 同上 | `/usr/local/sbin/lsp-init-sdcard` |
| 同上 | `/usr/local/sbin/lsp-set-rule` |
| 同上 | `/usr/bin/python3 -I /opt/oem-host/deploy/image-reset/worker.py --request`，仅固定完整参数 |
| 同上 | `/usr/bin/systemctl enable collect_system.service` 和 `/usr/bin/systemctl start collect_system.service`，仅这两个完整参数组合 |
| 同上 | `/usr/bin/systemctl stop collect_system` 和 `/usr/bin/systemctl stop collect_system.service`，仅单个采集单元 |
| `99-lsp-maintenance` | `/usr/local/sbin/lsp-maintenance on/off/status` 与无参数状态查询 |

kiosk 具有会话和设备所需组权限，构建期明确移出 sudo 组。
加入 sudo 组会叠加发行版 `%sudo ALL=(ALL:ALL) ALL`，因此不能仅检查一个白名单文件。
`NOPASSWD` 只是 sudo 身份认证策略，固定入口仍须自己校验允许的子命令、设备标识和输入值。
`lsp-init-sdcard`、`lsp-set-rule` 在 sudoers 中未指定参数，表示允许传参，约束由工具自身实现；
不能把这种写法理解为“仅无参数”。`systemctl` 与 Python worker 则逐条匹配完整参数。

采集服务授权不包含 `--now`、restart、disable 或额外 unit；enable 只设置下次开机自启，
start 只启动当前服务。镜像构建仍默认 disabled/stopped，授权本身不会启用或启动服务。
现场显式 enable 会让采集服务直接随 multi-user.target 启动，绕过更新器的启动门控；
默认的更新器管理路径仍由 `data-collection-boot.service` 控制。
`device_reporter`、`data-collection-boot` 的单独/组合 stop、数据盘 worker 和
`install_boot_update.sh --enable` 均不在白名单中。

`lsp-init-sdcard` 是管理员/SSH 的数据卡工具；HMI 私有 mount namespace 中的数据盘步骤
只检查挂载，具体限制见 [oem-host-integration.md](../oem-host-integration.md)。
其现有 sudo 授权允许指定非当前系统盘、owner 和挂载点，不能视为仅默认 Data1T 操作。
初始化器与镜像默认 ext4 条目均使用 `nosuid,nodev`，限制外部介质的特殊权限与设备节点，
同时保留已有文件模式与普通读写/执行；存量挂载的迁移边界见
[init-sdcard.md §4.1](../../storage/init-sdcard.md#41-镜像内置自动挂载与热插拔)。
设备绑定使用 [lsp-set-rule.md](../lsp-set-rule.md) 的模板化入口；规则目录无需对 kiosk 开放普通写权限。
维护模式见 [maintenance-mode.md](maintenance-mode.md)，on 从 stdin 接收口令并校验，未配口令时拒绝进入。

## 采集账户的免密关机

`collect_system.service` 以 `board` 运行，关机调用为 `sudo -n /sbin/poweroff`。
`/etc/sudoers.d/99-data-collection-poweroff` 单独放行：

```sudoers
board ALL=(root) NOPASSWD: /usr/sbin/poweroff ""
```

末尾 `""` 表示只允许无参数调用；镜像的 `/sbin` 链接到 `/usr/sbin`，两条 poweroff 路径均匹配。
`board` 原有 sudo 组权限仍要求密码；这条规则不为带参数关机、重启或其他命令提供免密授权。
构建安装为 root:root 0440 并用 visudo 校验，镜像体检核对内容、属主和权限。
缺少此规则时，LCD 关机流程会报 `POWEROFF_FAILED` 和 `sudo: a password is required`。

只读验证用 `sudo -n -ll -U board -- /sbin/poweroff`，应显示 `Options: !authenticate`；
带 `--force` 或换成 `/usr/bin/systemctl poweroff` 时不应显示该选项。这里只查询授权，不执行关机。

## 为什么要固定入口和参数

sudoers 的参数匹配在整串命令行上进行，`*` 可以匹配空格与 `/`。
例如对 `systemctl stop collect_system*` 的授权会同时匹配额外 unit，
因此本工程把允许的单个采集单元写成完整参数，禁止附加或替换服务。

授权通用 install 写任意规则文件，配合 udev RUN 和 trigger，可执行任意 root 命令；
也可将敏感 root 文件复制成公开可读文件。命令名固定并不能收紧这些行为。
授权来自受限 wrapper 对参数和规则模板的校验，不能用 sudoers 的 glob 替代路径或内容校验。

关机命令的 systemctl 与符号链接两种调用路径均明确列出，
本工程在 sudo 1.9.15p5 上验证仅列一种路径不会自动放行另一种。
后续扩大授权前须按实际调用路径重新核对。

`oem-host.service` 的 `ProtectSystem=strict` 仍限制提权子进程的写入视图。
sudo 白名单与 ReadWritePaths 必须配套；只有其中一项不足以完成操作，见 HMI 运行契约。

## 只读核对与验证依据

```sh
id -nG kiosk
sudo visudo -cf /etc/sudoers.d/99-lsp-kiosk
sudo visudo -cf /etc/sudoers.d/99-lsp-maintenance
sudo -l -U kiosk
sudo -l -U kiosk /usr/local/sbin/lsp-set-rule list
sudo -l -U kiosk /opt/data_collection/current/share/data_collection_trigger/scripts/install_boot_update.sh --enable
sudo -l -U kiosk /usr/bin/python3 -I /opt/oem-host/deploy/image-reset/worker.py --request
sudo -l -U kiosk /usr/bin/python3 -I /opt/oem-host/deploy/sd-mount/worker.py
sudo -l -U kiosk /usr/bin/systemctl enable collect_system.service
sudo -l -U kiosk /usr/bin/systemctl start collect_system.service
sudo -l -U kiosk /usr/bin/systemctl stop collect_system.service
sudo -l -U kiosk /usr/bin/systemctl enable --now collect_system.service
sudo -l -U kiosk /usr/bin/systemctl start collect_system.service ssh.service
sudo -l -U kiosk /usr/bin/systemctl stop device_reporter.service
sudo -l -U kiosk /usr/bin/systemctl stop collect_system device_reporter data-collection-boot
```

上述 `sudo -l` 只查询授权，不执行命令。展开结果不应包含任意 root 权限；
复位固定入口与采集单元 enable/start/stop 应允许；更新器安装入口、数据盘 worker、
其它单元的 stop、组合 stop、`--now` 和附加 ssh 的启动命令应拒绝。
复位 worker 授权保留 `-I` 隔离解释器环境，拒绝额外参数、其他脚本及无参数复位入口；
worker 和父目录必须 root 所有且 kiosk 不可写，入口仍检查请求。
worker 由上位机代码交付；当前仅验证授权，新路径的集成状态见 [上位机契约](../oem-host-integration.md#工位复位与数据盘挂载)。
`tools/verify-image.sh` 检查账号组、密码锁定、白名单权限和 live 授权结果。
`tests/oem-worker-sudo.py` 在隔离 rootfs 中用真实 sudo 查询采集单元和复位 worker 授权，
同时验证额外操作、额外 unit、通配 unit 和替换脚本均被拒绝，不执行任何被查询命令。
