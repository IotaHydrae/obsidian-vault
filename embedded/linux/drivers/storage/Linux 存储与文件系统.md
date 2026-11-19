# Linux 存储与文件系统

> 排查存储问题先确认设备类型与层次：闪存芯片、块设备、分区、文件系统、页缓存和应用数据各有不同的错误边界。

## 从介质到文件

```text
应用 read / write / fsync
→ VFS 与具体文件系统 → 页缓存及块 I/O
→ 块设备驱动 / MMC 等控制器 → eMMC、SD 或其他介质
```

这是常见块设备路径，不覆盖所有 I/O 模式。裸 NAND / NOR 可能走 MTD、UBI、UBIFS 等不同路径，不能把 eMMC 和裸 NAND 当成相同接口。

| 层次 | 关注什么 |
| --- | --- |
| MMC host 与 card | 控制器、供电、时钟、总线宽度、信号完整性及卡初始化 |
| eMMC / SD | 设备内部介质管理和协议；容量、分区、健康信息依产品而异 |
| 块设备与分区 | 请求和设备边界、分区表、挂载关系；设备节点名可能变化 |
| 文件系统 | 元数据、日志或检查点、空间与恢复语义，依具体文件系统而异 |
| 页缓存 / 写回 | 应用写入成功不一定已持久化；缓存策略和错误传播影响结果 |

`fsync()` 是应用请求持久化的重要接口，但整体可靠性仍取决于文件系统、块层、设备缓存、flush 支持和硬件供电。对文件创建、重命名等操作，目录元数据也可能需要单独同步；应按目标文件系统与应用协议验证断电恢复。

## 排查顺序

1. 从 `dmesg` 确认错误出现在控制器、介质、块 I/O 还是文件系统；同时记录设备路径和时间。
2. 用 `lsblk -f`、`findmnt` 判断设备、文件系统和实际挂载关系；不要仅凭 `/dev/mmcblkN` 的数字推断硬件身份。
3. 检查供电、时钟、总线模式、连接及温度；再按设备和文件系统的工具确认健康信息和完整性。
4. 涉及修复、格式化或写入实验前，先保护需要的数据，并确保没有其他挂载或主机同时写入。

## 参考

- 蜗窝科技：[MMC 框架](https://www.wowotech.net/comm/mmc_framework_arch.html)、[eMMC 与 Flash](https://www.wowotech.net/basic_tech/emmc_intro.html)、[F2FS](https://www.wowotech.net/filesystem/f2fs.html)、[文件系统栏目](https://www.wowotech.net/sort/filesystem/)。
- Linux 6.18.62：[VFS](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/filesystems/vfs.rst)、[MMC](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/driver-api/mmc/index.rst)。
