# 可移动介质：**不自动挂载**（但用户仍可手动挂载）

> 2026-09-12 实施并实测。适用于本镜像（Ubuntu 24.04 + GNOME）。
> 需求原话："镜像要禁用自动 mount u盘，读卡器，tf卡的功能，严格用 /etc/fstab 来确定挂载点。"
> 随后用户补充：**"不用过于严格，用户在桌面上还是可以手动挂载可移动设备的，只是系统不要自动挂载就好了。"**
> —— 所以最终只做"不自动挂载"，**不拦手动挂载**。

## TL;DR

- 策略：**不自动挂载**可移动介质（但用户仍可手动挂载）。
- ⛔ **不要再加回"更严格的两层"**：§2 解释了为什么（会把数据卡 / U 盘一起挡住）。
- 改造前的行为有实测证据（§1）；查清但没用上的事实见 §3；想长期挂一块 USB 硬盘的做法见 §4。
- 验证方法见 §5。

## 0. 结论

| 场景 | 行为 |
| --- | --- |
| 插 U 盘 / 读卡器 / SD・TF 卡 | **不自动挂载**、不自动打开文件管理器 |
| 用户在 GNOME「文件」里点它，或 `udisksctl mount` | **可以挂载**（保持原样，不做任何限制） |
| `/etc/fstab` 里写了的设备 | 开机由 systemd 挂载（与 udisks2 无关） |
| eMMC / NVMe / 桌面其它功能 | 不受影响 |

实现只有两处"设置默认值"（都是 overlay 整目录拷贝，构建脚本无需额外安装）：

| 文件 | 内容 |
| --- | --- |
| `overlay/etc/dconf/db/local.d/02-removable-media` | `[org/gnome/desktop/media-handling] automount=false`、`automount-open=false`（系统级 dconf，对所有用户生效） |
| `overlay/usr/share/glib-2.0/schemas/99-lsp-desktop.gschema.override` | 同样两条，作为"编译期默认值"的第二层 |

## 1. 改造前的行为（实测证据）

GNOME 会话里 `gvfs-daemons` + `udisks2` 都在，插卡后是 udisks2 自动挂载：

```
$ sudo journalctl -b -u udisks2 | tail -3
Sep 12 13:35:41 example-project udisksd[733]: Mounted /dev/mmcblk1p1 at /media/board/Data1T on behalf of uid 1000
Sep 12 13:36:22 example-project udisksd[733]: Mounted /dev/sda1 at /media/board/Data1T on behalf of uid 1000
Sep 12 13:57:13 example-project udisksd[733]: Mounted /dev/mmcblk1p1 at /media/board/Data1T on behalf of uid 1000

$ sudo -u board gsettings get org.gnome.desktop.media-handling automount
true          ← GNOME 默认值，就是它让插卡即挂载
```

挂载点是 udisks2 按卷标生成的 `/media/<用户>/<LABEL>`，**与 fstab 无关** —— 这正是要去掉的。
把它设成 `false` 之后，插入介质不再有任何自动动作；用户手动挂载的路径完全不变
（GNOME 的一键挂载走 polkit 的 `allow_active`，本策略没有动它）。

## 2. 为什么不做更"严格"的两层（重要，别再往回加）

> ⚠️ 这两层我们都实现并实测过，随后**按用户要求撤掉**了。结论写在这里，避免以后有人再"顺手加上"。
> 判据：**verify-image.sh 里那两条 `absent` 检查**就是防回归用的。

### 2.1 udev `UDISKS_IGNORE=1` —— 会让设备从桌面消失，用户就没法手动挂载

```
# 曾经写过（已删除）：overlay/etc/udev/rules.d/99-lsp-removable-media-ignore.rules
SUBSYSTEM=="block", SUBSYSTEMS=="usb", ENV{UDISKS_IGNORE}="1"
SUBSYSTEM=="block", ENV{ID_DRIVE_FLASH_SD}=="1", ENV{UDISKS_IGNORE}="1"
```

实测效果：设备属性变成 `UDISKS_IGNORE=1`，udisks2 把它标成 `HintIgnore: true`：

```
      HintAuto:                   true
      HintIgnore:                 true
```

而 gvfs 的卷监视器会读这个属性（`strings /usr/libexec/gvfs-udisks2-volume-monitor` 里有
`udisks_block_get_hint_ignore`）→ 设备**不出现在 GNOME「文件」里** →
用户没有"一键挂载"的入口了，与"桌面上仍可手动挂载"直接冲突。

⚠️ 顺带纠正一个常见误解：**`UDISKS_IGNORE` 并不阻止挂载**。装上它以后 root 照样能挂：

```
$ sudo udisksctl mount -b /dev/mmcblk1p1
Mounted /dev/mmcblk1p1 at /media/root/Data1T
```

