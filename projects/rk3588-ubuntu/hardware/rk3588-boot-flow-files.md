# 启动链路的文件映射、镜像组装与验证速查

> 本文是 [rk3588-boot-flow.md](rk3588-boot-flow.md) 的**配套速查**：那条链路上「哪个文件是谁生成的」
> （含板上实测）、关键文件对照表、本仓库镜像的六个具名阶段组装、验证命令、以及实测记录。
> 链路本身（BootROM → idbloader → u-boot.itb → U-Boot → 内核）见主篇 §1~§3。
> ⚠️ 正文**原样搬自主篇的 §3.6 / §4 / §5 / §6 / §7**（只改了标题层级；节号保留在标题里便于对照）。

## TL;DR

- **`/boot/firmware` 里那些文件各由谁生成**：内核 deb 的 postinst 钩子、`boot.scr`/`boot.cmd`、
  `ubuntuEnv.txt`、dtbs/dtbo 的来源都在 §3.6 的表里（板上实测）。
- 镜像组装使用**六个具名阶段**（§5）；验证用 §6 的命令；历史实测数据在 §7。


### 3.6 根文件系统 `/boot` 里的文件是谁生成的（板上实测）

注意 `/boot`（根分区 p2 上的**普通目录**）与 `/boot/firmware`（**启动分区 p1**）是两回事，来源也不同：

| 文件 | 位置 | 生成者 |
| --- | --- | --- |
| `vmlinuz-6.1.172` | `/boot` | **内核 deb 自带**（`linux-image-6.1.172`，即 `make bindeb-pkg` 产物里的 `/boot/*`；`dpkg -S` 可验证）。2026-10-03 板上实测 **42,383,872 B** |
| `System.map-6.1.172` | `/boot` | 同上（板上实测 **7,232,399 B**） |
| `config-6.1.172` | `/boot` | 同上（板上实测 **250,522 B**） |
| `initrd.img-6.1.172` | `/boot` | `update-initramfs`（initramfs-tools）：装内核时由 deb 的 `postinst` → `run-parts /etc/kernel/postinst.d` → `initramfs-tools` 钩子执行 `update-initramfs -c -k 6.1.172 -b /boot`；手动 `sudo update-initramfs -u` 只是**重新生成同一个文件** |
| `firmware/` | `/boot` | **挂载点**（`fstab`：`LABEL=system-boot /boot/firmware vfat defaults`）；目录本身由构建脚本 `mkdir -p`（`scripts/build-lsp-image.sh:2747`） |
| `vmlinuz`、`initrd.img` | `/boot/firmware` | `zz-update-firmware`（构建期由 `build-lsp-image.sh` 里 `cp … /boot/firmware/vmlinuz` 那两行直接 `cp`） |
| `dtbs/`（**7 个基础 dtb + 264 个 overlay dtbo**，2026-10-03 板上实测） | `/boot/firmware` | 同上（内核 deb 的 `/usr/lib/linux-image-6.1.172/.` + 仓库自定义 dtbo） |
| `boot.scr` / `boot.cmd` / `ubuntuEnv.txt` | `/boot/firmware` | `image` 阶段生成（见主篇 [rk3588-boot-flow.md](rk3588-boot-flow.md) §3.4 与本文 §4；机制细节见 [u-boot-boot-scr-uenv.md](u-boot-boot-scr-uenv.md)） |

> ⚠️ 表里的版本号与字节数是 **6.1.172（2026-10-03 板上实测）**；本文早先版本记的是 `6.1.118` 那版
> （`System.map` 7,191,246 B / `config` 248,949 B / overlay 257 个）—— **机制相同，数字随每次内核包变**。


板上实测（⚠️ 下面这段抄录**当时是 6.1.118 那版镜像**，2026-09-16；文件名的版本号换成 `6.1.172` 即可，机制与归属完全一样，今天的实际字节数见上表）：

```bash
$ dpkg -S /boot/vmlinuz-6.1.118 /boot/System.map-6.1.118 /boot/config-6.1.118
linux-image-6.1.118: /boot/vmlinuz-6.1.118
linux-image-6.1.118: /boot/System.map-6.1.118
linux-image-6.1.118: /boot/config-6.1.118
$ ls -l --time-style=long-iso /boot/
-rw-r--r-- 1 root root   248949 2026-09-12 11:53 config-6.1.118
drwxr-xr-x 3 root root     4096 1970-01-01 08:00 firmware          # FAT 分区，mtime 无意义
-rw-r--r-- 1 root root 18933145 2026-09-16 15:11 initrd.img-6.1.118
-rw-r--r-- 1 root root  7191246 2026-09-12 11:53 System.map-6.1.118
-rw-r--r-- 1 root root 42109440 2026-09-12 11:53 vmlinuz-6.1.118
```

