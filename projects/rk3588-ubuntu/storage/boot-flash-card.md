# SD 启动 / 烧录卡：卡里带镜像和工具

> 2026-09-12 实测（1 TB SanDisk，开发板 `/dev/mmcblk1`）。
> **不是**"开机自动烧录"的卡 —— 用户明确不要自动烧录：卡就是一套能启动的系统，
> 里面放好**标准镜像 + 写卡工具**，到目标机器上手动敲一条命令写 eMMC。

---

## TL;DR

- 这张卡**不是"开机自动烧录"卡**（用户明确不要自动烧录）：卡本身就是一套能启动的系统 + 镜像和工具。
- 卡上的系统用 **SD 专用卷标**，避免与 eMMC 的卷标冲突（§1）；最终布局见 §2。
- 做卡的命令（在开发板上实测）见 §3；到目标机器上怎么用见 §4（就是卡里 README 那段）。
- ⚠️ 做卡时抓到并修掉的工具 bug 见 §5；压缩统一走 `tools/compress-image.sh`（只用 zstd / zip，§6）。

## 1. 为什么卡上的系统要用"SD 专用卷标"

U-Boot **优先从 SD 启动**，而镜像里的 `boot.scr` 写死了 `root=LABEL=writable`：

- 如果 SD 和 eMMC 上都叫 `writable` / `system-boot`，会出现
  **「用 SD 的启动分区启动、根文件系统却是 eMMC 的」混合启动**；
- `x-systemd.growfs` 也可能作用到**另一块**介质上。

所以 `tools/make-sd-image.sh` 产出的是 **SD 变体**：卷标改成 `writable-sd` / `boot-sd`
（FAT 卷标上限 11 字符，所以启动分区不能叫 `system-boot-sd`），
并同步改写镜像里的 `fstab`、`boot.cmd`，再用 `mkimage` 重建 `boot.scr`，
最后**自检**镜像里不再残留旧标签（精确匹配标签结尾，避免 `writable` 误命中 `writable-sd`）。

**eMMC 那边照旧烧标准镜像**（`system-boot` / `writable`）—— 卡与 eMMC 从标签层面永不冲突。

---

## 2. 卡上布局

```
/dev/mmcblk1                    原生 SD 槽；USB 读卡器通常为 /dev/sdX
├─p1        ext4  boot-sd       SD 启动文件
└─p2        ext4  writable-sd   SD 根文件系统，容量以实际卡为准
     └─ /opt/lsp-flash/
          README.txt                                  仓库 tools/flash-card-README.txt
          IMAGE-INFO.txt                              发布镜像的版本、来源与校验值
          TOOLS.sha256                                工具校验清单，路径相对 payload 根目录
          images/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img         标准镜像，写 eMMC 用
          images/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img.sha256
          images/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img.rawsize
          tools/write-sd.sh                          整盘写入（首次烧录 / 分区表不同）
          tools/update-emmc.sh                       增量同步（只改了 rootfs 时）
          tools/verify-image.sh                      体检（启动后确认定制项都在）
```

README 使用仓库模板，按“核对当前系统盘 → 校验镜像和工具 → 显式选择镜像与目标盘 → 写入并读回校验”执行。
SD 启动系统与 payload 独立：更新 `/opt/lsp-flash` 不会更新卡自身的内核或 rootfs。
`verify-image.sh` 默认检查当前运行系统；从旧 SD 启动执行不能证明新 payload 已运行。

`/opt/lsp-flash/` 刻意做成**和构建仓库一样的 `images/` + `tools/` 布局**：
两个写盘脚本可以探测该目录；现场烧录显式传 `-i`，避免选到其它版本。
镜像 SHA256 清单只记录 basename，从 `images/` 内校验；工具清单从 payload 根目录校验。

卡里那份镜像是**标准镜像**（不是 SD 变体）—— 实测读回它的分区卷标是
`system-boot` / `writable`，写进 eMMC 后就是正常的生产系统。

---

## 3. 怎么做这张卡（在开发板上，实测命令）

