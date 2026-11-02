# 怎么开 BL32（OP-TEE）并上板验证

> 本文是 [bl32-optee.md](bl32-optee.md) 的**操作篇**：一条命令怎么开、怎么验证产物**真的**开了
> （不做这步会"以为开了其实没开"）、开关在 4 处代码里怎么实现、备选做法与厂商老路径，
> 以及**不碰 eMMC** 的 SD 卡上板验证法与回滚。
> "BL32 是什么、该不该开、内核/DT 要不要改、能测到哪一层、有哪些坑、会不会烧 OTP" 见主篇。
> ⚠️ 正文**原样搬自主篇的 §4~§6**（节号保留）。

## TL;DR

- 开启：`build.sh` 上一条命令（§4.1），但**必须验证产物**（§4.2：FIT 里要出现 `optee` 节点）。
- 上板验证走 **SD 卡法**（§5）：只动 32 KB 与 8 MiB 两处、都在 16 MiB 的 bootloader 预留区内，
  **不碰 eMMC**；判据是串口多出 `## Checking optee … + OK` 那一行。
- 回滚：把这两处换回原 `prebuilt/` 产物即可（§6）。


## 4. 怎么开（构建）

### 4.1 一条命令

```bash
sudo UBOOT_WITH_BL32=1 ./build.sh -b lubancat-5-v2 -u      # 带 BL32
sudo UBOOT_WITH_BL32=1 ./build.sh -b lubancat-5io  -u
# 不给这个环境变量 = 默认（不带 BL32），行为与以前完全一致
```

- 环境变量用 `sudo VAR=val cmd` 的形式**放在 sudo 后面**也能传进去（`build.sh` → `scripts/build-u-boot.sh`
  里 `export UBOOT_WITH_BL32`，再经 `dpkg-buildpackage` 作为 make 变量进 `debian/rules`）。
- 产物照旧落 `build/u-boot-lubancat-<board>_*.deb`；构建日志里会明确打印
  `[u-boot] BL32/OP-TEE：**启用**` 或 `不启用（默认）`。

### 4.2 验证产物（**必做**，否则可能"以为开了其实没开"）

```bash
for d in build/u-boot-lubancat-5-v2_*.deb; do
  t=$(mktemp -d); dpkg-deb -x "$d" "$t"
  echo "== $d"
  dumpimage -l "$t/usr/lib/u-boot/u-boot.itb" | grep -E 'Image [0-9]+ \('
  dumpimage -l "$t/usr/lib/u-boot/u-boot.itb" | grep -A3 '(optee)' | grep -E 'Data Size|Load Address'
  strings "$t/usr/lib/u-boot/u-boot.itb" | grep -oE 'fwver: v1\.[0-9]+' | sort -u
  rm -rf "$t"
done
```

**期望（2026-09-18 实测，两板同）**：

| 构建 | `u-boot.itb` | 节点 | optee |
| --- | --- | --- | --- |
| 默认 | 1,372,672 B | `(uboot) (atf-1) (atf-2) (atf-3) (fdt) (conf)` | **0** |
| `UBOOT_WITH_BL32=1` | 1,850,880 B | `(uboot) (atf-1) (atf-2) (atf-3)` **`(optee)`** `(fdt) (conf)` | **1**，Data Size 477,616 B、**Load Address `0x08400000`** |

⚠️ **`atf-` 节点数始终是 3**（那是 BL31 的三段）—— 判断 BL32 有没有进去**只能看有没有 `(optee)` 节点**。

### 4.3 开关是怎么实现的（4 处改动，改这里时要一起看）

