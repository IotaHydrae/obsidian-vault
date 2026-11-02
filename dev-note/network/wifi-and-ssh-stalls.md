# Wi-Fi 上 SSH 卡住：区分链路、QoS 与连接复用

> 将无线事件、网关延迟和 SSH 各阶段日志对齐，再一次只改变一个因素；能 ping 通不代表交互流正常。

## TL;DR

- 先确认目标路由及代理接管情况，再分析 Wi-Fi 和 SSH。
- BSSID 锁定、关闭省电、禁用 QoS 标记都是待验证的干预，不是统一修复。
- 流式 API 中断也可能发生在 Wi-Fi 完全没有掉线的时候。

## SSH 对照

```bash
ssh -vvv -o ConnectTimeout=10 \
  -o ControlMaster=no -o ControlPath=none \
  -o ServerAliveInterval=15 -o ServerAliveCountMax=3 \
  user@host.example
```

先区分 TCP 建连、密钥交换、认证和登录后交互。上述选项绕开旧复用连接并提供失联检测，不修复底层丢包。若完整流程仍复现，再单独加入 `-o IPQoS=none` 对照，避免同时修改多项后误判。

`IPQoS=none` 去掉 SSH 设置的服务质量标记，只有在路径设备对相关标记的处理确实影响流量时才可能改善。优先为明确主机设置，而非无证据地全局关闭。已有连接不会因修改配置自动变好。

## 无线取证与恢复

```bash
nmcli -f IN-USE,SSID,BSSID,CHAN,SIGNAL device wifi list
journalctl -b -u NetworkManager --since '-10 min'
```

对照故障时刻的重新关联、掉线、网关丢包、目标服务日志和代理日志。RSSI 较强不一定表示 AP 更空闲，也不能证明回程链路良好。

若怀疑同 SSID 漫游或某个 AP 路径，记录连接配置原有的 `802-11-wireless.bssid` 和 `powersave`，再在可中断会话中单独测试：

将下面的连接名和示例 BSSID 换成当前扫描及配置中的值。

```bash
CONNECTION='your-profile'
BSSID='02:00:00:00:00:01'
nmcli connection modify "$CONNECTION" 802-11-wireless.bssid "$BSSID"
nmcli connection up "$CONNECTION"
```

重新激活会中断当前无线会话。移动位置后，锁定 AP 可能恶化信号；恢复时写回记录的原值。只有原来未锁定时，才用空字符串恢复自动选择。省电开关同样先记录原值并做对照。

SSH 窗口已无响应时，可在默认转义设置下先回车，再输入 `~.` 断开；重新连接后验证，不能把旧连接的状态当作修复失败。

参考：[OpenSSH 配置](https://man.openbsd.org/ssh_config.5)、[NetworkManager Wi-Fi 属性](https://networkmanager.dev/docs/api/latest/settings-802-11-wireless.html)。
