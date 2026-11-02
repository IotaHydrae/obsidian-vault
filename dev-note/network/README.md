# 网络分流与排障

> 先确认路径和指标，再修改规则、选路或无线策略；每次实验都保留验证与恢复条件。

## TL;DR

- 修改分流配置：[持久覆盖与优先级](mihomo-routing-and-overrides.md)。
- 节点延迟好但业务慢：[延迟、吞吐与切换](proxy-node-selection.md)。
- 故障定位从[代理、TUN 与 DNS](proxy-path-diagnostics.md)开始，按证据进入无线或上游排查。

| 问题 | 入口 |
| --- | --- |
| 订阅更新覆盖修改 | [Mihomo 配置分层](mihomo-routing-and-overrides.md) |
| 节点选择与下载速度 | [节点选择](proxy-node-selection.md) |
| 集体超时、内网或 DNS 异常 | [路径诊断](proxy-path-diagnostics.md) |
| Wi-Fi 上 SSH 卡住 | [无线、QoS 与复用](wifi-and-ssh-stalls.md) |
| 两张网卡能否叠加 | [多接口带宽](wifi-multi-interface-bandwidth.md) |
| Android VPN 共享与双层代理 | [热点共享](android-vpn-hotspot-sharing.md) |
| IP 查询站标签不一致 | [公网出口归属](internet-egress-attribution.md) |
