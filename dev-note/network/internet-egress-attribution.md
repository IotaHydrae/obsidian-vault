# 公网 IP 归属、路由 ASN 与实际接入线路

> 地址持有人、BGP 宣告者和用户接入运营商描述不同对象，不能用一个查询站的标签代替完整路径证据。

## TL;DR

- 先证明查询请求走直连还是代理，再解释公网 IP。
- RDAP、路由观测和商业 IP 数据库回答的问题不同。
- 固定代理节点不保证直连流量使用固定公网出口。

| 证据 | 能支持的判断 | 边界 |
| --- | --- | --- |
| 受控路径下的出口查询 | 该请求在该时刻暴露的公网地址 | 不单独证明本地接入运营商 |
| RDAP / RIR 登记 | 地址登记的组织和网段 | 不等于实际最后一公里提供者 |
| BGP origin ASN 观测 | 观测到的前缀宣告来源 | 不完整描述转接、租用和物理链路 |
| IP 归属数据库 | 该库给出的标签和位置 | 可能滞后或互相冲突 |

保存查询时间、目标服务、代理/TUN 状态与出口观测。需要比较直连和代理时，先按[路径诊断](proxy-path-diagnostics.md)建立控制条件；只设置 `--noproxy` 还不能排除 TUN。

企业网关、多出口策略或 NAT 都可能使直连公网地址改变。具体原因需要网关与链路证据，不能看到地址变化就确定是哪一种机制。需要固定直连出口时，应在实际控制上游出口的层面解决。

参考：[APNIC RDAP](https://www.apnic.net/about-apnic/whois_search/about/rdap/)、[RIPEstat Prefix Overview](https://stat.ripe.net/docs/data-api/api-endpoints/prefix-overview.html)。
