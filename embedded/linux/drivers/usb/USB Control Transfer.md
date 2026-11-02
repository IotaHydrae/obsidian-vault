# USB 控制传输的阶段与方向

> 控制传输包含必选 Setup、可选 Data 和必选 Status；无数据阶段的请求，其 Status 方向为设备到主机。

| 请求类型 | Setup | Data | Status |
| --- | --- | --- | --- |
| Control Read | 主机发送 8 字节 setup | IN，设备到主机 | OUT，零长度 |
| Control Write | 主机发送 8 字节 setup | OUT，主机到设备 | IN，零长度 |
| No Data | 主机发送 8 字节 setup | 无，`wLength = 0` | IN，零长度 |

Setup 包包含 `bmRequestType`、`bRequest`、`wValue`、`wIndex`、`wLength`；有数据阶段时，`bmRequestType` 的方向位决定数据方向。`GET_DESCRIPTOR` 是常见读请求，`SET_ADDRESS` 和 `SET_CONFIGURATION` 是常见无数据请求。

对于 USB 2.0，数据阶段的事务从 DATA1 开始并按协议切换；实际可因短包提前结束，不能笼统断言每次都恰好传输 `wLength` 字节。不同速度和协议版本的包级细节应查对应规范。主机侧 `rusb` 示例见 [[Rust USB control transfer 例子]]。
