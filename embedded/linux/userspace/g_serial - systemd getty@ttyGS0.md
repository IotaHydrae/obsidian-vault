# 通过 USB Gadget 串口登录 Linux

> 先建立 gadget 串口，再在 `ttyGS0` 启动 getty，主机即可通过 USB 串口登录；这不保证能看到早期启动日志。

## 前提

设备需要支持设备模式的 UDC、正确的 USB 连接及 gadget serial 驱动。若已使用 configfs 组合 gadget，应配置其中的串口 function，避免再加载占用同一 UDC 的 legacy gadget。

## 使用 g_serial

```bash
sudo modprobe g_serial
ls -l /dev/ttyGS0
sudo systemctl enable --now serial-getty@ttyGS0.service
sudo systemctl status serial-getty@ttyGS0.service
```

使用目标发行版提供的串口 getty 模板；某些系统也可通过 `getty@ttyGS0.service` 配置，具体参数由模板决定。在主机上确认新出现的串口设备，以串口工具连接并回车检查登录提示。示例参数为 115200，但 USB 虚拟串口与物理 UART 的波特率含义不同。

需要开机加载模块时，在 `/etc/modules-load.d/g_serial.conf` 中写入 `g_serial`，再验证 UDC、驱动和 getty 的启动顺序。

## 日志与登录的区别

getty 只提供系统启动后的登录终端。内核 console 另有驱动和配置要求，且依赖 USB 枚举与主机连接；不能将 `ttyGS0` 当作从上电开始就可用的 early console。需要可靠的早期日志时使用目标平台的硬件串口或其他早期调试通道。
