# 应用源码、预置 Rust deb 与镜像安装

> 日常构建验证三个 Rust app 的仓库 deb，编译其余四个 C app；`rootfs` 阶段只安装。
> 源码与包必须匹配，缺包或过期包直接失败。Rust 打包与追溯见 [rust-app-debs.md](rust-app-debs.md)。

## TL;DR

- `apps/<name>/` 保存源码、`files/` 与 Makefile；C 产物在 `build/apps/`，Rust 发布包在 `packages/apps/`。
- `apps` 阶段准备全部应用并记录状态；`rootfs` 用 apt 安装 Rust 本地 deb、用 `install-only` 安装 C 产物。
- 默认仍需 C 编译器和 Make，无需 Rust 工具链；重打 Rust deb 才运行 `scripts/build-app-debs.sh`。
- 跨机器源码同步包含仓库 deb；C 产物与状态须显式 `--artifacts` 同步（§4）。
- 优化优先保行为和性能，再考虑体积；`lsp-powerkey` 保留静态链接（§7–§8）。

## 1. 目录约定

```text
apps/<name>/
├── Makefile               # 编译、安装与清理
├── src/                   # C / Rust 源码
└── files/                 # 应用配置与服务，路径对应 rootfs
packages/apps/
├── manifest.tsv           # 固定的 app、包路径、包摘要和源码摘要
└── *.deb                  # Rust arm64 发布包，由 Git LFS 管理
```

- `build/apps/<app>/` 与 Cargo `target/` 是本地构建产物，不提交；Rust 发布包与 manifest 一起维护。
- `files/` 放应用专属文件，系统公共配置放 `overlay/`；逐条 `install -D -m 0755/0644`，
  不用 `cp -a files/.` 传入 checkout 属主和权限，检查见 `tools/verify-image.sh`。
- 新命令、服务、udev 仍须[四处同步](../system/system-integration.md)。
  主构建脚本按应用类型使用通用循环，不增加应用专用构建分支。

## 2. Makefile 契约

```make
make build                        # 产出 $(OUT_DIR)/<app>
make install DESTDIR=<rootfs>     # 依赖 build，适合独立编译安装
make install-only DESTDIR=<rootfs> # 只安装已有产物
make clean
```

- 默认 `OUT_DIR=$(REPO_ROOT)/build/apps/<app>/`，按应用目录名核对可执行产物。
- `install` 依赖 `build`；`install-only` 缺产物必须失败，不得触发编译。C app 的镜像安装走此入口。
- Rust app 保留显式 `make build/install` 供开发；镜像安装使用 deb，源码变更须重新打包。
- 编译器按宿主架构选择：aarch64 原生，其他架构交叉；保留显式覆盖。
  Make 内置 `CC=cc`，须判断 `$(origin CC)` 是否为 `default`，不能仅用 `CC ?=`。

## 3. 独立应用阶段与状态

```bash
sudo ./build.sh -a                 # 准备全部应用，不需要板卡配置
sudo ./build.sh -a lsp-powerkey    # 只编一个 C app；Rust app 则验证仓库包
./scripts/build-apps.sh --list
./scripts/build-apps.sh --check-deps
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=apps
./scripts/build-app-debs.sh        # 在 noble arm64 板上重新编译、打包三个 Rust app
```

完整顺序为 `apps → base → rootfs → oem-host → board → image`，阶段参数只接受职责名称。
`--stage=apps` 不下载底包、不挂载或修改 rootfs；`build.sh -a` 的日志在 `build/logs/`。
`--from=base` / `--from=apps` 跑全流程；`--from=rootfs` 先准备 apps、检查 base 依赖，再续后续阶段。

默认准备 `lsp-mcap-view`、`lsp-rk-monitor`、`lsp-usb-map` 的固定 deb，
编译 `lsp-journal-serial`、`lsp-keyfilter`、`lsp-powerkey`、`lsp-sync-data`。
包校验失败不会转为源码编译；修改 Rust 源码后须按[打包流程](rust-app-debs.md)更新包。

全量准备成功后，`scripts/lib/apps.sh` 写入 `build/apps/.build-state`。
`apps-state-v2` 覆盖源码、Makefile、`files/`、共享构建脚本、显式构建参数、
manifest 与产物内容；Rust 项核对包摘要，C 项核对可执行产物。
`target/`、`.cache/`、`.git/` 不参与状态与源码探测；单项准备、失败或输入中途变化不发布全量状态。

