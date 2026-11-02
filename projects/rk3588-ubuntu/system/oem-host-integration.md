# oem-host 在镜像中的运行契约

> 本地 HMI 后端与浏览器以 kiosk 运行，采集以 board 运行；服务继承 systemd 的本机监听 socket，通过共享组和受限 sudo 访问设备状态。

## TL;DR

- 构建时从 GitLab `main` 最新提交生成 `overlay/opt/oem-host/` 快照，依赖装入系统 Python；`.git` 不进入镜像。
- `oem-host.socket` 监听 `127.0.0.1:8080`，`oem-host.service` 用 uvicorn `--fd 3` 接管。
- 后端只需 loopback；与 Chromium 并行启动，极简着陆页负责连接失败时重试。
- 后端与 socket 随 HDMI 会话启停，无屏时不监听；见 [session-design.md](kiosk/session-design.md)。
- HMI 数据归 kiosk，采集配置归 board；共享状态靠 board 组、setgid 与 `UMask=0007`。
- 配置背景集中在本文；源码刷新见 [oem-host-upstream-sync.md](oem-host-upstream-sync.md)。

## 镜像职责与目录

| 内容 | 镜像位置与约束 |
| --- | --- |
| 代码、素材、来源信息 | `/opt/oem-host/{app,media,requirements.txt,pyproject.toml,README.md,SOURCE}`，root:root、服务只读 |
| Python 依赖 | `requirements.txt` 钉定版本，系统 Python 安装；wheel 缓存 `build/pip-wheels/` |
| 运行配置 | `/etc/oem-host.env`，0600 root:root；构建期生成维护台口令哈希 |
| 服务与监听 | `/etc/systemd/system/oem-host.{service,socket}`，由 HDMI 检查单元按需启动 |
| 工位复位 | `/opt/oem-host/deploy/image-reset/`；固定 sudo 入口创建临时 systemd 服务，见下文 |
| 采集服务准备 | `/usr/local/lib/oem-host-collection-service/worker.py` 与配套 `.path/.service`；只启用请求监听器 |
| 状态与数据库 | `/var/lib/oem-host`，kiosk:kiosk |
| HMI 与整体采集日志 | `/var/log/oem_host_log/{host,collect}`，kiosk:kiosk |
| 采集报告与证据 | `example-project_COLLECTION_REPORT_DIR=/var/log/oem_host_log/collect/reports`，应用按需创建；复用采集日志的可写路径和 30 天清理策略 |
| 采集共享状态 | `/home/board/.lsp`，board 组可读写，目录 2775 setgid，`device.yaml` 0660 |
| ADB 授权 | board、kiosk 家目录各有一份相同密钥，目录 0700、私钥 0600 |
| 日志清理 | `/usr/lib/tmpfiles.d/oem-host.conf`，清理 30 天未使用的日志；验收日志排除 |

构建入口 `build.sh` 转调 `scripts/build-lsp-image.sh`，安装依赖、预编译 Python 字节码、
创建目录和权限，并检查 `rclpy` 仍可导入。升级依赖需重新运行上位机阶段。
上位机获取、依赖安装、字节码预编译与配置收尾都在 `rootfs` 阶段之后的独立 `oem-host` 阶段完成；
SSH 身份与提交变化触发重建的规则见 [源码同步](oem-host-upstream-sync.md)。

## 工位复位与数据盘挂载

镜像为 kiosk 放行以下唯一固定工位复位命令；sudo 参数边界见 [sudo 白名单](kiosk/sudo-policy.md)：

```sh
/usr/bin/python3 -I /opt/oem-host/deploy/image-reset/worker.py --request
```

新版 worker 校验固定请求，再通过 PID 1 创建 `oem-host-image-reset-<请求UUID>.service` 临时服务。
临时任务独立于后端 cgroup，完成后自动回收；不安装固定的重置 `.path/.service`。
SD 页面只检测已有挂载、ext4 格式和现有可写性，不提供上位机 SD 挂载 worker 或对应 sudo 授权。
镜像已有的数据卡自动挂载机制保持其独立契约，见 [数据卡](../storage/init-sdcard.md)。

`oem-host` 阶段从本次上游快照集成 `deploy/image-reset/`：

- worker、清单生成器和五条设备规则原件来自同一提交；规则须与工程 overlay 逐字节一致。
  `device.yaml` 重置原件由本镜像配置生成，同时更新原件 manifest，保留本镜像的版本日期。
- `expected-runtime.json` 由已安装的应用文件生成；九项 `expected-system.json` 由本镜像的实际系统配置生成。
  不复制上游来源设备的 `expected-*.json`；口令哈希等构建生成内容落定后再取摘要。
- `/opt/oem-host`、`deploy`、`image-reset` 及父目录为 root:root 0755；worker、原件与清单为 root:root 0644，
  无符号链接、无业务账户或组写权限。`-I` 不能替代安装权限保护。
- 复用 rootfs 时撤回旧 `/usr/local/lib/oem-host-{image-reset,sd-mount}`、固定四个旧单元及启用链接。