| 文件 | 做了什么 | 为什么 |
| --- | --- | --- |
| `packages/u-boot-lubancat-rk3588/debian/rkbin/rk3588_bl32_v1.20.bin` | 入库（466 KB，`sha256=76eeb0b48d3dbbbbac7988355de4d07a6d0bc71fbbe880333fffba9afb7b7e6b`） | 上游同款；`strings` 里是 OP-TEE，内核报的 `revision 3.13` 来自它 |
| `debian/targets.mk` | 每个平台加一行 `_bl32 := rk3588_bl32_v1.20.bin` | 只是把映射备好；**默认不参与构建**（`rules` 不用它） |
| `debian/rules` | `ifeq ($(UBOOT_WITH_BL32),1)` 时才定义 `UBOOT_TEE_ARG = TEE=../../../debian/rkbin/$($@_bl32)`，recipe 里用 `$(UBOOT_TEE_ARG)` | 用**递归变量**才能在 recipe 里展开 `$@`（平台名）；不开开关时展开为空 ⇒ 命令行多一个空参数，无副作用 |
| `scripts/build-u-boot.sh` | `export UBOOT_WITH_BL32` + 打印开关状态 + **打包后校验**：开了开关但 deb 里没有 `(optee)` 节点就 `return 1` | 防"静默不生效"——这个坑真踩过（见主篇 [bl32-optee.md](bl32-optee.md) §8 第 1 条） |

FIT 那一层（3 个脚本，`u-boot/arch/arm/mach-rockchip/`）：

1. `make_fit_atf.sh`：调用 `gen_bl32_node`（老树这行是**注释掉的**，所以以前光传 `TEE=` 完全没反应）。
2. `fit_nodes.sh` 的 `gen_bl32_node()`：**没有 `TEE` 且目录里没有 `tee.bin` 就直接 `return`**（默认路径不产出节点）；
   给了 `TEE=<路径>` 就按这个路径 `/incbin/`。外加修了上游一个 `FIRMWARE_SIGN` 多逗号的 bug。
3. `fit_args.sh`：算出 `TEE_LOAD_ADDR = DRAM_BASE + 0x08400000`
   （**RK3588 的 `CONFIG_SYS_SDRAM_BASE = 0`** ⇒ 就是 `0x08400000`；老树这里用的是没定义过的 `DARM_BASE`，
   靠 shell 把未定义变量当 0 兜住，现在改成真读 `autoconf.mk`）。

### 4.4 备选：不走 `build.sh`（例如在旧版脚本或别的机器上工作）

```bash
# 在 build/u-boot-lubancat-rk3588/ 里（脚本会先把 debian/ 从 packages/ 同步过来）
sudo UBOOT_WITH_BL32=1 UBOOT_PLATFORMS=lubancat-5-v2-rk3588 \
     dpkg-buildpackage -a arm64 -d -b -nc -uc
# 注意：这条不会做 scripts/build-u-boot.sh 的源码同步与产物校验，自己要按 §4.2 验一遍
```

### 4.5 厂商 `make.sh` 的老路径（我们不走，但兼容保留）

厂商 `u-boot/make.sh` 在 INI 里 `BL32_OPTION … SEC=1` 时会把 BL32 拷成构建目录下的 **`tee.bin`**，
再靠 `fit_nodes.sh` 的 `TEE="${TEE:=tee.bin}"` 捡起来。我们的 `debian/rules` **不用**这条路径
（显式传 `TEE=<路径>`），但 `gen_bl32_node` 里的判断保留了 `[ ! -f tee.bin ]` 这一半，所以那条路还能用。

---

## 5. 上板验证（**不碰 eMMC** 的 SD 卡法）

思路：**loader 从 SD 卡跑（新的、带 BL32），系统仍从 eMMC 起**（把卡上 `boot.scr` 暂时改名，
U-Boot 在卡上找不到启动脚本就会落到 eMMC）。这样 eMMC 一个字节不动，拔卡即恢复。

### 5.1 前置检查

```bash
# 板子上：确认哪个是系统盘、哪个是测试卡
ssh board@<板子> 'findmnt -n -o SOURCE /; lsblk -o NAME,SIZE,TYPE,MOUNTPOINT'
# 期望：系统盘 = /dev/mmcblk0p2（eMMC），测试卡 = /dev/mmcblk1 且 p2 的 LABEL 是 writable_sd
# ⛔ 工程机（<HOST_09>）的 eMMC 不要动（AGENTS §2.13）
```

### 5.2 写卡（只动 32 KB 与 8 MiB 两处，都在 16 MiB 的 bootloader 预留区内）

