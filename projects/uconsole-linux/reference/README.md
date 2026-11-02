# 硬件与平台参考资料

## TL;DR

- 这里保存可检索的接口事实、官方资料入口和固定来源身份；原始 PDF／CAD／模型缓存在仓库忽略目录。
- “参考资料已收集”不代表对应板型已接入构建或通过设备验证；实施范围见 [Roadmap](../project/roadmap.md)。
- 不同厂商即便使用同一 SoC，也要分别核对模块／载板、电源、DDR、引导和驱动配置。

| 目录 | 主要内容 | 使用边界 |
| --- | --- | --- |
| [Raspberry Pi CM](raspberry-pi-cm/README.md) | CM4／CM5 全针定义、CM3+ 对照、尺寸、STEP、IO Board KiCad | 区分模块机械模型与载板设计，不能冒充完整 SoM 电路 |
| [ClockworkPi uConsole](clockworkpi-uconsole/README.md) | 主板 J1/J4、CM4 adapter、键盘、扩展、A06/R01、制造资料 | 以 Clockwork 的实际接口为准，分开标注 CM3 直插依据和具体限制 |
| [WalnutPi CM2](walnutpi-cm2/README.md) | T527 计算模块引脚／机械、IO 底板图和镜像构建链 | 未公开模块完整 SoC／DDR／PMIC 原理图，CM2 与 2B 分开 |
| [Avaota-A1](avaota-a1/README.md) | T527 整板原理图、PCB／制造／BOM／3D、DRAM 模板、IBIS、AvaotaOS | 作为设计与 BSP 参考，不能直接替换 CM2 或按原样接入 uConsole |
| [T527 驱动源码核查](t527-driver-audit/README.md) | Avaota／Walnut NPU、ISP、GPU、视频与二进制交付边界 | 区分有源内核、预编译对象、用户库和固件，未验证6.18构建 |

## 保存与复核

各目录 `sources.json` 记录原始 public URL、仓库 pin／网页时间、字节长度、SHA-256 和缓存相对路径。
清单是调查快照，不是厂商签名，也不是自动下载／构建的机器接口；固定工具输入需另行实现。
原件保持不修改，Git 中维护事实索引和独立说明。许可按每份资料的上游条款，不统一套本仓库许可。
知识篇保留复用方法、关键差异和未知项，索引见 [硬件知识库](../knowledge/README.md)。
