# 配置 Linux core dump 的保存位置

> `kernel.core_pattern` 控制 core dump 的文件名或处理程序；`ulimit -c` 控制当前 shell 及其子进程的 core 文件大小限制。

## 查看当前配置

```bash
ulimit -c
cat /proc/sys/kernel/core_pattern
```

若 `core_pattern` 以 `|` 开头，core 数据交给配置的处理程序，不直接使用下面的文件路径。

## 临时配置示例

确保目标目录已存在且具备相应权限，然后执行：

```bash
ulimit -c unlimited
sudo sh -c 'echo /mnt/extsd/coredump-%e-%p-%s-%t > /proc/sys/kernel/core_pattern'
```

- `core_pattern` 是系统级设置，通常需要管理员权限；上述修改未配置为持久化。
- 是否产生 core 文件还受进程权限、文件系统及系统配置影响。修改后重新查看这两项配置；不要在重要进程上制造崩溃进行验证。
