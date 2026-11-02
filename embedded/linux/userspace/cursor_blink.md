# 关闭 Linux fbcon 光标闪烁

> framebuffer 控制台提供 `cursor_blink` 属性时，写入 `0` 关闭闪烁，写入 `1` 启用。

```bash
cat /sys/class/graphics/fbcon/cursor_blink
echo 0 | sudo tee /sys/class/graphics/fbcon/cursor_blink
# 恢复闪烁：
echo 1 | sudo tee /sys/class/graphics/fbcon/cursor_blink
```

此接口只影响对应 fbcon，不控制桌面或应用内的光标；节点是否存在依赖内核配置和当前控制台。以上为运行时设置，开机持久化需由目标系统另行配置。
