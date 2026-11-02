# Mihomo：让分流修改在订阅更新后继续生效

> 将个人策略放在客户端持久扩展层，按优先级生成规则，并以实际运行配置和连接记录验收。

## TL;DR

- 分开管理订阅节点、个人规则和运行时选择；订阅原文是可更新输入。
- 精确例外通常应先于宽泛规则，兜底规则放在末尾。
- 更新订阅、切换订阅和切换 TUN 是三个不同的回归场景。

## 规则与作用范围

以下是配置片段，`PROXY` 必须对应已定义的策略组；域名和网段按部署需求替换。

```yaml
rules:
  - DOMAIN,api.example.com,DIRECT
  - DOMAIN-SUFFIX,example.net,DIRECT
  - IP-CIDR,192.0.2.0/24,DIRECT,no-resolve
  - MATCH,PROXY
```

`DOMAIN` 精确匹配一个域名，`DOMAIN-SUFFIX` 匹配主域名及子域名。规则按顺序匹配：较早的宽泛直连规则可能遮蔽后面的例外。`no-resolve` 约束该 IP 规则的解析行为，不能代替域名规则或 DNS 配置。

直连还是代理是策略选择，需结合实际连通性和服务要求；域名后缀、IP 归属或节点所在地区本身不能证明哪条路径更好。

## 持久扩展

在支持 `main(config)` 的扩展脚本接口中，可前置规则并去重：

```javascript
function main(config) {
  if (config.rules !== undefined && !Array.isArray(config.rules)) {
    throw new Error("rules must be an array");
  }
  const additions = ["DOMAIN,api.example.com,DIRECT"];
  config.rules = [...new Set([...additions, ...(config.rules ?? [])])];
  return config;
}
```

客户端的合并规则和扩展顺序需要按版本核对。例如 Clash Verge Rev 文档说明，列表和部分 DNS 表会整体替换；只追加一个 `nameserver-policy` 条目，可能覆盖已有条目。FlClash 等客户端须分别检查其生成流程。

## 验收与回退

1. 保存修改前的扩展和有效配置，生成候选配置。
2. 用实际内核校验候选文件，例如 `mihomo -t -f /path/to/rendered.yaml`。
3. 重载后检查有效规则、规则顺序、组成员及实际连接命中；返回 HTTP 响应不单独证明走对了策略。
4. 分别测试订阅更新、重新选择订阅、TUN 开关和客户端重启。
5. 节点改名、删除或过滤后为空时，检查引用和兜底；恢复时撤回扩展并重新生成配置。

静态导出的完整配置是快照；是否继续订阅更新，取决于导入方式及保留的 provider 配置。连接可能沿用原路径，验证重载后应发起新连接。

相关：[节点选择](proxy-node-selection.md)、[路径诊断](proxy-path-diagnostics.md)。

## 参考

- [Mihomo 路由规则](https://wiki.metacubex.one/config/rules/)
- [Clash Verge Rev 扩展与合并规则](https://www.clashverge.dev/guide/extend.html)