单跑 `--stage=rootfs` 在操作 rootfs 前要求有效状态和当前 Ubuntu Base 底包。
安装过程中不调用编译器；完成时再次核对 app 状态，并把实际安装状态写入 rootfs 标记。
源码、deb、manifest 或 C 产物变化使状态失效，默认构建会重跑安装并使上位机、板卡和镜像阶段失效。

`LSP_APPS_OUT_DIR` 覆盖产物根目录，`LSP_APPS_DIR` 覆盖应用源码目录，主要用于隔离测试。
默认镜像可跳过状态有效的 `apps` 阶段；显式准备对 C app 执行 `make -B`。
Rust 重打包仍可复用 Cargo 依赖缓存，阶段拆分本身不代表编译提速。

## 4. 跨机器复用产物

普通源码同步包含 `packages/apps/` 的 manifest 与真实 deb；Git clone 后须取回 LFS 对象。
`tools/sync-to-board.sh --artifacts` 另同步 `build/*.deb`、`build/apps/` 及其状态，
不传 `apps/*/target/` 等缓存；普通同步保护远端 `build/`，`build/apt-cache` 始终保护。

同步后的源码、参数、manifest 和产物须匹配；仅拷几个二进制不算全量应用准备完成。
单独安装不需要 Cargo 或 C 编译器，但完整镜像的 apps 阶段仍需 C 编译器和 Make。
目标 ABI 与运行检查另见 §6，状态校验不能代替兼容性验证。

## 5. 构建机依赖

构建依赖安装在执行构建的宿主机，不因编译方便塞进运行镜像。
Ubuntu 24.04 的宿主包统一由 `sudo ./tools/install-build-deps.sh` 安装；`--dry-run` 预览，
`--check` 只核对安装状态，自动区分 amd64 交叉编译器与 arm64 原生编译器。包名只在该脚本维护。
安装入口已在 Ubuntu 24.04.5 arm64 的 CM5 上实际安装并通过 `--check`，原生 C 编译/运行通过；
amd64 选择及异常路径由隔离回归覆盖，尚未用此入口在全新 amd64 主机安装或做完整 BSP 构建。
`build-apps.sh --check-deps` 检查所选 C app 的工具和 Rust 仓库包，默认不探测 Cargo/rustc。

| 路径 | 需要工具 | 适用范围 |
| --- | --- | --- |
| 默认 C 构建 | `make`、C 编译器 | arm64 用 `gcc`；x86 交叉用 `gcc-aarch64-linux-gnu` |
| 默认 Rust 包准备 | `dpkg-deb`、摘要工具 | 校验包身份、arm64、源码与包摘要 |
| 显式 Rust 源码构建 | Cargo、rustc、C 链接器 | 发布包在 Ubuntu noble arm64 原生构建 |
| 其他 C++ / CMake app | C++ 编译器 / CMake | 按实际源码探测，不新增默认镜像依赖 |

显式 Rust 构建共用 `scripts/lib/rust-toolchain.mk`：先找账户 `.cargo/bin/cargo`，再找 PATH；
`CARGO` / `RUSTC` 与显式 homes 可覆盖。sudo 下识别 rustup 代理并复用 `SUDO_USER` 的工具链存储。
Ubuntu noble apt Cargo 1.75 读不了 v4 `Cargo.lock`，工具链还须满足锁定依赖的 Rust 版本。
`make check-rust-toolchain` 实际执行 Cargo/rustc；交叉开发还检查 arm64 标准库目录。
此预检只用于显式源码构建，不在默认 deb 安装路径执行。

## 6. Rust 应用的注意事项

以下工具链和下载源设置适用于显式源码构建、测试与重打包，日常镜像准备不访问 Cargo 源。

### 6.0 Rust 下载源

`tools/rust-mirror.sh` 按稀疏索引 `config.json` 的可达性和单次响应耗时选源，
不测 crate 下载吞吐或整次构建速度；工具链下载慢时可单独设置 rustup 源。

```toml
# ~/.cargo/config.toml —— crates 索引走中科大 sparse（备选 rsproxy.cn）
[source.crates-io]
replace-with = "ustc"
[source.ustc]
registry = "sparse+https://mirrors.ustc.edu.cn/crates.io-index/"
```

