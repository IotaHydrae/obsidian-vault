# 构建过程安装的 deb 包与用途

构建会装三类东西：**`packages/` 下预置的 deb**（`rootfs` 阶段按类型用 dpkg / apt 安装）、
**`build/` 下的内核/U-Boot deb**（`board` 阶段安装，需先编译）、以及**从 apt 源装的包**
（基础包 + ROS2 + rockchip-multimedia PPA + 桌面）。

## TL;DR

- 三类 deb 的定位见 §1（预置）/§2（自编内核、U-Boot）/§3（apt 源）；Rust app 日常使用仓库固定包，重打包见 [rust-app-debs.md](rust-app-debs.md)。
- ⚠️ **`packages/arm64/` 不是只装 MPP/RGA**：里面还有 `libv4l-rkmpp`、`rockchip-vaapi`（§1）；
  **Chromium 也不在 Ubuntu 仓库里**，同样是我们预置的 deb（§1.2）。
- 默认 apt 安装用 **`--no-install-recommends`**（§7）；必需的推荐组件显式安装，
  `DESKTOP_RECOMMENDS=Y` 补充未裁剪的桌面推荐应用。
- 排除项（snap / LibreOffice / 浏览器）与清理逻辑见 §6/§8。
- 找"某个包为什么在镜像里"：先按这三类定位，再看 `scripts/build-lsp-image.sh` 的对应段。

## 1. `packages/`：预置 deb（`rootfs` 阶段直接装）

参与镜像安装或打包的目录如下，各自装法不同：

| 目录 | 装什么 | 怎么装 |
| --- | --- | --- |
| `apps/` | 三个 Rust app 的 arm64 deb 与固定 manifest | apps 阶段校验源码、身份和摘要；rootfs 用 apt 安装本地包，见 [Rust deb](rust-app-debs.md) |
| `arm64/` | Rockchip 专用运行库（逐包表见下） | 脚本 `dpkg -i /tmp/*.deb` 那一段（拷进 chroot → `dpkg -i` → `apt-mark hold ffmpeg`） |
| `Chromium-rkmpp/` | **Chromium 151 + 硬解配套**（§1.2） | 脚本里 `packages/Chromium-rkmpp/*.deb` 那一段 |
| `cage-lsp/` | **我们 fork 的 cage**（按键过滤 + NumLock + 底色） | 见 `packages/cage-lsp/README.md` |
| `rockchip-vaapi/` | `rockchip-vaapi` 的打包原料 | 产物 deb 在 `arm64/` |
| `u-boot-lubancat-rk3588/` | 自建 U-Boot deb（§2） | `build.sh -u` → `build/*.deb` |

**`packages/arm64/` 逐包**（版本列是 2026-10-03 实查的文件名）：

| 包 | 版本 | 干什么 |
| --- | --- | --- |
| `librockchip-mpp1` | 1.5.0+git20260811.c08762eb | **MPP 用户态库**：应用通过它调用 VPU 硬解/硬编（H.264/H.265/VP9/AV1…） |
| `librockchip-mpp-dev` / `rockchip-mpp-demos` | 同上 | 头文件 / 示例程序（`mpi_dec_test`/`mpi_enc_test`，验硬编解码用） |
| `librockchip-vpu0` | 同上 | MPP 的 VPU 封装库（硬件访问层） |
| `librga2` / `librga-dev` | 2.2.0-1 | **RGA 2D 加速**：缩放/旋转/裁剪/格式转换走硬件 |
| `ffmpeg` | 76cd3b5（**魔改版**，见 §1.1） | 接了 MPP 硬解 + RGA 缩放 + 硬件编码器；`h264_rkmpp` 这些解码器就是它提供的。**装后 `apt-mark hold`** |
| `camera-engine-rkaiq` | （deb 名不带版本）6.30.4 | **RK3588 的 ISP 3A 库（AIQ）**：自动曝光/白平衡/对焦，配合 rkisp v30 |
| `camera-engine-rkisp` | v2.3.0 | 老型号 rkisp（rk3288/3399…）的 3A 库 —— 本板用 rkaiq，这个属兼容/冗余 |
| `fire-config-special` | 1.2.6 | EmbedFire 板卡配置工具 `fire-config`（改网络/启动项等） |
| `rktoolkit` | 1.0.0 | Rockchip SDK 里的零散小工具 |
| `libv4l-rkmpp` | **1.8.0-1** | V4L2 到 RKMPP 的桥（Chromium 硬解链路的一环，见 `notes/system/chromium-rkmpp.md`） |
| `rockchip-vaapi` | **1.0.12-1+lsp1** | VA-API 转换层（我们自己的 `+lsp1` 版本；见 `notes/system/vaapi-rockchip.md`） |

