# KT4 刷写与恢复能力

TL;DR：当前设备是生产锁定、开启安全启动的 KT4。正常系统的 root 权限和
Fastboot 的刷写权限是两套边界。没有验证可用于失败后写回镜像的离线通道，
因此工程采用保留厂商系统的 chroot 路线；不把备份视作救砖保证。

调查日期：2026-10-29。范围：用户这台 KT4，固件 5.18.1.1.1。
以下结论区分本机观察、对应固件公开源码及同型号公开报告。

## 各种“刷机”的具体含义

| 路径 | 当前判断 | 证据和限制 |
| --- | --- | --- |
| 官方更新包 | 有官方手动更新流程 | 复制型号对应的更新文件，再从设备设置触发；不是任意整盘镜像接口，本次未执行 |
| 运行系统下 root 写块设备 | 具备底层访问条件，未做写试验 | 已获得 root，system 块设备 RO=0；不能据此保证替换后的启动或恢复 |
| Fastboot 刷任意 system / boot | 不作为可用通道 | 本机 bootloader 有锁定分发逻辑；同型号公开实测 flash / erase / boot 被拒绝 |
| 通用 `oem unlock` | 没有可直接使用的实现 | 源码返回 FAILnot implemented；外层权限检查还可能先拒绝命令 |
| Amazon 签名解锁码 | 存在验证入口，不等于已有解锁能力 | 写入前调用 amzn_verify_unlock；未取得有效解锁材料，也不修改锁定状态 |
| 串口 U-Boot 任意命令 | 不能套用旧款教程 | 生产锁定时 CLI 受限制；不能假设串口连接后就能 mmc write |
| NXP ROM USB 下载、ISP / 编程器 | 未验证 | 未建立针脚、授权镜像、工具和完整写回验证链；不列作可用恢复方案 |

## 本机只读证据

启动参数包含 secure_cpu=1、androidboot.prod=1 和
androidboot.unlocked_kernel=false。生产锁定与启动链完整性由设备本身报告；
本次没有读取或修改 fuse 内容。

GPT 分区表的 header CRC 和 entry-array CRC 均通过校验：

| Linux 分区 | GPT 名称 | KiB |
| --- | --- | ---: |
| mmcblk1p1 | kernel | 16384 |
| mmcblk1p2 | wfm | 4096 |
| mmcblk1p3 | keys | 2048 |
| mmcblk1p4 | reserved0 | 512 |
| mmcblk1p5 | reserved1 | 512 |
| mmcblk1p6 | miscdata | 16384 |
| mmcblk1p7 | snapshot | 262144 |
| mmcblk1p8 | system | 524288 |
| mmcblk1p9 | cache | 98304 |
| mmcblk1p10 | userdata | 2891742 |

启动参数 root=/dev/mmcblk1p8 与 system 一致。p7 的实际名字是 snapshot；
不得把旧款分区图中的 diags 套用到这里。普通 USB 模式只暴露用户存储视图，
不是整块 eMMC，也不包括独立 boot0 / boot1 区。

boot0 中可见 U-Boot 2016.03、Jaeger DTB 名称、FAILlocked command 及
签名认证相关字符串。它们证明相关逻辑存在，不单独证明所有运行时分支。

## 官方公开源码中的限制

从 Amazon Source Code Notice 中 Kindle (10th Generation) 的对应版本链接
下载源码。归档固定信息见 manifest/amazon-source.toml；解包缓存被 Git 忽略。
这些源码不是本机 bootloader 的字节级复现；本机 bootloader 报告的构建日期
为 2020-02-19，固件更新未必同步更新 bootloader。

在 `gplrelease/uboot.tar.gz` 内：

- `drivers/usb/gadget/f_fastboot.c:2676`：UFBL_FEATURE_FASTBOOT_LOCKDOWN 开启时，
  在命令分发之前调用 is_restricted_command_on_locked_hw，拒绝时返回 FAILlocked command。
- 同文件 2597：oem unlock 的处理返回 FAILnot implemented。
- 同文件 2548：flash:unlock 分支调用 amzn_write_unlock_code，不是无条件解锁。
- `board/lab126/mx6sll_rex/secure_boot_cfg.c:270`：检查解锁码长度并调用
  amzn_verify_unlock，验证通过才允许写入 IDME。
- 同文件 53 / 304：锁定 CLI 的白名单是 fastboot 和 reset。
- `common/main.c:78`：生产锁定设备进入交互阶段后循环执行 fastboot。
- `drivers/usb/gadget/f_fastboot.c:1864`：启动分区中的 boot image 先经过
  amzn_authenticate_image；失败返回，不继续启动。
- `common/image-android.c:153`：boot image 长度由头、kernel、ramdisk 和 second
  组成；独立 system 分区不属于这段 boot image。

amzn 的部分安全库实现不在这份公开组件内。不能据此完整列出 Fastboot
所有允许命令，也不把源码中的条件编译配置当作本机逐个命令的实测。

## rootfs 是否可以单独替换

system 位于独立分区，公开启动路径认证的 boot image 包含 kernel / ramdisk 等。
因此“替换 rootfs 必须先关闭安全启动”不是从这份源码能推出的结论。
但签名 ramdisk 内还有启动逻辑，rootfs 的其他完整性要求、init、设备服务和
恢复逻辑未完成验证；不据此承诺任意 Alpine rootfs 都能启动。

直接覆盖 system 同时会失去当前提供 SSH、USB 导出和更新服务的运行环境。
在尚未验证离线写回能力时，该写入不可作为可安全回滚的实验。

## 备份与恢复

备份可以保存原始内容、设备专属数据，并用于分析或后续找到写回方法；
它不会自动提供恢复入口。整盘用户区镜像也不自动包含独立 eMMC boot 区，
运行中读取的可写文件系统也不是离线一致快照。

KindleModding 的旧款恢复说明明确把 KT4 / PW4 排除在其支持范围之外。
这限制的是该流程，不能扩展成“所有物理编程方法都绝对不可行”。
同型号 KT4 的公开帖子报告 Fastboot getvar / stage 可用，而 boot / flash /
erase 和若干 OEM 命令返回 locked command。它是同型号报告，不是本机测试。

本次遵从用户要求不创建整机备份，不执行闪存写入，不更改启动安全状态。
未来若要替换系统，先建立并验证独立于 system 分区的恢复路径，再讨论写入。

## 公开来源

- Amazon 官方源码索引：https://digprjsurvey.amazon.com/csad/help/node/200203720
- Amazon 手动更新流程：https://digprjsurvey.amazon.com/csad/help/node/GFAE5G5UGCYA25DW
- KindleModding 旧款恢复范围：https://github.com/KindleModding/kindlemodding.github.io/blob/main/content/jailbreaking/Legacy/backing-up/restoring.md
- KT4 / J9G29R Fastboot 实测报告：https://www.mobileread.com/forums/showthread.php?t=375969

论坛中的其他回复并未建立恢复方案，不把它们当作可用操作步骤。