```sh
export RUSTUP_DIST_SERVER=https://mirrors.ustc.edu.cn/rust-static
export RUSTUP_UPDATE_ROOT=https://mirrors.ustc.edu.cn/rust-static/rustup
```

- 历史板测（真拉索引 + `anyhow`）：**ustc 0.7 s ✓ / rsproxy 0.9 s ✓ / tuna 90 s 超时 ✗ / huawei rc=101 ✗**
  ⇒ 默认 ustc。rustup 源：直连 39 KB/s、中科大 **5.6 MB/s**。
- ⛔ 两样都只装在**构建机**，**不进镜像**。⚠️ `sudo` 跑构建时 Rust app 的 Makefile 会把 `CARGO_HOME`
  指回用户 home ⇒ **用户 home 里这份配置照样生效**，别去 root 家目录再配一遍。
- ✅ **构建时自动配置**：Rust app 的 Makefile 在所选工具链的 homes 环境中，先调用
  `tools/rust-mirror.sh` 再运行 Cargo；独立 `make build/install` 同样适用，`install-only` 不探测源。行为：
  在常见全局 Cargo 配置中发现镜像字段时跳过，不解析项目级配置；否则先探 crates.io，
  直连够快即结束，慢或失败才探中科大（`curl -m 5`，最坏 ~10 s），
  **直连失败或 ≥ `LSP_RUST_MIRROR_SLOW_S`（默认 3 s），且镜像可达**才写入 `$CARGO_HOME/config.toml`。
  HTTP 错误按探测失败处理，合法的亚秒响应不会误判。写配置失败时工具返回非零；
  工具不会输出配置成功，Makefile 仍允许 Cargo 继续，最终构建结果由 Cargo 独立判定。
  自测：`LSP_RUST_MIRROR_FORCE=1` 强制写、`LSP_RUST_MIRROR_SLOW_S=0.1` 模拟"直连慢"。

**其它 Rust 规矩**：

* **`Cargo.lock` 必须提交**（否则依赖漂移、构建不可复现）。
* 交叉开发：`cargo build --locked --release --target aarch64-unknown-linux-gnu`，链接器在 `.cargo/config.toml`
  里写死（`[target.aarch64-unknown-linux-gnu] linker = "aarch64-linux-gnu-gcc"`）⇒ `make build` 只有一条命令。
* **动态链接基线须满足目标机 glibc：noble 为 2.39**。检查最高符号要求：
  `readelf --version-info <bin> | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1`。
  实测（开发机 26.04 + GCC 15.2 交叉）一个 hello world 动态二进制最高只要 `GLIBC_2.34`；
  仍须核对 ISA、其它动态库并在目标系统运行，不能由这一项保证所有应用兼容。
  glibc 全静态建议只在不需要 DNS/NSS 时用（linker 会警告 `getaddrinfo`/`getpwuid` 需要运行时同版本 glibc）。
* 别用 `-C target-cpu=native`（编译机不是 RK3588，A55 上可能非法指令）。
* 发布前 `strip`（等价 `-s`，安全）；`panic="abort"`、`lto`、`opt-level="z"` **会动行为或性能**，
  按 §8 先量再用（体积大盘见 [image-size-trimming.md](image-size-trimming.md)）。

### 6.1 三个实例（细节单独成篇）

细节（各自的依赖、坑、界面约定、回归）已单独成篇 → **[apps-examples.md](apps-examples.md)**：

- **6.1 `lsp-mcap-view`**：终端版 MCAP 预览器（现场只能 ssh）。两个坑：MCAP 里**每个类型一条 Schema 记录**
  必须全文件合并成 registry；`.msg` 里的短类型名要按"同包优先 + 后缀匹配"解析成完整名。还负责把 `.mcap`
  设成默认打开程序（MIME/desktop/`mimeapps.list`）。
- **6.2 `lsp-rk-monitor`**：硬件占用率监控（替代 438 行 bash）。与脚本**同参数、同 JSON 键名**；
  ⛔ **百分比量程固定 `0~100%`**（自适应会让空闲核看着像满载）；⛔ 网格 rect 必须裁到区域内（否则 ratatui panic）；
  ⛔ `echo 密码 | sudo -S` 起 TUI 按键进不去（sudo `use_pty`），要 root 就 `ssh -t`。
  接口地图见 [rk3588-hw-monitor.md](../hardware/rk3588-hw-monitor.md)。
