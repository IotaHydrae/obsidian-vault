# 板载串口与控制台

> 板载调试串口为 UART2（`ttyFIQ0`），1500000 8N1；Type-C 固定 host，不提供 USB ACM 登录口。

## TL;DR

- UART2 使用 fiq-debugger，节点 `ttyFIQ0`、波特率 1500000、earlycon 地址 `0xfeb50000`。
- 本机 USB 转串口通常为 `/dev/ttyUSB0`，它连接板载 UART，与板子作为 USB device 的 ttyGS 无关。
- 镜像保留板载串口登录；USB gadget getty 规则和 drop-in 已移除。

节点与硬件连接见 [lubancat-5-v2-hardware.md](../../hardware/lubancat-5-v2-hardware.md)，
本机命令执行与后台采集见 [serial-exec.md](../../hardware/serial-exec.md)。

`plymouth.ignore-serial-consoles` 将图形启动画面与串口隔离。
journal 完整持久保存，串口仅跟随本次启动 warning 以上日志；
PID 1 状态输出独立控制，见 [journal-serial.md](../journal-serial.md)。
