# Linux VFS、Page Cache 与写回

> VFS 将路径、文件对象和具体文件系统操作连接起来；普通缓冲 I/O 常先经过 page cache，数据持久化还需要文件系统和块设备配合。

## TL;DR

- pathname、dentry、inode、`struct file` 和进程文件描述符是不同对象，生命周期和共享关系不同。
- 页缓存通常属于 inode 的 `address_space`；普通读写和文件 `mmap()` 可以访问同一文件缓存。
- `write()` 成功不等于数据已经落到可靠介质；脏页写回、错误报告、`fsync()` 和设备 flush 需要分开分析。
- `O_DIRECT` 的约束和实现依文件系统而异；绕过部分页缓存数据路径也不自动保证持久化。

范围：Linux 6.18.62 LTS，普通文件和常见块设备文件系统；DAX、网络文件系统和特殊文件可能有不同路径。

## 对象与查找

```text
进程 fd → struct file → path（mount + dentry）→ inode
                                           ↓
                                  address_space / page cache
```

| 对象 | 主要作用 |
| --- | --- |
| fd | 进程的文件描述符表索引，可指向打开文件对象 |
| `struct file` | 一次打开所关联的状态、操作、偏移等；多个 fd 可共享同一个打开文件对象 |
| dentry | 路径分量与 inode 关系的缓存对象；路径查找还受 mount namespace 影响 |
| inode | 文件系统对象的元数据与操作入口；不等于某一个完整路径 |
| `address_space` | 文件缓存及映射相关状态；缓存以 folio 等内存对象组织 |

硬链接可让多个目录项指向同一 inode；rename 可改变路径而不必改变一个已经打开的 file 对象。路径、文件名和 inode 不能互相当作唯一身份。

## 缓冲读写与映射

```text
read / write → VFS 和文件系统操作 → 适用的 page cache 路径
             → cache miss / 脏页写回 → 文件系统和块 I/O

mmap → VMA → 访问触发 file-backed fault → page cache / 文件系统
```

Linux 6.18.62 的 `fs/read_write.c` 中有 `vfs_read()` / `vfs_write()`，`mm/filemap.c` 中有 `filemap_read()` 和 `filemap_fault()`；实际文件系统可使用不同实现或 iomap，不能把这些入口当成所有文件 I/O 的唯一调用链。

缓存命中可以避免重复读取设备；脏数据在写回后仍需由文件系统、块层和设备处理。内存映射与私有 COW 的细节见 [[Linux 缺页与写时复制]]；文件系统 / 块设备整体入口见 [[Linux 存储与文件系统]]。

## 写回与持久化

| 操作 / 状态 | 能说明什么 | 不能单独证明什么 |
| --- | --- | --- |
| 缓冲 `write()` 返回 | 内核接受了返回长度的数据 | 数据和目录项已安全写入介质 |
| page dirty / writeback | 缓存页需要或正在进行写回 | 设备掉电后一定保留数据 |
| `fsync(fd)` 成功 | 文件相关同步请求按接口完成 | 硬件虚报 flush、未同步目录项或应用事务设计问题 |
| `O_DIRECT` | 采用文件系统支持的直接 I/O 数据路径 | 自动具有 `fsync` 的持久化语义 |

Linux 的 `filemap_write_and_wait_range()` 等接口参与适用的文件缓存写回与等待；文件系统元数据、journal、barrier / flush 和设备缓存还需按相应实现分析。文件创建或 rename 后需要持久化目录项的应用，还要考虑同步目录及错误返回。

## 排查

1. 用 `findmnt`、`lsblk -f` 确定 mount、文件系统和设备；分清文件系统问题与底层 I/O 错误。
2. 对写入延迟或内存压力，观察 dirty/writeback、设备延迟及进程 I/O；区分写入 page cache 与真正刷写介质。
3. 对数据丢失，核对应用 `write()` / `fsync()` / rename 顺序、错误处理、挂载选项及设备 flush 能力，并在目标硬件验证断电场景。
4. 使用可用的 ftrace / tracepoint 或块层统计缩小边界，避免仅凭一次吞吐量或 `write()` 返回推断持久化成功。

## 参考

- Linux 6.18.62：[VFS](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/filesystems/vfs.rst)、[page cache](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/mm/page_cache.rst)、[写缓存控制](https://github.com/gregkh/linux/blob/v6.18.62/Documentation/block/writeback_cache_control.rst)。
- [`fs/read_write.c`](https://github.com/gregkh/linux/blob/v6.18.62/fs/read_write.c)、[`mm/filemap.c`](https://github.com/gregkh/linux/blob/v6.18.62/mm/filemap.c)、[`include/linux/fs.h`](https://github.com/gregkh/linux/blob/v6.18.62/include/linux/fs.h)。
- 蜗窝科技：[文件缓存回写](https://www.wowotech.net/memory_management/327.html)、[文件系统系列](https://www.wowotech.net/sort/filesystem/)。