- **6.3 `lsp-usb-map`**：USB 设备 → 控制器 + 物理口。**通用性优先**（本机/自研板/开发板同一份二进制，
  只有 DT 认出 LubanCat 才把端口翻译成丝印口名）；工程机 **16 ms**（shell 版 0.95 s）。
  回归 `tests/lsp-usb-map.sh`（PC 5/0、工程机 6/0）；板级依据见 [device-mapping-method.md](../usb/device-mapping-method.md) §7。

## 7. 一个实例：`lsp-powerkey` 为什么是 `-static -s`

`lsp-powerkey` 使用 `-static -s`，保留静态链接：

* `-static` 的理由：它是**电源键最后手段通道**（2s 软关机 / 7s 重试 / 10s 写 PMIC 断电）。
  系统正在换 `libc.so.6`/`ld.so` 半损时，动态版**连启动都起不来**（`Restart=always` 空转），静态版还能接手；
  另外静态链接把"交叉工具链的 glibc"与"目标镜像基线"解耦（动态版实测要 `GLIBC_2.38` 的 `__isoc23_strtol`）。
  省下的体积是 780 KB，在 5.6 GiB 的镜像里是噪声 —— **健壮性 > 体积**。
* `-s`（链接期 strip）：实测**只去掉 `.symtab`/`.strtab`**，`.text`/`.rodata`/`.data` 逐字节不变
  ⇒ 行为完全一样，唯一代价是崩溃时看不到函数名。省 127,776 B（857,344 → 729,568）。
  要排查崩溃就用 `make -C apps/lsp-powerkey LDFLAGS=-static` 重建带符号的。

完整数字与 `/bin/sh` 那个"静态也保不住 2s/7s 那两级"的边界见
[../hardware/rk806-pwron.md](../hardware/rk806-pwron.md) §8。**给别的 app 的结论**：静态/动态按职责定 ——
"坏情况下也要能跑"的兜底通道值那 780 KB；普通应用（尤其 Rust：glibc 静态会让 `getaddrinfo`/`getpwuid`
需要运行时同版本 glibc）优先动态。

## 8. 体积：先量再改，**别拿性能换体积**

> 规则（`AGENTS.md` §2 第 12 条，用户 2026-09-14 明确）：产物要"尽量小"，但两条前置约束压过它 ——
> **① 行为不变 ② 性能（执行时长）不退化**。优先级就是这个顺序。

### 8.1 先想清楚"体积在哪"

**静态链接时体积由 libc 决定，调自己代码的优化级别几乎全是噪声。** `lsp-powerkey` 实测：

| 编译方式 | 文件大小 | `.text` |
| --- | --- | --- |
| `-O2 -static -s`（当前） | **729,568 B** | 678,952 B |
| `-Os -static -s` | **729,568 B**（一个字节没省） | 677,752 B |
| `-O2 -static -s` + `--gc-sections` | 729,552 B（省 16 B） | 676,336 B |
| `-O2 -static`（不 strip） | 857,344 B | — |
| `-O2 -s`（动态，参考） | 68,240 B | — |

`-Os` 把 `.text` 压小 1.2 KB，**但被对齐 padding 吃掉，净收益 0** ⇒ 在静态程序上开 `-Os`
（以及 `opt-level="z"`）是"**纯风险、零收益**"。真正有用的是 `-s`。

### 8.2 安全的手段（不改行为）

| 手段 | 效果 | 判据 |
| --- | --- | --- |
| `-s` / `strip` / Rust `[profile.release] strip = true` | powerkey 省 **127,776 B**（−15%） | **strip 前后 `.text`/`.rodata`/`.data` 逐字节相同** |
| 不链接用不到的库 | 视情况 | `readelf -d` 看 NEEDED 是不是真需要 |

```bash
for s in .text .rodata .data; do
    aarch64-linux-gnu-objcopy --dump-section $s=/tmp/a.$s <未strip>
    aarch64-linux-gnu-objcopy --dump-section $s=/tmp/b.$s <strip后>
    cmp /tmp/a.$s /tmp/b.$s && echo "$s 相同"
done
```

