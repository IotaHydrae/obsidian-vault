# rkdeveloptool：Linux 下通过 USB 烧录 Rockchip 裸镜像

> `rkdeveloptool` 可在 Linux 主机上完成 Windows RKDevTool 常用的设备识别、下载 Loader、
> 读写存储与复位操作。本工程完整 `.img` 应通过兼容的 USB 下载 Loader，从目标介质 LBA0 写入。

## TL;DR

- 工具名是 **`rkdeveloptool`**；`upgrade_tool` 是另一款 Rockchip 官方 Linux 工具。
- 流程：板子进入 MaskROM → 将兼容 MiniLoader 下载到 RAM → 确认目标介质 → 写完整镜像 → 读回比较 → 复位。
- Ubuntu 包常见的是 Pine64 分支，`read` 长度按**字节**；Rockchip 原版 `rl` 按 **512 B 扇区**，不可混用长度参数。
- 以下已核对主机软件、源码和原理图；**本工程尚未用此流程做 USB 烧录板测**。

## 1. 三个工具与两种镜像

| 工具 | 平台 / 定位 | 使用时的区别 |
| --- | --- | --- |
| RKDevTool | Windows 图形工具 | 常用“下载镜像”“升级固件”等界面 |
| `upgrade_tool` | Rockchip 官方 Linux 命令行工具，常随完整 BSP SDK 附带 | SDK 通常放在 `tools/linux/Linux_Upgrade_Tool/`；本工程当前未附带该目录 |
| `rkdeveloptool` | 开源 RockUSB 命令行工具，Linux 下可替代上述常用烧录操作 | 官方称其为 `upgrade_tool` 的开源实现；支持范围和命令语义须按版本核对 |

“能替代”指工作用途，不代表完整复制 RKDevTool 的 GUI、固件打包和全部厂商功能。
尤其 `upgrade_tool ul` 与 `rkdeveloptool ul` 的流程不同，不能按相同缩写互套命令。

| 输入文件 | 本工程如何处理 |
| --- | --- |
| `images/ubuntu-24.04-desktop-arm64-<board>.img` | 完整 GPT 裸磁盘镜像，含启动固件、boot/rootfs 两个 ext4 分区；写目标整盘 LBA0 |
| Rockchip `update.img` | 固件容器，内部含分区镜像与元数据；常用 `upgrade_tool uf` 处理，不能当裸盘执行 `write 0` |
| `.img.zst` / `.img.xz` 等 | 先解压，本文 `write` / `wl` 使用解压后的裸 `.img` |
| `rk3588_MiniLoaderAll.bin` | 厂商 USB 下载 Loader，必须匹配 RK3588、DDR 与板卡；本仓库不提供 |
| `prebuilt/idbloader.img`、`u-boot.itb` | 介质启动固件，已由构建流程放入完整镜像；不能直接代替 USB 下载 MiniLoader |

本工程包里的 `rkspi_loader.img` 实际是含 GPT 的 16 MiB 启动镜像，也不是 USB MiniLoader。
镜像布局与写卡方法见 [烧录与维护](flashing.md)，启动文件作用见 [启动链路](../hardware/rk3588-boot-flow.md)。

## 2. MaskROM、Loader 与连接口

工具运行在 PC 等 Linux **主机**；板子通过 USB 的 RockUSB 协议接受命令，不经 SSH，
也不需要在主机指定 `/dev/mmcblk0`。目标是板子上 Loader 暴露的存储介质。

```mermaid
flowchart LR
    A[板子进入 BootROM MaskROM] --> B[PC 经 USB 下载兼容 MiniLoader]
    B --> C[RAM 中初始化 DDR 和存储访问]
    C --> D[确认介质并从 LBA0 写裸镜像]
    D --> E[读回比较]
    E --> F[复位并从介质启动]
    F --> G[Linux 使用本工程 fixed host/source 策略]
```

| 板卡 | 原理图确认的 USB 下载口 | MaskROM 按键依据 |
| --- | --- | --- |
| LubanCat-5 v2 | J15 Type-C，TYPEC0 的 USB2 D+/D− | SW1 接 `BOOT_SARADC_IN0`，0 V 档为 USB / MaskROM |
| LubanCat-5io | 底板 J8 Type-C，跨 BTB 连接核心板 TYPEC0 USB2 | 底板 SW1 标为 MASKROM；SW2 为 RECOVERY，两者不等同 |

