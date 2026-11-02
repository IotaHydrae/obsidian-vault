# ATF 相关工具：FIT 解析脚本、`image` 阶段覆盖逻辑、打包坑与 `check-atf.sh`

> 本文是 [atf-missing-troubleshooting.md](atf-missing-troubleshooting.md) 的**工具/原理篇**：
> 那次故障修复后沉淀下来的东西 —— FIT 结构解析脚本（抄了就能用）、**`image` 阶段的 ATF 检测与覆盖完整逻辑**、
> `build/` 源码副本与打包规则相关的坑、以及"卡在 SPL→U-Boot 时用 `tools/check-atf.sh` 怎么定位"。
> "ATF 缺失"这一次的完整排查过程（现象 → 逐条排除 → 根因 → 修复 → 验证）见主篇。
> ⚠️ 正文**原样搬自主篇的 §7~§12**（节号保留；§7 是那次排查沉淀下来的通用套路）。

## TL;DR

- `image` 阶段的逻辑：镜像里**已含 ATF** 就不覆盖、**缺 ATF** 才用 `prebuilt/{idbloader.img,u-boot.itb}` 原地覆盖。
- 现场定位用 `./tools/check-atf.sh`（依次查 `prebuilt` → `build/*.deb` → `build/rootfs` → `images/` 最新镜像，
  `-d /dev/mmcblk0` 还能查设备上实际写入的 bootloader）。
- ⚠️ `build/` 里那份 u-boot 源码副本是**打包时拷的**，改了 `u-boot/` 下的东西要注意同步规则（§11）。


## 7. 方法论沉淀

1. **先把「没生成」和「没写对」分开**
   这次真正的问题在「产物里根本没这个数据」，而不是「写到了错误偏移」。
   一开始就怀疑 dd 偏移会浪费大量时间。

2. **别把「依赖文件存在」当成「产物正确」**
   `rkbin/` 里那份 BL31（当时叫 `rk3588_bl31_v1.47.elf`，现在是 `…_v1.53.elf`）一直在，
   规则也一直传 `BL31=`，但产物就是不含 ATF —— 中间环节失败可以完全不报错。

3. **警惕 shell 里被吞掉的失败**
   `无 set -e` + `for x in $(ls ...)` 是经典组合：命令失败 → 输出为空 → 循环跳过 → 静默成功。
   凡是「生成内容供后续消费」的脚本，都应该显式校验产物非空。

4. **用结构和魔数做客观判定，而不是「看起来对」**
   `strings | grep arm-trusted-firmware`、FIT 魔数 `d00dfeed`、RKNS 魔数、`load=`/`data-size=` 字段，
   这些是可复核的硬证据。相比之下「文件大小差不多」毫无意义。

5. **交叉验证：用两条独立路径推导同一个量再对拍**
   从 FIT 里读出 `atf-1` 的尺寸，与直接用 `rkbin/` 里那份 BL31（当时是 `rk3588_bl31_v1.47.elf`，
   现在是 `…_v1.53.elf`）解码出的段尺寸对比，两边一致才敢下
   「这份 bootloader 确实带了本仓库的 BL31」的结论。

6. **二进制写入必须同时验证「偏移」和「内容」**
   魔数确认落点正确，`cmp` 逐字节确认内容无偏移/截断。

7. **把一次踩坑变成机制**
   只修文件是治标；把校验加进构建（硬失败 + `board` 阶段告警 + `image` 阶段兜底），
   才是让这类问题不再复发。

## 9. 附：FIT 结构解析脚本

用于客观查看 `u-boot.itb` 的镜像节点、加载地址与数据布局（仅用标准库）：

