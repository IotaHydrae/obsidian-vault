# apt 下载缓存：导出 / 导入 / 备份到别的机器

> 面向"重建 rootfs 时不想重新下载几百 MB"。工具是 **`tools/apt-cache.sh`**，
> 回归测试是 **`tests/apt-cache-tool.sh`**（在板子上跑，不动真缓存）。
> 什么时候看这篇：换机器/重刷系统后要接着在板子上编 rootfs、把缓存备份到编译服务器、
> 或怀疑"这次构建怎么又下了一遍包"。

## TL;DR

- 目的：重建 rootfs 时**不重新下载**几百 MB；工具 `tools/apt-cache.sh`（`dir`/`stat`/`export`/`import`）。
- 目录结构与命令速查见 §1/§2；备份到服务器、在新机器恢复见 §3；坑见 §5。
- ⛔ `sync-to-board.sh` **永远不能删 `build/apt-cache`**（排除即保护；`--delete-excluded` 一律不许加）。
- ⚠️ **zstd 压不动 `.deb`**：`tar.zst` ≈ 原始大小（§5 第 1 条），备份就是这么大。

## 0. 实测数字（两轮，口径一致）

| 项 | 2026-09-15（首测） | **2026-10-03 复查** |
| --- | --- | --- |
| 缓存内容 | 1610 个 `.deb` / **750.7 MB**（787,155,276 B）：1354 arm64 + 256 all | **1703 个 / 807.6 MB**（846,839,462 B）：**1431 arm64 + 272 all** |
| 导出产物 | 单个 `tar.zst` **786,671,693 B**（≈750 MB） | 同量级（zstd 压不动，见 §5.1） |
| 导出耗时 | **2 s**（页缓存热）/ **10 s**（冷） | 未复测 |
| 导入后 | 与源缓存**逐字节一致**（1610 个文件全部 sha256 通过） | 未复测 |
| 省下什么 | `rootfs` 阶段打到 `[cache] 恢复 … 本次构建无需重新下载这些包`，整轮 rootfs **5m48s** | 未复测 |
| 传输 | 局域网板子→本机 ~**7 MB/s**，750 MB ≈ **2 分钟** | 未复测 |

- 复查命令：`~/ubuntu-lsp-rk3588 $ ./tools/apt-cache.sh stat`（开发板 `<HOST_07>`）。
- 缓存**只有 arm64/all 的包** ⇒ **x86 交叉构建机与 arm64 板子通用**，跨机器直接互相拷都行。
- `export` / `import` 用于跨机器保存包；只有所需版本齐全时才能零下载，新增或升级的包仍可能联网下载。

## 1. 缓存目录长什么样

```
build/apt-cache/                 # 整个 build/ 都被 .gitignore 忽略，不进 git
├── archives/                    # 1703 个 .deb（实测 2026-10-03）
│   ├── python3-yaml_6.0.1-2build2_arm64.deb
│   └── …
└── MANIFEST                     # 来源主机 / 日期 / 包数 / 大小 / 架构分布
```

- **谁写它**：`rootfs` 阶段收尾（`LSP_APT_CACHE=Y`，默认）把 chroot 里 `/var/cache/apt/archives/*.deb` 搬进 `archives/`。
- **谁用它**：重建 rootfs 时仅将缓存中的 `*.deb` 复制回 `/var/cache/apt/archives`，保留缓存源；
  apt 校验并复用匹配版本。保存或恢复失败报错，不继续发布完成标记。
- `LSP_APT_CACHE=N` 只禁用跨构建保存 / 恢复，不删除现有 rootfs 的 `.deb`；
  与 `--force` 配合才会重新解压 rootfs 且不恢复缓存。`rootfs` 阶段仅在收尾清理包与索引，失败续跑可复用下载。
- **架构来源**：文件名里的 `_<arch>.deb`。非 `arm64`/`all` 的包对 arm64 镜像没用，`export` **默认排除**并报告。
- 刚 clone 的仓库里没有缓存（`stat` 显示 0 个包），跑一次构建就有了。

## 2. 命令速查