```bash
# 1) 生成 SD 变体 —— **只出裸镜像**（压缩已解耦，见 §6）
sudo ./tools/make-sd-image.sh -i images/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img \
     -o images/ubuntu-24.04-desktop-arm64-lubancat-5-v2-sd.img --force
#    产出：-sd.img（4.88 GiB，直接用来写卡）；默认**不算** sha256，发布时加 --sha256
#    想额外留一份压缩存档：./tools/compress-image.sh images/...-sd.img --zstd（或 --zip），见 §6

# 2) 写卡（整盘；脚本会拒绝系统盘，所以不会误写 eMMC）
sudo ./tools/write-sd.sh -i images/ubuntu-24.04-desktop-arm64-lubancat-5-v2-sd.img -d /dev/mmcblk1 -y
#    实测：4.88 GiB / 67 s / 78 MB/s

# 3) 扩到整卡（不扩的话根分区只有 4.4G，放不下 4.88 GiB 的镜像）
sudo partprobe /dev/mmcblk1
sudo growpart /dev/mmcblk1 2                  # 4.4G → 953.2G，end=2000011230（末尾留 33 扇区给备份 GPT）
sudo partprobe /dev/mmcblk1
sudo e2fsck -f -y /dev/mmcblk1p2
sudo resize2fs /dev/mmcblk1p2

# 4) 放镜像与工具（布局见 §2；更新已有卡的 payload 时从这里开始）
# 确认 build/rootfs 是此镜像的生成源，且镜像已通过只读离线检查；版本记录不取自旧 SD 系统。
(
set -e
image_name=ubuntu-24.04-desktop-arm64-lubancat-5-v2.img
(cd images && sha256sum -c "$image_name.sha256")
test "$(cat "images/$image_name.rawsize")" = "$(stat -c %s "images/$image_name")"
sudo mount /dev/mmcblk1p2 /mnt/sdcard
sudo install -d -m 0755 /mnt/sdcard/opt/lsp-flash/{images,tools}
sudo rsync -aS --info=progress2 "images/$image_name" /mnt/sdcard/opt/lsp-flash/images/
sudo install -o root -g root -m 0644 "images/$image_name.sha256" "images/$image_name.rawsize" /mnt/sdcard/opt/lsp-flash/images/
sudo chmod 0644 "/mnt/sdcard/opt/lsp-flash/images/$image_name"
sudo chown root:root "/mnt/sdcard/opt/lsp-flash/images/$image_name"
sudo install -o root -g root -m 0755 tools/write-sd.sh tools/update-emmc.sh tools/verify-image.sh /mnt/sdcard/opt/lsp-flash/tools/
sudo install -o root -g root -m 0644 tools/flash-card-README.txt /mnt/sdcard/opt/lsp-flash/README.txt
image_info=$(mktemp)
{
    printf 'Image: %s\nSize: %s bytes\n' "$image_name" "$(stat -c %s "images/$image_name")"
    cat "images/$image_name.sha256" build/rootfs/usr/lib/os-release build/rootfs/etc/lsp-image-release
    dpkg-query --admindir=build/rootfs/var/lib/dpkg -W -f='${Package} ${Version}\n' linux-image-6.1.172 linux-headers-6.1.172 cage
    cat build/rootfs/opt/oem-host/SOURCE
} > "$image_info"
sudo install -o root -g root -m 0644 "$image_info" /mnt/sdcard/opt/lsp-flash/IMAGE-INFO.txt
rm -- "$image_info"
sudo bash -c 'cd /mnt/sdcard/opt/lsp-flash && sha256sum tools/write-sd.sh tools/update-emmc.sh tools/verify-image.sh > TOOLS.sha256 && chown root:root TOOLS.sha256 && chmod 0644 TOOLS.sha256'
(cd /mnt/sdcard/opt/lsp-flash && sha256sum -c TOOLS.sha256)
(cd /mnt/sdcard/opt/lsp-flash/images && sha256sum -c "$image_name.sha256")
sudo sync && sudo umount /mnt/sdcard
)
```

## 4. 到目标机器上怎么用（卡里的 README 就是这段）

**第一用途是启动测试**：插卡上电 = 从卡启动，目标机 eMMC 不会被挂载、不会被改动，
测完关机拔卡就回到原系统。开机后：

```bash
cd /opt/lsp-flash
sudo ./tools/verify-image.sh                                  # 期望：失败 0，结论"全部通过"
ls -l /dev/chest_cam_video /dev/camera_left /dev/camera_right \
      /dev/lcd_voice_bridge /dev/glove_left /dev/glove_right
sudo lsp-config status                                        # Type-C 角色 / WiFi / swap / 存储
```

