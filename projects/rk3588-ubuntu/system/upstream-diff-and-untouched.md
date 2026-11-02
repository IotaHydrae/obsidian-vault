# 与上游的责任划分：我们改了什么 / 上游带的我们没动

> 本文回答**"哪些是我们的改动、哪些是上游 EmbedFire 带的"**（排查时先分清责任再动手）：
> ① 上游带进来、我们**没动**的东西列成一节；② 相对上游的**完整改动清单**（可复现，别靠记忆）。
> 系统集成总账（命令/服务/udev/配置/启动分区/对厂商补丁/必须显式装的包）见
> [system-integration.md](system-integration.md)（§0~§8）。
> ⚠️ 正文**原样搬自该篇的 §9 与 §11**（节号保留）。

## TL;DR

- ⚠️ **改动清单别靠记忆**：§11 是可复现的完整列表（从上游 clone 逐项 diff 得来）。
- §9 列的是**上游带的、我们没动**的东西 —— 出问题时先确认它属于哪一边，再决定动不动。


## 9. 上游 EmbedFire 带的、我们没动（列出来只为区分责任）

- 蓝牙/音频那一串：`usr/lib/scripts/{alsa-audio-config,gdm-hack.sh}`、
  `usr/lib/systemd/system/{rtl8852be-reload,cpu-governor-performance,rtc-hym8563,alsa-audio-config}.service`
  - ⚠️ **2026-09-26 我们删掉了这一串里用不到的部分**（对齐上游 `f060c540d` 的清理方向）：
    `usr/bin/{brcm_patchram_plus,bt_load_rtk_firmware,rtk_hciattach,ubuntu-rockchip-install}`、
    `usr/lib/scripts/ap6275p-bluetooth.sh`、
    `usr/lib/systemd/system/{ap6275p-bluetooth,rtl8821cs-bluetooth,enable-usb2}.service`。
    依据（开发板 `board@<HOST_07>` 实测）：前面那些**根本没被装进镜像**（是 overlay 里的死文件），
    只有 `ubuntu-rockchip-install` 被 build 脚本显式装过（我们自己的写卡工具是 `tools/update-emmc.sh`
    与 `lsp-init-sdcard`，用不到它）；`.gitattributes` 里给它单列的 LFS 规则也一并删了。
    ⛔ **`rtl8852be-reload.service` 保留**：我们 `config/boards/lubancat-5-v2.conf` 与 `5io.conf`
    都 enable 了它（上游能删是因为同期更新了 WiFi 驱动，见 `../build/upstream-sync-commits.md` §2.3）。
  - ⚠️ `gpu-governor-performance.service` **原本也在这张单子里，2026-09-20 被我们删了**
    （上游写的是 kbase 的 sysfs 路径，panthor 上不存在 = 空转；且真设成 performance 会因 `opp-suspend`
    把 GPU 永久钉在 200MHz）。原因与证据见 `../hardware/gpu.md` §6。
- `usr/lib/scripts/gdm-hack.sh` + `99-gdm-hack.rules`（GDM 热插拔修复）
- ⚠️ **initramfs hooks：上游那 4 个 2026-09-26 全删了**（只剩我们自己的两个 + `fsck-ext4` 系）：
  `hooks/rtl-bt`、`hooks/mali-firmware`（把目录路径当 `copy_exec` 第二参数，只在目标已存在时才当目录
  ⇒ `rtl_bt` 落成一个文件；且固件本来就有两条自动通路：built-in → `add_builtin_firmware`、
  模块 → `dracut-install --firmwaredirs`）、`hooks/usb_modeswitch`（initrd 阶段没有设备需要切模式，
  运行期的 `/etc/udev/rules.d/40-usb_modeswitch.rules` 不受影响）、`scripts/functions`
  （与 `initramfs-tools-core 0.142ubuntu25.8` 包里那份逐字节相同）。
  证据、`lsinitramfs` 前后 diff 与替代机制见 [initramfs.md](initramfs.md) §3.3/§3.4/§4/§4.1。
  （同目录下的 `hooks/fsck-ext4`、`scripts/local-bottom/fsck-boot-firmware` 是我们自己的改动 —— 见 §11；⚠️ 这个 hook 2026-09-26 前叫 `fsck-vfat`，boot 分区改 ext4 后改名）
- `etc/mpv/mpv.conf`、`etc/X11/xorg.conf.d/`、`etc/netplan/`、`etc/adduser.conf`、`etc/rc.local`
- `usr/lib/systemd/system/NetworkManager-wait-online.service`（厂商覆盖版，开机没网会 failed，与本项目无关）

---


## 11. 附：相对上游的**完整改动清单**（可复现，别靠记忆）

上游基线是 `origin/24.04-panfrost`。**只要出现在下面两张表里，就不是上游原样**，
归属（我们加的 / 用户手加的）以上面各节的表为准。

