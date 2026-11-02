# DTB 合并与内核升级失败时保留启动文件

> `lsp-merge-dtb` 校验临时合并件后才替换 DTB；`zz-update-firmware` 准备完整启动文件后才发布，失败返回非零。

## TL;DR

- 单独合并 DTB：输出写入目标目录内的临时文件，`fdtoverlay`、`fdtget`、权限设置和同步全部成功后才 rename。
- 内核/initramfs 更新：新 `dtbs/`、`initrd.img`、`vmlinuz` 先暂存在启动分区；准备失败不修改当前启动文件。
- 发布失败会恢复旧文件；恢复失败保留事务目录并输出路径，不能把这次更新视为成功。
- 这是常规命令失败的保护，**不是三份启动文件在断电时的原子更新**。

## 合并输入与行为

`lsp-merge-dtb [BOOTFS]` 默认处理 `/boot/firmware`。配置是构建脚本生成、由 root 管理的
`lsp-dtb-merge.conf`，采用 shell 赋值语法：

```sh
BASE=rk3588-lubancat-5-v2.dtb
OUT=rk3588-lubancat-5-v2-lsp-merged.dtb
OVERLAYS="rk3588-lubancat-5-hdmi0-vp0-overlay rk3588-lubancat-5-v2-usbcon-host"
```

基础 DTB 位于 `dtbs/rockchip/`，overlay 位于其 `overlay/` 子目录，按清单顺序合入。
BASE、OUT、overlay 名称只允许简单文件名，OUT 不能等于 BASE。成功输出权限为 `0644`。
缺配置表示未启用合并，返回 0；配置存在但缺输入、工具或合并失败则返回非零。
失败时临时文件由退出/信号 trap 清理，原有 OUT 保留。运行依赖 `fdtoverlay`、`fdtget`
（`device-tree-compiler`，由镜像基础包清单安装并由 `tools/verify-image.sh` 检查）、
`flock`（`util-linux`）与 GNU coreutils。

构建期 DTB 合并不可用时，启动脚本会按板卡环境中的 `overlay_prefix` 和 overlay 名称，依次尝试
四种文件名形式，找到首个存在的文件后只应用一次。候选不存在不视为错误；所有候选均缺失或实际加载/
应用失败时才恢复基础 DTB。隔离回归也覆盖这些候选形式与回退路径。

## 升级钩子的准备、发布与恢复

`/etc/initramfs/post-update.d/zz-update-firmware <kernel-version>` 保留容器/chroot 守卫，
并确认挂载的是根磁盘的启动分区。钩子与独立合并命令对 BOOTFS 目录加非阻塞 `flock`，
阻止同时修改同一套启动文件。

1. 在 `/boot/firmware/.lsp-firmware.XXXXXX/new/` 准备内核包的 DTB、initrd 和内核。
2. 保留内核包未提供的自定义 overlay；同名文件使用新内核包版本。任何复制失败都中止。
3. 有合并配置时必须调用合并工具，工具缺失或合并失败都中止；检查 `ubuntuEnv.txt` 引用的 DTB/overlay 存在，DTB 可读。
4. 同步准备文件，再依次把现役文件 rename 到事务目录的 `old/`，把候选文件移到原位置。
5. 发布或最终同步失败时，逆序恢复旧文件；成功后清理事务目录。

若出现 `Rollback failed; recovery files retained at ...`，不要重启。该目录的 `old/` 保存尚未恢复的旧件，
`new/` 保存尚未发布或撤回的候选件；已恢复的旧件在原位置。先核对每个位置，恢复完整的
`dtbs/`、`initrd.img`、`vmlinuz` 并同步后再启动。脚本不会在回滚失败时删除这些恢复材料。

同文件系统 rename 可避免单文件写入截断，但目录交换和三份文件发布有多个步骤；
SIGKILL、断电或存储设备持续故障可能中断恢复。这种情况需检查保留目录，不能依赖 trap。

## 回归判据

```bash
python3 tests/dtb-update.py
```

测试使用真实 `dtc`/`fdtoverlay`/`fdtget` 与隔离目录，不触碰现役 `/boot`。
oracle 是“准备失败保留旧启动文件、发布失败恢复旧文件并返回非零”的不变量，
以及自定义 overlay 保留、同名使用新包的既有契约；真实 `RLIMIT_FSIZE` 限制验证写入失败不会截断 OUT。
复制、各发布步骤和恢复失败另用明确的命令故障注入覆盖。

2026-10-12 验证：本机与开发板（Ubuntu 24.04 arm64，内核 `6.1.172-9`）均为 `passed=39 failed=0`。
板上额外对现役启动分区运行真实合并的 `RLIMIT_FSIZE=128` 失败测试、缺内核包的准备失败测试，
两者均返回 1，旧内核/initrd/合并 DTB/配置的 SHA256 不变；正常合并与升级钩子均返回 0，启动文件 SHA256 也不变。
合并件属性仍为 `data-role=host`、`power-role=source`、`role-switch-default-mode=host`、HDMI0 `status=okay`，
输出权限 `0644`，无残留临时件。安装的两个脚本为 `root:root 0755`。

相关：[initramfs 生成链](initramfs.md)、[U-Boot 合并 DTB 的启动耗时](../hardware/boot-serial-log.md)。