```bash
./tools/apt-cache.sh dir                       # 打印缓存目录（cd "$(./tools/apt-cache.sh dir)"）
./tools/apt-cache.sh stat                      # 包数 / 大小 / 架构分布
./tools/apt-cache.sh export [-o FILE]          # 打包成 tar.zst（默认 ./lsp-apt-cache-<日期>.tar.zst）
./tools/apt-cache.sh export --keep-foreign     # 连非 arm64/all 的包一起打包
./tools/apt-cache.sh import FILE               # 合并导入（同名不覆盖）
./tools/apt-cache.sh import DIR                # 从目录导入（缓存目录 或 archives/ 都行）
./tools/apt-cache.sh import FILE -n            # 干跑：只打印计划，不落盘
```

| 选项 | 作用 |
| --- | --- |
| `-C, --cache-dir=DIR` | 换缓存目录（默认 `$APT_CACHE_DIR`，再默认 `<仓库>/build/apt-cache`）—— 测试/多份缓存并存时用 |
| `--keep-foreign` | `export` 时保留非 `arm64`/`all` 的包 |
| `-n, --dry-run` | `import` 只打印计划 |
| `-q, --quiet` / `-h, --help` | 少打印 / 帮助 |

**权限**：`export` 只读缓存 ⇒ 普通用户即可；`import` 要写缓存（构建期是 root 建的）⇒ **一般要 `sudo`**。
没有写权限时脚本会明确提示"加 sudo 再跑一次"，不会静默失败。

## 3. 备份到服务器 / 在新机器上恢复

### 3.1 从板子备份到服务器（推荐流程）

```bash
# ① 板子上导出（只读缓存，不需要 sudo）；文件名带日期，避免覆盖上一份
B=board@<HOST_07>
ssh -F /dev/null "$B" 'cd ~/ubuntu-lsp-rk3588 && ./tools/apt-cache.sh export -o /tmp/lsp-apt-cache-$(date +%Y%m%d).tar.zst'

# ② 拉到本机（~800 MB，~2 分钟）
rsync -a --info=progress2 "$B":/tmp/lsp-apt-cache-*.tar.zst ~/lsp-backup/

# ③ 再从本机推到编译服务器（同一份，服务器重建 rootfs 时也能用）
rsync -a --info=progress2 ~/lsp-backup/lsp-apt-cache-*.tar.zst board@<HOST_05>:~/ubuntu-lsp-rk3588/
```

> 本仓库的做法：备份落在 `images/board-backup-<日期>/`（`images/` 被 gitignore），
> 例如 `images/board-backup-20260914/lsp-apt-cache-20260914.tar.zst`。
> 仓库根的 `lsp-apt-cache-*.tar.zst`（`export` 的默认输出位置）也在 `.gitignore` 里。

### 3.2 在新刷的板子 / 服务器上恢复

```bash
sudo ./tools/apt-cache.sh import ~/lsp-apt-cache-20260915.tar.zst   # 合并导入（缓存目录属 root）
./tools/apt-cache.sh stat                                          # 核对包数/大小
```

之后照常构建，`rootfs` 阶段会自己用上：

```
[cache] 恢复 1703 个已下载的 .deb（808M）到 rootfs...
[cache] 恢复完成，本次构建无需重新下载这些包
```

### 3.3 不想打 tar：直接 rsync 目录

```bash
rsync -a --info=progress2 board@<板子>:ubuntu-lsp-rk3588/build/apt-cache/archives/ /tmp/cache/archives/
rsync -a --info=progress2 /tmp/cache/archives/ board@<新机器>:ubuntu-lsp-rk3588/build/apt-cache/archives/
sudo ./tools/apt-cache.sh import /tmp/cache/archives     # 两种形态（缓存目录 / archives 目录）都认
```

打 tar 的好处是**一个文件**（校验、传完就知道完整性、不怕半途断在目录里）；直接 rsync 目录的好处是**增量**。

### 3.4 和 `sync-to-board.sh` 的关系（别搞混）

- `sync-to-board.sh` 默认**完全排除 `build/`**（保护板子上编好的 deb 与缓存）。
- `--artifacts` 只额外放行 `build/*.deb` 与 `build/apps/***`，**仍然不放行 `build/apt-cache`** —— 刻意的：
  800 MB 塞进源码同步里既慢又难查，缓存请用 `tools/apt-cache.sh` 单独搬。
