# 通过 Configfs 配置 USB Gadget

> Configfs 负责组合内核已有的 gadget function；自定义 vendor 协议仍需内核或用户空间提供实际 function 实现。

## 前置检查

```bash
ls /sys/class/udc
mount | grep configfs
```

需要具备支持设备模式的 UDC、启用 configfs / libcomposite 及所选 function 的内核配置，且控制器未被其他 gadget 占用。VID/PID 必须有权使用；不要把 Linux Foundation 的 VID 当作自己的产品标识。

## 配置顺序

1. 在 `/sys/kernel/config/usb_gadget/` 下创建 gadget，设置 VID、PID、设备及字符串描述符。
2. 创建 configuration 和对应语言的配置字符串目录。
3. 创建目标内核实际支持的 function，例如按具体实现配置 FunctionFS 或已有的串口 function，再通过符号链接加入 configuration。
4. 确认应用和端点均已准备好，最后将明确选定的 UDC 名称写入 `UDC`；在**另一台 USB 主机**上检查描述符并测试协议。
5. 清理时先解除 UDC 绑定，再按依赖顺序移除链接、function、configuration 和 gadget，遵循 configfs 语义；不要对 configfs 使用 `rm -rf`。

`functions/<类型>.<实例>` 必须对应已实现的 function，端点和类描述符由该实现提供；设置 `bInterfaceClass` 不会自动创建 vendor function 或 bulk 端点。串口场景见 [[g_serial - systemd getty@ttyGS0]]。

参考：[Linux Gadget Configfs 文档](https://docs.kernel.org/usb/gadget_configfs.html)。
