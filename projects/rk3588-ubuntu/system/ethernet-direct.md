# RK3588 双网口的固定 IP 直连与 DHCP 角色

> 产品默认由 RK3588 提供 DHCP，电脑自动获取地址后通过 `<HOST_13>` 访问板子；开发板接上游共享网络时，显式改为 DHCP 客户端，不自动识别或切换角色。

## TL;DR

- 电脑网口设为自动获取 IPv4，直连任一板载网口即可；板端地址为 `<HOST_13>/24`。
- 板端分配 `<HOST_14>–199`，租期 1 小时，不下发网关或 DNS，不向电脑共享上网。
- 直连配置只有一个活动实例；两口任选一个，不能同时给两台独立电脑提供直连地址。
- 接路由器、公司局域网或笔记本共享交换机前，先[切为客户端](laptop-ethernet-sharing.md#开发板切为-dhcp-客户端)。客户端取址失败会继续重试，不改当服务器。

## 接线与访问

电脑关闭该网口的 Internet 共享或 DHCP 服务，设为“自动获取 IP / DNS”，再连接板子。
电脑不需要安装 DHCP 服务或手动设置静态 IP；等待链路协商及客户端取址完成后：

```bash
ping <HOST_13>
ssh board@<HOST_13>
```

固定网段应与电脑 Wi-Fi / VPN、板子其他上联网段分开；如有重叠，需统一修改固定地址与租约范围。
板子其他接口可以自行联网，但直连功能不启用 DNS、NAT、桥接或全局转发，保留电脑原有上网路径。
固定地址不改变服务监听范围；例如 `oem-host` 仍只监听板端 `127.0.0.1:8080`。

```mermaid
flowchart LR
    A[产品默认：板子提供 DHCP] --> B[电脑自动取址并访问 <HOST_13>]
    C[开发板接上游共享网络] --> D[手动停用服务端并启用客户端]
    D --> E[由上游分配地址；失败继续重试客户端]
```

两口不桥接；两台电脑同时作为 DHCP 客户端接入时，只有先激活直连配置的网口提供地址。
如需两台电脑同时独立直连，须另行设计每口网段与固定地址，当前单实例方案不提供此能力。

## 配置与安装契约

三个 profile 按 udev `ID_PATH` 匹配，不依赖 `eth0/eth1` 枚举顺序或固定 MAC：

| profile / UUID | 匹配路径 | 镜像默认 |
| --- | --- | --- |
| `lsp-ethernet-direct` / `00000000-0000-4000-8000-000000000001` | 两个 GMAC | 自动连接；`multi-connect=1`，固定地址服务端 |
| `lsp-ethernet-dhcp-gmac0` / `00000000-0000-4000-8000-000000000003` | `platform-fe1b0000.ethernet` | 不自动连接；启用后无限重试 |
| `lsp-ethernet-dhcp-gmac1` / `00000000-0000-4000-8000-000000000002` | `platform-fe1c0000.ethernet` | 不自动连接；启用后无限重试 |

客户端 `autoconnect-retries=0` 表示无限重试；`dhcp-timeout=15` 仅是单次事务预算，不会触发角色回退。
客户端 / 直连的自动连接优先级仍为 `-100` / `-999`；角色由显式启用状态决定。
已有更高优先级的用户连接可能覆盖工程默认；维护时先列出并确认用途，不批量删除连接。

| 文件 | 安装与职责 |
| --- | --- |
| `/etc/NetworkManager/system-connections/lsp-ethernet-*.nmconnection` | 镜像安装三个 profile，`0600 root:root` |
| `/etc/NetworkManager/dispatcher.d/90-lsp-ethernet-direct` | `0755 root:root`；依据当前活动连接协调 DHCP，处理延迟事件 |
| `/usr/local/sbin/lsp-ethernet-direct` | `0755 root:root`；启动前复核活动 UUID 和固定地址，只管理 DHCP 进程 |
| `lsp-ethernet-direct@.service` | 不 enable；随直连激活启动，拔线停止，绑定接口设备生命周期 |
| `/etc/lsp-ethernet-direct/dnsmasq.conf` | `0644 root:root`；`port=0`，不发送 router / DNS 选项 |

构建安装 `dnsmasq-base`，不安装全局 `dnsmasq` 服务。进程降权为 `nobody:nogroup`，
租约位于 `/run/lsp-ethernet-direct-<接口>/`，不跨重启保存。
开发板的 `nmcli connection modify` 会持久保存本地角色，不改变工程镜像默认。
启用 Netplan 集成时，修改可能保存到 `/etc/netplan/90-NM-<UUID>.yaml`，由 `/run/NetworkManager/` 生成活动 profile；
用 `nmcli -f NAME,UUID,FILENAME connection show` 核对实际来源，不假定仍是原 keyfile。
后续部署须保留本地角色，避免以镜像默认覆盖；原厂 profile 与本地 Netplan 配置也不能未经核对混用。

## 返回产品默认服务端

使用串口或独立 Wi-Fi 操作，先断开上游网线；不要通过待切换的有线 SSH 执行。
用 `nmcli device status` 和 `udevadm info --query=property --path=/sys/class/net/<接口>` 核对实际 GMAC 路径。
先禁用两个客户端自动连接；若它们当前仍活动，再分别执行 `nmcli connection down uuid <客户端 UUID>`。
最后一条激活命令前，接上用于直连的电脑，保持上游网络断开。

```bash
iface='<已核对接口>'
sudo nmcli connection modify uuid 00000000-0000-4000-8000-000000000003 connection.autoconnect no
sudo nmcli connection modify uuid 00000000-0000-4000-8000-000000000002 connection.autoconnect no
sudo nmcli connection modify uuid 00000000-0000-4000-8000-000000000001 connection.autoconnect yes
sudo nmcli --wait 20 connection up uuid 00000000-0000-4000-8000-000000000001 ifname "$iface"
```

禁用自动连接不会立即停用已有活动连接；有线配置激活需要该接口已有 carrier。

## 为什么取消自动回退

“未收到 DHCP”不能区分笔记本直连与上游服务暂时离线。已在共享交换机环境观察到旧自动回退方案
与上游 DHCP 同时运行，并向其他下游电脑 ACK 直连网段地址；上游恢复也不会主动抢占已活动的直连配置。
因此当前用固定角色消除误判：产品提供 DHCP，开发板接共享网络时明确切为客户端。

## 状态与验证边界

```bash
iface='<已核对接口>'
nmcli -f NAME,UUID,TYPE,DEVICE connection show --active
nmcli -f GENERAL.CONNECTION,GENERAL.CON-UUID,IP4.ADDRESS,IP4.GATEWAY device show "$iface"
systemctl status "lsp-ethernet-direct@${iface}.service"
journalctl -b -u "lsp-ethernet-direct@${iface}.service"
```

服务端应有固定板端地址及 DHCP 服务，电脑取得声明范围内的 `/24` 租约，且不新增网关或 DNS。
客户端应取得上游地址，对应直连 DHCP 实例 inactive。静态地址电脑不会请求 DHCP，须先改为自动获取。

LubanCat-5 V2 / BSP `6.1.172` / NetworkManager `1.46.0` 已核对两个 GMAC 路径；
Linux 电脑直连的取址、固定 IP 访问、ACK 无 router / DNS 及拔线停止 DHCP 已有板测记录。
本轮开发板显式切客户端后，有线 SSH 复查成功，两个直连 DHCP 实例均 inactive；
三份角色配置已核对保存于 Netplan，权限为 `0600 root:root`。
2026-10-23 在 NetworkManager `1.46.0` / dnsmasq `2.91` 上运行
`tests/ethernet-direct.py`（镜像源码：`tests/ethernet-direct.py`），24 项隔离回归通过：默认服务端即使看到上游 DHCP 也不改角色、
两口取址与固定地址访问、单实例、无网关 / DNS、保留已有默认路由、拔线停止 DHCP、显式切客户端后取上游租约并停止板端 DHCP，
以及客户端真实 DHCP 事务失败后仍不启用服务端。旧配置在新测试的默认服务端断言失败，确认测试能区分新旧策略。
夹具用私有网络命名空间、模拟 GMAC 路径及窄 `systemctl` stub，角色切换用 `--temporary`，不验证配置落盘；
不能替代真实 PID 1 的 sandbox / BindsTo 和物理 GMAC 插拔验证，Netplan 持久保存由上述现场独立读盘核对。
新默认服务端未重新板测，重启保持、真实交换机上游中断、另一网口完整物理流程、接口消失及 Windows 客户端仍待板测。

## 依据

- NetworkManager `1.46` 随包 `nm-settings-nmcli(5)`：`match.path`、自动连接、重试与 `multi-connect`。
- `NetworkManager-dispatcher(8)`：连接事件、UUID 环境及事件完成时序。
- dnsmasq `2.91` 随包 `dnsmasq(8)`：`port=0`、接口绑定、空 router / DNS 选项与租约配置。
- [双 GMAC 硬件对应关系](../hardware/lubancat-5-v2-hardware.md)、[笔记本共享上网](laptop-ethernet-sharing.md)。