构建与 `verify-image.sh` 只验证代码、原件、权限和摘要集成，不执行请求或真实工位重置；
实际备份、恢复、CPU 回写和临时服务生存期仍需独立授权的板上验收。
来源及生成器行为依据为上游 `deploy/SYSTEM-COMPONENTS.md`、
`deploy/image-reset/{worker.py,build-system-manifests.py}`；实际提交以镜像 `/opt/oem-host/SOURCE` 为准。

## 采集服务准备

上位机在显式请求后通过独立 root worker 准备 `collect_system.service`。
构建从同一快照的 `deploy/collection-service/` 安装以下三个文件，权限均为 root:root 0644：

| 来源文件 | 镜像位置 |
| --- | --- |
| `worker.py` | `/usr/local/lib/oem-host-collection-service/worker.py` |
| `oem-host-collection-service.path` | `/etc/systemd/system/oem-host-collection-service.path` |
| `oem-host-collection-service.service` | `/etc/systemd/system/oem-host-collection-service.service` |

仅 enable `.path`，构建不启动 worker、不发起请求、不 enable/start 采集服务，也不新增 sudo 权限。
监听 `/var/lib/oem-host/collection-service-request.json` 后触发固定 worker；回执在
`/var/lib/oem-host-collection-service/status.json`，由 root 单元的 `StateDirectory` 创建。
worker 校验任务、时效、SD 检测成绩和维护/重置屏障，与重置共享互斥锁。
来源目录保留在 `/opt/oem-host/deploy/collection-service/`，镜像体检比较安装副本与同提交来源的摘要。

静态检查与安装成功只证明组件齐全；显式请求及成功回执、采集端控制接口须另行验证。
采集服务出厂仍 disable / stop，见 [采集自启门控](tuning/boot-network-gating.md)。

## socket、页面和网络

`oem-host.socket` 提前建立监听口，浏览器连接在内核队列中等待后端处理。
服务的 `Sockets=oem-host.socket` 显式绑定 fd 来源，保证开机、socket 激活和手动重启均可使用 `--fd 3`。
单独依赖同名 socket 的隐式激活，手动重启时可能拿不到正确 fd。

后端不排 `network.target` 或 `network-online.target`，前置命令 `-+/usr/bin/ip link set lo up`
负责拉起 loopback。外设走本地 USB，本地 HMI 无需等待 Wi-Fi、DHCP 或互联网。
`StartLimitIntervalSec=0` 配合 `Restart=always`、`RestartSec=2`，使后端持续尝试恢复。

监听地址由 socket 决定。调整地址或端口时须同步核对：

- `oem-host.socket` 的 `ListenStream` 和 `/etc/oem-host.env` 的 bind 配置；
- `/etc/default/lsp-kiosk` 的 `LSP_KIOSK_URL`；
- `/usr/share/lsp-kiosk/10-lsp-kiosk.json` 的 `URLAllowlist`。

`lsp-kiosk.service` 的 `Wants/After` 只拉起和排序后端进程，不等待 HTTP 就绪。
Chromium 先打开本地极简着陆页，连接失败时重试，收到响应头后再导航到 HMI。
HEAD 探测不证明主页返回 HTTP 200 或业务可用；流程与失败边界见
[kiosk/session-design.md](kiosk/session-design.md)。

## 账户与写入权限

服务为 `User=kiosk`、`Group=kiosk`，补充组 `video audio plugdev board`。
`example-project_DEVICE_ACCOUNT=board`、`example-project_PERIPHERAL_HOME=/home/board` 保持采集侧路径一致。

HMI 原子更新 `device.yaml` 会创建临时文件并 rename，文件权限需要重新建立：
setgid 目录保证新文件属于 board 组，`UMask=0007` 保证文件有组内读写权限。
只给原文件 chmod，不能保证下一次原子替换后仍可共享。

`ProtectSystem=strict` 将服务的文件系统视图设为只读，sudo 提权也受同一 mount namespace 限制。
`ReadWritePaths` 保留以下写入通道：

| 路径 | 用途与边界 |
| --- | --- |
| `/var/lib/oem-host`、`/var/log/oem_host_log/{host,collect}` | 数据库、设置、规则备份、业务日志 |
| `/etc/udev/rules.d` | 受限 `lsp-set-rule` 写设备绑定 |
| `/var/log/lsp-maintenance.log` | 维护审计；须预置，避免轮转后 namespace 仍绑定旧 inode |
| `/run/sudo` | sudo 时间戳；免密调用也可能访问此目录 |
| `/run/lsp-kiosk` | 本次开机的 HDMI / 维护标记，维护入口需通过受限 sudo 写入 |
| `/home/kiosk/.android` | kiosk 的 ADB 状态 |
| `/home/board/.lsp` | 设备配置与外设脚本日志 |
| `/run/lsp_data_1T` | SD 真实挂载点及挂载根目录写入探针 |

