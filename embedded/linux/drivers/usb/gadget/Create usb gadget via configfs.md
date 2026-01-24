在现代 Linux（内核 3.10+，强烈推荐 4.4+ / 5.x / 6.x）上实现 **自定义 Vendor 类（class code = 0xFF）** 的 USB Gadget 设备，最推荐的方式是使用 **configfs**（而非已经过时的 gadgetfs 或老的 f_uvc/f_mass_storage 那种写法）。

下面是目前（2025–2026）最常用的完整流程，以创建一个带 **1个 Bulk IN + 1个 Bulk OUT** 的简单 Vendor 设备为例。

### 0. 前置条件检查

运行以下命令确认你的硬件支持：

```bash
# 确认有 UDC 控制器（大多数支持 OTG 的板子都有）
ls /sys/class/udc/
# 常见输出示例：dwc2 dwc3  ci_hdrc.0  musb-hdrc.0  等

# 确认 configfs 已挂载（一般默认已挂载）
mount | grep configfs
# 如果没有，手动挂载：
sudo mkdir -p /sys/kernel/config
sudo mount -t configfs none /sys/kernel/config
```

### 1. 最简 Vendor 类 Gadget 示例（推荐写成脚本）

创建一个 shell 脚本（例如 create_vendor_gadget.sh），内容如下：

```bash
#!/usr/bin/env bash

set -e

# ====================== 基本信息（可修改） ======================
GADGET_NAME="g1"
UDC_CONTROLLER=$(ls /sys/class/udc/ | head -n1)  # 自动取第一个控制器

VID="0x1d6b"    # Linux Foundation（测试用）
PID="0x0104"    # 随便一个没被占用的（可改成你自己的）
SERIAL="12345678"
MANUF="Wooden Gadget Lab"
PRODUCT="Custom Vendor Device 2025"

# ====================== 开始创建 ======================
modprobe libcomposite    # 确保模块加载

# 清空旧的（防止冲突）
[ -d /sys/kernel/config/usb_gadget/$GADGET_NAME ] && rm -rf /sys/kernel/config/usb_gadget/$GADGET_NAME

mkdir -p /sys/kernel/config/usb_gadget/$GADGET_NAME
cd /sys/kernel/config/usb_gadget/$GADGET_NAME

echo 0x0200 > bcdUSB          # USB 2.0
echo 0x0100 > bcdDevice

echo "$VID"  > idVendor
echo "$PID"  > idProduct

# 字符串描述符
mkdir -p strings/0x409
echo "$SERIAL"   > strings/0x409/serialnumber
echo "$MANUF"    > strings/0x409/manufacturer
echo "$PRODUCT"  > strings/0x409/product

# ====================== 配置（一个配置通常就够） ======================
mkdir -p configs/c.1
echo 500   > configs/c.1/MaxPower

# 把字符串链接到配置（Windows 上会比较友好）
ln -s strings/0x409 configs/c.1/strings/0x409

# ====================== 函数（Function） → Vendor 类型 ======================
# 创建一个 vendor-specific function（最重要部分）
mkdir -p functions/vendor.0

# 关键：告诉内核这是 vendor 类
echo 0xff > functions/vendor.0/bInterfaceClass
echo 0x00 > functions/vendor.0/bInterfaceSubClass   # 可自定义
echo 0x00 > functions/vendor.0/bInterfaceProtocol   # 可自定义

# 可选：设置 endpoint 数量和类型（默认就是 bulk in/out）
# 如果不设置，通常内核会给你 1 IN + 1 OUT bulk endpoint

# 把函数链接到配置
ln -s functions/vendor.0 configs/c.1/

# ====================== 绑定到 UDC 控制器 ======================
echo "$UDC_CONTROLLER" > UDC

echo "Vendor Gadget 已启动！"
echo "VID:PID = $VID:$PID"
lsusb | grep "$VID:$PID" || echo "lsusb 没看到？检查 dmesg"
```

赋予执行权限并运行：

```bash
chmod +x create_vendor_gadget.sh
sudo ./create_vendor_gadget.sh
```

### 2. 验证是否成功

```bash
# 在 Linux 主机上看
lsusb -v -d 1d6b:0104

# 应该看到类似：
bInterfaceClass       255 Vendor Specific Class
bInterfaceSubClass      0 
bInterfaceProtocol      0 
```