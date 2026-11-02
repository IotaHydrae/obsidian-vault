# CM4 启动时间：固件与桌面启动的实测优化

> Trixie CM4 的启动优化应逐项对照，并分别测量固件、内核和完整桌面边界。

## TL;DR

- 已验证的方向是禁用不使用的 cloud-init、gzip 内核、符合条件后直接挂载 SD 根分区，以及移除同步 Netplan 生成。
- 首次启动必须保留扩容、PARTUUID 和账户初始化依赖；不能把稳定运行阶段的无 initramfs 策略直接用于新卡。
- 完整桌面截图曾从内核后 23.53 s 到 17.43 s；该观察不是电源键计时或五秒冷启动证明。
- 本文是 2026-10-18 的 Raspberry Pi OS/Trixie 结果；Ubuntu 后续结论见[早期桌面](cm4-early-desktop.md)和[Netplan 专用入口](../network/ubuntu-netplan-generate.md)。

## 方法：按启动阶段建立可比边界

固定设备、内核与根存储后，分别记录固件 ARM 参考点、原始 dmesg、systemd 和完整桌面截图。
先区分首次初始化与日常重启，再逐项改变 cloud-init、内核压缩、根挂载或网络生成策略。
桌面判据要求审阅壁纸、面板和图标；各阶段时钟的解释见[测量时钟域](measurement-clock-domains.md)。

## Cloud-init 策略缺陷与受控结果

固定 pi-gen 的 `ENABLE_CLOUD_INIT=0` 只跳过种子复制，已安装服务仍执行。
工程仅在显式为 0 时创建 `/etc/cloud/cloud-init.disabled`，沿用软件包提供的禁用入口，
不删除账户/SSH 初始化流程。实测标记为 `disabled-by-marker-file`，相关单元未启动；
原始 Image 对照中 systemd 少 2.577 s。回滚删除标记并重启。

## Gzip 固件加载结果

同一 Image 从 29,317,632 压缩到 10,616,104 字节，解压后哈希一致。
固件日志“命令行读取完成→ARM 启动”区间从 4.943126 s 到 3.329544 s，
已包含加载后处理/解压。此阶段早于 Linux，systemd-analyze 不覆盖它。
不能把某条日志间隔直接解释为纯 SD 读取或纯 gzip 解压耗时。

## 直接挂载根文件系统：稳定运行阶段的收益与首次启动约束

试验省略 initramfs 后，固件 ARM 参考点从 11.909034 s 到 7.921233 s，内核
约 3.1 s 到 2.080 s；SD/ext4、root fsck、无线、横屏和电量仍正常。
但 pi-gen 的 initramfs 承担 `resize_early`、`set_partuuid`、`imager_fixup`，新卡必须保留。

工程 CM4 策略先显式加载恢复 initrd，启动 60 秒后再检查：根/启动分区同 MMC、
PARTUUID/fstab 一致、内核/config/initrd 匹配，根所需驱动内建。自定义早期初始化、
加密、独立 /usr、不支持布局或首次启动参数均拒绝。更新钩子恢复默认策略并禁止同次
启动重新批准。该检查器不会重启或删除 initrd，CM5/其他后端不继承该策略。

已验证 firmware include 确实读取 `uconsole/initramfs-policy.txt`；一次更新周期先加载
initramfs，再由定时器批准后续直接根启动。新导出镜像的完整首次启动周期仍需新卡验证。

## NetworkManager：移除同步 Netplan 生成

树莓派 NM `1.52.1-1+rpt4` 的 `reload_connections()` 同步调用 Netplan，
即使没有 YAML 仍生成并重载 udev/systemd。仅删 YAML 或改运行时插件不会移除编译路径。

Trixie 对照替换匹配版本的 Debian `network-manager/libnm0/network-manager-l10n`，
NM 从 3.414 s 到 0.841 s，systemd 少 2.705 s；配置哈希、Wi-Fi 与回滚包均保留。
工程仅对禁用云种子的 Trixie 设置这三包的限定优先级，不把此来源策略套到 Ubuntu。
Ubuntu 的保留语义方案见[专用 generate 入口](../network/ubuntu-netplan-generate.md)。

## 显示、Mesa 与未使用的 PCIe

- 精简私有 Mesa 仅供合成器，普通应用/Xwayland 仍用系统库；V3D 作业及 X11 GLX 查询验证隔离。
  一组观测的 labwc 读入量从约 88 MB 到 35 MB、桌面采样 18.93 s 到 17.62 s；systemd 无改善。
  构建和进程隔离的规范方法见[私有 Mesa](../desktop/private-mesa-startup.md)。
- 精确显示模块列表让 fbdev 从 7.918 s 到 5.082 s，不能证明物理首帧更早；见[显示提前加载](../display/cm4-early-display.md)。
- 官方 CM4 转接板未连接 PCIe，`dtparam=pcie=off` 移除约 345 ms 探测等待；通用载板配置单独维护。
- 约 0.59 s initcall 停顿已定位，锁竞争仍为假设；没有保留黑名单或删除 tracing，见[initcall 分析](cm4-initcall-profile.md)。

## 设备观察与测量边界

CM4 Rev 1.1、树莓派 SD、`6.18.57-v8-uconsole`。每行是一轮热重启，完整桌面要求
审阅 screencopy 内容，确认壁纸、面板和图标；进程存在、非黑像素和 graphical.target 都不足够。

| 配置 | systemd 总耗时 | 完整桌面截图的内核运行时间 | 同截图主机重启耗时 |
| --- | ---: | ---: | ---: |
| 原始 Image、cloud-init 开 | 20.467 s | 23.53 s | 40.392 s |
| 原始 Image、cloud-init 关 | 17.890 s | 21.74 s | 38.954 s |
| gzip、cloud-init 关 | 18.140 s | 21.99 s | 37.307 s |
| 无 initramfs、树莓派 NM | 17.329 s | 19.50 s | 30.766 s |
| 无 initramfs、Debian NM | 14.624 s | 17.43 s | 28.640 s |
| 综合配置复测 | 14.915 s | 18.76 s | 29.976 s |

这些单次采样有波动，不能把差值当作统计承诺。主机耗时含关机和采样，固件时钟未与
电源键校准，截图也不是实体首帧。早期内核事件应读原始 dmesg：本环境 journal
short-monotonic 的早期条目集中在约 4.57 s 的收录时刻，不能用于定位实际 initcall。

## 部署边界与留存证据

综合持久性启动 `00000000-0000-4000-8000-000000000034` 中：固件 ARM 参考点
7.978505 s、init 1.417912 s、fbdev 4.726610 s、systemd 15.000 s；完整截图覆盖内核后
16.98–17.33 s、主机后 27.786–28.801 s。该结果不是另一组独立单变量对照。
Wi-Fi 配置、横屏、电池报告、无降频和无失败单元分别采集，不据此宣称全部外设正常。

原始 Image/配置/匹配 initrd 与两来源 NM 包留作回滚；诊断参数和临时恢复任务已清理。
这些实机试验不自动更新既有导出镜像，也不证明新镜像冷启动。
证据入口为本地 `~/.cache/uconsole-cm4-postflash-20261018/` 的 `boot-*`、
`optimization-round2/`、`optimization-round3/` 和 `mesa-source-review/`。
历史完整过程可查 Git，知识库只保留可复用机制与判据。
