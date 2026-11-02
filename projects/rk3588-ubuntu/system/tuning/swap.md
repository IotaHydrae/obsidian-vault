# swap：镜像里不预置 swapfile，改首启创建 + `vm.swappiness=10`

> 构建期 `fallocate` 出来的 2 GiB swapfile 会被 `tar` **真的读成 2 G 的 0** 写进 `.img`
> —— 镜像凭空大 2 GiB，刷机还要再写一遍。改成**首启**创建（`fallocate` 只分配元数据、
> 不写数据块），换出倾向由 `vm.swappiness=10` 控制。

## TL;DR

- 镜像里不建；首启由 `lsp-swapfile.service` 跑 `fallocate` → `mkswap` → `swapon -p`，写入几乎为零。
- `/etc/fstab` **刻意没有 swap 行**：写了 systemd 会生成 `swapfile.swap` 单元，而开机时文件还不存在
  ⇒ `Failed to activate swap /swapfile`，且那个单元无法指定优先级。
- **优先级（`swapon -p`）只在有多个 swap 区时**决定先后；控制"要不要把匿名页换出去"的是 `vm.swappiness`。
- ⚠️ `swapon -p` 只接受 `0..32767`，**负值被静默忽略**（板上实测 `-1/-2/-10` 都 `rc=0` 但没设上）
  ⇒ 本项目默认 `SWAP_PRIORITY=0`（可设置范围内的最低档），脚本会回读核对。
- 配置入口：构建期 `LSP_SWAP_SIZE_MB=<MiB>`（`0` = 首启也不建）⇒ 写进 `/etc/default/lsp-swapfile`。

## 为什么不建在镜像里（2026-09-12 改）

以前是构建期在 chroot 里 `fallocate -l 2048M /swapfile` + `mkswap`，fstab 再写一行 swap。
代价是**实的**：`image` 阶段用 `tar | pv | tar` 把 rootfs 逐个文件写进镜像的 ext4，`fallocate`
在 rootfs 里本来只是"未写盘的 extent"，但 tar 会把它读成 2 G 的 0 并**真的写进 `.img`**：

| 项 | 影响 |
| --- | --- |
| 裸 `.img` | 实打实大 **2 GiB**（镜像大小按 `du -sm --apparent-size` + 1536M 算） |
| 刷机 | 这 2 GiB 还要再写进 eMMC/SD 一遍（时间 + 磨损） |
| 压缩分发 | 这 2 G 全是 0，压缩后几乎不占体积（**但刷机那 2 GiB 省不掉**） |
| 实际用途 | 8 G 内存的板子平时用不到（板上实测跑一天 `Used` 只有几 MiB） |

现在改为：**首启**由 `lsp-swapfile.service` 跑 `lsp-swapfile.sh` —— `fallocate`（只分配元数据、
不写数据块）→ `mkswap` → `swapon -p`。空间一样，写入几乎为零。

- **`/etc/fstab` 里刻意没有 swap 行**：写了的话 systemd 会由 fstab 生成 `swapfile.swap` 单元，
  开机时文件还不存在 → `Failed to activate swap /swapfile`（多一个 failed 单元），
  而且那个单元没法指定优先级。该服务自己 `swapon`，**不参与 `swap.target`**。
- 幂等：已启用的 swap 一律不动（不 `swapoff`、不重建）；文件存在但没启用则只 `swapon`。
- 新建时先在同目录准备候选文件，`mkswap` 与权限设置成功后才发布 `/swapfile`。失败清理候选，
  下次启动可重试；发布不覆盖并发出现的文件。已有文件不自动重新格式化。
- 配置在 `/etc/default/lsp-swapfile`；构建期用 `LSP_SWAP_SIZE_MB=<MiB>`（`0` = 首启也不建）
  把值写进镜像里的那份配置（脚本用 `sed` 替换，单一入口仍是这个变量）。

## 优先级（`swapon -p`）不是"少用 swap"的开关

- 优先级**只在有多个 swap 区时**决定先后（高优先级用满才动低优先级的）。只有一个 swapfile 时，
  设 `5` 还是 `-2`，行为完全一样。
- **真正控制"要不要把匿名页换出去"的是 `vm.swappiness`**。内核默认 60，这里设 **10**
  （`overlay/etc/sysctl.d/99-lsp-swap.conf`）：平时基本不动 swap，内存真紧张时仍能用它兜底。
- ⚠️ **`swapon -p` 只接受 `0..32767`，负值会被静默忽略**（板上实测：`-1`/`-2`/`-10` 都返回
  `rc=0`，但优先级根本没设上，退回内核按激活顺序自动分配的负数；`32768` 才报
  `failed to parse priority: Numerical result out of range`）。
  所以本项目的默认是 **`SWAP_PRIORITY=0`**（可设置范围内的最低档 = "最后才用它"），
  并且 `lsp-swapfile.sh` 会先把越界值判掉、`swapon` 后再回读核对，`tools/verify-image.sh`
  也会静态检查这一项 —— 免得出现"看着配了其实没配"。
- 将来若加 zram 或 swap 分区：给它们**更高**的值（如 zram `100`），磁盘 swapfile 自然排在最后。

## 自检与临时改法

```bash
swapon --show                                    # NAME/SIZE/USED/PRIO，PRIO 应为 0（本项目默认）
systemctl status lsp-swapfile                    # 首启是否成功建好
journalctl -b -u lsp-swapfile                    # 看 [lsp-swapfile] 的日志
cat /proc/sys/vm/swappiness                      # 应为 10
# 临时关掉 swap（不重刷）：把大小改 0 再重启服务
sudo sed -i 's/^SWAP_SIZE_MB=.*/SWAP_SIZE_MB=0/' /etc/default/lsp-swapfile
sudo systemctl restart lsp-swapfile              # 只影响下次创建，已建的不会被删
sudo swapoff /swapfile && sudo rm -f /swapfile   # 真要立刻腾出空间再手动删
```

---
