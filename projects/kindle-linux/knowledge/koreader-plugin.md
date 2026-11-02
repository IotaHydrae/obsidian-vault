# 从 KOReader 插件开发应用

TL;DR：工程以 `kindledev.koplugin` 为第一个交互应用。
使用 KOReader 的字体、触摸、对话框与墨水屏刷新；不依赖 Alpine，不改 Kindle 启动流程。
当前新版已通过 USB 存储部署并完成读回校验；已由用户打开面板、导出诊断和启用 USB；自动真实界面预览与截图也已验证，其余手工触摸覆盖仍待完成。

## 界面与状态

工具菜单入口：“Kindle Linux 示例”。也注册了“打开 Kindle Linux 示例”动作，
可在 KOReader 自己的手势配置中选择。

| 控件 | 行为 |
| --- | --- |
| 计数 +1 | 修改内存状态，更新原有对话框标题 |
| 归零 | 将计数设为 0 并刷新显示 |
| 设备信息 | 显示设备型号、分辨率、KOReader 版本和插件版本 |
| 返回阅读器 | 保存计数，关闭面板并交还原界面 |
| 点击面板外 | 使用相同的关闭和保存流程 |

计数保存在 KOReader 的用户设置目录 `settings/kindledev.lua`。
重复打开不会创建多个面板；没有变化时不重复写入设置。
关闭面板或接到 KOReader 的保存设置事件时写入，突然掉电可能丢失未保存计数。

## 代码组织

| 文件 | 负责内容 |
| --- | --- |
| `apps/koreader/kindledev.koplugin/main.lua` | 菜单、Dispatcher、控件与事件回调 |
| `apps/koreader/kindledev.koplugin/kindledevstate.lua` | 可独立测试的状态与设置保存 |
| `apps/koreader/kindledev.koplugin/_meta.lua` | 插件名称和说明 |
| `tests/lua/kindledev_spec.lua` | 状态和界面回调的行为契约 |
| `tools/plugin.py` | 安装、行为检查、显式预览及离线回执比较 |

先在 state 中实现业务逻辑，再由 main.lua 回调调用它，避免把业务和绘制混在一起。
当前只使用受控内存与本地设置；没有网络请求、外部命令或长时间任务。
未来需要 Alpine 或联网时，另行设计异步任务、超时及取消，避免阻塞 UI 线程。

## 本机开发流程

```sh
make app-check-local
make check
make app-status
```

本机 Lua 测试优先使用现有解释器；没有命令行解释器时，使用系统已安装的
liblua5.3 / liblua5.4，通过 Python 标准库 ctypes 运行纯 Lua 测试。
无需安装开发包。所有 UI 和存储接口均为明确的测试替身，不接触 Kindle。
如果本机缺少这些运行时，`app-check-local` 明确失败；通用 unittest 报告该项跳过。

已执行 35 条 Lua 行为断言，涵盖非法持久化值、计数/归零/保存、避免多余写入、
菜单与 Dispatcher、重复打开、标题更新、信息弹窗、关闭清理和保存事件。
这些测试不等于真实屏幕排版或触摸硬件测试。

## 部署到设备

先确认 `.device.toml` 中配置的 SSH 可达，再执行：

```sh
make app-install
make app-check
```

安装器把完整代码先放到不被 KOReader 当作插件加载的 staging 目录，
检查每个 Lua 文件语法和 SHA256 后才替换插件目录。旧目录保留在
`/mnt/us/koreader/settings/kindledev-backups/` 下；发布失败会尝试恢复旧目录。
现有 `settings/kindledev.lua` 保留，不随代码更新覆盖。

安装回执为忽略的 `work/plugin-installation.json`，包含源码哈希和原系统状态检查。
设备上的 `app-check` 使用 Lua 5.1 和同一个行为测试脚本，仍以替身 UI/存储测试，
不修改真实计数、不绘制、不启用服务。它是显式设备操作，不属于离线 make check。

`make app-status` 只比较本机源码与保存的回执，不连接设备。回执缺失或哈希过期时
报告不匹配，不把旧版本安装当作新版已部署。

## 查看真实界面

KOReader 未运行且没有待安装的阅读器更新包时，可执行：

```sh
make app-preview
```

它通过设备上已有的 koreader.sh 启动阅读器，并仅为这次进程设置预览环境变量。
插件初始化后打开面板；通过本次唯一标记确认加载，避免把旧日志当作启动成功。
不写入开机启动配置，不重启 Kindle，也不强行退出已运行的 KOReader。
正常启动 KOReader 时面板不会自动弹出。

若 KOReader 已运行，先从界面正常退出再预览，或直接使用工具菜单进入。
代码更新后也需要退出并重新打开阅读器，让 Lua 加载新版本。

预览命令保存 framebuffer 截图到 `work/koreader-plugin.png`，结果在
`work/plugin-preview.json`。截图证明缓冲区内容，触摸需要实际点击验证。

## 当前验证边界

2026-10-29：本机 32 项工程测试通过，其中包含 35 条 Lua 行为断言；
更新成功保留旧版本、发布失败回退及拒绝符号链接目标的文件系统测试通过。

新版已通过 USB 存储模式安装，读回 SHA256 一致，暂存代码的本机 Lua 行为测试通过。
已通过 USB 在设备 Lua 中执行行为检查，并启动真实界面、查看读回截图。手工触摸仅覆盖已报告的操作；
后续远程验证可使用恢复后的 Wi-Fi 或配置完成后的 USB 网络。详见 [USB 开发](usb.md)。

0.2.0 新增 USB 子面板与用户区 helper，安装器会同时校验 Lua 和 shell 文件。
前置条件未通过时不退出或启动 helper；通过后沿 KOReader 正常 Exit 事件保存退出。
设备端手动启用及 USB root SSH 已验证，步骤和日志路径见 [USB 开发](usb.md)。

设备日志已确认 v0.2.0 面板成功显示，用户通过真实触摸导出了 root 诊断。
这验证了面板加载和诊断按钮路径；计数/归零等其余按钮仍仅有行为测试证据。
USB 诊断发现原生脚本缺失，后续步骤见 USB 文档。

0.2.1 已在本机修正线缆检测：使用官方 BD71827 驱动定义的外部电源状态，
保留连接/未知时拒绝启动的逻辑，新增 10 项测试。已通过 USB 部署并核对版本及文件哈希，手动启用和实际 SSH 登录已验证。

USB 连接当前已验证，默认工程命令使用端口 2223。真实 app-preview 已打开面板并保存
work/koreader-plugin.png；设备 Lua 行为断言仍使用替身 UI，与真实渲染证据分别记录。
