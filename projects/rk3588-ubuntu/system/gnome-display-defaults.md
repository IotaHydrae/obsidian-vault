# GNOME 首次登录时为新 4K 屏幕设置 200% 缩放

> GNOME Wayland 登录时，单个新 4K 屏幕默认使用 200% 缩放，保持分辨率、刷新率与旋转不变。
> 已有 `monitors.xml` 中记录过的屏幕保留用户设置；后续由 GNOME 管理，不常驻监控显示器。

- 适用：Ubuntu 24.04 Noble、GNOME / Mutter 46、单屏维护桌面。
- 当前模式必须是原生 3840×2160 或 4096×2160；支持竖屏和旋转。
- 非 4K、非原生模式、未启用显示器、镜像及多屏组合均跳过。
- 本功能不修改 Cage kiosk 的显示配置。

## 登录与保存流程

`/etc/xdg/autostart/lsp-gnome-display-defaults.desktop` 仅在 GNOME 启动
`/usr/local/libexec/lsp-gnome-display-defaults`。程序核对 Wayland 会话，通过已有
`python3-gi` 调用 `org.gnome.Mutter.DisplayConfig`，完成后退出：

1. `GetCurrentState` 取得当前屏幕身份、模式、布局和支持的缩放比例。
2. 若用户 `~/.config/monitors.xml` 已含该屏幕的完整身份，保留其设置。
   身份由 connector、vendor、product、serial 组成，与 Mutter 的配置匹配方式相同。
3. 对尚无配置的单个原生 4K 屏幕，在该模式支持 2.0 时用临时配置设置 200%。
4. 回读确认模式、旋转、布局与缩放，再写标准 `monitors.xml`，之后交还 GNOME 管理。

新文件以 0600 权限原子创建，不能覆盖并发生成的文件。已有 v2 XML 只追加新屏幕配置，
原有字节保持不变，并先保存一份 `monitors.xml.lsp-defaults-*.bak`；替换使用原文件的
Gio etag，拒绝覆盖并发编辑。未知格式、无效 XML 和软链接均不改写。
写入失败时，只有显示配置仍等于刚应用的值，才恢复先前配置；发现用户已切换显示配置则不覆盖。

程序每次登录只执行一次。登录后新插入一台此前未记录的 4K 屏幕，需要重新登录才应用此默认值；
保存过的显示器由 Mutter 自己恢复。多屏与停用部分屏幕的组合仍使用 GNOME 的正常行为。

## 为什么不用全局缩放键

Mutter 46 的自动缩放依赖分辨率、EDID 物理尺寸与目标 DPI，4K 不保证自动得到 200%。
`org.gnome.desktop.interface scaling-factor=2` 是全局默认，不能只匹配 4K。
标准 `monitors.xml` 又要求精确显示器身份，不能预置任意型号的 4K 通配配置。

`ApplyMonitorsConfig` 的 `method=2` 会弹出显示变更确认，超时回退；本工具使用 `method=1`
临时应用并核对，再保存标准配置。XML 沿用 Mutter 自身生成的格式，不写目标版本不支持的
`layoutmode` 元素。目标默认无分数缩放实验特性，布局模式为 physical（D-Bus 值 2）；
启用实验特性后由 Mutter 按其默认 logical 布局加载配置。

显示器请求属性使用 `underscanning`，同时保留原来的开关状态。Mutter 46.2 的接口 XML 注释
仍写旧名 `enable_underscanning`，但实际解析函数只读取 `underscanning`；传旧名会按未指定
处理为 false。接口语义以目标实现 `create_monitor_config_from_variant()` 为准。

## 验证与边界

隔离回归：

```bash
/usr/bin/python3 tests/gnome-display-defaults.py
desktop-file-validate overlay/etc/xdg/autostart/lsp-gnome-display-defaults.desktop
```

回归使用真实 Gio 原子文件操作与 GVariant 序列化，显示服务使用临时夹具，不访问实际会话。
覆盖新屏幕默认、保留 100% 用户选择、非 4K、多屏、旋转、支持比例、精确备份、并发文件修改、
保存失败回滚，以及用户在执行过程中改变显示设置。不能据此宣称真实显示器或重新登录恢复通过。

维护桌面中可运行 `/usr/local/libexec/lsp-gnome-display-defaults --dry-run`，只读报告是否需要设置。
实际验收需要在新的 4K 显示器配置下登录，核对 DisplayConfig 中逻辑显示器 scale=2.0，
确认分辨率未变，再重新登录核对标准配置被恢复。已有用户配置的现场不能当作首次登录用例。

2026-10-23 验证：隔离回归 33 项通过，包含 Cage Wayland 环境无 D-Bus 时直接跳过且不生成配置，
以及按目标实现解析真实 GVariant 请求，检查
underscanning 开启 / 关闭的应用、回读、XML 与失败回滚；旧属性名的实现反证为 3 项失败。
开发板运行内核 #13、GNOME 46 Wayland，
AOC U27U3D 单屏在移走原用户配置后，由登录自启动真实生成 `monitors.xml`，
DisplayConfig 回读为 3840×2160@60、scale=2.0；再次登录仍恢复 200%。
已保存配置时自启动跳过，重复执行不改变文件。其他型号、4096×2160、竖屏及多屏的处理
仅完成隔离验证，尚未进行对应硬件测试；underscanning 开启的真实硬件路径也未板测，
上述现场的该选项为关闭状态。

本功能已进入构建指纹为 `2026-10-23T20:23:51+08:00` 的 `20261023` 镜像；离线核对
自启动入口、程序及权限、依赖、未预置显示器身份、无全局 200% 默认和 kiosk 配置通过。
该成品尚未烧录启动，上述开发板登录测试不能代替新镜像的首次登录验收。

## 依据

- [Mutter 46.2 `meta-monitor.c`](https://github.com/GNOME/mutter/blob/46.2/src/backends/meta-monitor.c)：
  `calculate_scale()` 的 DPI 推导、`meta_monitor_spec_equals()` 的精确身份匹配。
- [DisplayConfig 接口](https://github.com/GNOME/mutter/blob/46.2/data/dbus-interfaces/org.gnome.Mutter.DisplayConfig.xml)：
  `GetCurrentState`、`ApplyMonitorsConfig`、缩放支持、确认与布局模式编号。
- [请求实际解析](https://github.com/GNOME/mutter/blob/46.2/src/backends/meta-monitor-manager.c#L2490)：
  `create_monitor_config_from_variant()` 的 `underscanning` 键与默认 false 行为；其名称与上述 XML 注释不同。
- [配置存储](https://github.com/GNOME/mutter/blob/46.2/src/backends/meta-monitor-config-store.c)：
  v2 XML、刷新率、旋转、underscanning 与默认布局解析。
- [native 布局默认](https://github.com/GNOME/mutter/blob/46.2/src/backends/native/meta-monitor-manager-native.c#L509)。
- 相关入口：[维护桌面会话](desktop-session.md)、[系统集成总账](system-integration.md)。