时间戳能对上：三个 deb 自带的文件是 `dpkg -i` 解包时间（构建期镜像里的 mtime 被保留），
只有 `initrd.img-6.1.118` 是 `update-initramfs` 那次（`/boot/firmware/{vmlinuz,initrd.img}` 也被钩子
重新 `cp` 成同一时间）。

⚠️ **`/boot/vmlinuz`、`/boot/initrd.img` 这类"默认内核"符号链接在本镜像里不存在**
（`/vmlinuz`、`/initrd.img` 也没有），**这是正常的、不影响启动**：U-Boot 按**固定名字**
`/boot/firmware/vmlinuz`、`/boot/firmware/initrd.img` 加载（`boot.cmd:35-36`），不走符号链接。
（`/etc/kernel-img.conf` 也不存在。）原因两边互相甩锅，都查过了：

1. 我们的内核 deb 由内核自带的 `scripts/package/builddeb` 生成，`postinst` 只有一句
   `run-parts --arg=… /etc/kernel/postinst.d`，**没有**官方 `linux-image-*` 那句直接调用
   `linux-update-symlinks` 的逻辑；
2. Ubuntu 用来补位的钩子 `/etc/kernel/postinst.d/xx-update-initrd-links`（`linux-base`）
   第 7 行是 `[ -z "$DEB_MAINT_PARAMS" ] || exit 0`（注释："linux-image-* 自己会调，别重复做"），
   而我们的 `postinst` 第 6 行恰好 `export DEB_MAINT_PARAMS="$*"` ⇒ 钩子直接退出。

要恢复这套链接：`sudo linux-update-symlinks install <版本> /boot/vmlinuz-<版本>`
（`<版本>` 用 `uname -r` / `ls /boot/vmlinuz-*` 现场取；⚠️ 不要照抄文档里的旧版本号）。


## 4. 关键文件对照表

| 角色 | 源码 / 规则 | 产物 |
| --- | --- | --- |
| DDR 固件 | `packages/u-boot-lubancat-rk3588/debian/rkbin/rk3588_ddr_lp4_1848MHz_lp5_2400MHz_v1.18.bin` | 拼入 `idbloader.img` |
| ATF BL31 | `.../debian/rkbin/rk3588_bl31_v1.53.elf` | 经 `decode_bl31.py` 拆段后进 `u-boot.itb` |
| 板卡→固件映射 | `.../debian/targets.mk`（`<platform>_ddr` / `_bl31` / `_pkg`） | — |
| 打包规则 | `.../debian/rules` | `u-boot-<board>_*.deb` |
| FIT 生成 | `u-boot/arch/arm/mach-rockchip/{make_fit_atf.sh,fit_nodes.sh,fit_args.sh}` | `u-boot.its` → `u-boot.itb` |
| BL31 拆段 | `u-boot/arch/arm/mach-rockchip/decode_bl31.py` | `bl31_0x*.bin` |
| 板卡 defconfig | `u-boot/configs/<board>-rk3588_defconfig` | — |
| 启动脚本模板 | `image` 阶段内联生成 `boot.cmd` | `boot.scr` |
| 环境变量模板 | `config/uEnv/<board>.uEnv` | `ubuntuEnv.txt` |

各板卡的 DTB 名（`CONFIG_DEFAULT_DEVICE_TREE`）：`lubancat-5` / `lubancat-5-v2` 为
`rk3588-lubancat-5`，`lubancat-5io` 为 `rk3588-lubancat-5io`。


## 5. 本仓库镜像组装流程

`scripts/build-lsp-image.sh` 使用六个具名阶段；参数与重跑方法见 [构建详解](../build/build-guide.md)。

| 阶段 | 内容 | 完成依据 |
| --- | --- | --- |
| apps | 宿主编译全部工程应用 | `build/apps/.build-state` |
| base | 下载、解压 ubuntu-base | `build/rootfs/etc/os-release` |
| rootfs | 基础包、桌面、overlay、安装预编译应用 | `build/.stage-rootfs.done` |
| oem-host | 获取源码、安装依赖与配置、预编译字节码 | `build/.stage-oem-host.done` |
| board | 安装板卡内核 / U-Boot deb、执行板卡钩子 | `build/.stage-board.done` |
| image | 生成裸 `.img` 并完成 ATF 检查或补写 | `build/.stage-image-<rootfs名>-<板型>.done` |

