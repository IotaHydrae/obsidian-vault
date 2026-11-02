# 设备配置、上位机依赖与镜像体检项

> 本文收录从 README 移出的三部分说明：设备配置 `~board/.lsp/config/device.yaml`、
> 上位机依赖 `APP_PACKAGES` / `APP_PIP_PACKAGES`（含 Quest USB 访问规则与 pip 模块坑），
> 以及 `tools/verify-image.sh` 的检查项明细。README 只保留命令与用法。

## TL;DR

- 本文三部分：`~board/.lsp/config/device.yaml`（设备配置）、上位机依赖（`APP_PACKAGES` / `APP_PIP_PACKAGES`）、
  `tools/verify-image.sh` 的检查项明细。
- ⚠️ `device.yaml` 由**上位机（kiosk 用户）写、采集侧（board）读** ⇒ 靠**组权限 + setgid** 打通（§1）。
- 加 pip 依赖要**同时改两处**（§2），否则会在另一条路径上缺包。

## 1. 设备配置 `~board/.lsp/config/device.yaml`
上位机（工厂模式）读写的那份设备配置，源文件放在 `overlay/home/board/.lsp/config/device.yaml`，
构建时安装到镜像内的 `/home/board/.lsp/config/device.yaml`：

- **属主/权限**：`board:board`；文件 `0664`，目录 `~/.lsp` 与 `~/.lsp/config` 都是 `0775`（**组内可写**）。
  这样后续创建的工厂模式账号只要加入 `board` 组即可改写，不需要 sudo：
  ```bash
  sudo useradd -m -s /bin/bash -G board factory    # 注意：不要加到 sudo 组
  ```
- **目录也必须组内可写**：上位机通常用「写临时文件 + `rename`」原子替换，只给文件写权限
  而目录不可写会直接失败（`~/.lsp`、`~/.lsp/config` 都是 `0775` 就是为这个）。
- ⚠️ **git 不保存 `0664`/`0775` 这类权限位**（只区分 644 与 755），所以不要指望本地 `chmod`
  能带进镜像 —— 构建脚本用 `install -m 0664` + `chmod 0775` 显式设置，改了内容直接提交即可。
- 校验：`./tools/verify-image.sh`（「设备配置」组会检查存在性、非空、文件/目录组内可写、
  属主与家目录一致）。板子上临时改权限：

  ```bash
  sudo chown -R board:board ~board/.lsp && sudo chmod 775 ~board/.lsp ~board/.lsp/config && sudo chmod 664 ~board/.lsp/config/device.yaml
  ```

## 2. 上位机依赖（`APP_PACKAGES` / `APP_PIP_PACKAGES`）
设备端上位机（工厂模式）需要的库与工具，构建时随镜像装好，不用上机后再补：

| 类别 | 内容 |
| --- | --- |
| apt —— `APP_PACKAGES` | `python3-colcon-common-extensions`（来自 ROS2 源）、`cmake`、`tmux`、`adb`、`esptool`、`python3-bleak`、`python3-scipy`、`python3-opencv`、`python3-yaml`、`python3-dbus`、`python3-gi`、`python3-requests`、`python3-evdev`（上位机 `keyboard_trigger_node` 靠它读键盘事件，缺了会 `ModuleNotFoundError` 直接死掉，2026-09-12 实测）。Quest USB 访问规则见下节 |
| pip —— `APP_PIP_PACKAGES` | `pure-python-adb`、`bce-python-sdk`（Ubuntu 仓库没有）、`ruamel.yaml`（改 yaml 用；Ubuntu 仓库里是 0.17.21，而 0.18 起 API 破坏性变更、上位机按新版写的，所以走 pip 与设备上 `pip3 install ruamel.yaml` 一致）；默认走**华为云**镜像（`APP_PIP_INDEX_URL`，2026-09-12 板上实测比清华/中科大快且稳，见 [apt-sources-deb822.md](../build/apt-sources-deb822.md) §5.2），用 `--break-system-packages` 装到系统目录，失败重试 3 次 |
| 本来就在基础 / ROS2 清单里 | `ros-jazzy-ros-base`、`python3-numpy`、`python3-serial`、`python3-paho-mqtt`、`ffmpeg`（厂商 deb，同时提供 `ffprobe`）、`v4l-utils`（`v4l2-ctl`）、GStreamer 插件（`plugins-base/good/bad`、`tools`、`rockchip1`）、网络调试工具（`net-tools`、`iputils-ping`、`netcat-openbsd`、`tcpdump`）|

