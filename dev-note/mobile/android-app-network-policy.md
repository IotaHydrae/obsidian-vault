# Android 单个应用无法联网：先检查网络权限和 UID 策略

> 网络已连接、VPN 正常或其他应用能上网，都不能证明目标应用的 UID 已获准联网。

## TL;DR

- 检查应用自己的“允许网络访问”总开关，以及当前 Wi-Fi／移动数据权限。
- 放行后结束并重新打开应用，用搜索、详情等实时内容验收。
- `dumpsys netpolicy` 的字段随 ROM 变化；区分配置策略与最终有效阻断。

## 最小修复路径

适用于提供应用级联网开关的 ROM。以 Google Play 商店为例：

1. 进入应用信息 → 流量使用情况／移动数据和 Wi-Fi。
2. 打开“允许网络访问”，确认当前网络类型没有被单独禁止。
3. 确认没有正在下载的任务后，强行停止商店，再从桌面打开。
4. 搜索一个应用并打开详情，检查评分、截图等内容实际加载。

“后台数据”“不受限的数据使用量”和允许联网的总开关是不同项目；只检查前两项可能遗漏全面阻断。设置页名称和开关能力由 ROM 决定。

可选的电脑操作，须已授权 ADB；多设备时给命令加 `-s <serial>`：

```sh
adb shell pm list packages -U com.android.vending
adb shell dumpsys netpolicy > play-netpolicy.txt
adb shell am force-stop com.android.vending
```

前两条用于读取状态，最后一条只结束应用进程，不清除账号或数据；仍需在手机界面授予联网权限并重新打开应用。UID 必须从本机查询，不能复制另一台设备的数字。完整策略输出可能含其他应用信息，分析时只保留目标 UID 所需片段。

## 怎样读策略

| 观察 | 可以判断 | 不能判断 |
| --- | --- | --- |
| Wi-Fi／VPN 被系统标为 `VALIDATED` | 该网络通过了相应连通性检查 | 每个应用都能解析并访问业务 |
| UID 策略包含 `REJECT_ALL` | 该 ROM 为这个 UID 配置了全面限制 | 某个固定数字在所有 ROM 都代表同一位 |
| `blocked` 非空而 `effective=NONE` | 在提供这些字段的实现中，限制可能已被豁免 | 只看 `blocked` 就认定仍然阻断 |
| 页面出现 `ERR_NAME_NOT_RESOLVED` | 存在解析／连接路径失败的线索 | 一定是系统 DNS 或 Cronet 的缺陷 |

2026-10-20 的 LineageOS 24 案例中，商店的联网总开关关闭；放行对应 UID 并重新启动商店后，搜索、应用详情、评分和截图恢复。代理、DNS、hosts 未改动。这支持先修复应用策略，不能推广成所有 Play 商店错误都用同一方法解决。

## 无效时继续定位

解除策略后的第一次请求可能仍超时；重开进程、重新请求后再判断。权限已放行而业务仍失败时，再按 [代理路径诊断](../network/proxy-path-diagnostics.md) 检查目标域名、DNS、VPN 规则与系统时间。

搜索与详情恢复只证明内容加载；下载、支付和 Play Integrity 需要各自的验证。不要用商店首页缓存或网络错误消失宣称完整性认证通过。

## 参考

- [Android：ADB 与 dumpsys](https://developer.android.com/tools/adb)
- [LineageOS：NetworkPolicyManager 策略定义](https://github.com/LineageOS/android_frameworks_base/blob/lineage-23.2/core/java/android/net/NetworkPolicyManager.java)；分支只用于理解字段，实际设备以其构建和输出为准。