按住、上电、释放的具体时序以对应板卡厂商手册为准；这里尚未板测确认。
使用有数据通道的线并按板卡规定供电，先用 `list` 确认确实进入 MaskROM。
Linux 的 [Type-C 固定 host/source](../usb/typec-rollout-and-decision.md) 属于后续阶段，
不代表 BootROM 没有下载能力；运行 Linux 时直接接 PC 也不会自动变成下载设备。
当前配置与具体 PC / 线材在冷启动下载阶段的供电、CC 和枚举组合仍待验证。

**本文整盘流程限定为 MaskROM → 兼容 USB 下载 Loader。** 已有的任意 Loader 模式
不能直接视为物理 LBA 访问：官方文档记录过旧 miniloader 的地址映射差异。
须核对 Loader 暴露的介质与地址语义，不把旧平台的偏移常数套到 RK3588。

## 3. 安装与命令速查

在 Linux 主机安装，不需要把工具加入目标镜像：

```bash
sudo apt install rkdeveloptool
command -v rkdeveloptool
dpkg-query -W -f='${Version}\n' rkdeveloptool
rkdeveloptool --help
man rkdeveloptool
```

2026-10-22 本机 Ubuntu 包为 `1.32+pine64git20210920.cce7d2a-2`，属于 Pine64 分支。
其 `--version` 仍显示 `1.0.0`，因此应结合包版本、help 和对应源码判断接口。
该包 `-2` 已补上超过 4 GiB 读回长度和输出文件写入错误检查；工程镜像可能超过 4 GiB，
不要随意换成缺少这些修复的旧版。所核对 Rockchip 原版提交的 raw 写路径同样支持大于 4 GiB 文件。

| 操作 | 本机 Ubuntu / Pine64 写法 | Rockchip 原版写法 |
| --- | --- | --- |
| 枚举设备 | `list` | `ld` |
| 向 RAM 下载并启动 Loader | `boot <loader.bin>` | `db <loader.bin>` |
| 查询 Flash 容量 | `read-flash-info` | `rfi` |
| 查询 Flash ID / SoC 信息 | `read-flash-id` / `read-chip-info` | `rid` / `rci` |
| 查看 GPT | `list-partitions` | `ppt` |
| 写入文件 | `write <起始扇区> <文件>` | `wl <起始扇区> <文件>` |
| 读出文件 | `read <起始扇区> <字节数> <输出文件>` | `rl <起始扇区> <扇区数> <输出文件>` |
| 正常复位 | `reset` | `rd` |
| 持久更新存储中的 Loader | `upgrade-loader <文件>` | `ul <文件>` |
| 擦除 Flash | `erase-flash` | `ef` |

地址以 **512 B 扇区**计；本机 `read` 长度还必须是 512 的整数倍。
数值支持十进制和 `0x` 十六进制，避免用带前导零的十进制数。
例如从 LBA64 读取 512 B：本机是 `read 64 512 out.bin`，原版是 `rl 64 1 out.bin`。
本机也兼容 `ld/db/wl/rl` 等短别名，但 **`rl` 仍沿用本机字节语义**，所以本文使用长命令。

两份核对的实现都要求操作时只有一台 RockUSB 设备；枚举的 `DevNo` 不是选择设备参数。
`boot/db` 运行 RAM 下载程序，`upgrade-loader/ul` 持久修改启动固件，不能混淆。
本工程写完整 raw 镜像无需先执行 `erase-flash/ef` 或单独更新 Loader；
写入只覆盖镜像范围，不保证清零介质剩余部分。

## 4. 本工程完整镜像操作示例

以下以 **Ubuntu / Pine64 接口**为例，各步骤成功后再继续。
`write 0` 会覆盖目标盘分区表、启动固件和原系统，重要数据应事先完整备份。
主机只连接待操作的一块板子，并留足镜像与读回文件所需磁盘空间。

### 识别与加载

将占位路径替换为实际文件；`image` 按目标板卡选用 v2 或 5io：

```bash
loader=/path/to/rk3588_MiniLoaderAll.bin
image=/path/to/ubuntu-24.04-desktop-arm64-lubancat-5-v2.img
lsusb -d 2207:
sudo rkdeveloptool list
```

确认是目标 RK3588、模式为 MaskROM 后，将 USB Loader 下载到 RAM：