所以它既达不到"禁止挂载"的目的，又会毁掉手动挂载的入口 —— 里外不是，已删除。

### 2.2 polkit 拒绝 `filesystem-mount*` —— 会把手动挂载一起禁掉

```
// 曾经写过（已删除）：overlay/etc/polkit-1/rules.d/49-lsp-no-udisks-mount.rules
polkit.addRule(function(action, subject) {
    if (subject.uid === 0) return polkit.Result.NOT_HANDLED;
    var denied = ["org.freedesktop.udisks2.filesystem-mount", ... "loop-setup", ...];
    if (denied.indexOf(action.id) >= 0) return polkit.Result.NO;
    return polkit.Result.NOT_HANDLED;
});
```

实测（拿活动会话里的 gnome-shell 进程当 subject）：

```
$ pkcheck --action-id org.freedesktop.udisks2.filesystem-mount --process 1073
（装规则前：静默通过 = 已授权）
$ pkcheck --action-id org.freedesktop.udisks2.filesystem-mount --process 1073
Not authorized.                      ← 装规则后
$ sudo -u board udisksctl mount -b /dev/mmcblk1p1
Error mounting /dev/mmcblk1p1: GDBus.Error:org.freedesktop.udisks2.UnixProcess: Not authorized to perform operation
```

这正是用户不要的：**普通用户在桌面上再也挂不上任何东西**。已删除，polkit 恢复发行版默认
（活动会话 `allow_active=yes`：用户点一下就能挂载，不弹密码）。

## 3. 查清但没用上的事实（留给以后）

- **判 SD 卡不能用 `/sys/block/*/removable`**：内核不给 mmc 块设备设 `GENHD_FL_REMOVABLE`
  （`drivers/mmc/core/block.c` 里有注释："Since MMC block devices clearly fall under the second
  case, we do not set GENHD_FL_REMOVABLE"），板上实测 `/sys/block/mmcblk1/removable` 与 eMMC
  一样都是 **0**。要用 udev 属性：

  | 判据 | SD / TF 卡 | eMMC |
  | --- | --- | --- |
  | `ID_DRIVE_FLASH_SD` / `ID_DRIVE_FLASH_MMC` | `ID_DRIVE_FLASH_SD=1` | `ID_DRIVE_FLASH_MMC=1` |
  | `/sys/block/<dev>/device/type` | `SD` | `MMC` |
  | `/sys/block/<dev>/removable` | **0**（不能用） | 0 |

- 换到 USB3 读卡器后设备是 `/dev/sda`（`SUBSYSTEMS=="usb"`），当前其
  `udevadm info -q property` 里没有 `ID_BUS`（所以当时规则用的是 `SUBSYSTEMS=="usb"` 而不是 `ID_BUS`）。

## 4. 想长期用一个 USB 硬盘怎么办

写 `/etc/fstab`，**用 `UUID=` 或 `LABEL=`**（别用 `/dev/sdX`，插拔顺序会变）：

```
UUID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx  /mnt/data  ext4  defaults,nofail  0  2
```

`nofail` 让设备不在时也能正常开机。挂载点自己定（建议 `/mnt/<名字>`）。
`/etc/fstab` 里的设备由 systemd 挂载，与"不自动挂载"策略无关，也不受 udisks2 影响。

## 5. 怎么验证

```bash
# 离线（构建产物）
bash tools/verify-image.sh --root build/rootfs --no-color    # 看「可移动介质：不自动挂载」一组

# 板上（live）
gsettings get org.gnome.desktop.media-handling automount      # 期望 false
gsettings get org.gnome.desktop.media-handling automount-open # 期望 false
# 手动挂载应当**仍然可用**：
udisksctl mount -b /dev/mmcblk1p1                             # 普通用户（活动会话）应能挂上
# 插入介质后不应出现自动挂载点：
grep -E '^/dev/(sd|mmcblk1)' /proc/mounts                     # 期望为空（除非你刚手动挂过）
```

`tools/verify-image.sh` 的检查项：dconf 键文件里有 `automount=false`、gschema override 里也有、
`gsettings` 读到的实际值是 `false`（live 读会话值 / 离线读编译期默认值），
以及**不存在** udev `UDISKS_IGNORE` 规则与 polkit 拒绝规则（防回归）。

## 6. 相关文档

- tuning/（镜像源码：`notes/system/tuning`） —— 其它系统级定制（时间同步、swap、USB 自动挂起、DNS、永不休眠等）
- [sd-card-performance.md](../storage/sd-card-performance.md) §9 —— 同一张卡的 USB3 读卡器实测
- [usb-gadget.md](../usb/usb-gadget.md) —— 固定 USB host/source 的查询与外设重新枚举
