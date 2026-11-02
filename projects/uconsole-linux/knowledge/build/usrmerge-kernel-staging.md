# 内核候选部署：保护 usrmerge 的 /lib 链接

> 含 `lib/` 目录的 tar 包直接解到系统根目录，可能替换 `/lib -> usr/lib`，使所有动态程序无法启动。

## TL;DR

- 先检查目标的 `lib` 类型；内核模块应写到现有链接指向的模块目录，不替换链接本身。
- 诊断内核只使用全新的 release 和镜像名，预检整个包后再写入；默认启动配置保持独立。
- SSH 已认证但 shell 报 `No such file or directory` 时，同时检查 ELF 解释器路径。
- 链接损坏后停止重启/部署；从另一台机器离线检查 SD 根分区，保留错误目录再恢复链接。

## 候选文件暂存方法与不变量

1. 使用独立 release 的模块目录、独立候选 Image、config 和 System.map。
2. 在任何目标写入前检查全部归档成员：路径范围、重复项、文件类型、链接与冲突。
3. 只复制允许的普通文件；忽略归档目录的元数据，不在根目录调用通用解包。
4. 保留目标的 usrmerge 链接、已有文件权限和默认启动输入。失败时移除本次创建的内容。
5. 文件暂存、生成匹配 initramfs、单次启动选择和恢复验证分别处理。
   文件复制成功不能证明候选可启动或可恢复。

工程的[离线候选安装工具](../../project/kernel-candidate.md)落实上述文件写入边界：
默认预览，显式 `--apply` 才向离线根目录添加全新的候选文件；拒绝宿主 `/`。

`tar --keep-directory-symlink` 可改变目录链接处理，但仍不会约束归档载荷、覆盖范围
或默认启动文件；不能作为完整的候选部署流程。

## 根因与识别

Ubuntu 的 usrmerge 布局使用相对链接 `lib -> usr/lib`。ARM64 动态 ELF 的
解释器通常为 `/lib/ld-linux-aarch64.so.1`；即使 `/bin/bash` 和库文件仍在磁盘上，
解释器路径失效也会让 `execve()` 返回 `ENOENT`。

GNU tar 的默认提取会用归档中的真实目录替换已有的同名目录符号链接。归档先创建
`lib/`、再写入 `lib/modules/<release>/` 时，原有库仍留在 `usr/lib/`，但 `/lib`
只剩候选模块。SSH 认证进程可能尚在运行，不能据此判断新命令仍能启动。

2026-10-25 的 CM4 诊断部署出现上述症状；临时离线根目录复现同时观察到：
`lib` 不再为链接、原库内容保持、模块落入替换后的目录。生产镜像构建器的
Debian 包安装流程没有被此复现判定为存在相同问题。

## 离线恢复

先确定挂载的是目标 SD 根分区，检查实际布局及原库内容，避免在开发机 `/` 上操作：

```sh
rootfs=/path/to/mounted-sd-root
ls -ld "$rootfs/lib" "$rootfs/usr/lib"
ls -l "$rootfs/usr/lib/ld-linux-aarch64.so.1"
```

仅当确认原布局为 usrmerge、错误 `lib` 为本次新增目录、原解释器存在时：

```sh
sudo mv -T --no-clobber -- "$rootfs/lib" "$rootfs/lib.candidate-backup"
test ! -e "$rootfs/lib" && test ! -L "$rootfs/lib" &&
    sudo ln -s -- usr/lib "$rootfs/lib"
```

备份目标必须不存在；若移动未发生，不能继续建链接。恢复后核对链接、解释器和
默认启动文件，再同步并卸载。不要在证据不符时删除目录、复制另一套 libc 或重刷系统。
本记录给出恢复方法，当前设备的恢复结果见[开发状态](../../project/status.md)。

## 证据

- `~/.cache/uconsole-desktop-20261025/powerkey/usrmerge-tar-reproduction.json`：离线复现。
- 同目录 `s2idle-r1/device-stage.log`、`HANDOFF.md`：设备失败与恢复边界。
- GNU tar 手册的 directory symlink extraction 和 `--keep-directory-symlink` 语义。
