# 排查记录：自建 U-Boot 缺少 ATF，启动卡在 SPL→U-Boot

> **结论先行**：根因是 `u-boot/arch/arm/mach-rockchip/decode_bl31.py` 的 shebang 写死 `python2`，
> 而构建机（Ubuntu 24.04）已无 python2，脚本执行失败（exit 127）；由于 `fit_nodes.sh` 未启用
> `set -e`，失败被**静默吞掉**，最终生成的 `u-boot.itb` 里根本没有 ATF 节点。
> 已修复 shebang 并补充硬校验。

## TL;DR

- 现象：自建 U-Boot 启动**卡在 SPL 之后、U-Boot 之前**（没有 U-Boot 头，也没有 ATF banner）。
- 根因：`u-boot/arch/arm/mach-rockchip/decode_bl31.py` 的 shebang 写死 `python2`，构建机（Ubuntu 24.04）
  没有 python2 ⇒ 脚本 exit 127；而 `fit_nodes.sh` **没开 `set -e`** ⇒ 失败被**静默吞掉**，
  最终 `u-boot.itb` 里**根本没有 ATF 节点**。
- 修法与构建期断言见 §5；验证见 §6；`image` 阶段的检测/覆盖逻辑见 §10。
- 💡 方法论：这种静默失败要用**产物结构**判断（FIT 里到底有几个 `atf` 节点），别只看构建日志（§7）。
- 容易误判的方向：§8 列了本次已排除的几条（省得下次再走一遍）。

## 1. 现象

- 用本仓库编译出的镜像**无法启动**：卡在「U-Boot SPL 之后、完整 U-Boot 之前」，无进一步输出
- 把另一套「确定可用」的 `idbloader.img` + `u-boot.itb` 覆盖到镜像头部 16MB 区域后**可正常启动**
- 也就是说：问题出在 bootloader 产物本身，而不是分区表、文件系统或内核

## 2. 排查思路

先建立假设树，再逐条证伪 —— 关键是把问题**分层**：

```
「卡在 SPL→U-Boot」
├─ A. 数据没生成：BL31 压根没进 u-boot.itb
│    ├─ A1. rkbin 里的 BL31 文件缺失/不完整
│    ├─ A2. 构建时没把 BL31 传给 U-Boot
│    ├─ A3. 传入方式无效（环境变量 vs make 变量）
│    └─ A4. 处理 BL31 的脚本执行失败 ← ★ 实际根因
├─ B. 数据生成了但没写对：dd 偏移错、镜像被压缩后覆盖失效
└─ C. 数据写对了但版本不匹配：DDR 固件版本不符、BL31 与 U-Boot 不兼容
```

排查顺序上，**先用最低成本的手段确认 A（产物里到底有没有 ATF）**，
因为一旦确认 A 不成立，B/C 才有排查价值。

## 3. 逐步取证

### 3.1 对照物：确认「可用的」固件里确实有 ATF

```console
$ strings prebuilt/u-boot.itb | grep -iE "atf-1|arm-trusted-firmware"
atf-1
*arm-trusted-firmware
*arm-trusted-firmware
Zatf-1
```

→ 可用固件中 FIT 含 `atf-1` 节点与 `arm-trusted-firmware` 标识，**ATF 是可判定特征**。

进一步用 FIT 结构解析（脚本见文末附录）确认 ATF 数据块真实存在：

```
/images/atf-1   data-size = 0x3203c   load = 0x40000     os = arm-trusted-firmware
/images/atf-2   data-size = 0x9000    load = 0xff100000  os = arm-trusted-firmware
/images/atf-3   data-size = 0x6000    load = 0xf0000     os = arm-trusted-firmware
/configurations/conf  firmware = "atf-1"  loadables = "uboot", "atf-2", "atf-3"
```

→ `firmware = "atf-1"` 说明 **SPL 的第一跳就是 ATF**，这解释了为什么缺 ATF 会在 SPL 之后卡住。

### 3.2 排除 A1：rkbin 的 BL31 是否存在

> ⚠️ 下面这段是**当时的现场记录（BL31 还是 v1.47）**。2026-09-18 已把 `rkbin/` 里的 BL31
> 升到 **v1.53**（文件名/大小/哈希都变了，`targets.mk` 也跟着改了），见
> [../build/upstream-sync.md](../build/upstream-sync.md)。**排查手法与结论不受影响。**

```console
$ ls -la packages/u-boot-lubancat-rk3588/debian/rkbin/
-rw-r--r-- 1 developer developer 430880 rk3588_bl31_v1.47.elf                              ← 存在
-rw-r--r-- 1 developer developer  73456 rk3588_ddr_lp4_1560MHz_lp5_2400MHz_v1.17.bin
-rw-r--r-- 1 developer developer  75320 rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin
```

板卡到固件的映射也齐全（`debian/targets.mk`）：

```make
u-boot-rockchip_platforms += lubancat-5-v2-rk3588
lubancat-5-v2-rk3588_ddr  := rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin
lubancat-5-v2-rk3588_bl31 := rk3588_bl31_v1.47.elf          ← 映射正确（当时）
```

