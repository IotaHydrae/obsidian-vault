# CM4 适配 Raspberry Pi Linux 6.18

> CM4 共用面板/背光源码，但需独立的 BCM2711 接线、配置和 kernel8.img。

## TL;DR

- 使用 bcm2711_defconfig 与 CM4 overlay，不套用 RP1/CM5 设备树。
- 保留 GPIO/DSI/PMIC 接线；补丁由项目 series 管理，不修改固定上游子模块。
- 缺少 VCI 消费者不证明物理缺电，显示恢复和根因边界见启动排查。

## 适配方法

1. 从固定 Raspberry Pi Linux 提交和 `bcm2711_defconfig` 开始，按板型声明选取项目补丁。
2. 先核对 BCM2711 的 GPIO、I2C、DSI 和供电连接，再复用共享面板/背光驱动。
3. 编译基础 DTB 与 overlay，以 `fdtoverlay` 检查组合；随后分别验证内核包、镜像和实机。
4. 固件输入使用 CM4 的 `kernel8.img`、匹配 initramfs 和 overlay 映射文件。
   不能把 CM5 的 RP1 控制器节点或 `kernel_2712.img` 当作 CM4 输入。

## 接线与源码依据

- 基线：Raspberry Pi Linux `234188315037848e5b07d65b85d38edc64095caa`。
- 参考：`ak-rex/rpi-linux` 的 `874351b52621220e99c9a2d3849219fcce39c37f`，文件为 `arch/arm/boot/dts/overlays/clockworkpi-uconsole-cm4-overlay.dts`。
- 保留参考接线中的 DSI1、面板复位 GPIO8、背光 GPIO9、AXP221 地址 0x34/IRQ GPIO2、常开的 3.3 V 电源，以及接在 I2C1 GPIO44/45 上的 ADC101C。
- 为 PMIC 使用独立的 GPIO0/1 I2C 总线，不沿用参考实现的旧 BSC 驱动和别名覆盖。使用基线的 AXP221 ADC 与电源驱动，省略直接复制的电池容量和电流限制。
- 省略参考实现可选的 SPI/UART 和 `/chosen/bootargs` 覆盖。这些设置尚未被证实是显示或供电的前提，也不能替换镜像导出的根分区命令行。扩展接口需要单独进行板级验证。
- 参考 CM4 面板仅声明 `iovcc-supply`；缺少 VCI 时，regulator 框架提供虚拟电源。这本身不能证明面板缺电：官方原理图将 `DISP_1V8` 接到模块的 1.8 V 电源网络。单独试验过与正常 CM5 一致的 DCDC3/VCI 配置，仍未恢复 ID 读取或可见画面，详见启动排查记录。
- 在 BCM2711 上，固件的 `overlay_map.dtb` 将 `vc4-kms-v3d` 映射到 `vc4-kms-v3d-pi4`，`audremap` 则保持为 `audremap`。内核包需要包含该映射文件和固件前缀目录中的 README。

## 验证证据与边界

**离线验证：** CM4 选择的补丁可以应用到固定源码，无需应用 CM5 overlay 补丁。使用 fdtoverlay，可将编译后的 CM4 overlay、vc4-kms-v3d-pi4、audremap 和 dwc2 应用到编译后的 bcm2711-rpi-cm4 DTB。该检查不执行固件参数处理。历史完整内核、软件包及镜像结果记录在[开发状态](../../project/status.md)；输入变化后仍须重新校验当前产物。

**早期设备观察：** 工程内核在微雪底板和 uConsole CM4 上均能启动并使用 Wi-Fi/SSH。
uConsole overlay 可以绑定 PMIC，USB 供电、无电池时也枚举 USB HID。早期 DSI 试验存在
超时和黑屏；这些观察只描述该轮配置，不能作为当前桌面仍然黑屏的结论。

**后续观察的入口：** 显示恢复与仍未知的根因见[启动排查](../boot/cm4-uconsole-boot-debug.md)；
可见桌面、轨迹球/终端快捷键及扬声器等后续结果见[桌面记录](../desktop/labwc-shell-comparison.md)
和[开发状态](../../project/status.md)。HID 枚举不能替代按键事件测试，电量遥测不能替代充放电验证。
完整键盘、耳机/录音、充电精度、挂起与关机按各自证据维护，不能从启动成功推广。