### 8.3 有代价的手段（**先量性能，不退化才留**）

| 手段 | 风险 |
| --- | --- |
| `-Os` / Rust `opt-level="z"` | **可能让执行时间翻几倍**；在 powerkey 上省 0 字节 |
| `-flto` / `codegen-units=1` | 改变内联与代码布局 → 可能变好也可能变差，必须实测 |
| `--gc-sections` + `-ffunction-sections -fdata-sections` | 可能删掉"非常规引用"的代码/初始化节；powerkey 实测省 16 B，不值得冒险 |
| Rust `panic="abort"` | **这是行为改变，不是优化**：去掉 unwind ⇒ `catch_unwind` 失效、panic 时不跑析构 |
| `objcopy -R .eh_frame` / `-R .dynsym` | ⛔ **禁止**：打断 C++ 异常处理、`backtrace()`、`dlsym`/插件 |

### 8.4 性能怎么量（口径 + 现成工具）

**口径已经工具化了**：`tests/opt-flags-bench.sh` + `tests/optbench.c` —— 给一组编译选项，
实测"体积省多少 / 时间涨多少 / 结果变没变"：

```bash
./tests/opt-flags-bench.sh --remote=board@<板子> --details
```

它做的事：交叉编译 16 个变体 → tar 管道推到板子（板子**刻意没有 gcc**）→ `taskset` 钉核跑 →
回传原始数据本地统计。参考值与每个 workload 的机制分析见 `tests/README.md`。自己量时按这些口径：

* **必须在目标板上量**，别信 qemu 的数字（qemu 只是选 ISA 模型，见 [../hardware/cpu-coremark.md](../hardware/cpu-coremark.md)）。
* **钉核**：RK3588 是 4×A55(`cpu0-3`) + 4×A76(`cpu4-7`) 异构，不钉核的抖动比 `-O2`/`-Os` 差异还大
  ⇒ `taskset -c 4`（A76）或 `-c 0`（A55），两个版本用同一个核。
* governor 出厂就是 `performance`，跑前确认没被改过。
* **取数**：每轮内部取**最小值**，再对多轮取**中位数**；板子上没有 `/usr/bin/time`（实测），
  用程序内 `clock_gettime(CLOCK_MONOTONIC)`。
* **必须同时校验"结果没变"**：跑 checksum 与基线比 —— 改了计算结果就不能用于发布产物。
* **常驻服务别拿"启动一次"当基准**：给固定工作负载比 CPU 时间，否则量到的是进程启动开销。
* 板子上没有 `hyperfine`/`perf`（实测），要用就 `apt-get install -y hyperfine` —— **测试机**依赖，不进镜像。

### 8.5 判据：把"省下的体积"和"涨上去的时间"摆在一起看

镜像 5.6 GiB 里省 780 KB ≈ **0.013%**；为它让某个进程执行时间涨几倍，永远不划算。
**只有当体积收益是 MB 级时，才值得去碰有性能/行为风险的手段。** 实测（`tests/opt-flags-bench.sh`，A76 钉核）：

| 手段 | 体积 | 执行时间 |
| --- | --- | --- |
| `-s`（strip） | **省 4,584 B（6.3%）** | −1.4%（噪声级，机器码逐字节相同） |
| `-Os` / `-Oz` | 省 **32 B（0.04%）** | **+20.7%**；可向量化负载 **+289% / +353%** |
| `-lto` / `--gc-sections` / `-fno-omit-frame-pointer` | 0 ~ 几百字节（`--gc-sections` 反而大 400 B） | 噪声级 |
| `-O3` | 0 B | 只对可向量化负载有收益（vec_f32 −27.5%） |
| `-ffast-math` | **+1.6 KB** | 部分负载 −28%~−39%，**但计算结果变了** |

## 9. 「x86 交叉」和「板子原生」编出来差多少？→ 见单独一篇

同一份基准的交叉 / 原生产物在所测负载中结果一致，性能方向不定；不能推广为所有应用。
发布时选择与目标兼容的工具链 / sysroot，并验证 ISA、动态库与实际运行行为。
完整 A/B 表、测量范围与构建位置选择
→ **[apps-toolchain-baseline.md](apps-toolchain-baseline.md)**。
