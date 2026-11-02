# labwc 截图门户的运行时依赖

> labwc 的 wlr 截图门户需要 grim 和 slurp，不能以 ScreenCast 成功代替。

## TL;DR

- 两套桌面显式安装 `grim` 和 `slurp`，核对实际 Screenshot 后端选择。
- 发起真实截图请求，完成区域选择，并验证 response=0、URI 和已知窗口内容。
- ScreenCast 走 PipeWire；取色、截图和共享分别设定判据，CM5/其他平台另验。

## 功能验证方法

判据是同一个普通用户客户端完成门户请求，取得可读取的 PNG URI，并在图像中确认受控窗口内容；
交互截图还必须经过真实选区。服务进程、工具存在、返回成功或任意桌面图像均不足以单独通过。
取色应在受控纯色区域选择实际像素，将返回颜色与已知输入比较，不能仅检查返回值范围。
缺少工具时的后端错误可能由前端映射为不同响应，不将某个错误码当作固定规格。

该版后端将截图写入固定 `/tmp/out.png`。探针必须先保存已有文件，退出时只清理自己的产物并恢复原件；
不能通过删除用户截图或清空共享目录来准备测试。

## 原因：wlr 后端调用外部截图工具

门户配置（工程源码：`image/desktops/payload/etc/xdg/xdg-desktop-portal/labwc-portals.conf`）
声明 `org.freedesktop.impl.portal.Screenshot=wlr`。Ubuntu Noble 的
`xdg-desktop-portal-wlr 0.7.1-1build2` 对应上游
[v0.7.1 screenshot.c](https://github.com/emersion/xdg-desktop-portal-wlr/blob/v0.7.1/src/screenshot/screenshot.c)：

| 功能 | 后端实际调用 |
| --- | --- |
| 非交互整屏截图 | `grim -- <path>` |
| 交互区域截图 | `grim -g "$(slurp)" -- <path>` |
| `PickColor` 取色 | `slurp` 选择点，再由 `grim` 读取该点像素 |

此前配方只安装门户服务，没有声明两个工具；保留的导出根文件系统也缺少它们。
实机逐步部署时已有 `grim`，但缺少 `slurp`，因此不能用实机某条成功路径推断新镜像完整。
公共软件包清单（工程源码：`image/desktops/recipe.json`）现为两套桌面显式加入两者，
不需要重编译合成器、门户或内核。

## CM4 实机观察（2026-10-24）

在 Ubuntu Base 24.04.5 的既有 SD 安装中，`grim 1.4.0+ds-2build2` 已存在，
`slurp` 未安装。Waybar 会话的公共交互截图请求返回 response 2、无 URI；
仅安装 `slurp 1.5.0-1` 后，同一路径返回 response 0 和 `file:///tmp/out.png`。
该 PNG 可读取，并可见选区内带标题的受控测试窗口。

恢复 Noctalia 会话后，交互截图同样返回有效 PNG，图中可见受控窗口。
另以静态纯红窗口作为已知输入，通过真实选点调用 `PickColor`，返回 RGB 精确为 `(1, 0, 0)`。
这支持当前 Noctalia 取色路径，不扩大为两套外壳的全部门户功能均已验证。

本轮开始时 `/tmp/out.png` 不存在，结束时只删除自有截图产物，仍不存在。
本机非发布证据在 `~/.cache/uconsole-desktop-20261024/functional/waybar/`：
`screenshot-before-slurp.log`、`screenshot-after-slurp.log`、`screenshot-titled.png`、
`noctalia-screenshot.png`、同次已知输入的 `static-red-final.png`、
`device-evidence/pickcolor-final.log` 及运行/清理日志。
两个镜像准备计划均包含 `grim`、`slurp`，没有重新导出或刷写镜像。

## 验证边界

当前功能验证对象是 CM4 的既有 Ubuntu SD 安装；实机补装与构建配方分别记录。
已导出的镜像指纹和是否包含本轮依赖，以[开发状态](../../project/status.md)为准。
非沙箱门户探针不代表浏览器、OBS、Flatpak 应用或远程会议端已经验证。
