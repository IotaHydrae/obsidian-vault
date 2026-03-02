USB 设备的**枚举（Enumeration）过程**，是指当你把 USB 设备插入主机后，主机识别设备类型、分配地址、读取描述符并加载驱动的完整流程。

下面按时间顺序详细讲解整个过程（以 USB 2.0 为例，USB 3.x 原理类似但信号层更复杂）。

---

# 一、物理层检测阶段

### 1️⃣ 设备插入

当 USB 设备插入主机：

- 设备通过 **上拉电阻** 表明自己的速度类型
    
    - D+ 上拉 → **Full-Speed (12Mbps)**
        
    - D- 上拉 → **Low-Speed (1.5Mbps)**
        
    - USB 2.0 High-Speed 先以 FS 方式枚举，再切换
        

主机检测到线路电平变化 → 触发端口中断。

---

### 2️⃣ 端口复位（Reset）

主机：

- 对端口发送 **至少 10ms 的 SE0 信号**
    
- 设备收到 Reset 后：
    
    - 地址恢复为 0
        
    - 清除内部状态
        
    - 准备接受控制传输
        

此时设备进入：

```
Default State
```

---

# 二、默认地址阶段（Address 0）

USB 规定：

> 所有刚上电的设备地址都是 0

主机通过 **Endpoint 0（控制端点）** 与设备通信。

---

### 3️⃣ 读取设备描述符（Device Descriptor）

主机发送：

```
GET_DESCRIPTOR(Device)
```

第一次只读取前 8 字节，用来获取：

- bMaxPacketSize0（EP0最大包大小）
    

然后主机重新按正确包长再读完整 18 字节。

---

# 三、分配地址

### 4️⃣ SET_ADDRESS

主机发送：

```
SET_ADDRESS(new_address)
```

设备收到后：

- ACK 当前传输
    
- 状态阶段完成后才真正切换地址
    

此时设备进入：

```
Address State
```

之后所有通信都使用新地址。

---

# 四、读取完整描述符树

主机继续通过控制传输读取：

### 5️⃣ 设备描述符（Device Descriptor）

包含：

- VID（厂商ID）
    
- PID（产品ID）
    
- USB版本
    
- 设备类别
    

---

### 6️⃣ 配置描述符（Configuration Descriptor）

主机会先读 9 字节头部，然后根据 wTotalLength 再读完整配置。

配置里包含：

- Interface Descriptor
    
- Endpoint Descriptor
    
- Class-specific Descriptor
    

---

### 7️⃣ 字符串描述符（可选）

主机会读取：

- Manufacturer
    
- Product
    
- Serial Number
    

---

# 五、驱动匹配

操作系统根据：

- VID/PID
    
- Class Code
    
- Subclass
    
- Protocol
    

查找匹配驱动。

例如：

|设备类型|匹配方式|
|---|---|
|HID 键盘|Class 03|
|U盘|Mass Storage|
|自定义设备|VID/PID|

Linux 下可以看到：

```bash
dmesg
lsusb -v
```

---

# 六、设置配置

### 8️⃣ SET_CONFIGURATION

主机发送：

```
SET_CONFIGURATION(config_value)
```

设备进入：

```
Configured State
```

此时：

- 非0端点被激活
    
- 设备开始正常工作
    

---

# 整个枚举流程总结图

```
插入设备
   ↓
端口检测
   ↓
Reset
   ↓
Address = 0
   ↓
GET_DESCRIPTOR(Device 8 bytes)
   ↓
GET_DESCRIPTOR(Device full)
   ↓
SET_ADDRESS
   ↓
GET_DESCRIPTOR(Configuration)
   ↓
加载驱动
   ↓
SET_CONFIGURATION
   ↓
设备工作
```

---

# 七、USB 状态机

USB 设备有 4 个核心状态：

|状态|含义|
|---|---|
|Attached|物理连接|
|Powered|获得电源|
|Default|Reset 后，地址 0|
|Addressed|已分配地址|
|Configured|已配置|

---

# 八、抓包分析工具

如果你在做驱动或协议分析（你之前有做 Linux DRM 和底层开发，这里可能用得上）：

### Linux

- usbmon
    
- Wireshark + usbmon
    

### Windows

- USBPcap + Wireshark
    

---

# 九、典型枚举失败原因

1. EP0 最大包大小写错
    
2. 描述符长度错误
    
3. wTotalLength 填错
    
4. 未正确处理 SET_ADDRESS 时序
    
5. High-Speed 切换握手失败
    
6. 未正确响应标准请求
    

---

# 十、补充：USB 3.0 差异

USB 3.x 在物理层：

- 使用独立 SuperSpeed 差分对
    
- 有独立的 Link Training
    
- 枚举流程逻辑仍类似（控制端点 0）
    

---

如果你愿意，我可以：

- 给你画一个 **时序级 USB 枚举图**
    
- 或者结合 Linux Gadget / UDC 讲一次设备侧实现
    
- 或者讲 Host Controller (EHCI/XHCI) 视角的枚举过程
    
- 或者分析一个真实 usbmon 抓包
    

你想从 **设备实现角度** 还是 **主机控制器角度** 深入？