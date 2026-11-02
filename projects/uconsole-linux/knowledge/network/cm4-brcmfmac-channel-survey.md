# CM4：启动时 brcmf_set_channel 返回 -52

> brcmf_set_channel 的 -52 是固件拒绝信道，不足以判断 Wi-Fi 连接失败。

## TL;DR

- 先检查关联结果、国家规则与 `regulatory.db`，不要只隐藏日志。
- 该轮补齐缺失的 `wireless-regdb`；安装与规则重载不证明启动报错已消除。
- 不要把规则库缺项与每条信道拒绝直接建立已证实的因果关系。

## 诊断与复核方法

先核对实际关联与规则库是否存在；补齐缺项后，在一次独立正常启动中检查加载日志、`iw reg get`、
`iw dev wlan0 link` 及原信道报错是否重现。如果仍然报错，继续核对固件报告的
信道能力和原始固件错误，避免用降低日志等级掩盖问题。国家设置应对应实际部署地区。

判据分别是规则库可加载、关联链路可用以及原报错是否重现，不能用其中一项代替另两项。

## 源码含义

固定内核的 `drivers/net/wireless/broadcom/brcm80211/brcmfmac/cfg80211.c`
中，`brcmf_set_channel()` 在 `brcmf_cfg80211_dump_survey()` 信道调查路径调用，
通过 `chanspec` iovar 向固件设置待调查的信道。

`fwil.c` 的 `brcmf_fil_cmd_data()` 将固件负错误统一转换为 `-EBADE`，在本机为
`-52`。原始固件错误码没有保留在这条日志里，不能把 `-52` 直接等同于
`BCME_BADCHAN`、超时或供电问题。`net/wireless/reg.c` 声明规则库与签名为固件输入。

## 设备观察

2026-10-19 检查 CM4、Ubuntu Base 24.04.5、`6.18.57-v8-uconsole`：

- 报错集中在启动约 11 秒和 18 秒，参数包括 `0x100c`、`0x100d`、`0x100e`
  和 `0xd022`、`0xd026`、`0xd02a`、`0xd02e`，均发生在成功关联前。
- NetworkManager 随后连接成功。检查时使用信道 9、2452 MHz，SSH 可用；
  这不代表其他信道或长期稳定性已经验证。
- 启动约 5 秒时，cfg80211 加载 `regulatory.db` 返回 `-2`；原系统没有安装
  `wireless-regdb` 和 `iw`。
- 安装两包后，`/lib/firmware/regulatory.db` 及签名路径均指向有效文件。
  `iw reg reload` 成功，世界域规则从内置表变为数据库规则，连接保持正常。
  全局域仍为 `00`，PHY 域为 `99`。本轮没有重启、重载无线驱动或断开连接，
  因此不能宣称连接前的原信道调查报错已消除。

## 原始证据

本次观察保存在设备 `/var/lib/uconsole-desktop/20261018/` 下的
`wifi-survey-observations.json` 和 `wifi-regdb-reload.json`，未记录密码。
