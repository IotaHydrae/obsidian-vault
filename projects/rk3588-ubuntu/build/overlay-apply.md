# 把 `overlay/` 装到正在跑的板子上（`tools/apply-overlay-to-board.sh`）

> `tools/sync-to-board.sh` 只同步源码工作树；现场验证 overlay 用 `tools/apply-overlay-to-board.sh`，
> 先预览，再 `--apply` 安装到运行系统或已挂载的 rootfs，最后复核 md5 和权限。
> 现场安装保留多数已有目标属性，仅对下述项目固定属性；并非完整复现镜像构建。

## TL;DR

```bash
./tools/sync-to-board.sh -H board@<板子>          # 1) 先同步源码（板子上要编 rootfs、查文档都靠它）
./tools/apply-overlay-to-board.sh -H board@<板子>          # 2) 看差异（dry-run，不写）
LSP_BOARD_SUDO_PASS="<SECRET>" ./tools/apply-overlay-to-board.sh -H board@<板子> --apply
```

三条要点：

1. **两个工具是接力关系**，不是二选一：`sync-to-board.sh` 管源码，本工具管 overlay → `/`。
2. **更新器六个文件固定权限与 root:root**：五个私有文件 0600，`release-public.pem` 0644。
   `home/board/updater-bootstrap` 目录固定 0775，内容归目标 `/home/board` 的 uid/gid；其余文件沿用目标权限和属主。
3. **默认不碰**现场设备绑定、`opt/`、`boot/`、顶层 plymouth 图、netplan、networkd override，
   各有各的原因（见工具文件头的清单）。要连现场绑定一起装得显式 `--with=device-binding`。

---

## 1. 为什么需要它：两条路径的差别

| | `tools/sync-to-board.sh` | `tools/apply-overlay-to-board.sh` |
| --- | --- | --- |
| 目标 | 板子的 `~/ubuntu-lsp-rk3588/`（工作树） | 板子的 `/`（真实的系统路径） |
| 内容 | README / notes / scripts / tools / config / overlay / apps / kernel-6.1 / u-boot 源码 | 只处理 `overlay/` 下**该落到 `/`** 的那部分 |
| 什么时候生效 | 立刻（文件就在工作树里） | 立刻（直接写 `/`） |
| 谁在用它 | 板子上编 rootfs、跑测试、查文档 | 让板子上**正在跑的程序**变成新版 |

## 2. 权限规则

镜像里的权限来自 `scripts/build-lsp-image.sh` 里逐条写死的 `install -m 0755/0644/0600 …`
（以及整目录拷贝时的 `normalize_overlay_perms()`）。这些**与仓库工作树里的权限位无关**：

| 文件 | 仓库里 | 镜像里 | 出处 |
| --- | --- | --- | --- |
| `usr/local/sbin/lsp-set-rule` | 0664 | **0755** | 构建脚本 `install -m 0755` |
| `etc/data-collection-updater/{boot.json,bos-device-credentials.json,download-bos.json,download.json,gitlab-deploy-token}` | 0664 | **0600 root:root** | 用户指定；构建与现场安装固定 |
| `etc/data-collection-updater/release-public.pem` | 0664 | **0644 root:root** | 公钥，构建与现场安装固定 |
| `home/board/.android/adbkey` | 0644 | **0600** | 构建脚本 `install -m 0600` |
| `etc/NetworkManager/system-connections/lsp-ethernet-{dhcp-gmac0,dhcp-gmac1,direct}.nmconnection` | Git 不保存 0600 | **0600 root:root** | 构建显式安装；现场新装固定 0600 |
| `etc/apt/…`、`usr/share/…` | 0644 | 0644 | 整目录拷贝 + `normalize_overlay_perms` |

所以本工具的做法是：

* **上表更新器六个文件** → 固定 mode/uid/gid；即使内容一致，属性不符也记为 `differ`，
  `--apply` 才修正。`--root` 安装这些文件需要 root。
* **`home/board/updater-bootstrap`** → 顶层目录固定 0775，目录及选中的内容归目标 `/home/board` 的 uid/gid。
  新内容保留源权限，已有文件或子目录保留目标权限；只选子文件也检查顶层目录属性。
* **其余目标上已有的文件** → 沿用目标权限位，内容一致记为 `same`。
* **目标上没有**（新加的文件） → 回退规则：镜像固定收紧的那几个（`TIGHT_WHEN_NEW`，0600）
  按表走；`usr/local/bin/*`、`usr/local/sbin/*` 固定 0755，`etc/sudoers.d/*` 固定 0440，
  其余按仓库文件的可执行位给 0755 / 0644。
  三份 `lsp-ethernet-*.nmconnection` 新装为 0600，避免 Git checkout 的 0644 被 NetworkManager 拒绝；
  这只约束上述三个路径，不批量收紧其它连接，已有文件仍保留目标属性。
* **属主与目录属性**：除上述明确规则外，已有文件保留 uid/gid；新文件继承最近已有父目录 uid/gid。已有目录不改
  mode/uid/gid，新 `.android` 为 0700。归档先解到暂存目录再逐文件 install，不直接解到目标 `/`。

## 3. 默认不碰的东西（以及为什么）

