# 服务就绪、网络等待与本地启动的分工

> 启动顺序不等于功能就绪；本地功能和需要联网的消费者应各自定义依赖与失败策略。

## TL;DR

- `After=` 只约束已入队任务顺序；`Wants=`、`Requires=` 与 ready 各有独立语义。
- `network.target` 不保证地址或远端可达，wait-online 也不是持续联网监控。
- 不能把 Type=notify 改成 simple 仅为了缩短报表；实际就绪契约必须保持。
- 本仓库 Ubuntu 禁用 NM-wait-online，但保留发行版 NM 类型与 Netplan 功能。

## 检查当前依赖

```sh
systemctl cat <UNIT>
systemctl show <UNIT> -p Type -p After -p Before -p Wants -p Requires
systemd-analyze critical-chain <UNIT>
journalctl -b -u <UNIT> -o short-monotonic
```

这些只读命令查看生效单元与记录。应用命令成功不自动证明终点功能。
本地 loopback 服务也可能需要明确启动依赖与连接重试，不能依靠“进程已存在”。

## 有界网络等待

消费者若只需要网络启动事务完成，可以使用符合需求的有界等待；
若需要 DNS、路由、TLS、远端服务，则分别检查并定义失败/重试或离线回退。
超时放行是一种产品策略，不是“网络已经正常”的证据。
避免让本地显示、UART 登录或维护入口不必要地依赖外网。

启动协调器等待业务服务时，业务不能反向 Requires/After 协调器完成，否则可能形成等待环。
需要事务完整性的操作应给内部阶段预算，不能用随意的总超时在多文件发布中途强杀。

## drop-in 与升级

依赖型列表不能假定用空 `After=` 在 drop-in 中全部清除。
完整覆盖厂商单元可以移除依赖，但同时承担跟进发行版更新的维护成本。
改动后比较 `systemctl cat/show` 与 `systemd-analyze verify`，不要仅检查写入的文件。

## 本仓库状态与证据

Ubuntu configure_services（工程源码：`scripts/image_backends/ubuntu_base.py`）启用 NM/SSH/扩容，最后禁用 wait-online，
不采用其他工程的私有服务、超时数值或启动类型覆盖。
Netplan 的优化范围见 [专用生成入口](../network/ubuntu-netplan-generate.md)，
不同发行版的策略不能互相套用。
语义参考 [systemd.unit](https://www.freedesktop.org/software/systemd/man/latest/systemd.unit.html)
和 [network targets](https://systemd.io/NETWORK_ONLINE/)。
