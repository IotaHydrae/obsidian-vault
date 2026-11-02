# initramfs：重建、查看与板级实测记录

> 本文回答**"改完 initrd 相关配置后怎么让它生效、怎么查看里面有什么"**：
> `update-initramfs` 的两种用法、查看工具（`lsinitramfs`/`unmkinitramfs`）、
> 两条观察（`rtl_bt` 空转、`COMPRESS=gzip`），以及 2026-09-26 的板级实测记录。
> ⚠️ 最重要的一条前提：**initrd 是按生成那一刻的配置做的** —— 改了
> `MODULES=dep` 这类 conf **必须重建**才生效（板上现役那份仍是 183 个模块的原因，见
> [initramfs.md](initramfs.md) §3.2.1 的复核注）。
> initramfs 里有什么、怎么生成见 [initramfs.md](initramfs.md)（§1~§3）。
> ⚠️ 正文**原样搬自该篇的 §4**（节号保留）。

## TL;DR

- 改了 `/etc/initramfs-tools/**`（尤其 `conf.d/10-lsp-modules.conf` 的 `MODULES=`）后：
  `sudo update-initramfs -u` 重建当前内核的那份；`-k all` 全部内核。
- 看里面有什么：`lsinitramfs /boot/initrd.img-$(uname -r)`（列内容）、
  `unmkinitramfs <initrd> <目录>`（解开）。
- 现役 initrd 的实际画像与"重建后应该是什么样"见 §4.1。


## 4. 重建、查看与两条结论

```bash
# 重新生成（板上；会顺带重跑钩子：重拷 vmlinuz/initrd + 重建 dtbs/ 并保住自定义 dtbo）
sudo update-initramfs -u -k "$(uname -r)"

# 只看目录清单 / 解包细看
lsinitramfs -l /boot/firmware/initrd.img | grep -E 'rtl_bt|arm/mali|fsck|gconv'   # -l 带权限/大小
unmkinitramfs /boot/firmware/initrd.img /tmp/initrd     # 解到 /tmp/initrd/{early,main}

# 镜像里那份怎么查：`board` 阶段之后在 build/rootfs/boot/firmware/initrd.img
# （`rootfs` 阶段的 build/rootfs 里没有它 —— initrd 是 `board` 阶段装内核时才生成的）

# 验收断言：tools/verify-image.sh 的「initramfs 内容（固件 / fsck）」组（板上/离线都能跑）
./tools/verify-image.sh -q | sed -n '/initramfs 内容/,/汇总/p'
```

- ⚠️ **`update-initramfs` 会重建 `/boot/firmware/dtbs/`**：自定义 dtbo 只存在启动分区，
  靠 `zz-update-firmware` 在覆盖后放回去。忘记这条的典型后果：PD 的 `try-power-role=source` 失效、
  "dtbo 明明在仓库里却不生效"。见 `notes/hardware/atf-missing-troubleshooting.md`。
- ⚠️ **`zz-update-firmware` 在构建期也会被执行**：`update-initramfs` 生成完 initrd 会
  `run-parts /etc/initramfs/post-update.d/`（`/usr/sbin/update-initramfs:159-161`），而镜像是在
  宿主机的 chroot 里建的（`/proc` 是 bind mount）。此时 `findmnt -o SOURCE /` 给的是**构建机**的根设备，
  脚本会去 `mount /dev/<构建机根分区>1 <chroot>/boot/firmware` 再 `rm -rf dtbs/`。现在有两道闸：
  脚本开头的 `systemd-detect-virt --quiet --chroot`，以及 2026-09-26 加的硬判据
  （`/` 与 `/proc/1/root/` 的 `%d:%i` 不同 ⇒ 在 chroot 里 ⇒ `exit 0`）。⛔ 两道都别删。
- **结论一（2026-09-26）：`rtl-bt` 钩子是错的，已删。** 板上实测（修复前）initrd 里
  `usr/lib/firmware/rtl_bt` 是个 **18344 B 的文件**（= glob 里第一个 `rtl8192ee_fw.bin.zst`），
  一份可用的 BT 固件都没有 —— 因为 `copy_exec` 的第二个参数只有在 `${DESTDIR}` 里**已存在**
  时才会被当目录（`hook-functions` 的 `copy_file()`）。同一个用法错误也出现在 `mali-firmware` 上
  （它只是碰巧遇到了已存在的目录，落成扁平化副本）。两个 hook 都删了；固件有两条自动通路
  （built-in 用 `add_builtin_firmware`、模块用 `dracut-install --firmwaredirs`），不需要自定义 hook。
  2026-09-16 那条"是个空目录 / 不影响功能"的观察到此为止 —— 现在连那个垃圾文件也不会再生成。
- **结论二：蓝牙固件进不进 initrd，取决于驱动在不在 initrd 里。** 本镜像 `MODULES=most` →
  `auto_add_modules` 只装写死的那批类别（base/hw_random/net/scsi/block/mmc/usb_storage/fb…），
  蓝牙不在其中 ⇒ `btrtl.ko` 不在 initrd ⇒ 这个阶段不会加载 BT、也就不会请求固件；
  BT 初始化发生在切到 rootfs 之后（那时 `/usr/lib/firmware/rtl_bt/` 是完整的 46 个文件）。
