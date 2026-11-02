# APT 源改用 deb822 格式（Ubuntu 24.04）

> **结论先行**：Ubuntu 24.04 的底包**只带 deb822 格式的 `/etc/apt/sources.list.d/ubuntu.sources`，
> 没有 `/etc/apt/sources.list`**。脚本必须改写那个 `.sources` 文件，**不能**再按旧格式另写
> `sources.list` —— 否则同一套 suite 被配置两次，apt 报 "configured multiple times" 并重复向官方源取索引
> （国内访问 `ports.ubuntu.com` 很慢，明显拖慢构建）。另外 **URIs 只能用 http**：底包里没有任何 TLS 证书。

## TL;DR

- 底包**只有 deb822 的 `ubuntu.sources`** ⇒ 脚本改写它、并 `rm` 掉旧格式文件（§1、§3）。
- ⛔ **首次 `apt-get update` 不能走 https**（底包没有 CA 证书，鸡生蛋；§2）。
- **ROS 2 源与签名密钥内置在 `overlay/`**，构建期不联网；密钥有效期实测到 **2030-06-01**（§5）。
- ROS 2 镜像**会"同步中"**（Release 更新了、Packages 还没同步完）⇒ 脚本必须能换源（§5.1）。
- 换源前先跑 `tools/bench-mirrors.sh` 测速；**当前默认：apt/ROS2/pip 全走华为云**（§5.2）。

## 1. 怎么确认底包用的是哪种格式

底包是 `ubuntu-base-<版本>-base-arm64.tar.gz`，不用解压整包，直接查里面的文件与内容：

```bash
TARBALL=build/ubuntu-base-24.04.5-base-arm64.tar.gz
tar tzf "$TARBALL" | grep -E '^etc/apt/'                      # 底包 /etc/apt 下有什么
tar xzf "$TARBALL" -O etc/apt/sources.list.d/ubuntu.sources    # deb822 内容（权威模板）
tar xzf "$TARBALL" -O etc/apt/sources.list                     # 报错/空 ⇒ 没有旧格式文件
```