```bash
# 备份卡上的 loader 区（回滚用）
ssh board@<板子> "echo <口令> | sudo -S -p '' dd if=/dev/mmcblk1 bs=1M count=16 status=none" > sd-boot-16M.bin
sha256sum sd-boot-16M.bin

# 把新产物拷过去，然后写（守卫：系统盘必须是 mmcblk0p2、mmcblk1p2 的 label 必须是 writable_sd）
scp build/u-boot-lubancat-rk3588/debian/build/lubancat-5-v2-rk3588/{idbloader.img,u-boot.itb} board@<板子>:/tmp/
ssh board@<板子> '
  set -e
  [ "$(findmnt -n -o SOURCE /)" = "/dev/mmcblk0p2" ] || { echo "✗ 系统盘不是 mmcblk0p2"; exit 1; }
  [ "$(echo <口令> | sudo -S -p "" blkid -o value -s LABEL /dev/mmcblk1p2)" = "writable_sd" ] || { echo "✗ 不是测试卡"; exit 1; }
  echo <口令> | sudo -S -p "" dd if=/tmp/idbloader.img of=/dev/mmcblk1 seek=64    conv=fsync,notrunc
  echo <口令> | sudo -S -p "" dd if=/tmp/u-boot.itb  of=/dev/mmcblk1 seek=16384 conv=fsync,notrunc
  echo <口令> | sudo -S -p "" sync
'
```

### 5.3 回读校验 + 让系统仍从 eMMC 起

```bash
# 回读 itb（大小按实际产物字节数换算扇区：1,850,880 / 512 = 3615）
ssh board@<板子> "echo <口令> | sudo -S -p '' dd if=/dev/mmcblk1 bs=512 skip=16384 count=3615 status=none | sha256sum"
#   应与本地 sha256sum u-boot.itb 完全一致

# 卡上 p1 的 boot.scr 改名 ⇒ U-Boot 落到 eMMC 的 system-boot
ssh board@<板子> '
  echo <口令> | sudo -S -p "" mount /dev/mmcblk1p1 /mnt/c1
  echo <口令> | sudo -S -p "" mv /mnt/c1/boot.scr /mnt/c1/boot.scr.hold
  echo <口令> | sudo -S -p "" sync; echo <口令> | sudo -S -p "" umount /mnt/c1
'
```

### 5.4 重启与判据

```bash
ssh board@<板子> 'echo <口令> | sudo -S -p "" systemctl reboot'
# 等 ~20 s 回来，然后：
ssh board@<板子> '
  echo "根: $(findmnt -n -o SOURCE /)"              # 应为 /dev/mmcblk0p2（系统来自 eMMC）
  ls -l /dev/tee*                                   # 应出现 /dev/tee0 与 /dev/teepriv0
  dmesg | grep -i optee                             # 应看到 revision / initialized driver
  grep MemTotal /proc/meminfo                       # 比不带 BL32 时少 ~16 MB
  dmesg | grep -i "psci: PSCI"                      # BL31 的 PSCI 仍正常
'
```

**2026-09-18 实测对照**（v2 板，BL31 v1.53）：

| 项 | 默认（BL31-only） | 带 BL32 |
| --- | --- | --- |
| `/dev/tee*` | 不存在 | ✅ `/dev/tee0`、`/dev/teepriv0`（`crw------- root:root`） |
| 内核 optee | `optee: api uid mismatch` → `probe of firmware:optee failed with error -22` | ✅ `optee: revision 3.13 (1e844e4c)`、`dynamic shared memory is enabled`、**`optee: initialized driver`** |
| `MemTotal` | 8,121,060 kB | **8,104,708 kB**（**−16,352 kB**） |
| PSCI / Type-C / USB 挂起 / 温度 / 启动耗时 | — | 全部不变（PSCIv1.1；`[host]`/`[source]`；13 个 `on`；35 ℃；~20 s） |

**串口日志该看什么**（`tools/check-atf.sh` 也能打印 BL31 版本）：

- ✅ **已实测（2026-09-18 串口日志）—— 之前那两条推断全部命中**：

