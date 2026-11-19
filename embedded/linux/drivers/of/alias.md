# Linux 设备树 aliases 与设备编号

> `/aliases` 将简短名称关联到设备树节点；编号是否影响 Linux 设备名由具体子系统或驱动决定。

## TL;DR

- 常见形式是 `serial0 = &uart0`、`i2c2 = &i2c_controller`；数字属于别名，不能直接当成硬件寄存器地址。
- `of_alias_get_id(np, "serial")` 按节点和前缀查找编号；找不到时返回负错误码。
- 别名本身不会创建设备、启用硬件或强制所有驱动使用该数字；还需检查节点状态、总线和驱动实现。
- `/chosen/stdout-path` 可以引用串口别名或路径；具体 console 参数及驱动注册仍需配合。

范围：Linux 6.18 LTS，使用设备树的系统。

## 表达与读取

下面只说明语法，`uart_a` 和 `i2c_a` 应替换为实际 DTS 中存在的节点标签：

```dts
/ {
    aliases {
        serial0 = &uart_a;
        i2c2 = &i2c_a;
    };
    chosen {
        stdout-path = "serial0:115200n8";
    };
};
```

设备树编译后别名记录的是目标路径；驱动拿到 `struct device_node *` 后可调用 `of_alias_get_id()`。返回 `0` 是合法编号，不表示失败；Linux 6.18.62 中找不到匹配返回 `-ENODEV`。是否将该 ID 用于 adapter、串口等编号需看具体注册路径。

## 排查编号变化

1. 确认运行时 DTB 的 `/aliases` 和目标节点，而不是只检查源码文件；同时确认 bootloader 没有加载另一份 DTB。
2. 核对驱动是否调用别名查找接口，或由所属子系统采用其他编号规则。
3. 检查编号冲突、节点启用状态和设备创建顺序；不要仅靠 `/dev/tty*` 或 `/dev/i2c-*` 数字识别硬件。
4. 串口输出还要核对 `/chosen/stdout-path`、内核命令行、驱动和 console 设置。

设备树节点与设备创建见 [[platform|Linux 设备树与 Platform 设备]]；串口路径见 [[Linux 串口与 TTY 数据路径]]。

## 参考

- Linux 6.18.62：[`drivers/of/base.c`](https://github.com/gregkh/linux/blob/v6.18.62/drivers/of/base.c) 的 `of_alias_scan()`、`of_alias_get_id()`。
- [设备树使用模型](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/devicetree/usage-model.rst)；[蜗窝科技 Device Tree](https://www.wowotech.net/device_model/dt_basic_concept.html)。