当前底包为 [Ubuntu Base 24.04.5 arm64](https://cdimage.ubuntu.com/ubuntu-base/releases/24.04.5/release/)，
仍使用 `noble`。2026-10-13 已按官方 `SHA256SUMS` 核对 tar；与 24.04.4 比较，
两份 `ubuntu.sources` 字节相同，均无旧格式 `sources.list`，内容是两组 stanza：

```
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: noble-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

> ⚠️ `Suites:` 里写的是具体代号（`noble`），**换发行版要跟着改**；照抄别的发行版的配置会让本构建挂掉。

## 2. 为什么不能改用 https

底包里**没有任何 TLS 证书**：

```bash
tar tzf "$TARBALL" | grep -cE '^usr/share/ca-certificates|^etc/ssl/certs'   # → 0
tar tzf "$TARBALL" | grep -c 'etc/ssl/certs/ca-certificates.crt'            # → 0
```

而 `ca-certificates` 包必须**通过 apt 装**——要先能 `apt-get update`，鸡生蛋问题。所以首次 update 之前
源必须是 `http`。镜像站在国内直连，http 没有实质风险；真要 https 得先加一个只读 http 源装完证书再切，收益很小。

## 3. 脚本现在的做法

`scripts/build-lsp-image.sh` `rootfs` 阶段改写底包自带的 deb822 文件（而不是另写 `sources.list`）：

```
Types: deb
URIs: ${mirror}                                    # 默认华为云 http://repo.huaweicloud.com/ubuntu-ports/
Suites: ${release} ${release}-updates ${release}-backports
Components: main restricted multiverse universe
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: ${mirror}
Suites: ${release}-security
Components: main restricted multiverse universe
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

随后 `rm -f /etc/apt/sources.list` 清掉旧格式文件，避免重复。ROS2 源同样是 deb822
（写到 `/etc/apt/sources.list.d/ros2.sources`）：`Suites` 用 chroot 内 `/etc/os-release` 的
`UBUNTU_CODENAME`、`Architectures` 用 `dpkg --print-architecture` 现算 ⇒ 换发行版/架构不用改这里。

> ⛔ **别把 ROS2 源想成 USTC**：脚本现在的默认是 **华为云**（`ROS2_MIRROR="http://repo.huaweicloud.com/ros2/ubuntu"`，
> 2026-09-12 测速后定的），回退链 **清华 → 上游**（§5.2）。本文早先版本写的是 USTC，已过期。

## 4. 验证方法

```bash
# 主源是脚本级 write_file，dry-run 能直接看到写出来的内容
bash scripts/build-lsp-image.sh --board=lubancat-5-v2 --dry-run | sed -n '/ubuntu.sources/,/内容如上/p'
# 构建后在 chroot 里确认没有重复配置、且没有旧格式文件
apt-get update 2>&1 | grep -i "multiple times"     # 应无输出
ls /etc/apt/sources.list.d/                        # ubuntu.sources / ros2.sources（无 .list）
```

> 混用是允许的：`add-apt-repository` 加的 PPA 目前仍写 `.list`（旧格式），apt 两种都支持。

## 5. ROS 2 apt 源与签名密钥（内置在 `overlay/`，构建期不联网）

ROS 2 的 apt 源走 `ROS2_MIRROR`（默认华为云），**签名密钥直接内置在仓库里**：
`overlay/usr/share/keyrings/ros-archive-keyring.gpg`，构建时（BASE 会话之前）拷进 rootfs
⇒ 整个构建不需要访问 GitHub。

必须内置的原因：官方唯一有效的下载地址是 GitHub raw（国内经常不可达），而
`http://packages.ros.org/ros.key` 上是**同一把 key 的旧自签名**，有效期停在 **2025-06-01**
（指纹 `C1CF6E31E6BADE8868B172B4F42ED6FBAB17C654`）—— 拿它当 `Signed-By:` 只会让 apt 报 `EXPKEYSIG`，
而且报错信息完全看不出是"key 过期"。

- ✅ **内置文件实测（2026-10-03）**：`pub rsa4096 2019-05-30 [expires: 2030-06-01]`，
  指纹同上。构建脚本每次都会用 `gpg --show-keys --with-colons` 自检有效期，过期就直接报错并打印更新命令。
- ROS 延期后更新（更新完重新构建即可）：
  ```bash
  curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
      -o overlay/usr/share/keyrings/ros-archive-keyring.gpg
  ```
- 万一 overlay 里的文件被删，脚本会退回到联网下载（`--connect-timeout 10 --max-time 120`、重试 2 次、
  有前后日志），失败时给出一条可复制的补救命令。

### 5.1 ⚠️ ROS 2 镜像会「同步中」—— 构建必须能换源（踩过，`rootfs` 阶段被打断）

症状：`Err:6 … noble/main arm64 Packages / File has unexpected size (1958240 != 2032459). Mirror sync in progress?`
⇒ `apt-get update` 失败、`set -e` 直接中断 `rootfs` 阶段。

**不是"等一会儿就好"**：实测 2026-09-11 22:11 那次同步之后，USTC 的 ROS2 源挂了**十几个小时**仍不自洽。
当时各源实测（Release 声明的大小 vs 实际提供）：

| 源 | Release 日期 | 声明 | 实际 | 结论 |
| --- | --- | --- | --- | --- |
| `mirrors.ustc.edu.cn/ros2/ubuntu`（**当时**默认） | 09-11 22:11 | 2032459 | **1958240** | ✗ 坏 |
| `mirrors.tuna.tsinghua.edu.cn/ros2/ubuntu` | 09-11 02:57 | 2032603 | 2032603 | ✓ |
| `packages.ros.org/ros2/ubuntu`（上游） | 09-11 22:11 | 2032459 | 2032459 | ✓ |

**脚本现在的做法**：① `ROS2_MIRROR`（默认华为云）+ `ROS2_MIRROR_FALLBACK`（清华 → 上游）**逐个试**；
② 判据**刻意不是**"`apt-get update` 是否成功"，而是 **`apt-cache show ros-jazzy-ros-base` 能否解析** ——
任意一个**无关**源失败都会让 update 返回非 0，而 apt 其实沿用了旧索引（自己会说 "old ones used instead"），
那种情况**不该**换源；③ 会话里所有 `apt-get update` 都包了 `apt_update_retry`（3 次、间隔 10 s）。

自己核对任意源是否自洽（期望值取 `Release` 里那行、实际值取 `Content-Length`）：

```bash
u=http://mirrors.ustc.edu.cn/ros2/ubuntu
curl -s "$u/dists/noble/Release" | grep 'main/binary-arm64/Packages.gz$'
curl -sI "$u/dists/noble/main/binary-arm64/Packages.gz" | grep -i content-length
```

手动指定源：`ROS2_MIRROR=<url> sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=rootfs`。

### 5.2 镜像源测速：`tools/bench-mirrors.sh`（2026-09-12）

apt / ROS2 / pip 三类源用同一个工具测，**选源前先跑它**。

**为什么要有工具，而不是凭一次 `curl` 下结论**：

- **单次结果不可信**：同一个源同一分钟内两次能差 10 倍（`mirrors.ustc.edu.cn/ubuntu-ports` 先 0.43 MB/s、
  几分钟后 4.2 MB/s）⇒ 工具**交错多轮**（每轮每个源都测一次）再**取中位数**，避免某一刻链路抽风落到某个源头上。
- **大小文件分开测**：索引 ~1.8 MB，镜像可能对大文件另有限速 —— 工具从索引里挑一个 ≥`--large-mb` 的 deb，
  用 HTTP Range 只取前 N MB，测持续速率。
- **http / https 都测**：运营商可能对 http 限速；而构建 chroot **第一次 update 只能用 http**（§2），
  装完证书才能上 https —— 两套都测才知道"以后要不要换 https"。
- **官方源默认不测**（国内基本不可用），要测加 `--include-official`。

```bash
tools/bench-mirrors.sh                             # apt + ros2 + pip，3 轮，大文件 64 MB（约十几分钟）
tools/bench-mirrors.sh --kind=apt --rounds=5       # 只测 apt，5 轮
tools/bench-mirrors.sh --rounds=1 --large-mb=16    # 快速看一眼
tools/bench-mirrors.sh --list                      # 只列源
```

**在真正跑构建的那台机器上跑**（`rootfs` 阶段 的 apt/pip 是在这台机器上执行的，换台机器结论会变）。
输出按大文件速度倒序；过程全程打进度，要安静加 `--quiet`。

**已测结果（2026-09-12，开发板，`--rounds=2 --large-mb=64`，整轮 247 s）** ——
小文件 = `Packages.gz`(1.78 MB)，大文件 = `openjdk-17-jdk-headless`(70.5 MB) 取前 64 MB：

| apt（ubuntu-ports） | 协议 | 小文件 | 大文件 | TTFB |
| --- | --- | --- | --- | --- |
| **华为云** | http | **5.66 MB/s** | **13.80 MB/s** | 0.16 s |
| 清华 | http | 5.53 MB/s | 13.61 MB/s | 0.12 s |
| 华为云 | https | 4.82 MB/s | 13.53 MB/s | 0.15 s |
| 中科大 | https | 2.60 MB/s | 13.27 MB/s | 0.19 s |
| cernet | http | 1.89 MB/s | 13.08 MB/s | **2.91 s** |
| **中科大（当时默认）** | http | 2.80 MB/s | 12.69 MB/s | 0.13 s |

| ROS2 | 协议 | 小文件 | 大文件 | TTFB |
| --- | --- | --- | --- | --- |
| **华为云** | http | **7.02 MB/s** | **13.64 MB/s** | 0.10 s |
| 清华 | http | 4.69 MB/s | 13.41 MB/s | 0.26 s |
| 华为云 | https | 5.33 MB/s | 13.24 MB/s | 0.15 s |
| 中科大 | https | 3.48 MB/s | 12.45 MB/s | 0.19 s |
| 中科大 | http | 3.21 MB/s | 11.43 MB/s | 0.11 s |
| 清华 | https | 6.32 MB/s | 9.84 MB/s | 0.14 s |

| pip（三家都只有 https 通；小包 `ruamel.yaml` ~120 KB、大包 numpy ~15 MB） | 大包 | 小包 |
| --- | --- | --- |
| 华为云 | 5.28 MB/s | 0.08 MB/s |
| 清华 | 5.25 MB/s | 0.08 MB/s |
| 中科大 | 5.04 MB/s | 0.07 MB/s |

> ⚠️ pip 的**小包那列没有意义**：120 KB 的包完全被 pip 自身的解析/解释器启动开销主导
> （0.08 MB/s ≈ 1.5 s 拉完一个 120 KB 的包），要看区分度只能看大包。
> 更早一轮（单源、样本少）的结论一致：华为云 4.64–5.00 MB/s 最稳，清华偶发 9 s 抖动，
> 腾讯云 1.65 / 阿里云 1.17 / 官方 pypi.org 0.22 MB/s。

**从这轮数据能读出来的三件事**：

1. **大文件不限速**：所有源的大文件速率挤在 12–13.8 MB/s，基本就是链路上限。
2. **http 与 https 没有系统性差异**（同源两种协议互有胜负、差值在噪声内）；但首次 update 仍只能用 http（§2）。
3. **构建期是"几百个小文件"型负载，小文件那列才是关键**：华为云/清华 5.5–7.0 MB/s，中科大只有 2.6–3.5 MB/s。

**结论（默认值已按这轮改）**：apt（ubuntu-ports）与 ROS2 都从**中科大 → 华为云 http**
（`http://repo.huaweicloud.com/ubuntu-ports/` / `http://repo.huaweicloud.com/ros2/ubuntu`，回退清华 → 上游）；
pip 走 `APP_PIP_INDEX_URL` = 华为云。⚠️ 华为云那份 ROS2 `Packages.gz` 比 ustc 旧 74 KB，
构建期靠 `apt-cache show ros-jazzy-ros-base` 验证，不通过自动回退清华。任何一类都能用环境变量覆盖：

```bash
APP_PIP_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple \
  sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=rootfs --force
```

### 5.3 `/var/lib/apt/lists` 的体积与处置

历史索引实测 **204 MB**（其中翻译索引 61 MB），构建期**故意清掉**；默认关闭翻译索引
（`overlay/etc/apt/apt.conf.d/99-lsp-no-translations`）。当 `DISABLE_AUTO_UPDATE!=N` 时，
`rootfs` 阶段在首个 chroot 会话前安装此配置，首轮 `apt-get update` 即生效；`N` 只跳过安装，
复用 rootfs 时不删除已有配置。**索引继续清**
（204 MB ≈ 镜像 4%，只为省每台机器一次 13 秒的 `apt update` 不划算）；**翻译索引关掉**
（该次设备测量少下 ~30 MB、少写 61 MB，耗时 30–40 s → 13 s；不是当前构建提速承诺）。
完整数据、决策理由与测量方法在 [image-size-trimming.md](image-size-trimming.md) §4。

## 6. 历史记录

上游遗留脚本 `scripts/build-rootfs.sh`（**2026-09-24 已从仓库删除**）当年是按旧格式写 `sources.list` 的 ——
现在 `build-lsp-image.sh` 已完全切到 deb822。
