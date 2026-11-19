# Linux 蓝牙协议栈与排查

> 蓝牙问题先分开控制器、主机协议栈和应用服务；BLE 广播、连接、配对、加密及 GATT 操作是不同阶段。

## 分层

```text
应用 → Linux 用户空间蓝牙服务 / API → 内核蓝牙子系统
→ HCI 传输（如 UART、USB）→ 蓝牙控制器 → 无线链路
```

常见 Linux 系统使用 BlueZ；具体服务、版本和访问权限由发行版决定。HCI 是主机与控制器之间的接口，不是空口上的 GATT 数据格式。

| 过程 | 关注点 |
| --- | --- |
| 发现 | 广播内容、扫描状态、地址类型、信号与扫描过滤 |
| 建立连接 | 连接参数、控制器能力、射频环境与对端状态 |
| 配对 / bonding | 密钥协商与是否保存密钥；成功连接不等于已完成配对 |
| 加密 / 身份认证 | 按双方能力和安全策略协商；加密不自动证明应用层身份 |
| GATT | 通过服务和特征值进行读写、通知等操作；权限可能要求加密或认证 |

## 排查

1. 检查控制器是否被识别、固件是否加载、`rfkill` 是否阻止，以及 UART / USB 传输是否正常。
2. 用发行版提供的工具观察适配器状态和系统日志；可使用 `btmon` 同时记录 HCI 交互，注意抓取权限和敏感内容。
3. 区分扫描不到、连接失败、配对失败和 GATT 权限失败，不要用“蓝牙不通”概括所有层次。
4. BLE 地址可能具有隐私特性；不要把一次观察到的随机地址直接当成永久硬件身份。

## 参考

- 蜗窝科技：[BLE 广播](https://www.wowotech.net/bluetooth/ble_broadcast.html)、[BLE 连接](https://www.wowotech.net/bluetooth/ble_connection.html)、[BLE 安全机制](https://www.wowotech.net/bluetooth/le_security_manager.html)。
- [BlueZ 官方项目](https://www.bluez.org/)、[Linux 蓝牙子系统源码](https://github.com/gregkh/linux/tree/v6.18.62/net/bluetooth)。