```bash
# 覆盖清单（例如只要运行时用的，不要 cmake/colcon）
sudo APP_PACKAGES="tmux adb esptool python3-bleak python3-scipy python3-opencv" \
     ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=rootfs --force
sudo APP_PIP_PACKAGES="" ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=rootfs --force   # 跳过 pip 项
```

> **ROS2 环境**：`rclpy` 等装在 `/opt/ros/jazzy` 下，**不 `source` 就无法 import**（实测
> `python3 -c "import rclpy"` 在未 source 的 shell 里会失败）。构建会写好
> `/etc/profile.d/ros2.sh`（登录 shell）并追加到 `~board/.bashrc`（交互 shell）；
> 非登录/非交互场景（systemd 服务、GUI 启动器）需要自己 `source /opt/ros/jazzy/setup.bash`。
> 另外 `ffmpeg` 是厂商 deb，**包名不叫 `ffmpeg`**，判断装没装请看命令 `command -v ffmpeg`。
> （`/etc/profile.d/ros2.sh` 的 bash 守卫问题见 [profile-d-ros-guard.md](tuning/profile-d-ros-guard.md)。）

### 板子当 USB 主机连接 Quest

本仓库只维护 `overlay/etc/udev/rules.d/99-meta-quest.rules`：匹配 Oculus VID `2833`，
将对应 USB 设备节点设为 `MODE="0666", GROUP="plugdev"`，供非 root 账户使用 adb。
构建脚本仍将 `board` 和 `kiosk` 加入 `plugdev` 组；该规则本身允许其他账户访问，
不能将组设置理解为仅允许组成员访问。

`android-sdk-platform-tools-common` 不在默认依赖清单中：已核对的 noble 包 `28.0.2+9`
带 `/lib/udev/rules.d/51-android.rules`，仅删除 overlay 同名文件会让包规则重新生效。
`adb` 包 `34.0.4-1build3` 只将它列为 Recommends；构建默认 `--no-install-recommends`，
并在 `rootfs` 阶段重新配置 rootfs 时清理旧包及 `/etc/udev/rules.d/51-android.rules`。

这不保证所有 Android 厂商设备可用。非 Quest 设备按需核对 VID、节点权限及系统现有规则，
再添加针对该设备的规则；不要对全部 `/dev/bus/usb/*/*` 批量放宽权限。

### `pure-python-adb` 的模块名坑
顶层模块**不是** `adb` —— 0.2 起改名为 **`ppadb`**
（PyPI 上 latest 是 `0.3.0.dev0`，实测包内目录就是 `ppadb/`），只有 0.1.x 才叫 `adb`。
上游 README 里写的 `from adb.client import ...` 是过时内容，别照抄。

我们的上位机用的就是 **`ppadb`**，所以默认的 `APP_PIP_PACKAGES="pure-python-adb ..."`
**不需要锁版本**：
```python
from ppadb.client import Client as AdbClient     # 0.2+（我们的用法）
```
只有那些还写着 `import adb` 的老代码才需要锁旧版本：
```bash
sudo APP_PIP_PACKAGES="pure-python-adb==0.1.7.dev0 bce-python-sdk" ./scripts/build-lsp-image.sh ...
```
注意 `adb` **CLI**（apt 包 `adb`）和这个 Python 库是两回事，`verify-image.sh` 也是分开检查的。

## 3. `tools/verify-image.sh` 检查项明细
把"我们改了什么"逐条断言出来，避免刷完机器靠肉眼试。命令用法与退出码见 README；
检查项（分组输出 ✓ 通过 / ✗ 失败 / ! 警告 / - 跳过）：

