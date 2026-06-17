https://github.com/markbirss/rk3506-ubuntu/tree/luckfox-bpi

## 安装 Ubuntu 24.04.4 LTS

```bash
[prepare]
Use either a docker Ubuntu 22.04 or Ubuntu 22.04 environment

Install dependency packages.

sudo apt update

sudo apt-get update && sudo apt-get install git ssh make gcc libssl-dev \
liblz4-tool expect expect-dev g++ patchelf chrpath gawk texinfo chrpath \
diffstat binfmt-support qemu-user-static live-build bison flex fakeroot \
cmake gcc-multilib g++-multilib unzip device-tree-compiler ncurses-dev \
libgucharmap-2-90-dev bzip2 expat gpgv2 cpp-aarch64-linux-gnu libgmp-dev \
libmpc-dev bc python-is-python3 python2 

sudo ln -sf /usr/bin/python2 /usr/bin/python 

[build instructions]
git clone -b luckfox-bpi https://github.com/markbirss/rk3506-ubuntu.git

cd rk3506-ubuntu/device/rockchip/.chips/rk3506
ln -s .chips/rk3506 ../../rk3506
ln -s .chips/rk3506 ../../.chip
cd ../../../../

#sha256sum
#d6f58545b0b9c679665a8ff58dd2a7a75aa2b2648871e4be5a2c2288b4261545  ubuntu_24.04.3.tar.gz

git clone https://github.com/markbirss/ubuntu_24.04.3.git
cd ubuntu_24.04.3
rm -fr .git
7z x ubuntu_24.04.3.7z.001
sha256sum ubuntu_24.04.3.tar.gz

rm -f ubuntu_24.04.3.7z.*

mv ubuntu_24.04.3.tar.gz ../
cd ../
mkdir ubuntu
mv ubuntu_24.04.3.tar.gz ubuntu

#./build.sh lunch
# sudo ./build.sh
# sudo ./rkflash.sh update
```

### 安装 wifi 驱动

```bash
adb shell "cd /home/lyra/aic800/ && make install; reboot"
```

### 更换 ubuntu镜像源

```
root@luckfox:~# cat /etc/apt/sources.list.d/ubuntu.sources

#Types: deb
#URIs: http://ports.ubuntu.com/ubuntu-ports/
#Suites: noble noble-updates noble-security noble-backports
#Components: main restricted universe multiverse
#Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
Types: deb
URIs: https://mirrors.ustc.edu.cn/ubuntu-ports
Suites: noble noble-updates noble-backports
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: https://mirrors.ustc.edu.cn/ubuntu-ports
Suites: noble-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

## 安装 xfce 桌面

```bash
apt -y update; apt install -y --no-install-recommends xserver-xorg-input-all xserver-xorg-core xinit xfce4-terminal xserver-xorg-video-fbdev x11-utils;

apt -y install --no-install-recommends xfce4 dbus-x11 mesa-utils xubuntu-default-settings xfce4-goodies lightdm-gtk-greeter lightdm-gtk-greeter-settings;
```

### 开机自动加载屏幕驱动

```
mkdir -p /lib/modules/$(uname -r)/kernel/drivers/gpu/drm/tiny/
cp /root/panel-mipi-dbi.ko /lib/modules/$(uname -r)/kernel/drivers/gpu/drm/tiny/

depmod -a

echo panel-mipi-dbi > /etc/modules-load.d/panel-mipi-dbi.conf
```

### 开机自动登录指定用户

```
/etc/systemd/system/getty@tty1.service.d/autologin.conf

[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin root --noclear %I $TERM
```

### 开机自动启动 xfce

```
~/.bash_profile

if [[ -z $DISPLAY ]] && [[ $(tty) = /dev/tty1 ]]; then
    startx
fi
```

```
~/.xinitrc

startxfce4
```

### 可选的 x11vnc

板子执行

```bash
apt install -y x11vnc

x11vnc
```

PC端安装 tigerVNC [https://sourceforge.net/projects/tigervnc/files/stable/1.16.2](https://sourceforge.net/projects/tigervnc/files/stable/1.16.2)
打开软件后，在 VNC 服务器输入框内填入：板子IP:5900，举个例子：`192.168.50.67:5900`
然后点击连接