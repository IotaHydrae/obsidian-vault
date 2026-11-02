# 用 clangd 索引内核 / U-Boot（`compile_commands.json`）

> 目的：在 VSCode 里改内核或 U-Boot 源码时，有**正确的**跳转、补全、诊断、重命名
> —— 而不是靠猜的 IntelliSense。核心是给 clangd 一份 `compile_commands.json`。
>
> 一键工具：`tools/gen-compile-commands.sh kernel|u-boot|all`（本文就是它的说明书）。
> 实测环境（2026-09-16，**当时内核树是 `6.1.118`**；现在树是 `6.1.172`，条目数会随源码变化）：
> Ubuntu 26.04 宿主、clangd 22（llvm-22）、U-Boot `2017.09+20230711`。

---

## TL;DR

- 目的：在 VSCode 里改内核 / U-Boot 时有**正确的**跳转、补全、诊断（靠生成的 `compile_commands.json`）。
- 原理：kbuild 留下的 `.*.cmd` 就是编译命令；内核与 U-Boot 分别见 §2 / §3。
- VSCode 侧怎么配见 §4。
- ⚠️ 索引缓存（`kernel-6.1/.cache`、`u-boot/.cache`）几百 MB，**不要同步到板子** ——
  `tools/sync-to-board.sh` 已经排除它们。
- ⚠️ **`compile_commands.json` 不进 git**（内核那份 26 MB、里面是本机绝对路径）：新 clone 里
  `kernel-6.1/` 与 `u-boot/` 下**都没有**，跑一次 `tools/gen-compile-commands.sh` 才有
  （2026-10-03 复查：当前 checkout 里两份都不存在）。
- 复现 / 自查见 §6。

## 1. 原理：kbuild 的 `.*.cmd` 就是编译命令

内核和 U-Boot 都用 kbuild，每编一个 `.o` 都会在旁边写一个 `.名字.o.cmd`，第一行就是完整的
编译命令（`cmd_xxx.o := aarch64-linux-gnu-gcc ... -c -o xxx.o xxx.c`）。内核自带
`scripts/clang-tools/gen_compile_commands.py` 把这些 `.cmd` 扫成 `compile_commands.json`；
U-Boot **没有**带这个脚本，但格式一样 ⇒ 直接借内核那份用（本工具就是这么做的）。

两条容易踩的点，工具里都处理了：

| 坑 | 说明 | 处理 |
| --- | --- | --- |
| **内核是原地构建在副本里** | `scripts/build-kernel.sh` 把 `kernel-6.1/` 拷成 `build/linux-rockchip` 再编，生成出来的 `file` 指向**副本**，而你编辑的是 `kernel-6.1/` | 把「源码树里真实存在」的条目改指 `kernel-6.1/`；构建期生成的文件（`.mod.c` 等）保持指 build 树 |
| **U-Boot 是树外构建** | 源码在仓库的 `u-boot/`，编译发生在 `build/u-boot-lubancat-rk3588/debian/build/<平台>/` | `-d` 指**平台目录**（`.config`/生成头在那）；JSON 放**源码树 `u-boot/`**，`file` 也改指过去（clangd 只沿被编辑文件的父目录往上找 DB） |

`directory` 字段（clangd 的 cwd）**故意保持指向构建树**：`-I./include/...`、
`-include ./include/linux/kconfig.h` 和 `include/generated/` 里的生成头都在那边。

---

## 2. 内核

```bash
tools/gen-compile-commands.sh kernel            # 生成 kernel-6.1/compile_commands.json
tools/gen-compile-commands.sh kernel --check    # 顺带 clangd --check 自检一条
```

实测（2026-09-16，`build/linux-rockchip` 是 09-15 那次 `-k` 编到打包才挂的完整编译树）：

```
[cc] 内核：build/linux-rockchip → kernel-6.1/compile_commands.json
   改指源码树 8726 条，留在构建树 2032 条（构建期生成的文件），合计 10758 条
[cc] clangd --check main.c
   Building preamble...
   All checks completed, 21 errors
[cc] 完成：26M  kernel-6.1/compile_commands.json
```

- 10758 条全是 `.c` 且都带 `-c`，**0 条文件不存在**；耗时约 4 秒。
- 那 2032 条留在 build 树的，构成是：**1997 个 `.mod.c`**（模块元数据，构建期生成）+
  `.vmlinux.export.c` + `*.asn1.c` + `utf8data.c` —— 源码树里根本没有这些文件。