```bash
sudo rkdeveloptool boot "$loader"
sudo rkdeveloptool list
sudo rkdeveloptool read-chip-info
sudo rkdeveloptool read-flash-id
sudo rkdeveloptool read-flash-info
```

下载后可能发生 USB 重枚举；等待设备可识别、存储信息可读取。
结合 Loader 的介质配置、Flash ID 和容量确认目标确为所需 eMMC，容量不小于镜像；
仅看见 `Loader` 标签或容量接近，都不能单独证明目标身份。
本机版本无 `cs` 介质选择命令。所核对的 Rockchip 原版支持 `cs 1` 选 eMMC、
`cs 2` 选 SD、`cs 9` 选 SPI NOR，但还需要 Loader 支持，不能套用到本机版本。

可先只读保存前 16 MiB，供分析 GPT / 启动固件使用：

```bash
sudo rkdeveloptool read 0 16777216 before-first16MiB.bin
```

这份文件**不含完整 boot/rootfs 或用户数据**。需要整盘备份时，按查询得到的总扇区数
乘 512 作为 `read` 字节数，并核对备份文件大小与命令退出状态。

### 写入完整裸镜像

```bash
stat -c '%n: %s bytes' "$image"
sha256sum "$image"
sudo rkdeveloptool write 0 "$image"
```

等待进度完成并检查退出状态；命令成功只代表写入流程成功，不等于已全量读回校验。
完整 `.img` 从 **LBA0** 写入，不能照搬分区镜像示例中的 `0x8000` 等地址。
镜像已含成对的启动固件，无需再单独覆盖 `idbloader.img` / `u-boot.itb`。

### 4.1 旧野火镜像同布局时是否需要先擦除 eMMC

**结论：不需要。** 2026-10-29 对照野火上游仓库与当前镜像后，确认旧镜像和本工程镜像的分区起点一致；
在 RKDevTool 中加载匹配的 MiniLoader 后，从 `0x0` 强制按地址写完整裸 `.img`，已经会覆盖旧 GPT、裸启动固件、boot 分区和 rootfs 分区。
“先擦除再写入”不增加正常启动所需的步骤，也不能替代正确的 Loader、目标介质和镜像校验。

实证记录：

| 来源 | GPT / 分区结果 |
| --- | --- |
| 野火 `ubuntu-ebf-rk3588/scripts/build-image.sh` | `mklabel gpt`；p1 `16MiB` 至 `528MiB`；p2 `528MiB` 至镜像末尾 |
| 当前 `ubuntu-24.04-desktop-arm64-lubancat-5-v2-20261023-full-test.img` 的 `sfdisk --json` | p1 `start=32768`、`size=1048576`（16 MiB 至 528 MiB）；p2 `start=1081344`（528 MiB） |

这里的前 16 MiB 是裸启动固件预留区，不是一个分区；boot 分区从 16 MiB 开始。
旧 eMMC 中已有野火固件不会阻止新镜像写入，因为完整镜像从 LBA0 写入并包含新的 GPT、启动固件和两个 ext4 分区。
Windows RKDevTool 的 `boot` 项只是把 `rk3588_MiniLoaderAll.bin` 下载到 RAM，供 MaskROM 阶段初始化 DDR 和访问 eMMC；
它和镜像里的持久启动固件不是同一件事。

完整写入不会清零镜像文件末尾以外的 eMMC 物理扇区。若 eMMC 容量大于镜像，范围外旧数据可能仍留在介质上，但不会被新 GPT 的分区使用；
只有数据销毁、清理镜像范围外残留，或厂商针对特殊介质故障明确要求时，才考虑额外擦除。
擦除前仍须备份旧数据，因为从 `0x0` 写完整镜像本身已经会覆盖原系统。

### 读回比较与复位

仍在下载环境中，先读回镜像覆盖的范围，再比较原文件字节：

```bash
image_bytes=$(stat -c %s "$image")
read_bytes=$(( (image_bytes + 511) / 512 * 512 ))
readback="$image.readback"
sudo rkdeveloptool read 0 "$read_bytes" "$readback" &&
    test "$(stat -c %s "$readback")" -eq "$read_bytes" &&
    cmp -n "$image_bytes" -- "$image" "$readback" &&
    printf 'Read-back verification: PASS\n'
```

读回也有进度，耗时取决于镜像大小和 USB 链路。字节数向上取整是为了满足扇区对齐；
`cmp -n` 只比较原镜像范围，成功返回 0。**读取、大小检查和比较都成功后**才复位：