```python
#!/usr/bin/env python3
"""fit-dump.py —— 解析 U-Boot FIT (u-boot.itb) 结构
用法: python3 fit-dump.py u-boot.itb
"""
import struct
import sys

data = open(sys.argv[1], 'rb').read()
if data[:4] != b'\xd0\x0d\xfe\xed':
    sys.exit("不是 FIT/FDT 镜像（魔数应为 d00dfeed）")

_, totalsize, off_struct, off_strings = struct.unpack_from('>4I', data, 0)


def name_at(off):
    end = data.index(b'\0', off_strings + off)
    return data[off_strings + off:end].decode()


pos, nodes = off_struct, []
print(f"FIT FDT 大小={totalsize}  文件大小={len(data)}\n")

while True:
    tag, = struct.unpack_from('>I', data, pos)
    pos += 4
    if tag == 1:                                   # BEGIN_NODE
        end = data.index(b'\0', pos)
        nodes.append(data[pos:end].decode())
        pos = end + 1 + ((4 - (end + 1 - pos)) % 4)
    elif tag == 2:                                 # END_NODE
        if nodes:
            nodes.pop()
    elif tag == 3:                                 # PROP
        ln, noff = struct.unpack_from('>2I', data, pos)
        pos += 8
        val = data[pos:pos + ln]
        pos += (ln + 3) & ~3
        nm = name_at(noff)
        path = '/' + '/'.join(nodes)
        if nm in ('data-offset', 'data-size', 'load', 'entry') and ln == 4:
            v = f"0x{struct.unpack('>I', val)[0]:x}"
        elif nm in ('os', 'type', 'description', 'compression', 'firmware',
                    'loadables', 'fdt', 'default'):
            v = val.rstrip(b'\0').decode(errors='replace')
        else:
            continue
        print(f"{path:26s} {nm:14s} = {v}")
    elif tag == 9:                                 # END
        break
```

判定是否含 ATF 的最简方式（无需脚本）：

```bash
strings u-boot.itb | grep -q "arm-trusted-firmware" && echo "含 ATF" || echo "缺 ATF"
```

## 10. `image` 阶段中的 ATF 检查与补写

`image` 阶段先生成裸 `.img`，再检查镜像头部的 ATF。检查或补写失败时不发布镜像完成标记。

- 已含 ATF：保留本次构建的 U-Boot。
- 缺少 ATF：使用成对 `prebuilt/idbloader.img`、`prebuilt/u-boot.itb` 原地覆盖镜像头部，
  两处偏移分别为 32 KB（`seek=64`）和 8 MB（`seek=16384`），写后重新检查。
- `--atf`：即使镜像已经含 ATF，也使用 prebuilt 覆盖。
- 重新封装并检查：`sudo ./scripts/build-lsp-image.sh --board=<BOARD> --stage=image`。
- 只读检查已有镜像：`./tools/check-atf.sh -i <镜像>`；该命令不修改镜像。
- 制镜像、ATF 检查或补写，以及启用时的 SHA256 生成全部成功后，才发布
  `build/.stage-image-<rootfs名>-<板型>.done`；不再维护独立 ATF 阶段或 `.atf-ok` 标记。

ATF 补写只改裸镜像头部，不负责压缩。分发归档在构建成功后交给 `tools/compress-image.sh`。
镜像布局与检查依据见 [rk3588-boot-flow.md](rk3588-boot-flow.md) 第 6 节。

## 11. `build/` 里的源码副本与打包规则相关的坑