> 这些 deb 是闭源/预编译的，来自野火官方；其中 ffmpeg、camera-engine 等与 Ubuntu 自带的同名包**并存**
> —— 靠 `overlay/etc/apt/preferences.d/rockchip-multimedia-ppa` 的 `Pin-Priority: 1` 把 PPA 版压住。
>
> ⚠️ **这个 pin 的 `Origin` 必须与实际配置的 PPA 完全一致**（`LP-PPA-<owner>-<name>`，
> 从 `/var/lib/apt/lists/*_InRelease` 读），否则**静默失效**、apt 不报任何错：
> 2026-03-24 起的上游版本写的是 `LP-PPA-jjriek-rockchip-multimedia`（那时用 jjriek 的 PPA），
> 而构建脚本 add 的是 `ppa:liujianfeng1994/rockchip-multimedia` —— 实测（2026-09-15 开发板）
> `apt-get -s upgrade` 会把厂商版 `librga2`/`librga-dev` 换成 PPA 的 `2.2.0-1+git20231208…~noble`。
> **先 pin 厂商版**（用户 2026-09-15 明确）：PPA 里那些 git 快照没在设备上测过，一律不自动替换。
> 已改成正确的 Origin，`tools/verify-image.sh` 加了"pin 的 Origin 必须等于源推出的 Origin"检查
> （故意改错时该检查会失败，实测过）。

### 1.1 那个 `ffmpeg` deb 的上游是哪个 fork（2026-09-30 查证）

> 用户问"我们板子上这个 ffmpeg 上游是哪里"、"jjm2473/ffmpeg-rk 是不是比我们的新"。

