# 墨水屏与第三方应用

TL;DR：已经验证 Alpine Python → 原系统 FBInk → 中文页面的显示链路。
需要触摸界面时，优先在 KOReader 中写 Lua 插件；独立 Alpine 应用目前只提供单次文字绘制。
原系统、内核及启动链保持不变。

## 立即运行显示示例

在本机工程目录执行，先退出已有 Alpine shell：

```sh
make display-demo
```

程序将本地示例更新到 Kindle 用户区，在 Alpine 中运行 Python，再由宿主 FBInk
显示 UTF-8 内容。命令结束后 chroot 会释放挂载。屏幕页面可以继续保留，
但厂商界面仍在运行，随时可能重绘覆盖它。这个演示不接管输入或长期占有屏幕。

设备上也可从原系统 SSH shell 执行，不依赖本机生成页面：

```sh
/bin/sh /mnt/us/alpine/display.sh
```

内容源文件：`apps/hello/main.py`。修改它的 `page()`，再运行 `make display-demo`
就能看到变化。`image/display.sh` 是宿主绘制桥，调用已安装的 FBInk 和中文字体。

```python
# Alpine 应用只输出内容，不需要访问 /dev/fb0。
print("你好，Kindle！\n这是我的应用。")
```

当前桥使用 KOReader 自带 NotoSansCJKsc 字体、28 像素字号和 32 像素边距。
文字由 Python 输出，交给 FBInk 做字体排版和一次全屏刷新。它没有每秒刷新循环。
需要图片时，应另行核实绘制后端；本机已有 FBInk 构建标明 Image=No，
不能直接套用完整 FBInk 构建的图片命令。

## 为什么把绘制留在宿主

本机读到的 FBInk 是 ARM ELF，动态解释器为 `/lib/ld-linux-armhf.so.3`，
依赖 Kindle 的 glibc 环境；Alpine 使用 musl，不能只复制这份二进制就直接运行。
采用宿主桥不必向 chroot 暴露 framebuffer 或 eMMC 块设备，也不用替换系统库。

实际调用关系：

```text
SSH 手动运行 display.sh
  → Alpine Python 生成 UTF-8
  → 退出 chroot 并释放镜像
  → Kindle 宿主 FBInk 渲染文字
  → framebuffer + EPDC 更新请求
  → 墨水屏显示
```

墨水屏的像素缓冲和面板刷新是两步。只修改 `/dev/fb0` 中的内存，不能据此保证
面板已经更新。FBInk 负责相应驱动接口；示例使用 `-w` 等待刷新结束。
本机有效图像为 600×800、8-bit Y8，行跨度是 608 字节，截图必须去掉每行填充。

```sh
make display-probe
python3 tools/display.py capture --output work/current-screen.png
```

截图是 framebuffer 的读回，不是摄像头拍摄，不能单独证明面板的光学显示质量。

## 带触摸的 KOReader 插件

工程已有 `apps/koreader/kindledev.koplugin/`：

- `_meta.lua`：插件名称和说明。
- `main.lua`：菜单、中文面板、计数 / 归零 / 设备信息 / 返回。
- `kindledevstate.lua`：计数状态与按需保存。

```sh
make app-install
```

安装到 `/mnt/us/koreader/plugins/kindledev.koplugin/`，不改原插件。
退出并重新打开 KOReader 后，在工具菜单中查找“Kindle Linux 示例”；
具体子菜单取决于所用 KOReader 版本和菜单配置。

插件直接运行在 KOReader Lua 环境；它不是 Alpine 程序。使用 UIManager、
ButtonDialog 等现有组件可以复用触摸分发、字体和阅读器刷新流程。
关闭示例对话框会返回阅读器；无需停止 Kindle 系统服务。

新版已补齐状态保存、设备信息与测试，并通过 USB 存储部署和读回校验；
尚未验证设备端加载和真实触摸。当前开发流程见 [KOReader 插件](koreader-plugin.md)。
后续可以让这个插件负责界面、让 Alpine 中的 Python 负责数据处理；
跨环境调用应异步处理并定义超时，避免在 UI 回调中等待长任务。

## 选择开发路线

| 应用类型 | 起点 | 尚需工作 |
| --- | --- | --- |
| 中文菜单、按钮、阅读辅助 | KOReader Lua 插件 | 业务界面和真实触摸验证 |
| 日历、状态页、联网信息展示 | Alpine Python + 宿主 FBInk | 数据获取、布局、刷新策略 |
| 自定义全屏触摸程序 | 独立渲染 + input 事件 | 前后台所有权、触摸坐标、退出和恢复界面 |

设备已枚举到触摸屏 `cyttsp5_mt`，本次对应 event2；不能把编号当成所有启动都固定的接口。
独立全屏程序需要自行处理输入、与现有界面的竞争及退出恢复，当前示例尚未实现这些。
初期更适合在阅读器插件中验证交互，再按需要扩展独立程序。

## 验证与参考

2026-10-29 已观察到 FBInk 成功渲染全部 15 行中文内容，等待更新完成，
600×800 截图读回布局正确；原系统启动参数和只读根挂载一致，chroot 无遗留关联。
本地结果在忽略的 `work/display-check.json`、`work/display-demo.png`。
离线测试覆盖截图 stride / offset、PNG 校验和及探测数据解析。

本机现有 FBInk 版本为 92e1270。官方接口文档：
https://github.com/NiLuJe/FBInk 。KOReader 当前安装版本的插件示例和
`frontend/ui/widget/buttondialog.lua` 是本工程插件 API 的直接参考。