> **关于 `build/` 里的副本**：`build-u-boot.sh` 每次都会同步 `packages/.../debian/` 打包控制文件
> 并重置 `debian/files`（保留 `debian/build/` 编译缓存）。重置这个文件是必要的：
> `dpkg-buildpackage` 带 `-nc` 会跳过 `clean`，而 `debian/files` 记录着「上次产出了哪些 deb」，
> 只编单平台时会残留其它板卡条目，导致 `dpkg-genbuildinfo: error: cannot fstat file
> ../u-boot-lubancat-4_*.deb`。但 **u-boot 源码只在首次创建副本时拷贝**：改过 `u-boot/` 下与
> ATF/FIT 相关的源码（`fit_nodes.sh`、`decode_bl31.py`）后，需先 `sudo rm -rf build/<u-boot 包目录>`
> 再编；忘记删时脚本会打印「与 u-boot/ 下的不一致」提示。
>
> 编完可用这条确认 itb 里确实有 ATF（要看到 `(atf-1)` 之类的行；一条都没有就别刷机）：
> ```bash
> tmp=$(mktemp -d); dpkg-deb -x build/u-boot-lubancat-5-v2_*.deb "$tmp"
> dumpimage -l "$tmp/usr/lib/u-boot/u-boot.itb" | grep -E '\(atf'
> ```
> （**现在不用手敲了**：`build-u-boot.sh` 打包后会自己对本次产出的 deb 做这个结构性校验，
> 不含 ATF 直接让构建失败，并打印可直接复制的复核命令。）
>
> **在本机（较新的 dpkg）上编译报 `cp: cannot stat 'debian/build/<平台>/u-boot.itb'`**：
> dpkg 从 **1.23** 起把 `Rules-Requires-Root` 的缺省值从 `binary-targets` 改成了 `no`，于是
> `dpkg-buildpackage -b` 认为这是 rootless 构建，**不再单独调用 `debian/rules build`** —— 它假设
> `binary` 目标自己会触发编译（Debian 政策要求）。而本包的平台目标是「与 `debian/build/<平台>` 同名
> 的目录目标」且没有 prerequisites，原来的 `binary-arch` / `package-*` 又都没依赖它，编译于是被整个
> 跳过，最后 `cp` 一个不存在的 `u-boot.itb` 才报错。修法有两层，都已落在仓库里：
> - `packages/u-boot-lubancat-rk3588/debian/rules`：用静态模式规则 `package-%: %` 把「打包」挂在
>   「该平台编译」之后（旧版 dpkg 先跑 `build`，那时平台目标已最新、自动跳过，不会重复编译）
> - `scripts/build-u-boot.sh`：平台目录在但缺 `u-boot.itb`（上次编译中途失败）时自动删掉重编，
>   避免 make 把同名目录当成「已满足的目标」
>
> Ubuntu 24.04 的 dpkg 是 1.22.6（仍是旧默认值），所以在板子上不会遇到这个报错。

## 12. 卡在 SPL→U-Boot / 镜像里没有 ATF：用 `tools/check-atf.sh` 定位

ATF 可能丢在五个地方，逐个查比猜快。**2026-09-16 起这个工具不只判"有没有 atf 节点"，还会打印
sha256 并逐段核对**（`u-boot.itb` 的 FIT 内部记录值 vs 现场重算值、`idbloader.img` 的 RKNS 头
里 DDR 固件/SPL/头部三个哈希），末尾给一张跨位置汇总表：

```bash
./tools/check-atf.sh                          # 查 ①prebuilt ②build/*.deb ③build/rootfs ④images/最新镜像
./tools/check-atf.sh -i images/xxx.img        # 指定镜像（-i= 形式同样支持）
sudo ./tools/check-atf.sh -d /dev/mmcblk0     # 再查设备上实际写入的 bootloader（-d= 同样支持）
./tools/check-atf.sh -q                       # 只打失败/提示项
```

明细长这样（每个节点都打**完整**的记录值与重算值）：

```
      atf-1  load=0x00040000 size=204860
             记录 sha256 6a4a192c104cc98c4b7e63ad0c6728b27b1af0a95fb9555b19faf55f8cf5871c
             重算 sha256 6a4a192c104cc98c4b7e63ad0c6728b27b1af0a95fb9555b19faf55f8cf5871c  ✓ 一致
```

汇总表（2026-09-16 开发板实测，`-d /dev/mmcblk0`）：