```bash
sudo rkdeveloptool reset
```

Rockchip 原版可按速查表换用 `ld/db/wl/rd`；读回必须改为
`rkdeveloptool rl 0 "$((read_bytes / 512))" "$readback"`，长度不能直接照抄。
启动后还需验证串口、分区与业务；读回一致不能证明镜像能在目标板卡上正常运行。

## 5. 常见问题与边界

| 现象 / 需求 | 判断与处理 |
| --- | --- |
| `lsusb` 没有 Rockchip 设备 | 核对下载口、数据线、供电与 MaskROM 进入时序；Linux 的普通 USB host 模式不接受下载 |
| `lsusb` 有设备，工具打不开 | 先用 `sudo` 排除 USB 节点权限，再查工具对 RK3588 PID 的支持与其它进程占用 |
| `boot/db` 失败或随后 Flash 不可读 | 核对 USB MiniLoader 的 SoC / DDR / 板卡匹配；不能拿介质启动像替换 |
| 多台设备报错 | 只保留一台，不能把 `DevNo` 当作位置参数 |
| 读回长度错误或超过 4 GiB 异常 | 先核对分支、字节/扇区单位和 Debian 补丁；再查主机空间、文件系统大小限制与退出状态 |
| 写入成功但不能启动 | 核对板卡镜像、目标介质、读回结果及启动串口；检查启动固件 / ATF，不凭 100% 进度认定成功 |
| 想写 NVMe / SPI | 先核对 USB Loader 是否支持；不能把 `wl` 想成能访问 Linux 所有块设备。当前 U-Boot 默认也不扫描 NVMe，见 [NVMe 启动边界](uboot-changelog.md) |

## 6. 依据与验证范围

- **主机实查**：2026-10-22，`/usr/bin/rkdeveloptool` 的包版本、help、manpage；未发送设备命令。
- **Rockchip 原版**：提交 [`304f0737`](https://github.com/rockchip-linux/rkdeveloptool/tree/304f073752fd25c854e1bcf05d8e7f925b1f4e14)，
  `main.cpp` 的 `handle_command`、`download_boot`、`write_lba`、`read_lba`、`change_storage`；
  [`Readme.txt`](https://github.com/rockchip-linux/rkdeveloptool/blob/304f073752fd25c854e1bcf05d8e7f925b1f4e14/Readme.txt) 给出 `db → wl → rd` 示例。
- **Ubuntu 包源码**：[归档目录](https://archive.ubuntu.com/ubuntu/pool/universe/r/rkdeveloptool/)，
  `1.32+pine64git20210920.cce7d2a` 的 `main.cpp`、`doc/rkdeveloptool.1.scd` 与 `-2` Debian 补丁 `0003/0004`，核对长命令、别名、单位和大文件读回修复。
- **官方工具 / 协议说明**：[rkdeveloptool](https://opensource.rock-chips.com/wiki_Rkdeveloptool)、
  [upgrade_tool](https://opensource.rock-chips.com/wiki_Upgradetool)、[RockUSB](https://opensource.rock-chips.com/wiki_Rockusb)。
- **工程文件**：`scripts/build-lsp-image.sh`（镜像源码：`scripts/build-lsp-image.sh`） 的 `image` 阶段；
  `debian/rules`（镜像源码：`packages/u-boot-lubancat-rk3588/debian/rules`） 的 `mkimage -T rksd` 与 `rkspi_loader.img` 配方。
- **原理图**：v2 V2R2（镜像源码：`hardware-docs/lubancat-5-v2/lubancat_5_v2_EBF410149V2R2_SCH_20250908.pdf`） PDF p15/16/32/41；
  5io 核心板 V1R0（镜像源码：`hardware-docs/lubancat-5io/LubanCat5CB_EBF410152V1R0_SCH_20240820.pdf`） p12/23、
  5io 底板 V1R0（镜像源码：`hardware-docs/lubancat-5io/LubanCat5IOB_EBF410153V1R0_SCH_20240820.pdf`） p5/9；页码按 PDF 页序。

待板测确认：实际 MaskROM 时序、线材与供电组合、MiniLoader 版本 / 哈希、目标 eMMC 选择、
大镜像写入与完整读回、复位后的启动结果。本文不把源码核对写成刷机成功记录。