`image` 阶段内写入 bootloader 的两个分支：

```bash
# 分支一：合并镜像
dd if=.../u-boot-rockchip.bin of=<loop> seek=1 bs=32k conv=fsync
# 分支二：分离镜像（本仓库常规路径）
dd if=.../u-boot/idbloader.img of=<loop> seek=64    conv=notrunc
dd if=.../u-boot/u-boot.itb    of=<loop> seek=16384 conv=notrunc
```

`image` 阶段的作用与判定逻辑见 [atf-missing-troubleshooting.md](atf-missing-troubleshooting.md)。


## 6. 验证命令速查

```bash
# --- 1. idbloader：魔数与 DDR 固件版本 ---
xxd -l 16 prebuilt/idbloader.img                    # 期望开头 "RKNS"
strings prebuilt/idbloader.img | grep -i "DDR .*fwver"

# --- 2. u-boot.itb：是否为 FIT、是否含 ATF ---
xxd -l 4 prebuilt/u-boot.itb                        # 期望 d00dfeed（FDT/FIT 魔数）
strings prebuilt/u-boot.itb | grep -c "arm-trusted-firmware"   # >0 表示含 ATF

# --- 3. 镜像内 bootloader 是否含 ATF（`image` 阶段的判定方式）---
dd if=<img> bs=1M skip=8 count=4 2>/dev/null | grep -qa "arm-trusted-firmware" && echo "含 ATF"
dd if=<img> bs=1M count=4        2>/dev/null | grep -qa "arm-trusted-firmware" && echo "含 ATF（合并镜像布局）"

# --- 4. 解析 FIT 结构（列出节点 / load 地址 / 数据偏移）---
#     见 atf-missing-troubleshooting.md 附录的 fit-dump 脚本

# --- 5. 校验 dd 写入位置（魔数 + 逐字节比对）---
xxd -s $((64*512))    -l 8 -p <img>                 # 期望 524b4e5300000000 (RKNS)
xxd -s $((16384*512)) -l 8 -p <img>                 # 期望 d00dfeed...
dd if=<img> of=/tmp/itb bs=1 skip=$((16384*512)) count=$(stat -c%s prebuilt/u-boot.itb)
cmp /tmp/itb prebuilt/u-boot.itb && echo "写入逐字节一致"
```


## 7. 实测记录（供复核）

- ⚠️ **本节测的是升级前那份 `prebuilt/`（BL31 v1.47）**：下面的哈希/尺寸与 v1.47 绑定，
  **2026-09-20 已把 `prebuilt/` 重建为 v1.53**（详见 `AGENTS.md` 与 `bl31-mechanism.md` §1 表格）
  ⇒ 本节这些数字**不再对应当前文件**；要复现 v1.47 那份请用 `git show <2026-09-20 之前的提交>:prebuilt/u-boot.itb`。
- 环境：Ubuntu 24.04（无 `python2`），`prebuilt/u-boot.itb` 与 `prebuilt/idbloader.img` 由本仓库
  `build/u-boot-lubancat-rk3588/` 构建产出
- `prebuilt/u-boot.itb`：1,441,280 字节，`sha256=e28e170eb83695f5f5b83392b8f0b9e0caefe039859590fa53d7743b4b8ff834`
- `prebuilt/idbloader.img`：305,152 字节，`sha256=b734d0e3352d02a4358e823d104f12ca3de6643d992cc1c63a1e36329a146ee2`
- ATF 三段尺寸与直接用 `rk3588_bl31_v1.47.elf`（**当时那份**）解码出的 `bl31_0x*.bin` **完全一致**，
  证明该 u-boot.itb 确实嵌入了本仓库的 BL31：

  | FIT 节点 | load | 尺寸 | 对应解码产物 |
  | --- | --- | --- | --- |
  | `atf-1` | `0x40000` | 204,860 | `bl31_0x00040000.bin` = 204,860 ✓ |
  | `atf-2` | `0xff100000` | 36,864 | `bl31_0xff100000.bin` = 36,864 ✓ |
  | `atf-3` | `0xf0000` | 24,576 | `bl31_0x000f0000.bin` = 24,576 ✓ |