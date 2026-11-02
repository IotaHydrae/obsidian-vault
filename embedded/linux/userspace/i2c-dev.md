# Linux 用户空间访问 I2C

> 打开 `/dev/i2c-N` 并选择从设备后，按器件手册和 adapter 支持的事务类型读写；强制占用可能与内核驱动冲突。

```c
int fd = open("/dev/i2c-0", O_RDWR);
if (fd < 0)
    return -1;
if (ioctl(fd, I2C_SLAVE, 0x50) < 0) {
    close(fd);
    return -1;
}
/* 使用设备规定的 read/write、I2C_RDWR 或 SMBus 操作。 */
close(fd);
```

需要 `<fcntl.h>`、`<sys/ioctl.h>`、`<unistd.h>` 和 `<linux/i2c-dev.h>`；先通过 `I2C_FUNCS` 确认 adapter 能力。`I2C_SLAVE_FORCE` 可绕过已绑定驱动的保护，可能破坏状态或数据，不应作为默认选项。

## 器件差异

- AT24C16 / AT24C02 EEPROM 的写页大小、地址编码、写周期和 ACK polling 需按具体型号手册确认；写入会改变持久数据，先备份并选定安全区域。
- BH1750 的读事务须匹配器件协议，不能仅因结果为两个字节就使用 SMBus word-data。按手册确定命令、转换等待时间、读事务、字节序和量程系数，并初始化所有请求字段。

参考图：[EEPROM 写序列](https://img2022.cnblogs.com/blog/2605173/202202/2605173-20220228093544279-1578358120.png)、[BH1750 量程](https://img2022.cnblogs.com/blog/2605173/202205/2605173-20220529152442996-345380276.png)、[BH1750 示例](https://img2022.cnblogs.com/blog/2605173/202205/2605173-20220529153136770-1384660734.png)。
