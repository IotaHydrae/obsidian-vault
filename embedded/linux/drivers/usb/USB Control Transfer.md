一次完整的 USB Control Transfer（控制传输）通常包含 最多 3 个阶段（stages），这是 USB 2.0 / USB 3.x 规范中明确定义的结构。

### 标准三个阶段

| 阶段名称         | 英文名称         | 是否可选 | 方向（从主机视角）          | 主要内容                          | 典型数据量          | PID 序列特点                  |
| ------------ | ------------ | ---- | ------------------ | ----------------------------- | -------------- | ------------------------- |
| Setup Stage  | Setup Stage  | 必选   | OUT → 设备           | 主机发送 8 字节的 Setup Packet（请求命令） | 固定 8 字节        | SETUP token + DATA0 + ACK |
| Data Stage   | Data Stage   | 可选   | IN ← 设备 或 OUT → 设备 | 实际传输请求的数据（读/写）                | 0 ~ wLength 字节 | 从 DATA1 开始，DATA0/1 交替     |
| Status Stage | Status Stage | 必选   | 与 Data 阶段相反        | 报告整个控制传输的最终成功/失败状态            | 固定 0 字节（空包）    | ZLP（零长度包） + DATA1         |
### 根据 wLength 和 bmRequestType 的四种典型组合

|控制传输类型|bmRequestType 数据方向|wLength 值|Setup|Data Stage|Status Stage|俗称|典型例子|
|---|---|---|---|---|---|---|---|
|No Data|无数据|0|有|无|OUT（空包）|无数据控制|Set Configuration, Set Address|
|Control Write|主机 → 设备 (Host-to-Device)|> 0|有|OUT（多事务）|IN（空包）|写控制|Set Descriptor, Set Feature|
|Control Read|设备 → 主机 (Device-to-Host)|> 0|有|IN（多事务）|OUT（空包）|读控制|Get Descriptor, Get Status|
|（特殊情况）|—|0 但有方向|有|无|与方向相反的空包|—|极少见，规范不允许 wLength=0 但标方向|

### 每个阶段内部包含的包（packet）级别流程（以最常见情况为例）

- **Setup Stage**（永远是 OUT 方向）
    - SETUP Token Packet (PID = SETUP)
    - DATA0 Packet（8 字节 Setup 结构：bmRequestType, bRequest, wValue, wIndex, wLength）
    - ACK Handshake（设备必须应答）
- **Data Stage**（如果 wLength > 0）
    - 第一个数据事务永远从 **DATA1** 开始
    - 后续事务 DATA0 ↔ DATA1 交替（toggle 机制）
    - 每个事务：Token (IN/OUT) → DATAx Packet → ACK/NAK/STALL/NYET
    - 必须正好传输 wLength 字节（不能多也不能少）
- **Status Stage**（方向与 Data Stage 相反，或无 Data 时为 OUT）
    - Token Packet（IN 或 OUT，与 Data 相反）
    - DATA1 Packet（**零长度**，即 ZLP — Zero Length Packet）
    - 对方应答 ACK（成功） / STALL（失败/不支持）

### 快速记忆口诀

- **Setup 永远先来**（8 字节，告诉我要干嘛）
- **Data 看 wLength**（>0 才有，方向看 bmRequestType bit 7）
- **Status 永远最后**（空包，方向相反，代表“干完了，成不？”）

### 常见实际例子

- **Get Descriptor**（设备描述符） → Control Read → Setup + Data(IN) + Status(OUT 空包)
- **Set Address** → No Data → Setup + Status(OUT 空包)
- **Set Configuration** → Control Write → Setup + Data(OUT) + Status(IN 空包)