**要刷 eMMC 时**（⚠️ 会清空目标盘）：

```bash
cd /opt/lsp-flash
findmnt -n -o SOURCE /                                      # 确认当前根分区在 SD
lsblk -o NAME,SIZE,TYPE,LABEL,MOUNTPOINTS
sha256sum -c TOOLS.sha256
(cd images && sha256sum -c ubuntu-24.04-desktop-arm64-lubancat-5-v2.img.sha256)
sudo ./tools/write-sd.sh -i images/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img -d /dev/mmcblk0 --verify
sudo poweroff                                                  # 关机 → 拔卡 → 再上电
# 只同步差异（eMMC 已烧过同一镜像）：sudo ./tools/update-emmc.sh -d /dev/mmcblk0
```

⚠️ `update-emmc.sh` 在挂载/写入之前会核对**镜像与目标两个分区的文件系统类型**：boot 分区从
2026-09-26 起由 vfat 换成 ext4，目标若是老镜像烧的 vfat 分区（分区起点一样、过得了布局校验），
它会直接拒绝并让你改用 `write-sd.sh` 整盘写 —— vfat 存不下属主/权限/xattr，`rsync -aHAX`
会写出半套内容。实测（以开发板上的 SD 卡为目标，卡 p1 是 vfat）：
`p1: 镜像 ext4 vs 目标 vfat` → `RC=1`，卡上 UUID 与挂载状态都没变。

设备名：`/dev/mmcblk1` = 这张 SD（当前系统盘），`/dev/mmcblk0` = 板载 eMMC。
`write-sd.sh` 自带"系统盘拒写"护栏，**两个方向上都是对的**（实测 dry-run）：

| 从哪启动 | 系统盘 | 对 `-d /dev/mmcblk0` 的结果 |
| --- | --- | --- |
| 从这张 SD 卡启动（目标机器） | `mmcblk1` | 允许写 ✓ |
| 从 eMMC 启动（开发板） | `mmcblk0` | `Error: /dev/mmcblk0 是系统盘（根分区/swap 所在），拒绝写入！` ✓ |

⚠️ **写完 eMMC 必须拔卡再上电**：U-Boot 优先从 SD 启动，不拔卡还是从卡起来的。

---

## 5. 做这张卡时抓到并修掉的工具 bug

**① `mv` 自己搬自己（原来必现）**：最后一步是无条件

```bash
xz -3 -T0 -f -k "${RAW_IMG}"      # 就地产生 ${RAW_IMG}.xz
mv -f "${RAW_IMG}.xz" "${OUTPUT}"
```

而 `RAW_IMG = ${OUTPUT%.xz}` —— `RAW_IMG.xz` **永远等于** `OUTPUT`，于是 `mv` 报
`are the same file`，`set -eE` + ERR 陷阱直接**退出码 1**；更隐蔽的是后面的
`sha256sum > ${OUTPUT}.sha256` **根本不会执行**（产出镜像没有校验文件）。

**② 中间文件名写死剥 `.xz`（加新压缩方法时踩到）**：`RAW_IMG="${OUTPUT%.xz}"` 一旦换成
zstd/zip，中间文件就与输出**同名** → 压缩器变成"自己压自己"：zstd **报错退出 1**，
zip 更糟（输出被写坏）。当年按 `${COMP_EXT}` 剥修好了；**2026-09-15 压缩整段搬去
`tools/compress-image.sh`**（那边就没有"输出与中间同名"这回事了），本脚本不再有这套逻辑。

**③ 无条件压缩**：原来不管你要不要都 `xz`，于是 `images/` 里会冒出 `.img.xz`，
与"镜像默认不压缩"的约定冲突 → 先改成显式开关，**2026-09-15 起干脆整个移除**：
`make-sd-image.sh` 只出裸 `.img`，压缩统一走 `tools/compress-image.sh`（见 §6）。

> 上面这些踩坑当年用**假镜像 harness** 逐项验过，下表是**当时**的判据。压缩那几行现在已经不属于
> 本脚本（遇到 `--zstd`/`--zip`/`--lz4` 一律报错并指向新工具；压缩用例见
> `tests/compress-image-tool.sh`）：