```
位置                       FIT区段sha256(前16) uboot节点(前16) atf-1节点(前16) ATF自洽
① prebuilt                 f8eb76fb45ce52e8    715d6ea2acde5772  6a4a192c104cc98c  ✓
② build/u-boot-*.deb       5912ac74a479370e    0d7dbcfbf3551370  6a4a192c104cc98c  ✓
③ build/rootfs             5912ac74a479370e    0d7dbcfbf3551370  6a4a192c104cc98c  ✓
④ 镜像 8MB 处              5912ac74a479370e    0d7dbcfbf3551370  6a4a192c104cc98c  ✓
⑤ /dev/mmcblk0 8MB 处      5912ac74a479370e    0d7dbcfbf3551370  6a4a192c104cc98c  ✓
· 跨位置一致的节点：atf-1 atf-2 atf-3 fdt
! 跨位置不同的节点：uboot（ATF/DTB 可复现；U-Boot proper 内嵌 __DATE__/__TIME__，不同构建必然不同）
```

**怎么读**：②③④⑤ 四列一致 ⇒ 设备上那份 bootloader 就是"这个 deb → rootfs → 镜像"那条链产物；
① 与它们不同**不是错误**（`prebuilt/u-boot.itb` 是另一次构建，见 `boot-serial-log.md` §4）。
真正的错误形态只有两种：某列 **ATF自洽 ✗**（FIT 里记录值与重算值不符 = 文件被改坏/截断），
或 ②③④⑤ 之间**ATF/DTB 节点**不一致（说明设备上的引导与手里的镜像不是一套）。

| 断点 | 常见原因 | 处理 |
| --- | --- | --- |
| ② `build/*.deb` 不含 ATF | **`build/u-boot-lubancat-rk3588/` 里的源码副本是旧的**（脚本只在首次创建时才从 `u-boot/` 拷贝源码，`decode_bl31.py` 的 python3 修复进不去）| `sudo rm -rf build/u-boot-lubancat-rk3588 && sudo ./build.sh --board=<BOARD> -u`（只删 `debian/build` **不够**）|
| ③ `build/rootfs/.../u-boot.itb` 不含 ATF | rootfs 里装的是旧 deb | 先修 ②，再 `--stage=board` → `--stage=image` |
| ④ 镜像不含 ATF | 镜像未完成 `image` 阶段，或文件后来被替换| 先用 `tools/check-atf.sh -i <镜像>` 定位，再以 `--stage=image` 重新封装；需要强制 prebuilt 时另加 `--atf`|
| ⑤ **设备**不含 ATF（镜像没问题）| 用 `tools/update-emmc.sh` 增量更新时**没带 `--bootloader`** —— 它默认只 rsync 两个分区，不碰 32KB~p1 的裸引导区 | `sudo ./tools/update-emmc.sh -d /dev/mmcblk0 --bootloader`，或整盘 `tools/write-sd.sh` |
| 任一列 `ATF自洽 ✗` | FIT/RKNS 内部记录值与重算值不符（写入被截断、拷贝不完整、flash 坏块）| 重新出包/重刷；对比未损坏的那一份的完整 sha256 |
| ②③④⑤ 之间 atf/DTB 节点不同 | 设备上的引导与手里这套产物不是同一次构建 | 重刷：`tools/write-sd.sh` 整盘写，或 `update-emmc.sh --bootloader` |

> ⚠️ 退出码语义：**0 = ATF 齐全且内部 sha256 自洽**（跨位置 uboot 节点不同只算"提示"，不改变退出码）；
> **1 = 有缺 ATF 或内部哈希不符**。所以它既能当"体检"，也能当脚本里的断言。
>
> 依赖：`python3`（解析 FIT/RKNS）、`u-boot-tools` 的 `dumpimage` 仅作为无 python3 时的弱回退；
> `xdelta`/`zstd`/`xz`/`gzip` 只在读压缩镜像（`-i *.img.zst` 等）时用到。

> `update-emmc.sh` 现在会在同步前**对比镜像与设备这段裸区域的哈希**，不一致就明确提示加
> `--bootloader` —— 改了 U-Boot / ATF 却只同步分区，是最容易出现"rootfs 是新的、引导却还是旧的"
> 的场景。
