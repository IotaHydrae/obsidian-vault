要将设备的**控制台**（kernel boot messages + login prompt + shell）重定向到 /dev/ttyGS0（即 USB gadget serial 提供的串口），这样你就可以从 PC 上直接通过串口工具（如 PuTTY、screen、minicom）访问设备的完整控制台（包括 early boot log），最可靠的方式有两种：

- **方式1**：只把 login shell（getty）放到 ttyGS0 上（最常用、最稳定，推荐先用这个）
- **方式2**：把 kernel console 也重定向到 ttyGS0（可以看到完整的 boot log，但有 timing 问题风险）

下面针对 Raspberry Pi Zero / Pi Zero 2W / 类似开发板（最常见场景）说明步骤，其他板子（如 Rockchip、Allwinner）类似，但 cmdline.txt 位置可能不同。

### 方式1：启用 [getty@ttyGS0.service](mailto:getty@ttyGS0.service)（推荐，绝大多数教程用这个）

这个方式**不改 kernel console**，只在系统启动后把登录提示放到 USB 串口上。boot log 还是走 HDMI/原串口，但登录后就能从 PC 操作 shell。

1. 加载 g_serial 模块（如果还没）：

```bash
sudo modprobe g_serial
```

2. 启用并启动 getty 服务（systemd 会自动在 /dev/ttyGS0 上跑 login prompt）：

```bash
sudo systemctl enable getty@ttyGS0.service
sudo systemctl start getty@ttyGS0.service
```

- enable 让它开机自启
- start 立即生效（测试用）