```text
## Checking optee 0x08400000 ... sha256(76eeb0b48d...) + OK    ← 新增的一行；哈希 = BL32 文件自身的 sha256 前缀
...
INFO:    BL31: Initializing BL32                              ← 不再有 "No OPTEE provided…" / "opteed_fast" 报错
I/TC: OP-TEE version: 3.13.0-999-g1e844e4ca #hisping.lin (gcc <PRIVATE_NETWORK>) #2 Fri May  9 00:41:09 UTC 2025 aarch64, fwver: v1.20
I/TC: OP-TEE memory size: TEEOS 0x200000 TA 0xc00000 SHM 0x200000
I/TC: Primary CPU initializing
I/TC: Primary CPU switching to normal world boot
INFO:    BL31: Preparing for EL3 exit to normal world
```

- ⭐ **两个数字自洽**：OP-TEE 自报 `TEEOS 2 MB + TA 12 MB + SHM 2 MB = 16 MB`，与实测的
  `MemTotal` 少 **16,352 kB** 对得上；`## Checking optee` 那行的哈希前 5 字节就是
  `rk3588_bl32_v1.20.bin` 的 `sha256=76eeb0b48d…` 前缀 ⇒ 打进 FIT 的正是这个文件。
- 📄 完整日志已归档到 [boot-serial-log.md](boot-serial-log.md)（带逐行注解）。

**用户态 → 安全世界：探针实测**（`.scratch-boot/tee-probe.c`，板上 `gcc` 编译，只读操作）：

```console
$ sudo ./tee-probe
[1] TEE_IOC_VERSION: impl_id=1 (OPTEE) impl_caps=0x1 gen_caps=0xd GP REG_MEM MEMREF_NULL
[2] TEE_IOC_OPEN_SESSION:
  system PTA (内建)   ✅ session=0 ret=0xffff0001 TEE_ERROR_ACCESS_DENIED    ← PTA **存在**，但拒绝该会话（只允许特权调用方）
  bogus UUID (对照)   ✅ session=0 ret=0xffff0008 TEE_ERROR_ITEM_NOT_FOUND   ← 错误确实来自安全世界
```

⇒ 用户态 → `/dev/tee0` → 内核 optee 驱动 → SMC → BL31 → OP-TEE → 返回错误码，
**整条链是通的**（不是内核敷衍的 EINVAL，而是 TEE 层的返回码）。

⛔ **写这类探针时的两个坑**：
1. 内核 `tee_core.c` 对 `buf_len` 是**严格相等**校验：`sizeof(arg) + num_params*sizeof(param)` 必须**正好等于**
   `buf.buf_len`（否则 `EINVAL`）；而且 `TEE_MAX_ARG_SIZE` 只有 **1024**。
2. **没有 `tee-supplicant` 时 `TEE_IOC_OPEN_SESSION` 会不可中断阻塞**：进程进入 D 状态，
   `SIGTERM`/`SIGKILL`/`timeout` **都杀不掉**（实测同时卡了 4 个进程；起上 supplicant 后它们自己放行退出）。
   ⇒ 探针必须带"先确认 supplicant 在跑"的前置检查，或只用 `TEE_IOC_VERSION`。

- 顺带记住一条**判据差异**：**BL31 的版本只有串口能确认**（`NOTICE: BL31: … fwver: v1.53`；
  `/dev/mem` 读不了 DRAM、`/proc/device-tree/chosen` 里没有 `u-boot,version`），
  而 **BL32 在 Linux 侧就能确认**（`/dev/tee0` + `optee: revision …`）—— 那是 OP-TEE 自己回话的。

---

## 6. 回滚

```bash
# 拔卡：下一次上电就回到 eMMC 上的 loader（最省事）
# 或把备份写回卡（16 MiB 预留区）
ssh board@<板子> "echo <口令> | sudo -S -p '' dd if=<卡备份>.bin of=/dev/mmcblk1 bs=1M count=16 conv=fsync,notrunc"
# 卡上 boot.scr 记得改回来（否则插着卡时卡上那套系统不会被启动）
ssh board@<板子> 'mount /dev/mmcblk1p1 /mnt/c1 && mv /mnt/c1/boot.scr.hold /mnt/c1/boot.scr && sync && umount /mnt/c1'
```

要"永久不带 BL32"就把新编的默认 loader 写回介质；要"永久带"就把它烧进 eMMC
（⚠️ 动 eMMC 前先跟用户确认；备份/偏移与上面一致，换 `of=/dev/mmcblk0`）。

---