- `All checks completed, N errors` 里的 N **不是编译错误**：那是 clangd `--check` 自检
  `SwapBinaryOperands` tweak 的 "new replacement overlaps with an existing replacement"
  噪声（同一次跑不同文件是 6~21 不等）。真正的编译诊断是 0 条。

### `kernel-6.1/.clangd`（这个要进 git）

不加它的话 clangd **建不出 preamble**，整个文件飘红，日志是：

```
E[..] [drv_unknown_argument] unknown argument: '-fno-allow-store-data-races'
E[..] [drv_unknown_argument] unknown argument: '-fconserve-stack'
E[..] Could not build a preamble ... error: unknown target ABI 'lp64'      ← 致命
```

所以 `kernel-6.1/.clangd` 里 `CompileFlags.Remove` 掉三项：`-mabi=*`（致命那个）、
`-fno-allow-store-data-races`、`-fconserve-stack`。**只去这三个** ——
`-Wa,-march=armv8.5-a`、`-mbranch-protection=pac-ret+leaf` 实测 clang 都接受，
留着诊断更贴近真实编译。

### 重建时机

`build/linux-rockchip` 每次 `-k` 都会被 `rm -rf` 重建、u-boot 的平台目录每次 `-u` 也会重编
⇒ **编完哪棵树就重新生成哪一份**。
DB 没更新、或构建树被删掉时，clangd 找不到匹配条目会退化成"无参数"，又变成满屏红。

---

## 3. U-Boot

```bash
tools/gen-compile-commands.sh u-boot                       # 自动挑唯一一个已构建的平台
tools/gen-compile-commands.sh u-boot --platform=<名字>     # 多平台时指定
```

实测：

```
[cc] u-boot（平台 lubancat-5-v2-rk3588）：build/u-boot-lubancat-rk3588/debian/build/lubancat-5-v2-rk3588
                                        → u-boot/compile_commands.json
   改指源码树 632 条，留在构建树 4 条（构建期生成的文件），合计 636 条     ← 0 条文件缺失
Building preamble 通过，0 条编译诊断
```

- **源码树在仓库里：`u-boot/`**（git 跟踪；`notes/README.md` 也是这么写的）。
  编译目录是树外的 `build/u-boot-lubancat-rk3588/debian/build/<平台>/`（`.config`、
  `include/generated/autoconf.h`、951 个 `.*.cmd`、885 个 `.o` 都在那），源码文件本身不在里面。
- **`-d` 必须指平台目录**（那里才有 `.config` 与生成头，clangd 的 cwd 也在那）；`file`/命令末尾路径
  按 `.cmd` 的基准（源码副本根）改指回 `u-boot/` —— 与内核同一套路，只是**两个基准不同**：
  内核是"构建目录 = 路径基准"，u-boot 是"平台目录 ≠ 源码副本根"。
  如果 `-d` 错指成源码副本根，会得到 681 条但 `directory` 是副本根（那里没有 `.config`/生成头），**别这么用**。
- **U-Boot 不需要 `.clangd`**：它的参数里没有 clang 不认的 GCC 专有项。唯一一条是驱动警告
  `unsupported option '-ffixed-r9' for target 'aarch64-linux-gnu'`（`-ffixed-r9` 是 ARM32 的），
  不影响 preamble；洁癖可以在 `.clangd` 里 `Remove: ['-ffixed-r9']`。
  也没观察到 `-Werror` 把警告显示成错误（该文件 0 诊断）。
- DB 默认就写在源码树里（`u-boot/compile_commands.json`，可写，**不需要 sudo**），
  被根 `.gitignore` 忽略（内核那份是 `kernel-6.1/.gitignore` 自己带的规则）。
- `scripts/build-u-boot.sh` 现在**每次构建都从 `u-boot/` 增量同步源码副本**（2026-09-18 起：
  `rsync -rlptD -c -i`，排除 `debian`/`.cache`/`compile_commands.json`，**不加 `--delete`**），
  所以改了 `u-boot/` 下的东西**下次构建就生效**，不必再手工清副本。兜底仍可
  `sudo rm -rf build/u-boot-lubancat-rk3588`（全量重编）；`UBOOT_NO_SYNC=1` 可跳过同步
  （留给"就要用副本里那份手改过的源码"的场合）。细节见 [upstream-sync.md](upstream-sync.md) §6.0 与
  [../hardware/atf-missing-troubleshooting.md](../hardware/atf-missing-troubleshooting.md)。

---

## 4. VSCode 侧怎么用