- `COMPRESS=gzip` 下 initrd 19 MB；若哪天要压体积，可以先量 `zstd` 的启动期解压开销再决定
  （改 `conf.d/compression.conf` 即可，但**别在没量过的前提下改** —— 这是最早被读进内存的文件之一）。

### 4.1 板级实测记录（2026-09-26，开发板 `<HOST_07>`，内核 6.1.172）

按新 hook 集合重生成了一次 initrd（`sudo update-initramfs -u -k $(uname -r)`，14 s，rc=0），
`lsinitramfs` 路径级 diff（3 删 3 增，增的 3 条是 Sep 16 以来 rootfs 自己的漂移）：

```
< usr/lib/firmware/rtl_bt                       ← 那个 18344 B 的垃圾文件，没了
< usr/lib/firmware/arm/mali/mali_csffw.bin.zst  ← 扁平化的无用副本，没了
< usr/lib/firmware                              ← 少一条重复的目录条目
> etc/ld.so.conf.d/fakeroot-aarch64-linux-gnu.conf      ┐
> usr/lib/udev/rules.d/60-pico-usb-display.rules        │ rootfs 9/16 之后的正常漂移
> usr/lib/udev/rules.d/80-wch-link.rules                ┘
```

保留项逐条确认：`arm/mali/{arch10.8,arch10.10}/mali_csffw.bin.zst`、`usr/sbin/fsck.vfat`、
`gconv/IBM850.so`、`scripts/functions`、`scripts/local-bottom/fsck-boot-firmware` 都在；
`scripts/local-bottom/ORDER` 内容 = `/scripts/local-bottom/fsck-boot-firmware "$@"` + param.conf 那行。
启动分区也验证了：`/boot/firmware/{initrd.img,vmlinuz}` 时间戳更新、自定义
`rk3588-lubancat-5-v2-usbcon-host.dtbo` 的 md5 前后一致（`69343e2b…`）、`ubuntuEnv.txt` 的
`overlays=` 没变 ⇒ `zz-update-firmware` 真的跑了（新加的 chroot 硬守卫**没有**误判真机）。

⚠️ **踩坑（这次实测抓到的）**：一开始把 `local-bottom` 开头那段 `PREREQ`/`prereqs()` 当成
"hook 才用的死代码"删了，结果构建日志立刻冒出
`.../fsck-boot-firmware: 7: .: cannot open /scripts/functions: No such file`
—— `mkinitramfs` 是靠 **执行 `"$script" prereqs`** 问依赖的（`hook-functions: get_prereq_pairs()`，
结果给 tsort 生成 `ORDER`），发行版自带的 `scripts/init-top/*`、`scripts/local-premount/*`
全都是这个写法。已改回来。**所以 boot script 的 `prereqs()` 千万别删。**

同一块板上接着做了三件事，结果一并记在这里（2026-09-26）：

1. **补装 `wireless-regdb`**（这块板的镜像是 9/16 之前刷的，没有这个包；deb 从构建机
   `apt-get download` 拿的，与工程机同版本 `2026.05.30-0ubuntu1~24.04.1`）：
   `update-initramfs` 的 `W: Possible missing firmware` 从 **8 条降到 6 条**
   （消失的两条就是 cfg80211 的 `regulatory.db{,.p7s}`；剩下 6 条是内建 DVB 调谐器，本板没硬件），
   新 initrd 里出现 `usr/lib/firmware/regulatory.db` + `.p7s`。
2. **删掉 `hooks/usb_modeswitch`**：initrd 里 `usr/sbin/usb_modeswitch` 与两份
   `40-usb_modeswitch.rules` 随之消失（运行期那份 `/etc/udev/rules.d/40-usb_modeswitch.rules` 不受影响）。
3. **重启验证**（`sudo reboot`）：
   - `boot_id` 变了、`uptime` 归零、`systemctl --failed` **空**；
   - 跑着的 initrd 就是新生成那份：`md5sum /boot/firmware/initrd.img` == `/boot/initrd.img-$(uname -r)`；
   - `dmesg`：`cfg80211: Loading compiled-in X.509 certificates for regulatory database`，
     没有 `Failed to load regulatory.db`；
   - **`/run/initramfs/` 里有 `fsck-boot-firmware` 戳**，`fsck.log` 里能看到：
     ```
     Log of fsck -C -a -T -t ext4 /dev/mmcblk0p2
     writable: clean, 489032/3810016 files, ...
     Log of fsck -C -a -T -t vfat /dev/mmcblk0p1
     /dev/mmcblk0p1: 341 files, 15725/130811 clusters
     ```
     ⇒ `local-bottom/fsck-boot-firmware` 真的在启动时跑了，而且 FAT 检查成功；
   - 自定义 dtbo 仍然生效：`/proc/device-tree/i2c@fea90000/fusb302@22/connector/try-power-role`
     = **`source`**（工程机同样 `source`）⇒ `zz-update-firmware` 重建 `dtbs/` 后把 overlay
     放回去这件事是好的，新加的 chroot 硬守卫也没在真机误判。

> ⚠️ 板上的备份（`/tmp/ir-backup/*.old`）重启后随 `/tmp` 消失；要回退旧 hook 用 `git show HEAD:<路径>`
> （这批改动还没提交，HEAD 里仍是老文件）。

---
