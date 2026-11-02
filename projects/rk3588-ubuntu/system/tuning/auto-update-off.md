# 关闭自动更新检测（构建期开关）

> 嵌入式设备不需要每天联网检查/安装更新：构建脚本默认 `DISABLE_AUTO_UPDATE=Y`（mask 掉所有
> apt 自动更新相关的 timer/service）并 `PURGE_SNAP=Y` 移除 snapd。**只关"自动"**，手动
> `apt-get update && apt-get upgrade` 不受影响。

## TL;DR

- 默认已关：`DISABLE_AUTO_UPDATE=Y` + `PURGE_SNAP=Y`；`=N` 分别恢复 Ubuntu 默认 / 保留 snapd。
- 手段是四层：`systemctl --root= mask` 各 timer/service、写死 `Periodic` 标志（双保险）、
  删 `update-motd.d` 里的更新提示脚本、用 `Hidden=true` 屏蔽桌面 autostart，
  再用 `/etc/systemd/user/` 指向 `/dev/null` 的符号链接屏蔽 **user** 单元
  （`systemctl --root=` 只管 system 单元）。
- ⛔ `update-notifier` / `update-manager` 是 `ubuntu-desktop` 的**硬依赖**，不能删，
  只能 `Hidden=true` + user 单元屏蔽。
- `Acquire::Languages "none"`：不要 apt 翻译索引（设备上占 61 MB、每次 update 多下 ~30 MB）。

## 具体做法

Ubuntu 默认会**每天联网检查并自动安装更新**，嵌入式/机器人设备一般不需要。构建脚本默认关闭
（`DISABLE_AUTO_UPDATE=Y`），并顺带移除 snapd 等（`PURGE_SNAP=Y`）：

```bash
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2                       # 默认：关闭 + 移除 snapd
sudo DISABLE_AUTO_UPDATE=N ./scripts/build-lsp-image.sh --board=lubancat-5-v2 # 保持 Ubuntu 默认行为
sudo PURGE_SNAP=N ./scripts/build-lsp-image.sh --board=lubancat-5-v2          # 关检测但保留 snapd
```

具体做法：

| 机制 | 处理 |
| --- | --- |
| `apt-daily.timer`（每天 6:00/18:00 联网 `apt-get update`）| `systemctl --root= mask` |
| `apt-daily-upgrade.timer` + `unattended-upgrades.service`（自动装更新）| mask |
| `update-notifier-download/motd.timer`（桌面"有更新"提示、包信息下载）| mask + autostart 加 `Hidden=true` |
| `motd-news.timer`、`ua-timer.timer`、`apt-news.service`、`esm-cache.service` | mask |
| `fwupd-refresh.timer/.service`（联网刷新固件元数据）| mask |
| `/etc/apt/apt.conf.d/20auto-upgrades` 与 `10periodic` 的 Periodic 标志 | 写入 `"0"`（`apt.systemd.daily` 会读，双保险）|
| `/etc/apt/apt.conf.d/99-lsp-no-translations` | `Acquire::Languages "none";` —— 不要 apt 翻译索引（设备上占 61 MB、每次 update 多下 ~30 MB/多花 ~20 秒），见 [apt-sources-deb822.md](../../build/apt-sources-deb822.md) §5.3 |
| `/etc/update-motd.d/90-updates-available`、`98-reboot-required` | 删除 |
| `/etc/xdg/autostart/` 里的 `update-notifier.desktop`、`ubuntu-advantage-notification.desktop`（Ubuntu Pro 提示）、`ubuntu-report-on-upgrade.desktop`、`gnome-software-service.desktop` | 追加 `Hidden=true`（登录时不再拉起）|
| `update-notifier-release/crash/livepatch.path\|.service`、`ubuntu-report.path\|.service`（**user** 单元）| 在 `/etc/systemd/user/` 放指向 `/dev/null` 的 symlink（`systemctl --root=` 只管 system 单元）|
| `/etc/update-manager/release-upgrades` 与 `release-upgrade-available` | `Prompt=never` + 删除状态文件 |
| `snapd` / `snap-store` / `gnome-software` / `packagekit` | `apt-get purge`（`PURGE_SNAP=N` 可保留）|

> 为什么 `update-notifier` / `update-manager` 不直接删：它们是 `ubuntu-desktop` 的**硬依赖**
> （noble 官方依赖关系），删了会连带移除桌面元包，所以用 `Hidden=true` + user 单元屏蔽让它不启动。
> `packagekit` 同理可能被保护网跳过（`ubuntu-desktop` 依赖 `gstreamer1.0-packagekit`），它只提供
> D-Bus 服务、不弹窗，保留无妨。

> **只关「自动」**：手动 `apt-get update && apt-get upgrade` 完全不受影响。
> purge 前会先 `apt-get -s purge` 模拟，确认不会连带移除 `ubuntu-desktop` 元包，否则跳过并提示。
