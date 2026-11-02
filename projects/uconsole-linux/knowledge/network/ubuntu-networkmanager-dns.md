# Ubuntu Base：能连接 Wi-Fi，但域名无法解析

> 未安装 systemd-resolved 的 Ubuntu Base 应让 NetworkManager 使用实际 DHCP DNS。

## TL;DR

- 用本地 `dns=default` 覆盖发行版 `dns=systemd-resolved`。
- 核对 `/etc/resolv.conf` 目标与 `/run/NetworkManager/resolv.conf`，再测试域名解析。
- Wi-Fi/SSH 可达不等于 DNS 正常；不要硬编码公共 DNS 掩盖配置错误。

## 配置方法与原因

Ubuntu 构建器（工程源码：`scripts/image_backends/ubuntu_base.py`）生成
`/etc/NetworkManager/conf.d/90-uconsole-dns.conf`：

```ini
[main]
dns=default
```

保留发行版文件及现有 resolv.conf 链接，不固定路由器 IP，也不添加额外解析服务。
NM 1.46 的 `man/NetworkManager.conf.xml` 规定：本地 conf.d 优先于发行版目录，
`dns=default` 使用连接 DNS，且 NM 始终生成运行时 resolv.conf；`rc-manager=auto`
在没有外部解析管理器时回退为 symlink。因此本方案无须额外覆盖 rc-manager。

回归夹具（工程源码：`tests/test_ubuntu_base.py`）模拟发行版强制选择 resolved，
验证实际生成的本地配置覆盖它，并保留发行版原字节。这验证构建配置约定；
真实 DNS 查询仍须在设备上检查。

## 故障观察（2026-10-24）

2026-10-24，CM4 的原版 `network-manager 1.46.0-1ubuntu2.8`：

- Wi-Fi 已连接，SSH、默认路由和网关 ping 正常；DHCP 下发了 DNS。
- `/usr/lib/NetworkManager/conf.d/10-dns-resolved.conf` 强制使用
  systemd-resolved，但该服务没有安装，也没有进程监听本地 53 端口。
- `/etc/resolv.conf` 链接到 `../run/NetworkManager/resolv.conf`，内容却是
  `nameserver 127.0.0.53`；`no-stub-resolv.conf` 中才是 DHCP 下发的地址。
- `getent ahostsv4 archive.ubuntu.com` 返回 2。故障在 NetworkManager
  启动耗时试验前已存在，不能归因于后续候选程序。

## 修复后的实测证据与限制

在发行版原版 NM 上安装同一配置并执行 `nmcli general reload` 后，运行时
resolv.conf 改用 DHCP 的 DNS，域名查询返回 0。正常重启
`00000000-0000-4000-8000-000000000022` 后再次查询成功；Wi-Fi、路由和桌面恢复，
无失败 unit，固件保护标志为零。配置持久化已验证，导出的 SD 镜像尚未重新生成。

设备原配置观察保存在 `/root/uconsole-dns-20261024/`，开发机证据为
`~/.cache/uconsole-desktop-20261024/boot-resume/dns-fixed-reboot-health.json`。
这验证当前网络下的解析链路，不能代替不同 DHCP、IPv6 或 VPN 场景的检查。
