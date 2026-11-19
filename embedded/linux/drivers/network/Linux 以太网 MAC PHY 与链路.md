# Linux 以太网 MAC、PHY 与链路

> 网络不通时先确认物理链路，再检查接口、地址、路由和上层服务；网卡驱动加载成功不代表链路已经建立。

## TL;DR

- 没有 carrier 先检查 PHY、MDIO、复位、时钟、供电和对端。
- 有 carrier 但无法通信，再依次检查地址、路由、邻居表、防火墙和服务。
- RGMII 的 DTS `phy-mode` 描述 PCB 已提供的延迟；传给 PHY 的接口模式描述它还要补哪一向。不要把两者混为一谈。

适用范围：Linux 6.18 LTS。

## 组成与路径

```text
用户程序 / 网络协议栈 ↔ 网络设备与 MAC 驱动
↔ MAC 控制器 ↔ PHY ↔ 网线 / 对端
                    ↑
                 MDIO 管理总线
```

MAC 负责帧收发与控制器接口，PHY 负责物理链路；MDIO 用于管理与读取 PHY 状态，通常不是传输正常以太网数据的通道。某些设备采用集成或不同链路形式，实际关系应按芯片与驱动确认。

PHY 接口模式、PHY 地址、复位、时钟、供电和时序由硬件与设备树 binding 共同决定。RGMII 等接口需要正确配置延迟：核对 SoC、PHY 和板级布线的具体责任，不能把某个 `phy-mode` 值当作通用修复。

## 驱动与链路协商

MAC 驱动注册网络设备并与 PHY / phylink 等机制协作；MDIO 总线负责访问 PHY 寄存器，PHY 驱动解析能力、协商结果和链路状态。具体系统也可能使用固定链路、交换芯片或其他拓扑，不能假定每个网络接口都对应独立外置 PHY。

## 分层验证

| 检查点 | 典型命令或观察 | 结论边界 |
| --- | --- | --- |
| 驱动 / 硬件 | `dmesg`、`ethtool -i <网卡>` | 驱动绑定不等于链路可用 |
| 物理链路 | `ip -br link`、`ethtool <网卡>` | carrier 不等于 IP 和服务可达 |
| 接口统计 | `ip -s link`、`ethtool -S <网卡>` | 错误增长需结合双端和时间段分析 |
| 地址与路径 | `ip address`、`ip route`、`ip neigh` | 地址存在不等于路由及邻居解析正确 |
| 端到端 | 按目标检查 ARP / IPv6 邻居、ping、端口和服务日志 | 失败位置可能在设备、链路或远端 |

在千兆不稳定但低速稳定时，可以把接口时序和信号完整性作为假设之一，再结合双方协商、错误统计和硬件测量验证；不要仅凭该现象断言一定是 RGMII 延迟。

## RGMII 延迟责任

| 名称 | 用作 DTS `phy-mode`：PCB 已提供的延迟 | 传给 PHY 驱动的接口模式：PHY 要加的延迟 |
| --- | --- | --- |
| `rgmii` | RX、TX 两向都已提供 | 不额外加延迟 |
| `rgmii-id` | 两向都未提供 | RX、TX 都加 |
| `rgmii-rxid` | 只提供 TX | 只加 RX |
| `rgmii-txid` | 只提供 RX | 只加 TX |

Linux 的 phylib / phylink 路径默认由 MAC 把 DTS 值传给 PHY 补齐延迟。如果 MAC 必须自行加延迟，DTS 仍按 PCB 填写，MAC 驱动须从传给 PHY 的模式里去掉已由 MAC 补偿的方向。例如 PCB 两向都没有延迟、MAC 硬件提供双向延迟时，DTS 仍是 `rgmii-id`，传给 PHY 的才是 `PHY_INTERFACE_MODE_RGMII`。如果 PCB 和 MAC 都有无法关闭的同向延迟，则该组合本身不受支持。

RX / TX 指 RGMII 接口方向，不要按网线方向猜测；按原理图及走线、MAC / PHY binding、驱动实际配置和测量结果核对。少加或重复加入同一路延迟都可能造成错误帧，即使 PHY 已报告 carrier。

## 分辨链路与 IP 问题

`ethtool` 报告 Link detected 但不能访问远端时，继续检查地址和路由、ARP / 邻居解析，再检查对端及防火墙；若双端协商出的速率、双工或错误计数异常，先回到物理链路分析。`ip -s link` 与 `ethtool -S` 的计数含义由驱动和硬件决定，关注同一时间窗口内的增量。

## 排查顺序

1. `ip -br link` 查看接口与 carrier，`ethtool <网卡>` 查看驱动报告的链路、速率与双工；权限及驱动能力影响可见字段。
2. 无链路时检查 PHY 是否被识别、供电、复位、参考时钟、接口模式、布线以及对端；查看 `dmesg` 和 PHY 相关日志。
3. 链路正常后检查 `ip address`、`ip route`、防火墙和应用；carrier 存在不证明 IP 配置或远端服务可用。
4. 丢包或吞吐异常时结合 `ip -s link`、驱动统计、协商结果和双端测试；错误计数与链路协商结果不单独证明具体根因。

## 参考

- 蜗窝科技：[PHY 与 MDIO 初始化](https://www.wowotech.net/linux_kenrel/470.html)、[以太网驱动 probe](https://www.wowotech.net/linux_kenrel/469.html)。
- Linux 6.18.62：[PHY 驱动框架](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/networking/phy.rst)、[以太网控制器 binding 的 RGMII 说明](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/net/ethernet-controller.yaml)、[以太网 PHY binding](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/bindings/net/ethernet-phy.yaml)。