| 路径 | 为什么不装 |
| --- | --- |
| `opt/`（oem-host 快照 + `data_collection` 发布树） | 需按各自部署契约更新；`/opt/data_collection/current` 是运行时软链；上位机源码快照用 `tools/sync-oem-host.sh` 刷新 |
| `boot/`（dtbo） | 装了也要 `lsp-merge-dtb` 合并 + 重启才生效 |
| 顶层 4 个 plymouth PNG | 镜像构建时**装到别的路径**（`/usr/share/plymouth/`、`/usr/share/pixmaps/`、`/usr/share/backgrounds/`）⇒ 按 overlay 的相对路径写就是往 `/` 扔垃圾 |
| `etc/netplan/01-network-manager-all.yaml` | NetworkManager 接管后会把它改名成 `*.dpkg-backup`，写回去等于没写 |
| `etc/systemd/system/systemd-networkd-wait-online.service.d/override.conf` | 构建收尾**故意删掉**（桌面用 NM 不是 networkd） |
| 5 条 `99-*.rules` + `home/board/.lsp/config/device.yaml` | 现场按设备写过的绑定（`lsp-set-rule` 的产物）⇒ 覆盖 = 恢复出厂绑定。要装得显式 `--with=device-binding` |

想装被默认排除的东西，两种办法：`--with=device-binding`（只放行现场绑定那一组），
或 `--only=<前缀>`（点哪儿装哪儿，**不再套用默认排除**，例如 `--only=etc/netplan`）。

## 4. 这个工具**不做**的事（边界）

它只做"文件搬运 + 权限归一"。构建期才有的动作要靠对应命令：

| 场景 | 还得自己跑 |
| --- | --- |
| 改了 systemd unit | `sudo systemctl daemon-reload` |
| 改了 `sysctl.d` / logind / resolved 之类的配置 | 对应服务 restart（如 `systemctl restart systemd-logind`） |
| 改了 `etc/initramfs-tools/conf.d/` | `sudo update-initramfs -u -k all`（否则只影响以后重建） |
| 换了 mcap 的 MIME/desktop 文件 | `update-mime-database` / `update-desktop-database` |
| 改了 dtbo | `lsp-merge-dtb` + 重启 |
| 改了 `apps/` 的源码 | 产物在 `build/apps/`，本工具不处理。编译后用 `make -C apps/<name> install-only DESTDIR=<已挂载的目标rootfs>` 安装；跨机器同步产物及前提见 [apps.md](apps.md) |

## 5. 实测与回归

* **隔离回归入口**：`tests/apply-overlay-tool.sh` 覆盖选文件、默认排除、`--with`/`--only`、
  权限、属主、`--apply` 落地、幂等、JSON 和退出码。全程使用临时目录，不碰板子：
  ```bash
  bash tests/apply-overlay-tool.sh
  ```
* **已有隔离记录**：61 项通过、0 项失败；未记录源码版本，不能作为后续改动自动通过的依据。
* **已有 SSH 安装记录**（2026-10-02，未记录板型及软件版本）：向开发板安装原先缺失的
  `etc/initramfs-tools/conf.d/10-lsp-modules.conf` 后，属性为 `0644 root:root`，md5 与源文件一致。
  另一次临时假家目录回归观察到家目录 `board:board 750`、`.android` `board:board 700`、
  私钥 `board:board 600`，board 可读取；该次记录未注明日期与版本，仅支持所测安装路径。

## 6. 安装与诊断限制

1. **sudo 密码与归档不能共用 stdin**：先将归档保存到目标临时文件，再提权安装。
   否则可能出现 `This does not look like a tar archive`；该错误需同时检查归档完整性和 stdin 用途。
2. **定位仓库根用 `readlink -f`**：不带 `-f` 时，普通文件不会输出目标路径且退出非 0，不能据此拼接目录。
3. **权限比较必须统一进制**：`$((0755))` 的十进制值为 493，`stat -c %a` 输出的 `755`
   则是八进制文本；需转换为相同进制再比较，例如 `$((8#$x))`。
4. **root-only 的文件（`/etc/sudoers.d/*`、0600 的 token）普通用户 md5 不出来**：带
   `LSP_BOARD_SUDO_PASS` 时比对也走 sudo；不带就标 `noread`，按"要装"处理并明说。
5. **板子的 ssh 也要密码时**，本工具自己没有输密码的口子（`sync-to-board.sh` 同样没有）——
   和它一样靠 `SSH_ASKPASS`：

   ```bash
   SSH_ASKPASS=/tmp/.lsp-askpass-<板子>.sh SSH_ASKPASS_REQUIRE=force \
   LSP_BOARD_SUDO_PASS="<SECRET>" \
       ./tools/apply-overlay-to-board.sh -H board@<板子> --apply
   ```

   （助手里就一句 `#!/bin/sh` + `echo <ssh 密码>`，`chmod 700`；⛔ 别放进仓库。）

## 7. 相关

* 源码同步：`tools/sync-to-board.sh`、[build-guide.md](build-guide.md)
* overlay 的落地机制（`rootfs` 阶段怎么拷 overlay、`normalize_overlay_perms`）：
  [build-internals.md](build-internals.md)、[../system/system-integration.md](../system/system-integration.md) §0
* 回归测试：`tests/apply-overlay-tool.sh`
