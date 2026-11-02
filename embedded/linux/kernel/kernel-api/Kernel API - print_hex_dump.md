# Linux `print_hex_dump()`

> `print_hex_dump()` 将二进制缓冲区按行写入内核日志；只需格式化一行到字符串时使用 `hex_dump_to_buffer()`。

```c
print_hex_dump(KERN_DEBUG, "data: ", DUMP_PREFIX_OFFSET,
               16, 1, buf, len, true);
```

参数依次指定日志级别、前缀、地址或偏移类型、每行字节数、分组大小、缓冲区、长度及是否显示 ASCII。常用 `rowsize` 为 16 或 32，`groupsize` 为 1、2、4 或 8；大于 1 字节的分组结果需注意目标字节序。

`hex_dump_to_buffer()` 只处理一行，返回值可用于判断目标字符串缓冲区是否截断；具体空缓冲区和返回值行为以目标内核的 `lib/hexdump.c` 为准。避免打印密钥、个人信息或大量高速数据，必要时使用条件日志或限流。
