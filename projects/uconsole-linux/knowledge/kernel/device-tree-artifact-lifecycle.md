# 设备树从源码到运行状态的验证链

> DTS、构建出的 DTB、固件选中的文件与 Linux 的运行设备树是不同对象，必须逐层核对。

## TL;DR

- 固定基础 DTB、overlay 顺序与内核版本，再检查固件配置和运行属性。
- `fdtoverlay` 成功只说明该次合并可执行，不包含固件参数 fixup 或真实硬件验证。
- 更新时先准备并验证完整启动文件，失败保留旧件和恢复材料。
- 单个 rename 原子不等于内核、DTB 和 initrd 多文件更新在断电时原子。

## 四个对象

| 对象 | 核对内容 |
| --- | --- |
| 源码 | binding、节点、父总线 cells、供应者依赖、目标内核 pin |
| 编译件 | dtc/binding 检查、输入摘要、overlay 合并顺序 |
| 启动引用 | config.txt/os_prefix、实际 DTB/overlay 文件、固件参数与 map |
| 运行树 | /sys/firmware/fdt 或 /proc/device-tree、驱动绑定、probe 日志 |

同名文件可以来自不同构建；比较哈希与报告，不只看目录路径。
设备 status 为 okay 不代表时钟、电源、reset 和 pinctrl 均正确；
驱动绑定成功也不代表全部数据路径满足功能要求。

## 离线验证

在副本上合并，检查必要属性和结构，再发布候选文件；输出不能覆盖唯一基础 DTB。
路径限制为项目拥有的输入与临时输出，拒绝目录逃逸和无关文件。
自动升级还需核对自定义 overlay 保留、同名文件采用哪一版本，以及包配置和 initramfs hook 的顺序。

Pi 内核后端（工程源码：`scripts/kernel_backends/raspberry_pi.py`）将选中启动文件放入私有固件目录，
镜像检查器（工程源码：`tools/image_inspect.py`）比较板型配置、DTB/overlay 与报告摘要。
它不模拟 Raspberry Pi 固件的完整 fixup。

### 本次 CM5IO 工具版本观察（2026-10-28）

同一组构建出的 CM5IO DTB、`vc4-kms-v3d-pi5.dtbo`、`uart0-pi5.dtbo`，
宿主 DTC 1.7.0 的 `fdtoverlay` 返回 `FDT_ERR_NOTFOUND`；固定内核构建目录内的
DTC `1.7.2-g52f07dcc` 工具完成合并，输出中 `serial0` 指向 RP1 UART0，状态为 `okay`。
因此离线合并失败时应同时记录工具版本，并用目标内核配套工具对照，
不能直接断定板型 DTS 缺节点。本次未进一步隔离两个工具版本的具体差异，
也未据此修改设备树；合并结果仍不证明 GPIO 串口已在实机工作。
本地证据在忽略的 `image/work/cm5-io-board-20261027/cm5io-kms-uart0.{dtb,json}`。

## 更新失败与恢复

准备阶段在同文件系统临时目录完成复制、合并、权限与摘要检查后再修改现役文件。
多文件发布需要有明确旧件位置和逆序恢复策略；恢复失败保留事务目录，不能清理唯一副本。
SIGKILL、断电与持续存储故障可能绕过 trap，必须保留独立可恢复启动路径。
这些是未来工具的设计条件，不表示本仓库实现了通用多文件更新事务。

## 板型隔离

`cm5-io-board` 只使用官方 CM5IO DTB、标准 KMS 与 GPIO UART overlay。
uConsole 的 PMIC/DSI overlay 只属于对应载板，不能因同为 CM5 就共用。
现有移植依据见 [CM4](cm4-kernel-port.md)、[CM5](cm5-kernel-port.md)，
驱动检查见 审查技能（工程源码：`skills/engineering-embedded-linux-driver-engineer/SKILL.md`）。
