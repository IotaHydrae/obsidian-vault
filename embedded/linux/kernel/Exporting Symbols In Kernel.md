# 内核符号导出与模块引用

> `extern` 声明只让编译器知道符号类型；可加载模块引用其他对象的符号时，还需要该符号被内核导出。

```c
int __i2c_first_dynamic_bus_num;
EXPORT_SYMBOL_GPL(__i2c_first_dynamic_bus_num);
```

- 内建代码之间的非 `static` 全局符号通常由最终链接解析，不因此要求 `EXPORT_SYMBOL`；定义、配置和链接顺序仍须满足实际构建条件。
- 模块引用未导出的符号，通常在 `modpost` 报错或警告；若构建仍产出模块，加载时会出现 `Unknown symbol`。
- `EXPORT_SYMBOL_GPL` 对模块的 GPL 兼容许可证声明有要求。是否可以使用还取决于符号命名空间、版本及构建配置。
- `/proc/kallsyms` 中能看到某个名称，不代表该符号已向模块导出。应结合目标内核源码、`Module.symvers`、构建日志及 `dmesg` 检查。

此处以 I2C 变量说明导出用法；它的实际定义和导出状态以目标内核源码为准。
