# 系统代理、TUN 与 DNS：如何证明请求走了哪条路径

> 同时观察应用代理、内核路由、DNS 和代理连接记录，才能解释实际出口；单个 HTTP 状态或测速面板不足以定因。

## TL;DR

- `curl --noproxy '*'` 只跳过应用层代理，不能绕过仍在接管流量的 TUN。
- 内网直连规则、TUN 路由排除和内网 DNS 分别解决不同问题。
- 全部节点一起超时应先查公共依赖，并用独立目标验证探测器。

## 对照测试

```bash
TARGET='https://example.com/'
FORMAT='http=%{http_code} dns=%{time_namelookup} connect=%{time_connect} tls=%{time_appconnect} first=%{time_starttransfer} total=%{time_total}\n'
curl --noproxy '*' --connect-timeout 10 --max-time 30 \
  -sS -o /dev/null -w "$FORMAT" "$TARGET"
curl --noproxy '' --proxy http://127.0.0.1:7890 \
  --connect-timeout 10 --max-time 30 -sS -o /dev/null \
  -w "$FORMAT" "$TARGET"
```

端口按客户端配置替换。第一条测试仍可能进入 TUN；需要物理直连基线时，另外控制 TUN、路由或隔离测试环境，并记录条件。绑定网卡也不自动证明绕开了策略路由。

时间字段有其累计、代理和重定向口径，适合同条件对照，不能直接解释成纯服务器计算时间。还需查看响应体、完整下载及业务层结果；403、404、502、503 不应一概归为分流失败。

## 分层取证

| 层次 | 检查内容 |
| --- | --- |
| 应用 | 系统代理、环境变量、浏览器自有代理或 DoH |
| 路由 | `ip rule`、`ip route`、`ip route get <目标IP>`；同时考虑 IPv4/IPv6 |
| DNS | 实际请求由谁解析、返回地址、私有域是否用了内部 DNS |
| Mihomo | 实际运行配置、命中规则、策略链及使用的节点 |
| 业务 | 所需资源是否完整、API 是否真正完成、连接是否中途断开 |

浏览器首页成功但样式或下载失败时，检查资源与重定向涉及的域名、各自出口及连接复用，不能只测试首页。

## 内网与解析

`DIRECT` 指示代理内核使用直连出口；`tun.route-exclude-address` 在适用平台绕过 TUN 自动路由。二者不能替代私有域名解析。内网域应使用能回答它的 DNS，节点域名解析也要避免依赖尚未建立的代理形成循环。

`fake-ip`、`redir-host`、`nameserver-policy` 和 `proxy-server-nameserver` 的组合按实际内核与客户端核对。没有原生 IPv6 默认路由时，先分别测试直连和代理的 IPv6 能力，再决定局部禁用或调整；不要将某次环境问题固化为全局禁用 IPv6 的通用规则。

## 集体超时

检查本地链路、DNS、出口与节点可达性，再用不同运营方的探测目标交叉验证。共同探测目标失败会让多个正常节点一起变红；反过来，健康检查成功也不代表特定网站或长连接正常。适度加大超时只能检验“探测窗口过短”的假设。

参考：[TUN](https://wiki.metacubex.one/config/inbound/tun/)、[DNS](https://wiki.metacubex.one/config/dns/)、[curl 手册](https://curl.se/docs/manpage.html)。
