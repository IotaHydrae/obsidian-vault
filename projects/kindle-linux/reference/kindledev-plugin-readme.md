# Kindle Linux 插件起点

这是一个直接在 KOReader 中运行的 Lua 应用，无需启动 Alpine。

- `main.lua`：菜单、可绑定手势的 Dispatcher 动作、中文按钮面板和设备信息。
- `kindledevstate.lua`：计数状态、恢复与按需保存，不依赖界面组件。
- `_meta.lua`：插件名称和说明。
- `usb-debug.sh`：只读 USB 诊断与手动会话启动器。

打开面板，点击“计数 +1”或“归零”；“返回阅读器”和点击面板外都会关闭并保存。
数据通过 KOReader 的 LuaSettings 保存在其用户设置目录下的 `kindledev.lua`。
面板打开期间发生的突然掉电可能丢失尚未保存的计数。

本机工程目录中的开发流程：

```sh
make app-check-local
make app-install
make app-check
make app-preview
```

`app-preview` 仅在 KOReader 未运行时，通过现有启动脚本打开应用预览。
若阅读器已运行，从工具菜单进入；更新代码后需要退出并重新打开 KOReader。
`app-install` 不自动重启。真实触摸测试仍需在设备上点击按钮。

修改业务时先改 `kindledevstate.lua`，同步补充 `tests/lua/kindledev_spec.lua` 的状态契约；
调整控件与交互时改 `main.lua`。本例没有定时刷新或后台任务。
`KINDLEDEV_PREVIEW=1` 是显式开发预览选项，正常启动不会自动打开面板。
完整说明见工程的 `docs/koreader-plugin.md`。

0.2.0 新增“USB 有线连接”：先拔线，在插件内点击“启用并退出阅读器”，
等待屏幕提示就绪再连接。启动前检查不满足会留在阅读器；连接能力仍待实机验证。
也可只导出诊断，不改变 USB 模式。完整边界见 `docs/usb.md`。