| 调用 | 结果 |
| --- | --- |
| 默认（不带压缩开关） | `退出码=0`，只有 `<名>-sd.img`，**没有**压缩文件（`--sha256` 才多一个 `.sha256`）|
| `--zstd` / `--zstd=1` / `--compress=zstd:5` | `退出码=0`，`.zst` + `.rawsize`（+ `--sha256`），往返一致 |
| `--zip` / `--zip=0` / `--compress=zip:9` | `退出码=0`，`.zip` + `.rawsize`（+ `--sha256`），往返一致 |
| `-o /tmp/mycard --zstd=1 --keep-work` | 自动补扩展名 → `mycard.zst`；裸的 `mycard` 也留着 |
| `--lz4` / `--gzip` / `--xz` | `退出码=1`，提示"已不再提供…请用 zstd 或 --zip" |
| 输入已经是 `-sd` 变体 | `退出码=1`，明确拒绝二次转换 |
| 产出镜像内部 | 标签仍是 `boot-sd` / `writable-sd`（自检没被破坏） |

> 本次那张卡是在 bug ① 修复前做的 —— 镜像本身没问题（标签/fstab/boot.scr 的自检都在报错点之前
> 跑完了），只是脚本以非 0 退出、少个 `.xz.sha256`（后来单独补上了）。

## 6. 压缩：统一走 `tools/compress-image.sh`（只用 zstd 与 zip）

> 校验和策略（2026-09-15）：**默认不算 `.sha256`**（构建/压缩/做 SD 变体都是），发布时分发件再加
> `--sha256`；`.rawsize` 始终生成（写卡工具要用）。实测 sha256 在板子上 1.16 GB/s
> （ARMv8 SHA 扩展），几 GB 要 5~15 s —— 日常反复重建不值得，详见 AGENTS.md §6。

> 2026-09-15 起**压缩从两个做镜像的工具里摘掉**（`make-sd-image.sh` 与 `scripts/build-lsp-image.sh`
> 都只出裸 `.img`），统一由 `tools/compress-image.sh` 负责 —— 之前压缩代码两边各一份，
> 改级别要改两处。回归测试：`tests/compress-image-tool.sh`。

源镜像 4.88 GiB / 8 核。**保留两个**：

| 方法 | 命令 | 压缩后 | 压缩比 | 压缩耗时 | 解压耗时 | 用途 |
| --- | --- | --- | --- | --- | --- | --- |
| **zstd -3（默认）** | `zstd -T0 -3` | 1.10 GiB | 4.42:1 | 15.9 s | **8.1 s** | **推荐**：体积最小、解压最快 |
| `zstd -1`（`--zstd=1`） | `zstd -T0 -1` | 1.21 GiB | 4.02:1 | **10.1 s** | 8.0 s | 只要快（比 pigz -1 又快又小） |
| **zip -1（`--zip`）** | `zip -1 -j` | 1.29 GiB | 3.80:1 | 79.0 s | 39.9 s | **给 Windows / RKDevTool** |
| `zip -0`（`--zip=0`） | `zip -0 -j` | 4.88 GiB | 1.00:1 | 31.7 s | 42.6 s | 只想打成 Windows 能认的包 |

**已删掉的三种**（同样在这块板子上测的）：

| 方法 | 实测 | 为什么删 |
| --- | --- | --- |
| lz4 -1 | 1.61 GiB / 14.5 s（解压 4.7 s） | 体积大 **46%**，压缩也不比 zstd -1 快 |
| gzip -1（单线程） | 1.29 GiB / 78.0 s | 太慢（≈ zip -1） |
| pigz -1（gzip 并行） | 1.28 GiB / 15.1 s | **zstd -1 更快而且更小** |
| xz -T0 -3 | 0.89 GiB / 约 2 分钟 | 慢 4 倍，只小 20% |

> ⚠️ 数字是同一轮连续测的（源镜像已在页缓存里），但板子只有**一块 eMMC**、压缩产物也写在上面，
> 同一方法不同轮次能差 ±30%（实测 zstd -3 出现过 15.9 s 和 26.9 s）。**看排序，别抠绝对值。**

用法（不给方法时**默认 zstd -3**；`--compress[=zstd|zip[:级别]]` 与短写等价）：

