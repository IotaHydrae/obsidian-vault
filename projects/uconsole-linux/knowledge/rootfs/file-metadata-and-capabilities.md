# rootfs 复制：所有权、链接与 file capability

> rootfs 的正确性包含元数据；文件内容相同不能证明权限、链接和能力位仍然有效。

## TL;DR

- 保留 numeric UID/GID、权限、硬链接、符号链接、ACL 与扩展属性。
- `security.capability` 丢失会使普通用户功能失败；不要用 sudo 或放宽系统策略掩盖。
- 复制到 FAT 的固件与 ext4 rootfs 的语义不同，不能用一种复制方式处理全部文件。
- 本仓库导出器已有 rsync `-aHAX --numeric-ids` 路径；最终文件系统仍需独立核对。

## 每种属性保护什么

| 属性 | 丢失的后果 |
| --- | --- |
| UID/GID 和 mode | 服务账户不能读写，或意外扩大权限 |
| 符号链接 | usrmerge 加载器路径被实体目录覆盖，程序可能报 ENOENT |
| 硬链接 | 额外占空间，某些更新关系改变 |
| ACL | 非所有者的访问授权改变 |
| xattr/capability | 非特权程序无法执行原有网络或设备操作 |
| 稀疏布局 | 逻辑长度相同但空间占用大幅增长 |

数值 UID/GID 应按目标账户数据库解释，不能按宿主同名账户重新映射。
详见 [usrmerge 保护](../build/usrmerge-kernel-staging.md)。

## 归档和复制

GNU tar 归档与解包均需显式选择扩展属性；涉及能力位时核对包含 `security.*` 的策略。
例如 `--numeric-owner --xattrs --xattrs-include='*'` 体现这些维度，
具体 ACL、设备节点和权限需要结合权限及目标文件系统验证。
旧归档已丢失的元数据不能靠新解包选项补回。

rsync 的 `-a` 不包含全部 H/A/X 语义；需要时显式使用 `-aHAX --numeric-ids`。
不要跟随离线 rootfs 中指向宿主绝对路径的 symlink。
本仓库同设备导出直接使用原 rootfs 填充 ext4，跨设备路径通过保元数据的副本；
这不免除最终 ext4 镜像的属性检查。

## capability 的独立验收

在允许读取的源 rootfs 与只读挂载的最终镜像上比较 `getcap`，并检查 owner、mode 和 xattr。
不能只比较 SHA-256，因为摘要仅覆盖文件字节。
对某程序是否应有 capability，应取自其包 postinst/规范与项目需求；不要假定所有 ping 包都相同。
设备上普通用户的功能检查另行进行，离线检查不执行目标二进制。

## 构建器的检查点

1. Base 解包后保留元数据，安装程序不把宿主账户属性带入目标。
2. 内核/应用包安装尊重现有 usrmerge 路径。
3. 临时复制或归档完成后比较目标属性。
4. 最终文件系统导出后再次验收，验证中间目录不能代替最终镜像。

源码依据为 rpi_image.py（工程源码：`scripts/image_backends/rpi_image.py`） 的 _root_payload/export_image，
语义参考 [GNU tar 扩展属性](https://www.gnu.org/software/tar/manual/html_node/Extended-Attributes.html)。
本文没有报告本次新生成镜像的 capability 实测。
