# 用 pastebinit 分享命令输出

> 将日志通过管道传给 `pastebinit`，命令返回可分享的链接。

## 示例

```bash
raspinfo | pastebinit
```

示例将树莓派的 `raspinfo` 输出提交给 paste 服务。

## 注意

- 提交前检查日志是否包含账号、令牌、内网地址或其他敏感信息。
- 链接的可见范围和有效期取决于所选服务的配置。