- `--delete` 也不会删缓存（排除即保护；**不要**加 `--delete-excluded`）。

## 4. 回归测试（`tests/apt-cache-tool.sh`，板上 21 项）

```bash
$ ssh board@<HOST_07> 'cd ~/ubuntu-lsp-rk3588 && ./tests/apt-cache-tool.sh'
  真实缓存: …/build/apt-cache（1610 个 .deb）
  1) dir / stat / --help                 ✓
  2) export（默认排除非 arm64/all）        ✓ 产出 tar.zst、附 MANIFEST、包内 .deb 数 = 真实缓存
  3) export 的错误路径                    ✓ 目标已存在拒绝覆盖 / 空缓存拒绝打包
  4) import tar → 临时缓存，逐字节核对      ✓ 与真实缓存全部 sha256 一致
  5) 幂等 / 干跑                          ✓
  6) import 目录（两种形态）               ✓
  7) --keep-foreign                       ✓
  8) import 的错误路径                     ✓（不存在/结构不对/不可写/未知选项）
  ================ 通过 21   失败 0 ================
```

它**只用临时目录**，不动真缓存；`-C <目录>` 可对指定缓存跑全套。参考值与约定见 `tests/README.md`。
端到端（2026-09-14 刷完 eMMC 后）：`import` 回 1610 个包 → `rootfs` 阶段全程零下载、`5m48s` 出 rootfs。

## 5. 坑与注意（都实测过）

1. **zstd 压不动 `.deb`**：`tar.zst` 750.2 MB ≈ 原始 750.7 MB（`.deb` 里已经是 xz/zstd 压过的 `data.tar`）。
   ⇒ **备份就是 ~800 MB，别指望变小**；要省空间只能少备份。
2. **`import` 是合并语义、同名不覆盖**（`tar --skip-old-files` + 逐文件判存在）⇒ 多台机器的缓存混在
   一起是安全的（apt 自己按版本校验，旧包只占空间，不会装错）。
3. **目标文件已存在时 `export` 拒绝**（`Error: 目标文件已存在…用 -o 换个名字`）—— 防止误盖上一份备份；
   用带日期的文件名就自然避免了。
4. **包内结构必须是 `archives/` 开头**：`import` 先看前 5 个条目校验，拿错文件（比如镜像 tar）会明确报错
   而不是解到一半；`MANIFEST` 附在包**末尾**。
5. **`MANIFEST` 会落到缓存根**（已存在则跳过），可以看"这份缓存是谁、什么时候导的"：
   `source-host` / `date` / `packages` / `size-bytes` / `arch`。
6. **非 arm64/all 的包默认排除**（x86 机器的缓存里可能有 amd64），结尾报告排除了几个；要保留加 `--keep-foreign`。
7. **`import` 前会检查磁盘空间**：目标盘不够直接拒绝（"磁盘空间不足：需要 …，只剩 …"）。
8. **导入 ≠ 立即可用**：缓存只是"下次重建 rootfs 时被 `cp` 进 chroot"的原料；**已经在跑的系统里
   apt 不会自动用它**。
9. **`-C` / `APT_CACHE_DIR` 可以换缓存目录**：测试、多份缓存（"干净的"与"带实验包的"）并存时用。
10. **别手工往 `archives/` 里塞非 `.deb`**：`stat` 统计包文件，`export` 则打包整个 `archives/`
    （默认只排除非 arm64/all 的 `.deb`），其它文件也会被带走。构建恢复只复制 `*.deb`。

## 6. 相关

- [deb-packages.md](deb-packages.md) —— 镜像里到底装了哪些 deb、为什么装这些。
- [build-internals.md](build-internals.md) —— 阶段间复用 `build/rootfs`、缓存的读写时机。
- [image-size-trimming.md](image-size-trimming.md) —— 缓存**不进镜像**（打包前 `archives/` 被搬走），
  以及 `/var/lib/apt/lists` 那条 204 MB 的取舍。
- `AGENTS.md` §8「板子接入与 git」 —— 板子/服务器/开发机各自的角色与同步方式。
