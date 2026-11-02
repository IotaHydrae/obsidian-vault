# Luckfox Lyra Zero W：Ubuntu 和 XFCE 安装记录

> 使用 `rk3506-ubuntu` 的 `luckfox-bpi` 分支构建镜像，再配置 XFCE、显示驱动和可选 VNC。

## TL;DR

- 构建环境为 Ubuntu 22.04 或相应容器，系统镜像输入为 Ubuntu 24.04.3；若目标为 24.04.4，需另行确认升级流程。
- 构建、刷写及驱动安装均依赖仓库、固件版本和实际板卡；先确认目标并准备恢复手段。
- Python 构建依赖宜放在隔离环境中；root 自动登录和 VNC 配置仅用于受控的测试设备。

## 镜像构建

参考 [rk3506-ubuntu 的 luckfox-bpi 分支](https://github.com/markbirss/rk3506-ubuntu/tree/luckfox-bpi)。构建依赖包括交叉编译工具、`live-build`、设备树编译器、Python 2 和 `7z`；具体包名及版本按构建环境和仓库说明核对。

```bash
git clone -b luckfox-bpi https://github.com/markbirss/rk3506-ubuntu.git
cd rk3506-ubuntu/device/rockchip/.chips/rk3506
ln -s .chips/rk3506 ../../rk3506
ln -s .chips/rk3506 ../../.chip
cd ../../../..

git clone https://github.com/markbirss/ubuntu_24.04.3.git
cd ubuntu_24.04.3
7z x ubuntu_24.04.3.7z.001
sha256sum ubuntu_24.04.3.tar.gz
```

此示例的 `ubuntu_24.04.3.tar.gz` SHA-256 为 `d6f58545b0b9c679665a8ff58dd2a7a75aa2b2648871e4be5a2c2288b4261545`；该校验值仅对应完全相同的输入文件。

将校验后的压缩包放到构建仓库的 `ubuntu/` 目录；确认目标文件不存在后执行：

```bash
mv ubuntu_24.04.3.tar.gz ../
cd ..
mkdir -p ubuntu
mv ubuntu_24.04.3.tar.gz ubuntu/
```

构建与刷写入口包括 `./build.sh lunch`、`sudo ./build.sh` 和 `sudo ./rkflash.sh update`，具体参数按仓库说明核对。刷写前确认目标设备并备份数据；Python 2 依赖应在构建环境内解决，避免直接改写共享宿主机的 `/usr/bin/python`。

## 安装桌面

### Ubuntu 软件源

Ubuntu 24.04 ARM 的 deb822 软件源位于 `/etc/apt/sources.list.d/ubuntu.sources`。使用 USTC 镜像的示例：

```deb822
Types: deb
URIs: https://mirrors.ustc.edu.cn/ubuntu-ports
Suites: noble noble-updates noble-backports noble-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

替换前保留现有配置，并确认目标镜像提供相应 suite；官方源为 `http://ports.ubuntu.com/ubuntu-ports/`。

### 安装软件包

在目标设备上安装桌面环境：

```bash
apt update
apt install -y --no-install-recommends xserver-xorg-input-all xserver-xorg-core \
  xinit xfce4-terminal xserver-xorg-video-fbdev x11-utils
apt install -y --no-install-recommends xfce4 dbus-x11 mesa-utils \
  xubuntu-default-settings xfce4-goodies lightdm-gtk-greeter \
  lightdm-gtk-greeter-settings
```

需以管理员权限运行，并根据发行版确认包是否可用。将匹配运行内核的 `panel-mipi-dbi.ko` 安装到内核模块目录，运行 `depmod -a`，再按需在 `/etc/modules-load.d/` 配置开机加载。

```bash
mkdir -p "/lib/modules/$(uname -r)/kernel/drivers/gpu/drm/tiny"
cp /root/panel-mipi-dbi.ko "/lib/modules/$(uname -r)/kernel/drivers/gpu/drm/tiny/"
depmod -a
echo panel-mipi-dbi > /etc/modules-load.d/panel-mipi-dbi.conf
```

## 登录和远程访问

- 本地自动启动桌面的配置涉及 getty、`~/.bash_profile` 中针对 `tty1` 的 `startx`，以及 `~/.xinitrc` 中的 `startxfce4`。优先使用普通用户；root 自动登录仅在受控环境评估。
- 远程桌面可使用 `x11vnc`，客户端连接板卡的 5900 端口；访问前配置认证和网络隔离。
- Wi-Fi 驱动示例在板卡的 `/home/lyra/aic800/` 目录执行 `make install`；源码、路径和内核适配条件未记录。

### 自动启动 XFCE

在目标用户的 `~/.bash_profile` 中仅对本地 `tty1` 启动 X，避免 SSH 登录也执行：

```bash
if [[ -z $DISPLAY ]] && [[ $(tty) = /dev/tty1 ]]; then
    exec startx
fi
```

该用户的 `~/.xinitrc`：

```bash
exec startxfce4
```

受控设备若需要免交互本地登录，在 `/etc/systemd/system/getty@tty1.service.d/autologin.conf` 中配置实际用户，保留终端访问权限控制：

```ini
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin YOUR_USER --noclear %I $TERM
```

VNC 客户端可使用 [TigerVNC](https://sourceforge.net/projects/tigervnc/files/stable/1.16.2)。VNC 认证和传输保护需单独配置；端口可达不等于访问安全。
