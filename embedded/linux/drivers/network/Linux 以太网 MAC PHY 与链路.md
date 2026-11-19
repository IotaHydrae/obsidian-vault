# Linux 以太网 MAC、PHY 与链路

> 网络不通时先确认物理链路，再检查接口、地址、路由和上层服务；网卡驱动加载成功不代表链路已经建立。

## 组成与路径

```text
用户程序 / 网络协议栈 ↔ 网络设备与 MAC 驱动
↔ MAC 控制器 ↔ PHY ↔ 网线 / 对端
                    ↑
                 MDIO 管理总线
```

MAC 负责帧收发与控制器接口，PHY 负责物理链路；MDIO 用于管理与读取 PHY 状态，通常不是传输正常以太网数据的通道。某些设备采用集成或不同链路形式，实际关系应按芯片与驱动确认。

PHY 接口模式、PHY 地址、复位、时钟、供电和时序由硬件与设备树 binding 共同决定。RGMII 等接口需要正确配置延迟：核对 SoC、PHY 和板级布线的具体责任，不能把某个 `phy-mode` 值当作通用修复。

## 排查顺序

1. `ip -br link` 查看接口与 carrier，`ethtool <网卡>` 查看驱动报告的链路、速率与双工；权限及驱动能力影响可见字段。
2. 无链路时检查 PHY 是否被识别、供电、复位、参考时钟、接口模式、布线以及对端；查看 `dmesg` 和 PHY 相关日志。
3. 链路正常后检查 `ip address`、`ip route`、防火墙和应用；carrier 存在不证明 IP 配置或远端服务可用。
4. 丢包或吞吐异常时结合 `ip -s link`、驱动统计、协商结果和双端测试；错误计数与链路协商结果不单独证明具体根因。

## 参考

- 蜗窝科技：[PHY 与 MDIO 初始化](https://www.wowotech.net/linux_kenrel/470.html)、[以太网驱动 probe](https://www.wowotech.net/linux_kenrel/469.html)。
- Linux 6.18.62：[PHY 驱动框架](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/networking/phy.rst)、[设备树以太网 PHY binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/net/ethernet-phy.yaml)。