→ **A1 排除**：依赖文件齐全。

### 3.3 排除 A2：构建规则是否传了 BL31

`packages/u-boot-lubancat-rk3588/debian/rules`：

```make
@cp debian/rkbin/$($@_bl31) debian/build/$@/bl31.elf                              # 第 35 行：拷入构建目录
make O=debian/build/$@ CROSS_COMPILE=$(CROSS_COMPILE) ARCH=arm $(NJOBS) \
  BL31=../../../debian/rkbin/$($@_bl31) \                                          # 第 42 行：传给 make
  spl/u-boot-spl.bin u-boot.dtb u-boot.itb
```

→ **A2 排除**：规则确实传了 `BL31`，而且额外把 `bl31.elf` 拷进了构建目录。

### 3.4 查 A3：BL31 的传递方式是否真的生效

先看 defconfig 是否使用 ATF 生成器：

```console
$ grep -E "SPL_FIT_GENERATOR|SPL_ATF" u-boot/configs/lubancat-5-v2-rk3588_defconfig
CONFIG_SPL_FIT_GENERATOR="arch/arm/mach-rockchip/make_fit_atf.sh"
CONFIG_SPL_ATF=y
```

调用链：`make u-boot.itb` → 生成 `u-boot.its` → `make_fit_atf.sh` → `source fit_nodes.sh` → `gen_bl31_node`
→ `decode_bl31.py`。

再看 U-Boot 的 Makefile 是否把该 make 变量导出给子进程：

```console
$ grep -n "export.*BL31" u-boot/Makefile
（无输出）
```

而 `decode_bl31.py` 是按**环境变量**读的：

```python
def main():
    if "BL31" in os.environ:
        bl31_elf = os.getenv("BL31")
    elif os.path.isfile("./bl31.elf"):
        bl31_elf = "./bl31.elf"        # ← 回退路径（rules 第 35 行已把 bl31.elf 拷到构建目录）
    else:
        os.system("echo 'int main(){}' > bl31.c")     # ← 都没有时生成「假 ELF」，并警告 non-functional
        ...
```

→ make 命令行变量不会被自动导出，所以第一分支取不到值；**但第二分支（`./bl31.elf`）本应兜住**。
说明问题不在「传参」，而在**脚本能不能跑起来**。

### 3.5 定位 A4：脚本能否执行（★ 根因）

```console
$ head -1 u-boot/arch/arm/mach-rockchip/decode_bl31.py
#!/usr/bin/env python2
```

直接在「有 bl31.elf 的目录」里执行，模拟构建时的调用方式：

```console
$ cp .../rkbin/rk3588_bl31_v1.47.elf ./bl31.elf      # 当时那份；现在是 rk3588_bl31_v1.53.elf
$ ./decode_bl31.py ; echo "exit=$?"
pyenv: python2: command not found
exit=127
$ ls bl31_0x*.bin
（未生成任何文件）
```

换 python3 执行**同一个脚本**：

```console
$ python3 decode_bl31.py ; echo "exit=$?"
exit=0
$ ls bl31_0x*.bin
bl31_0x00040000.bin   bl31_0x000f0000.bin   bl31_0xff100000.bin
```

→ **根因确认**：脚本本身**完全兼容 python3**，唯一障碍是 shebang 指向已被移除的 python2。
Ubuntu 23.04 起 python2 已从仓库移除，24.04 上不存在该解释器。

### 3.6 解释「为什么没有任何报错」

`fit_nodes.sh` 开头没有 `set -e`，而 `gen_bl31_node` 用命令替换 + `ls` 来收集产物：

```bash
function gen_bl31_node()
{
	${srctree}/arch/arm/mach-rockchip/decode_bl31.py     # ← 失败（127），但没有人检查返回值

	NUM=1
	for ATF in `ls -l bl31_0x*.bin | sort --key=5 -nr | awk '{ print $9 }'`
	do
		...生成 atf 节点...
	done        # ← ls 匹配为空 → 循环体一次都不执行 → 一个 atf 节点都不输出
}
```

于是：

- `decode_bl31.py` 失败 → 退出码被丢弃（无 `set -e`、无显式检查）
- `ls bl31_0x*.bin` 匹配为空 → 循环体不执行
- 生成的 `u-boot.its` 里**没有任何 `atf-*` 节点**，但**构建仍然成功**
- mkimage 照常打包出 `u-boot.itb` —— 一个不含 ATF 的、无法启动的 FIT

这就是典型的**静默失败（silent failure）**：构建全绿，产物却是坏的。

## 4. 根因链路

```
decode_bl31.py shebang = python2
        │  Ubuntu 24.04 无 python2 → 执行失败 exit 127
        ▼
bl31_0x*.bin 未生成
        │  fit_nodes.sh 无 set -e，失败被忽略
        ▼
gen_bl31_node 的 for 循环体不执行 → ITS 中无 atf-* 节点
        │  mkimage 正常打包，构建返回成功
        ▼
u-boot.itb 不含 ATF
        │  FIT conf 的 firmware = "atf-1"，SPL 第一跳指向 ATF
        ▼
SPL 交接失败 → 卡在 SPL→U-Boot 阶段 ❌
```