```bash
./tools/compress-image.sh images/xxx.img              # 默认 zstd -3（推荐）
./tools/compress-image.sh images/xxx.img --zstd=1     # 最快的一档
./tools/compress-image.sh images/xxx.img --zip        # Windows / RKDevTool 用
./tools/compress-image.sh images/xxx.img --zip=0      # 只打包不压缩
./tools/compress-image.sh images/xxx.img --lz4        # ✗ 报错：已不再提供（并告诉你用 zstd/zip）
./tools/compress-image.sh                            # 不给镜像：自动挑 images/ 下最新的裸 .img
```

产物旁边**始终**生成 **`.rawsize`**（解压后字节数，给 `write-sd.sh`/`update-emmc.sh` 做容量校验与进度用；
zip/lz4 没有便捷的大小查询接口）；`.sha256` **默认不算**（2026-09-15 用户要求 —— 几 GB 的校验和
留给发布/分发时算），要就加 `--sha256`。**重压一次会顺手删掉过期的旧 `.sha256`**，避免拿旧哈希
对着新包"以为没问题"。这两个脚本现在也都能直接读 `.zip`（`unzip -p`）。

### 6.1 给 Windows / RKDevTool 的 `.zip`

RKDevTool 是 **Windows** 下的烧录工具，直接吃 `.img`。所以除了裸镜像，我们也出一份 zip：

```bash
# 标准镜像（写 eMMC 的那份）
./tools/compress-image.sh images/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img --zip
```

实测：`images/ubuntu-...-v2.img.zip` = **1.29 GiB**，压缩 79 s，解压出来与原始镜像 **sha256 一致**。

- ⚠️ **>4 GiB 的 zip 必然是 Zip64**（`zip` 3.0 自动处理）。Windows 10/11 资源管理器一般能解，
  老系统/精简版不认就用 **7-Zip / WinRAR**。**本工程没有 Windows 环境，解压这一侧没实测** ——
  第一次用请自己对一下 sha256。
- 只想快速做个 Windows 能认的容器、不在乎体积：`--zip=0`（31.7 s / 4.88 GiB，仅打包）。
- zip 自己没有百分比输出，工具会**盯产物大小打印百分比**（分钟级步骤不许静默）。

## 7. 其它注意

- ⚠️ **要看镜像里的文件，一律只读挂载**（`losetup --read-only` + `mount -o ro`）：
  镜像文件是**一个活的 ext4**，rw 挂载会因 atime/日志回写而**改动镜像内容** ⇒ 镜像自带的
  `.sha256` 立刻对不上（2026-09-15 实测：rw 挂载核对一轮后 `sha256sum -c` 报 `FAILED`，
  而 `cmp` 源文件与拷贝**逐字节相同** —— 唯一的差异就是那次 rw 挂载写进去的元数据）。
  要挂 rw 就先复制一份再挂。
- ⚠️ **`-sd.img` 的 sha256 不能跨轮比较**：`make-sd-image.sh` 会用 `mkimage` 重建 `boot.scr`，
  而 FIT 头里带 `Created: <时间>` ⇒ 同一份源镜像跑两次，产物字节不同
  （2026-09-15 实测：同一输入两次得到 `b785c379…` / `4fbee1f1…`）。**判据看内容**
  （卷标 `boot-sd`/`writable-sd`、fstab 引用 `LABEL=writable-sd`、`boot.scr` 存在），不要比哈希。
- FAT 卷标用小写 `boot-sd` 时 `fatlabel` 会警告 `lowercase labels might not work properly on some systems`；
  镜像本来就用小写（`system-boot`），`blkid` 读回一致、`mount LABEL=` 与 U-Boot 都正常 —— **忽略这个警告**。
- 卡上系统首启会跑 growroot / `x-systemd.growfs`：因为已经离线扩到整卡，它们无事可做（也不会破坏什么）。
- 做这张卡会**清空卡上原有数据**（本次抹掉的是 1 TB 数据卡 `Data1T`；开发板 `/etc/fstab` 里那行
  `/mnt/data_1T` 已同步删除，否则每次开机会等一个不存在的设备）。
- 卡上系统与普通镜像一样：用户名 `board`、管理地址走 WiFi（`example-project`，网卡是 `p2p0`）。