1. 装 **clangd** 扩展（`llvm-vs-code-extensions.vscode-clangd`），
   并在 C/C++ 扩展里把 IntelliSense 关掉（`C_Cpp.intelliSenseEngine: disabled`），两边别打架。
2. 工作目录：
   - 内核 → 打开 `kernel-6.1/`（`compile_commands.json` 与 `.clangd` 都在这）；
   - U-Boot → 打开 `u-boot/`（源码树，`compile_commands.json` 就在同目录）。
3. **不需要** `--query-driver`：实测不带它也能建 preamble（内核 `-nostdinc` 自带头文件，
   交叉 gcc 的系统头用不上）。真要用可以加
   `"clangd.arguments": ["--query-driver=/usr/bin/aarch64-linux-gnu-*"]`。
4. 首轮会后台索引 1 万多个文件（吃 CPU/内存几分钟）；不想要就
   `"clangd.arguments": ["--background-index=0"]`（代价是跨文件跳转变弱）。

### 排障对照表

| clangd 日志 | 含义 | 处理 |
| --- | --- | --- |
| `Loaded compilation database from ...` | 找到了 DB | 正常 |
| 没有这行 / 用了 `fallback command` | 没找到 DB（路径不对或已被删） | 重新生成；确认 DB 在被编辑文件的父目录链上 |
| `unknown target ABI 'lp64'` + `Could not build a preamble` | `.clangd` 没生效 | 确认 `kernel-6.1/.clangd` 在，且 `Remove` 里有 `-mabi=*` |
| `[drv_unknown_argument] unknown argument: '-fno-...'` | GCC 专有参数 | 加进 `.clangd` 的 `Remove` |
| `All checks completed, N errors`（`--check`） | N = 自检 tweak 噪声 | 看有没有 `error:`/`warning:` 诊断行；没有就是正常的 |

---

## 5. 有没有比 clangd 更好的？

没有"全面更好"的，按用途分工（可用性一列是 **2026-09-16 在某个开发容器里**用
`apt-cache policy` / `command -v` 实测的）：

| 方案 | 强项 | 弱项 | 当时那台机器的可用性 |
| --- | --- | --- | --- |
| **clangd** + compile_commands | C 语义最准（跳转/补全/诊断/重构），官方内核工具链 | 每次编完要重新生成 DB；首轮索引吃资源 | ✅ 已配好（内核 + U-Boot） |
| **ccls** | 唯一像样的 clangd 替代：更省内存、可只索引指定目录 | 生态小 | ❌ 仓库无候选（要自编） |
| **cscope** | **不用编译**：`make cscope`（内核 ~1 分钟）跨全树找调用点极快、**永不误报** | 只有符号级，无类型/补全 | 📦 apt 有（15.9），未装 |
| **GNU GLOBAL (gtags)** | 同上，`make gtags`；`global -r` 查引用 | 同上 | 📦 apt 有（6.6.14），未装 |
| **bear** | 任何构建系统通用：`bear -- make ...` 拦编译器调用生成 DB | 只解决"拿 DB" | ✅ 已装（3.1.6） |
| MS C/C++ (cpptools) | 也吃 `compile_commands.json`，VSCode 原生 | 更重、语义更弱 | VSCode 扩展 |

> ⚠️ **这一列跟机器走，别当"本机就是装好的"**：2026-10-03 在当前开发机上复查
> `command -v {clangd,bear,cscope,gtags,ccls}` —— **五个全部未装**。用之前先自己查：
> `command -v <工具>`、`apt-cache policy <包>`（镜像里刻意不带这些，属"构建机/开发机依赖"）。

两个树都自带索引目标，可直接用（不需要先编译）：

```bash
make -C kernel-6.1 cscope          # 或 gtags（需装 cscope / global）
make -C u-boot cscope
```

**建议**：主力 clangd（语义正确），需要"全树找调用者"而手头这棵树还没编过时用 cscope/gtags 兜底，
换别的工程懒得写脚本时用 `bear`。

---

## 6. 复现 / 自查

```bash
# 生成（内核 + U-Boot 一起）
tools/gen-compile-commands.sh all

# 单独自检：内核拿 init/main.c、U-Boot 拿 common/main.c
clangd --check=kernel-6.1/init/main.c
clangd --check=u-boot/common/main.c

# 只想知道 DB 里有多少条、有没有指向不存在的文件
python3 -c "
import json,os
db=json.load(open('kernel-6.1/compile_commands.json'))
print(len(db), sum(1 for e in db if not os.path.exists(e['file'])))"
```