**上游 = [`nyanmisaka/ffmpeg-rockchip`](https://github.com/nyanmisaka/ffmpeg-rockchip)，
commit `76cd3b5103ca`（2026-03-24）。** 四条独立证据：

1. 板上 `ffmpeg version 76cd3b5` 的短 hash 在该仓库**命中**（`/commits/76cd3b5` → 200；
   换 `jjm2473/ffmpeg-rk` 则 422）；
2. 二进制的 codec 描述与那仓库 README 的表格**逐条一致**（`Rockchip MPP … decoder`、`Rockchip RGA … compositor`）；
3. 构建配置里的 `--enable-rkmpp --enable-rkrga` **是 fork 加的开关**（upstream FFmpeg 没有）；
4. `/usr/bin/ffmpeg` 的文件日期是 **Mar 25 2026**，正好是那个 commit 的**次日**。

基线与打包：**FFmpeg 8.x**（板上 `libavutil 60.26.100`，该仓库 master 现在 `RELEASE=8.1.2`、同为 `LIBAVUTIL 60.26`）；
deb 是**野火**拿它的源码用 **checkinstall 1.6.3** 打的（`Maintainer: root@lubancat`，gcc 11 / Ubuntu 22.04）。

⚠️ **它只有静态产物，没有共享库**（实测 `dpkg-deb -c` 里 `libav*.so` = **0**，只有 `/usr/bin/{ffmpeg,ffprobe}` +
`libav*.a`；`ldd /usr/bin/ffmpeg | grep -c libavcodec` = 0）⇒ `h264_rkmpp` **只有 `/usr/bin/ffmpeg` 自己能用**
—— 这正是 mpv 用不了 `--hwdec=rkmpp` 的根因（它链的是 Ubuntu 的 `libavcodec60`，里面没有 rkmpp）。
见 `notes/system/vaapi-rockchip.md` §3.4。

| | 我们（nyanmisaka fork） | `jjm2473/ffmpeg-rk` |
| --- | --- | --- |
| 上游 FFmpeg 基线 | **8.x**（libavutil 60.26） | 最新分支 `enc-5.1` = **5.1**（libavutil 57.33）；`master`/`enc` 其实是 **4.4** |
| 我们用的是它吗 | 是（commit `76cd3b5`） | **不是**（它的分支里没有这个 commit） |
| 能不能换 | — | **不能**：它是 `libavcodec.so.59`，noble 系统是 **`.60`**，ABI 不兼容，换会把 mpv/gstreamer/浏览器一起打坏 |

⇒ **不需要换、也不是它**。若哪天要让 mpv/GStreamer 也用上 rkmpp/rkrga，正解是拿同一个上游
（按需选更新的 commit）编一份 `--enable-shared` 的包；⚠️ 但 soname 跟着 FFmpeg 8.x 走，
要么连 mpv/gstreamer 一起重编，要么**只提供 `ffmpeg`/`ffprobe` 二进制、不去顶系统库**。
参考：外部仓 `yisding/rock-5b-ysp` 已把该 fork 移植到 FFmpeg 8.1.2（分支 `ffmpeg-81` + 28 patch），
但它是 Ubuntu 26.04/arm64、与 noble 的 `libavcodec.so.60` ABI 不同，**不能直接装**
（见 `notes/system/rock-5b-ysp-crossref.md` §1.3）。

### 1.2 `packages/Chromium-rkmpp/`：Chromium 是我们预置的 deb

⛔ **别按"Ubuntu 仓库装浏览器"的思路找它**：noble 仓库里的 `chromium-browser`/`firefox` 都是 **snap 过渡包**
（§6），所以镜像是把我们自己那份 PPA deb 放进 `packages/Chromium-rkmpp/` 再 `dpkg -i`：

| deb | 版本（2026-10-03 实查） |
| --- | --- |
| `chromium` / `chromium-common` / `chromium-sandbox` | **151.0.7922.169-1**（arm64） |

依赖（`libgtk-3-0t64` / `libgbm1` / `libdav1d7` / `libopenh264-7` / `libminizip1t64`…）由 apt 补；
硬解链路（RKMPP + `libv4l-rkmpp` + 伪节点 `/dev/video-dec0`）见 `notes/system/chromium-rkmpp.md`。

## 2. `build/` 下：内核 / U-Boot deb（`board` 阶段，需先编译）

脚本位置：`:1165-1201`。这些 deb 是 **`kernel-6.1/` 编译的产物**，编译太久，通常在别的机器编好
拷进 `build/`（脚本 `cd build/` 后按 `linux-image-*.deb` 等通配找到）。

| 包 | 干什么 |
| --- | --- |
| `linux-image-6.1.172`（当前版本） | 内核镜像 + **全部驱动模块**（含厂商 `rtl8822ce.ko`/`rtl8852be.ko`、主线 rtw88/rtw89、mpp/vpu 等）+ **设备树**。安装后 `depmod -a 6.1.172` 生成模块依赖，并 `apt-mark hold` |
| `linux-headers-6.1.172` | 内核头文件（给要自己编模块的人用），也 hold |
| `u-boot-lubancat-5-v2/5io` | Bootloader：`idbloader.img` + `u-boot.itb`（`image` 阶段写入镜像 32KB/8MB 处）+ `rkspi_loader.img`（SPI NOR）。装后 hold |

**实测数量（2026-10-03 开发板，`linux-image-6.1.172`）**：**1995 个 `.ko`**；
`/boot/firmware/dtbs/rockchip/` 下 **7 个基础 `.dtb` + 264 个 overlay `.dtbo`**
（⚠️ 这两个数**跟着内核包版本变**，本文早先版本写的 "5 + 255" 是旧包的；要准数就在目标机上 `ls | wc -l`）。

## 3. 从 apt 源安装（`rootfs` 阶段）

按顺序：**最小工具**（`ca-certificates curl gnupg locales`，装 `locales` 后才能 `locale-gen`）
→ **ROS2**（`software-properties-common` + `ros-jazzy-ros-base`，源默认**华为云**、回退清华→上游，
见 [apt-sources-deb822.md](apt-sources-deb822.md) §5.2）→ **BASE_PACKAGES** → **PPA**。

- **BASE_PACKAGES**（65 个，用户确认清单）：网络/远程（`network-manager` `wpasupplicant`
  `openssh-server` `net-tools` `wireless-tools` `ifupdown`）、Python（`python3-{paho-mqtt,numpy,pip,serial}`）、
  音视频（`pulseaudio` `bluez` `gstreamer1.0-*` 含 `gstreamer1.0-rockchip1` `v4l-utils` `alsa-utils` `sox`）、
  视频/ISP 周边（`libv4l-*` 含 `libv4l-rkmpp`、`mesa-*`、`libegl/libgles/libgl`、`mesa-vulkan-drivers`）、
  无线/USB（`usb-modeswitch` `usb-modeswitch-data` `ir-keytable` `dvb-tools`）、
  工具（`htop` `vim` `nano` `i2c-tools` `dosfstools` `zstd` `xz-utils`）、
  时间/杂项（`chrony` `fake-hwclock` `rockchip-multimedia-config`）。
  （2026-09-24 补了 `rsync`：现场要在设备上同步文件，且 `tools/sync-to-board.sh` 本身就是 rsync 驱动的。）
- **rockchip-multimedia PPA**（`add-apt-repository ppa:liujianfeng1994/rockchip-multimedia`）：
  提供 `gstreamer1.0-rockchip1`、`rockchip-multimedia-config`、`libv4l-rkmpp` 等（与 `packages/` 的 deb 配套，
  但走 apt 源分发）。⚠️ 它依赖 Launchpad，不可达时构建会挂 —— 上游已把这类包改成仓库内 `.deb`（见
  [upstream-sync-commits.md](upstream-sync-commits.md) §2.2 的 gstreamer 那条）。

## 4. 桌面会话（`rootfs` 阶段桌面）

GNOME 使用显式的 Noble 桌面组件清单，不安装 `ubuntu-desktop` / `ubuntu-desktop-minimal`
元包；GDM、GNOME Shell / Settings / Nautilus 等保留。应用为 mpv，中文显示由
`fonts-noto-cjk` 提供，`language-pack-zh-hans` 在基础列表，GNOME 不再安装文泉驿或 Rhythmbox。

HDMI 连接时默认运行 kiosk，GDM 仅在维护模式临时启动；见
[会话契约](../system/kiosk/session-design.md)。包裁剪清单、依赖来源与板测边界见
[镜像裁剪](image-size-trimming.md)。

## 5. 移除与清理

- `REMOVE_BASE_PACKAGES`：`cloud-init` `landscape-common` `cryptsetup-initramfs`（云端初始化/遥测用不到）。
- `REMOVE_BUILD_PACKAGES`：独立的 RIST / DVB / V4L / Mesa 开发包及 DVB 文档；
  宿主构建依赖不受影响，RGA / MPP 开发包与内核 headers 暂保留。
- `01-lsp-vendor-sdk` 在厂商包解包前配置精确 `path-exclude`，去掉 FFmpeg / RKAIQ
  专属静态库、头文件与 FFmpeg pkg-config 文件；系统 multiarch 开发文件和运行库保留。
- GNOME 桌面先显式安装保留组件，再 purge Yelp、图形升级、指定打印 / Ghostscript、
  文泉驿、Rhythmbox 与两个旧桌面元包；共享运行库由 APT 依赖决定，见 [镜像裁剪](image-size-trimming.md)。
- `PURGE_SNAP=Y`（默认）模拟后尝试移除 `snapd`、`snap-store`、`gnome-software`、`packagekit`；
  会连带移除保留桌面组件时跳过，尤其保留 `gstreamer1.0-packagekit`。
- 阶段收尾执行 APT 孤儿包清理与缓存清理；当前仍被功能依赖的包不会因文件看似无用而强删。

## 6. 排除项：snap / LibreOffice / 浏览器

**镜像里没有 snap、没有 LibreOffice、Ubuntu 仓库版浏览器也没有**（Chromium 是我们预置的 deb，§1.2）。
`ubuntu-desktop` 对 `snapd`、`firefox`、`libreoffice-*` 都只是 **`Recommends`**，而 Ubuntu 的 `firefox` deb
其实是 **snap 过渡包**（`PreDepends: snapd`）。为了不把这些带进嵌入式镜像，`overlay/etc/apt/preferences.d/`
下用 pin 让 apt **一开始就不装**（比"装完再 purge"省下几百 MB 下载，也不会连带删掉别的东西）：

- `no-snap`：`snapd`、`firefox` → `Pin-Priority: -1`
- `no-libreoffice`：`libreoffice`、`libreoffice-*`（通配符可用）

该目录由脚本**整目录拷贝**（`copy_overlay_dir`），新增 pin 文件无需改脚本。
若某包不是 Recommends 而是硬 Depends，则不能 pin（会依赖报错），改用 `REMOVE_DESKTOP_PACKAGES`
（装完再 purge）。**需要别的浏览器**请自选 deb 版：`epiphany-browser`（GNOME Web，Ubuntu 主仓，轻量）
或 Mozilla 团队的 deb firefox PPA。

## 7. 推荐组件与体积控制

默认 `DESKTOP_RECOMMENDS=N`，APT 安装用 `--no-install-recommends`，另通过
`DESKTOP_KEEP_PACKAGES` 显式补齐终端、密钥环、portal、权限、主题与本地解析等组件。
包是否缺席仍须看完整依赖图，不能把“某个元包只推荐它”当作不会安装的保证。

| 包 | 少了会怎样 |
| --- | --- |
| `gnome-terminal` | 没终端（dock 收藏里也列了它） |
| `gnome-keyring` + `libpam-gnome-keyring` | 密钥环不可用/登录不解锁 |
| `xdg-utils` / `xdg-desktop-portal-gnome` | `xdg-open` 失效 / GTK 文件选择框失效 |
| `policykit-desktop-privileges` / `gvfs-fuse` | 挂 U 盘不弹授权 / Nautilus 回收站与挂载整合失效 |
| `network-manager-config-connectivity-ubuntu` | 与我们的 NetworkManager 配置不配套 |
| `yaru-theme-*` + `fonts-ubuntu` | 外观退回原版 Adwaita + DejaVu |
| `avahi-daemon` + `libnss-mdns` | `主机名.local` 解析失效 |

`DESKTOP_RECOMMENDS=Y` 保留同版原 GNOME 元包未裁剪的推荐应用；仍遵循既有
Snap / LibreOffice 禁用策略和本轮帮助、音乐、打印裁剪，不恢复完整 Ubuntu 桌面。
自定义 `DESKTOP_KEEP_PACKAGES` 只调整补充清单，GNOME 的裁剪根包仍会被移除。
完整策略和实际收益统一记录在 [镜像裁剪](image-size-trimming.md)，不通过重新安装元包比较体积。

> Recommends 按实际需求选取。`adb` 推荐的 `android-sdk-platform-tools-common` 带通用 Android
> USB 规则，默认不安装；仓库只维护 Quest 规则，并在重新配置 rootfs 时清理旧包和旧通用规则。
> 依据与范围见 [app-and-device-config.md](../system/app-and-device-config.md)。

Type-C 固定为 source + host 后，`rootfs` 阶段清理旧 `umtp-responder` 包与 USB shared 网络配置。
`dnsmasq-base` 由[板载以太网直连](../system/ethernet-direct.md)显式安装，仅按需提供 DHCP；
不安装全局 `dnsmasq` 服务，也不恢复 USB shared 网络。
`adb` 主机客户端及 Quest 授权密钥保留，见 [USB 主机工具](../usb/usb-gadget.md)。

`verify-image.sh` 的包数与安装后大小只用于包数据库体检；文件实占和最终 `.img`
长度须按 [镜像裁剪 §3/§4](image-size-trimming.md) 分开测量。

## 8. `PURGE_LIBREOFFICE` 已删（2026-09-14）

`no-libreoffice` 的 `Pin-Priority: -1` 让 LibreOffice **从源头就装不进来**。以前 `rootfs` 阶段还额外做一件事：
因为默认复用旧 `build/rootfs`，怕老 rootfs 里留着早期装进去的 LibreOffice，每次都用 `dpkg-query` +
`apt-get -s purge` 检查并清一遍（开关 `PURGE_LIBREOFFICE`）。**2026-09-14 删掉这段**，理由是实测它已是死代码
（构建日志每次只打"LibreOffice：未安装，无需处理"，rootfs 里也没有任何 libreoffice 文件）。
防回归没有削弱：`tools/verify-image.sh` 的「移除项（应不存在）」分组**仍然**检查
`libreoffice*` / `ure` / `uno-libs3` / `python3-uno` 一个都不在（离线也跑）。

## 9. 安装后状态（可在 `build/rootfs` 里核对）

```bash
ls build/rootfs/lib/modules/$(ls build/rootfs/lib/modules | head -1)/kernel/drivers/net/wireless/rtl8822ce/  # 8822ce.ko
chroot build/rootfs dpkg -l | grep -iE "mpp|rga|rkaiq|ffmpeg|rktoolkit|fire-config|chromium"
chroot build/rootfs apt-mark showhold          # 被 hold 的包（升级时不会被替换）
```