## 5. 修复

### 5.1 根因修复：shebang 改为 python3

`u-boot/arch/arm/mach-rockchip/decode_bl31.py`

```diff
-#!/usr/bin/env python2
+#!/usr/bin/env python3
```

脚本正文本身就是 py2/py3 兼容写法（只用 `struct`、`logging`、`with open` 等），无需其他改动。

### 5.2 防复发：`gen_bl31_node` 增加硬校验

`u-boot/arch/arm/mach-rockchip/fit_nodes.sh`，在循环结束后判断：

```bash
	# 硬校验：未产出任何 ATF 段时直接失败，避免静默生成不含 ATF 的 u-boot.itb
	if [ ${NUM} -eq 1 ]; then
		echo "ERROR: no ATF (bl31) segments were generated!" >&2
		echo "       u-boot.itb would lack ARM Trusted Firmware and fail to boot after SPL." >&2
		echo "       Check that decode_bl31.py is runnable (it needs python3) and that" >&2
		echo "       bl31.elf (BL31) exists in the build directory." >&2
		exit 1
	fi
```

要点：
- 输出到 **stderr**（该脚本 stdout 被重定向进 `u-boot.its`，写到 stdout 会消失在 ITS 文件里）
- `exit 1` 会让 make 直接失败，把「静默坏产物」变成「构建报错」

### 5.3 构建脚本增加校验与兜底

`scripts/build-lsp-image.sh`：

- **`board` 阶段**：安装 U-Boot deb 后校验 `/usr/lib/u-boot/u-boot.itb` 是否含 `arm-trusted-firmware`，缺失则告警
- **`image` 阶段**：自动检测镜像内 bootloader，**缺 ATF 才**用 `prebuilt/` 覆盖；已含 ATF 则保留自建 U-Boot
  （`--atf` 可强制覆盖）—— 这样修好根因后，自己编译的 U-Boot 就不会被覆盖掉

## 6. 验证

| 验证项 | 命令 / 方法 | 结果 |
| --- | --- | --- |
| shebang 修复生效 | 在含 `bl31.elf` 的目录执行 `./decode_bl31.py` | `exit=0`，生成 3 个 `bl31_0x*.bin` ✓ |
| 硬校验触发 | 模拟 `NUM=1`（无 ATF 段） | 报错并 `exit=1` ✓ |
| 硬校验不误报 | 模拟 `NUM=4`（正常） | `exit=0` 不打扰 ✓ |
| ATF 检测（真实布局） | 8MB 处写入 `u-boot.itb` 后检测 | 判定含 ATF ✓ |
| ATF 检测（无 ATF） | 全零镜像 | 判定缺 ATF ✓ |
| ATF 检测（合并布局） | 32KB 处写入 | 判定含 ATF ✓ |
| ATF 检测（文件缺失） | 传不存在路径 | 安全返回 false ✓ |
| 写入位置正确 | `xxd -s $((64*512))` / `xxd -s $((16384*512))` | `524b4e53`(RKNS) / `d00dfeed`(FIT) ✓ |
| 写入内容正确 | 从镜像抽取两段与源文件 `cmp` | 逐字节一致 ✓ |
| 交叉验证 | FIT 内 ATF 段尺寸 vs 直接用 `bl31.elf` 解码 | 204860 / 36864 / 24576 完全一致 ✓ |

## 8. 容易误判的方向（本次已排除）

| 猜测 | 实际情况 |
| --- | --- |
| DDR 固件版本不对 | 无关。idbloader 里的 DDR 固件 `fwver v1.18` 与 rkbin 一致，SPL 阶段正常 |
| U-Boot 版本不兼容 | 无关。U-Boot proper 本身没问题，是它**之前**的一跳断了 |
| `dd` 偏移写错了 | 无关。32KB / 8MB 两个偏移与 Rockchip 标准一致，写入也正确 |
| `BL31=` 没传给 make | 传了，但即使没传也有 `./bl31.elf` 回退；真正的问题是脚本跑不起来 |
| mkimage 打包有问题 | 打包正常，是**输入给它的 ITS 里就没有 ATF 节点** |
| 应该换一套 bootloader 了事 | 能绕过，但会失去「后续修改 U-Boot」的能力，且掩盖根因 |

## 9.~12. FIT 解析脚本 / `image` 阶段覆盖逻辑 / 打包坑 / `check-atf.sh` → 见 [atf-fit-and-image-validation.md](atf-fit-and-image-validation.md)

本次修复后沉淀的工具与原理（FIT 解析脚本、`image` 阶段的检测与覆盖完整逻辑、`build/` 副本与打包坑、
`tools/check-atf.sh` 的用法与判据）**已搬成单独一篇** →
**[atf-fit-and-image-validation.md](atf-fit-and-image-validation.md)**。