| 组 | 内容 |
| --- | --- |
| 品牌与登录 | hostname、`/etc/hosts`、GDM 自动登录用户/会话、AccountsService |
| 欢迎向导 | 向导 autostart 已删、user service 指向 `/dev/null`、`.config/gnome-initial-setup-done` 已预置 |
| 登录 motd | `00-header`/`30-sysinfo` 存在、Ubuntu 默认片段已删、静态 `/etc/motd` 已清空、`00-header` 不再打印 `Welcome to` |
| 设备配置 | `~board/.lsp/config/device.yaml` 存在非空、文件 0664 / 目录 0775 组内可写、属主与家目录一致 |
| 登录失败延迟（PAM） | `pam.d` 下无 `pam_faildelay.so`、`common-auth` 的 `pam_unix` 带 `nodelay`、`pam-configs/unix` 档案同步带 `nodelay` |
| ROS 2 环境与 USB 主机工具 | `/etc/profile.d/ros2.sh` 带 Bash 守卫；`lsp-usb` 提供主机查询与重新枚举；`usbdevice.service` 被 mask，旧 gadget/getty/shared-network/MTP/HID 配置已清理；固定 ADB 主机密钥与 Quest 规则保留 |
| 永不熄屏/不锁屏/不休眠 | `idle-delay=0`、`lock-enabled`、`idle-activation-enabled`、`idle-dim`、`sleep-inactive-*-timeout`；gschema override + 系统级 dconf + `dconf profile` + 已编译 db |
| 更新弹窗 | 4 个 autostart 的 `Hidden=true`、17 个 system 单元 mask、8 个 user 单元屏蔽、APT `Periodic` 置 0、motd/release-upgrade 提示已关 |
| 构建期加速痕迹 | `/etc/ld.so.preload` 无 eatmydata、eatmydata 未安装（绝不能进镜像）|
| 移除项 | LibreOffice（`libreoffice*`/`ure`/`uno-libs3`/`python3-uno`）未安装、`cloud-init`/`landscape-common`/`cryptsetup-initramfs`/`flash-kernel` 未安装（`snapd`/`packagekit`/`gnome-software` 只做信息提示，保护网跳过属正常）|
| 上位机依赖 | Debian 包（colcon/cmake/tmux/adb/esptool/bleak/scipy/opencv…）、命令可用、Python 模块（含 `ppadb`）、ROS 2 环境脚本、`99-meta-quest.rules` 的 VID `2833` 与节点权限 + `board` 在 `plugdev` 组；通用 Android 规则及其提供包未安装 |
| swap 与扩容 | 镜像里**不预置** swapfile（首启由 `lsp-swapfile` 脚本/单元创建并启用，`fstab` 里**没有** swap 行）、`vm.swappiness=10`、`lsp-growroot` 脚本/单元/已 enable（live 还会看 swap 是否真启用 + 根分区是否已扩到全盘）|
| U-Boot / ATF | `idbloader.img`/`u-boot.itb` 存在、itb 里是否真有 `atf` 节点（`dumpimage -l` 的结构化判断）；live + root 还会读 `/dev/mmcblkX` 8MB 处**实际写入**的 bootloader，并核对 `ubuntuEnv.txt` 的 `fdtfile`/`overlays=` 对应的 dtb、dtbo 是否都在启动分区 |

- 退出码：有 FAIL 返回 1，可直接用于脚本/CI。
- 离线模式下 gsettings 用 `GSETTINGS_BACKEND=memory` 读**编译期默认值**（不碰宿主机的 dconf，避免误判）；
  live 模式读的是**生效值**。
- 脚本里的期望值与单元清单同 `scripts/build-lsp-image.sh`，两处都带注释标明，改一处要同步另一处。
- 若某项 FAIL，按提示处理：桌面类改动重跑 `--stage=rootfs`；bootloader 相关看 `--stage=image`。
