# U-Boot 启动机制：`boot.scr` + `ubuntuEnv.txt` + dtb overlay

> 本文回答**"U-Boot 拿到的那套东西是怎么拼起来的"**：`boot.scr`（编译自 `boot.cmd`）、
> `ubuntuEnv.txt`（`bootargs` / `overlays=` / `fdtfile`）、以及 dtb overlay 在什么时候合并。
> 启动链路全景见 [rk3588-boot-flow.md](rk3588-boot-flow.md)、文件来源见
> [rk3588-boot-flow-files.md](rk3588-boot-flow-files.md)；
> ⚠️ 正文**原样搬自 [lubancat-5-v2-hardware.md](lubancat-5-v2-hardware.md) §12**（节号保留）。

## TL;DR

- 启动分三段：**`boot.scr`**（U-Boot 脚本，编译自 `boot.cmd`）→ 读 **`ubuntuEnv.txt`** 里的
  `bootargs`/`overlays=`/`fdtfile` → 载 `vmlinuz`/`initrd.img` 并**在 U-Boot 里合并 dtb overlay**。
- ⛔ `ubuntuEnv.txt` 是**启动分区上的文本文件**（可以改完重启生效）；U-Boot 的**环境变量本身不持久化**
  （`CONFIG_ENV_IS_NOWHERE=y`）⇒ `fw_setenv` 改不动它（见 `AGENTS.md` §6.3）。


## 12. U-Boot 启动机制：boot.scr + ubuntuEnv.txt + dtb overlay

**FAT 启动分区（`/boot/firmware`，LABEL=`system-boot`，16MiB–528MiB）** 里放：

| 文件 | 来源/说明 |
| --- | --- |
| `boot.scr` | 由 `boot.cmd` 用 `mkimage` 编译的 U-Boot 脚本（`image` 阶段生成）|
| `ubuntuEnv.txt` | 明文环境文件：`bootargs` / `fdtfile` / `overlay_prefix` + **board 的 `config/uEnv/<board>.uEnv` 全文追加** |
| `vmlinuz` / `initrd.img` | 内核与 initrd（`board` 阶段从 `/boot/` 拷入）|
| `dtbs/rockchip/*.dtb` | 基础设备树（含 `rk3588-lubancat-5-v2.dtb`，来自 `/usr/lib/linux-image-*/`）|
| `dtbs/rockchip/overlay/*.dtbo` | 全部 dtb overlay（内核编译产物）|

**U-Boot 执行流程（boot.scr 逻辑，`image` 阶段 `boot.cmd` 模板）：**

1. `load /ubuntuEnv.txt` → `env import -t`（把 bootargs、fdtfile、overlay_prefix、overlays 读入环境）
2. `load ${fdtfile}`（默认 `rk3588-lubancat-5-v2.dtb`）→ `fdt addr` + `fdt resize`
3. 遍历 `overlays=` 列表，每个名字依次尝试
   `overlay_prefix-<名>.dtbo` → `<名>.dtbo`，找到就 `fdt apply`；
   **任一失败则 `overlay_error=true`，重载原始 DTB 放弃所有 overlay**（容错）
4. `booti vmlinuz initrd.img <应用过 overlay 的 FDT>`

**默认启用的 overlay**（`config/uEnv/lubancat-5-v2.uEnv` 第 1 行，随构建追加进 ubuntuEnv.txt）：

```
overlays=rk3588-lubancat-5-hdmi0-overlay rk3588-lubancat-5-hdmi1-vp2-overlay
```

即出厂默认 **DP0 + HDMI0 + HDMI1 三路输出**。uEnv 里其余都是注释掉的可用项
（40pin I2C/SPI/UART/PWM/CAN、各 VP 的显示插件、6 路摄像头 imx415/ov8858/gc 系列、扩展板）。

**如何改 overlay：**

- **构建前**：编辑 `config/uEnv/<board>.uEnv`，改 `overlays=` 行（改的是「下次烧录进 ubuntuEnv.txt 的内容」）
- **设备上（不改镜像）**：直接编辑 `/boot/firmware/ubuntuEnv.txt`，重启即生效
  （U-Boot 每次启动都重新读它；只改 overlay **无需重编译 boot.scr**，仅当要改
  `boot.cmd` 里的启动逻辑时才需 `mkimage` 重编）
- 关掉某路显示 = 把对应 overlay 名从 `overlays=` 行去掉；要动 DSI 屏幕/HDMI 输入等再按
  uEnv 注释里的清单加名字即可
- **自定义 dtbo（推荐）**：把 `.dtbo` 放进工程 `overlay/boot/firmware/dtbs/rockchip/overlay/`，
  构建时自动 ①拷进镜像 `/boot/firmware/dtbs/rockchip/overlay/` ②基名追加到
  `ubuntuEnv.txt` 的 `overlays=`（排在 uEnv 基础项之后，**无需手工改 uEnv**）。
  例如已放 `rk3588-lubancat-5-v2-usbcon-host.dtbo` → 生成的 overlays= 为：
  `rk3588-lubancat-5-hdmi0-overlay rk3588-lubancat-5-hdmi1-vp2-overlay rk3588-lubancat-5-v2-usbcon-host`。
  用法详见该目录下 `README.md`。

---
