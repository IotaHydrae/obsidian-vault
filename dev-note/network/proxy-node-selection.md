# 代理节点选择：延迟、吞吐与故障切换

> 延迟检测适合判断响应和可用性；吞吐选择需要目标流量实测，稳定业务还需要切换抑制。

## TL;DR

- `url-test` 不测下载带宽，`fallback` 也不自动按吞吐量排名。
- 浏览、长连接和大文件可以使用不同策略组，按目标服务验收。
- 探测间隔、超时、切换容差和业务超时不是同一个参数。

## 选择机制

| 机制 | 适合解决的问题 | 不能据此保证的内容 |
| --- | --- | --- |
| `select` | 明确指定业务出口 | 自动故障切换 |
| `url-test` | 按探测结果自动选择 | 下载速度或目标站点可用性 |
| `fallback` | 按候选顺序选择可用出口 | 实时带宽最优 |
| 独立吞吐探测 | 比较特定目标的传输表现 | 其他时段和站点同样快 |

以下片段要求已有 `node-A`、`node-B`。数值仅演示配置及单位，不是所有网络的最优设置。

```yaml
proxy-groups:
  - name: Latency
    type: url-test
    proxies: [node-A, node-B]
    url: https://www.gstatic.com/generate_204
    interval: 300
    timeout: 8000
    tolerance: 50
    lazy: false
  - name: Web-Stable
    type: fallback
    proxies: [node-A, node-B]
    url: https://www.gstatic.com/generate_204
    interval: 300
    timeout: 8000
```

`interval` 以秒计，`timeout` 与 `tolerance` 以毫秒计。`lazy` 会影响未使用分组的探测。通过 `use` 引入 provider 时，还要检查 provider 自己的 health-check；不能假定组的 URL 会检测所有引入节点。

## 低延迟但下载慢

固定测试目标、文件、时长和节点，记录完整传输是否成功、有效吞吐、错误及重复测量的分布。同时检查本地 Wi-Fi、上游出口和目标限速，避免把节点当成唯一变量。带宽测试会消耗流量，应控制候选数量和频率。

需要自动按吞吐切换时，由外部测量程序向控制接口提交选择，并设置最短驻留时间及有意义的改善阈值。阈值按目标要求或重复测量定义，不能将一次最快读数直接变成验收标准。

## 看起来没有自动切换

- 查询组类型、当前成员、`now`、可用状态及是否存在 `fixed` 固定选择。
- 核对客户端是否持久保存了手动选择，以及探测是否实际执行。
- `max-failed-times` 触发强制健康检查，不等于所有业务都能在某个固定秒数内恢复。
- 切换策略不保证已有 TCP、下载或流式请求无缝迁移，应用可能需要重新连接。

参考：[通用字段](https://wiki.metacubex.one/config/proxy-groups/)、[url-test](https://wiki.metacubex.one/config/proxy-groups/url-test/)、[fallback](https://wiki.metacubex.one/config/proxy-groups/fallback/)、[provider](https://wiki.metacubex.one/config/proxy-providers/)、[控制 API](https://wiki.metacubex.one/api/)。