`/mnt/data_1T` 在 SD 成功挂载后才发布为链接；服务放行稳定的真实挂载点，
挂载/撤回机制见 [init-sdcard.md](../storage/init-sdcard.md)。
HMI 内的数据盘单步测试检查挂载状态；分区、格式化、卸载须在主机 namespace 中执行，
不能直接从这个私有 namespace 操作已挂载的数据卡。

服务保留 setuid 通道以执行固定 sudo 白名单入口，不能设置 `NoNewPrivileges=true`。
授权由 `99-lsp-kiosk`、`99-lsp-maintenance` 控制，见 [sudo-policy.md](kiosk/sudo-policy.md)。
默认 `KillMode=control-group`：重启 HMI 会同时终止它启动的外设子进程。

## 环境配置的关键约束

`EnvironmentFile` 先读兼容文件 `/etc/quest-station.env`，后读 `/etc/oem-host.env`；
同名变量以后读者为准。修改配置后重启 `oem-host.service`。

- 素材通过 `/static/media/<文件名>` 提供，默认目录为 `/opt/oem-host/media`，随代码快照进入镜像。
- 相机左右端口从配置允许列表中选择，胸前相机使用其独立列表；额外 VID:PID 可逗号分隔或留空。
  唯一 SN 的要求由 `LINGHOU_CAMERA_REQUIRE_UNIQUE_SN` 决定。
- 相机帧率读取 `LINGHOU_CAMERA_STATUS_TOPIC_PREFIX` 对应的三路状态话题，避免另开 V4L2 抢设备。
- ROS setup、domain 与 discovery range 必须与 `collect_system.service` 一致，否则 HMI 探活看不到节点。
- Quest 单步测试使用不重启头显的启动脚本；文件名中的 `roboot` 按部署树原拼写保留。
- 生产模式设备绑定走 `lsp-set-rule`：备份、写入、重载、等待节点、回读校验，失败还原。
  环境中的原始 install/udevadm 命令属于兼容配置，不能据此扩大 sudo 白名单。
- 部署脚本路径指向运行产物树；MQTT 可选卸载脚本留空时，重置流程要求人工回滚。
- 手套固件目录供维护台读取版本；外设自动启动与各阶段 timeout 按环境配置执行。
- APK 包名须与安装脚本参数一致，用于验证安装结果。
- HMI 维护台哈希由 `LSP_OEM_HOST_ADMIN_PASSWORD` 构建变量生成；系统维护入口单独校验
  `/etc/lsp-maintenance.passwd`，调用契约见 [maintenance-mode.md](kiosk/maintenance-mode.md)。

## 采集自启与收尾

三个采集单元构建时显式禁用，首次启动不运行；重跑 `rootfs` 阶段也会清理旧启用链接。
账户与更新器适配见 [系统集成总账](system-integration.md#采集更新器的入口与运行时)。现场收尾创建 `boot-enabled`，启用协调器
`data-collection-boot.service`；协调器脚本是 `/usr/local/sbin/boot-data-collection-update`。
`boot-enabled` 与每台设备的 `device-id` 均不在镜像中预置。

kiosk 的采集服务授权仅含 `collect_system.service` 的固定 enable/start 和单独 stop 命令；
`install_boot_update.sh --enable`、`device_reporter` 及三服务组合 stop 未放行，完整边界见
[sudo-policy.md](kiosk/sudo-policy.md)。v1.1.9 中 `install_boot_update.sh` 会用 release 内的
`config/*.service.bak` 和 `scripts/boot_update.py` 覆盖系统单元与协调器；
本地 overlay 的 v1.1.9 已临时将这些副本、配置示例和安装/数据卡脚本的账户及家目录改为 `board`，
用于新镜像试验；这不是重新发布的签名包。采集模板仍保留 `network.target` 排序，
上报模板仍无 `nm-online` 预等待；执行安装会覆盖工程主单元中的网络策略。

`example-project_OEM_FINALIZE_USE_IMAGE_SCRIPTS` 选择收尾路径：
启用时依次调用镜像内 `install_boot_update.sh --enable` 和
`install_rollout_agent.sh --device-id <编号>`，属于一次性 provisioning；
须先停止三个采集单元，设备配置的 hardware_id 与目标编号须一致，已固定的 ID 不能更换。
wrapper 路径所需的 `enable-collection-autostart` 及授权由现场部署提供。

## 验证与依据

```sh
systemctl show oem-host.service -p User -p SupplementaryGroups -p UMask -p Sockets
systemctl status oem-host.socket oem-host.service --no-pager
curl -fsS -o /dev/null -w '%{http_code}\n' http://127.0.0.1:8080/
journalctl -b -u oem-host
```

运行配置以 `overlay/etc/systemd/system/oem-host.{service,socket}`、`overlay/etc/oem-host.env`
及构建脚本为准；`tools/verify-image.sh` 核对 fd、账户、权限、目录、依赖及策略的配套关系。
日志清理用 `systemd-tmpfiles --clean --dry-run /usr/lib/tmpfiles.d/oem-host.conf` 预演；
`station-acceptance.log` 长期追加且排除清理，维护审计文件独立保留。