```bash
# overlay 里相对上游的全部已提交改动
git log --name-only --pretty=format: origin/24.04-panfrost..HEAD -- overlay/ | grep -v '^$' | sort -u
# 再加工作区未提交的（新增文件在第二列）
git status --porcelain overlay/ | awk '{print $2}' | sort -u
# overlay 之外被改过的脚本（不属于上面那条 pathspec）
git log --name-only --pretty=format: origin/24.04-panfrost..HEAD -- scripts/ tools/ build.sh config/ packages/
```

**定制文件基线**（`overlay/` 下，去掉前缀；已删除的 device 文件不再列出）：

```
bgrt-fallback.png
boot/firmware/dtbs/rockchip/overlay/README.md
boot/firmware/dtbs/rockchip/overlay/rk3588-lubancat-5-v2-usbcon.dts
boot/firmware/dtbs/rockchip/overlay/rk3588-lubancat-5-v2-usbcon-host.dtbo
etc/apt/preferences.d/no-libreoffice
etc/apt/preferences.d/no-snap
etc/chrony/chrony.conf
etc/dconf/db/local.d/00-favorite-apps
etc/dconf/db/local.d/01-desktop-behavior
etc/gdm3/custom.conf
etc/hosts
etc/resolv.conf
etc/udev/rules.d/99-chest-camera.rules              ← 用户手加
etc/udev/rules.d/99-lcd_voice_bridge.rules          ← 用户手加
etc/udev/rules.d/99-left-hand-camera.rules          ← 用户手加
etc/udev/rules.d/99-meta-quest.rules                ← 用户手加
etc/udev/rules.d/99-right-hand-camera.rules         ← 用户手加
etc/update-motd.d/00-header
etc/update-motd.d/30-sysinfo                        ← 用户手加
home/board/.lsp/config/device.yaml                    ← 用户手加（overlay/home/）
opt/banner.txt                                      ← 用户手加（overlay/opt/）
opt/logo.txt                                        ← 用户手加（overlay/opt/；2026-09-26 起不在登录横幅显示，文件保留）
ubuntu-logo-icon.png
ubuntu-logo.png
usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf
usr/lib/systemd/system/lsp-growroot.service
usr/local/bin/check-hdmi-plymouth.sh
usr/local/bin/lsp-usb
usr/local/sbin/lsp-growroot.sh
usr/share/backgrounds/lockscreen.png
usr/share/backgrounds/wallpaper.png
usr/share/glib-2.0/schemas/99-lsp-desktop.gschema.override
usr/share/initramfs-tools/hooks/fsck-ext4
usr/share/initramfs-tools/scripts/local-bottom/fsck-boot-firmware
usr/share/keyrings/ros-archive-keyring.gpg
usr/share/plymouth/themes/spinner/watermark.png
warty-final-ubuntu.png
```

**工作区未提交的**（2026-09-12，本会话新增/改动）：

```
boot/firmware/dtbs/rockchip/overlay/README.md                    改
etc/initramfs/post-update.d/zz-update-firmware                   改（保住自定义 dtbo）
etc/systemd/system/lsp-usb-powerfix.service                      新
etc/systemd/system/plymouth-quit-wait.service.d/check-hdmi.conf  改（超时修正）
etc/udev/rules.d/99-lsp-typec-powerfix.rules                     新
home/board/.android/                                               新（adb 固定密钥）
usr/local/bin/lsp-usb                                            固定 host/source 查询与外设重新枚举
usr/local/bin/lsp-usb-powerfix                                   新
```

**`overlay/` 之外的改动**（同样不是上游原样，改构建时容易漏看）：

- `scripts/build-lsp-image.sh` —— `rootfs` / `board` / `image` 阶段的安装块、`BASE_PACKAGES`、厂商补丁、
  ROS2 源回退、`apt_update_retry`、dtbo 拷贝与 `overlays=` 合并…
- `tools/` —— 新增 `apt-cache.sh`、`check-atf.sh`、`sync-to-board.sh`、`verify-image.sh` 等；
  `verify-image.sh` 的分组与检查项是随每次改动同步长的
- `build.sh` —— `-c` 保留 `build/apt-cache`、`-C` 全清
- `packages/u-boot-lubancat-rk3588/debian/rules` —— 修 dpkg ≥1.23 不跑 `build` 的坑
- `config/` —— 板卡配置与 uEnv（**用户自己写的，见 [system-integration.md](system-integration.md) §8**）

> ⚠️ 维护提示：**归属靠这张表 + 各节的"谁加的"列，不要凭印象**。
> 我在 2026-09-12 就把用户手加的 5 条相机/串口/Quest 规则错标成了"上游"，
> 原因是只看了目录名（`99-*`）没去比对 git delta。
