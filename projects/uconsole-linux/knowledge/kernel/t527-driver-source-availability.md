# T527：内核驱动源码、预编译模块与用户态库的区别

## TL;DR

- 所查 Avaota／WalnutPi 公开 BSP 中，NPU 和 VIN／ISP 内核部分都有实现源码与构建规则，不能概括为“只提供 `.ko`”。
- 确认存在的二进制依赖包括 NPU VIPLite 用户态运行库、ISP 算法／管理库，以及部分视频／GPU用户态交付；wrapper公开不等于这些底层实现公开。
- 核查只覆盖固定源码、构建规则和实际软件包；没有编译验证，也不从源码可见推导6.18已可用或全部许可已审查。

## 核查对象与判据

2026-11-01，核对 AvaotaSBC/linux 独立BSP main／A1集成5.15分支、AvaotaOS，
以及 walnutpi/linux 5.15.147、walnutpi-build、软件包索引／发布包和官方wrapper仓库。
固定提交、目录覆盖、文件／包摘要和检查证据见 [审计目录](../../reference/t527-driver-audit/README.md)
及 [sources.json](../../reference/t527-driver-audit/sources.json)。

`.ko`是编译产物，`obj-m`生成它是正常行为。是否“只有ko”要看组成对象有无对应源码、
是否引用缺失实现的预编译ELF/archive，以及镜像／包是否从其他渠道带入模块。
代码中叫galcore、g2d、isp或“GPU HAL”都不足以单独判定硬件用途／选中状态。
本次区分：源码可见、源码＋二进制对象、用户态二进制、固件、尚未确认；不混用“预编译”和“闭源”。

## 内核侧实际证据

| 组件 | 关键源码／构建入口 | 本次判断 |
| --- | --- | --- |
| NPU VIPLite | `bsp/drivers/npu/aw_nna_vip/`，Kbuild列`gc_vip_kernel`、MMU／command／allocator／Allwinner platform等 | 从源码组成`vipcore.ko`，不是仅提供现成ko |
| NPU GALCORE | `bsp/drivers/npu/aw_nna_galcore/`，HAL／MMU／command／memory／Linux platform实现 | 构建`galcore.ko`，不是仅有wrapper加现成模块 |
| VIN／ISP | `bsp/drivers/vin/Makefile`、`vin-isp/sunxi_isp.c`及isp500／520／600等寄存器配置 | ISP内核逻辑与CSI／VIN源码可见；`vin_io`／`vin_v4l2`等是输出模块 |
| 视频引擎VE | `bsp/drivers/ve/cedar-ve/`，`cedar_ve.c`／`flush_cache.S`等 | ARM侧内核接口有源码；不等于codec用户态全部公开 |
| GPU | Avaota的DRM/Panfrost源码；Walnut的Mali内核目录与Kbuild | 不能归为只有ko；主线Panfrost路线与厂商Mali用户态分开 |

Avaota独立BSP的NPU和VIN对象清单做了静态源码对应核对，未发现这些对象必须由无源码`.o`提供。
这是构建规则／树内文件的对应关系，不是已编译证明，也不保证生成头文件、工具链和全部依赖齐备。
Walnut配置显式选择`CONFIG_AW_NNA_VIP=m`、`CONFIG_AW_NNA_GALCORE=m`及VIN模块。
Avaota NPU Kconfig有`default m`，不能因defconfig未列变量就说NPU关闭；最终选项应以生成的`.config`为准。
两个NPU实现存在不表示实际运行时都加载或同一个用户库可随意互换。

## NPU：源码边界在运行库／模型工具这一层

实际下载并静态检查 `walnutpi.npu_1.1.2_all.deb`，包内没有`.ko`。
官方 `walnutpi.npu`公开 Python、`py_awnn_lib.c`、`awnn_lib.c`、`awnn_quantize.c`及头文件。
Makefile从这些文件生成`libawnn_t527.so`，同时链接已提供的两个AArch64 ELF库：

- `libVIPlite.so`：149,464字节，依赖`libVIPuser.so`，导出`vip_init`、建网络／运行／buffer接口。
- `libVIPuser.so`：38,776字节，包含`/dev/vipcore`设备路径，承担对应用户态接口。

这两个库的实现源码没有出现在已核对的wrapper仓库中；wrapper源码不等于VIPLite／VIPuser完整源码。
官方模型转换仓库使用Pegasus／Vivante IDE等工具，不是完整神经网络编译器源码。
结论限定为“此交付提供二进制、在核查来源中未找到对应实现”，不宣称所有公开渠道永远不存在源码。

## ISP：内核采集驱动与3A算法分开

实际 `libisp-t527_1.0.0_all.deb`提供：

- `libAWIspApi.so` → 动态依赖`libisp.so` → `libisp_ini.so`；三者为AArch64 ELF。
- `libisp_algo.a`、`libisp_dev.a`、`libisp_ini.a`、`libisp_math.a`、`libiniparser.a`。
- API头文件和`vin.conf`，后者请求`vin_io`、`vin_v4l2`、sensor等模块，但包内本身没有ko。

`libisp_algo.a`包含AE／AWB／AF、tone mapping等对象；`.c.o`成员名仍是已编译对象，不是C文件。
`libisp.so`保留debug信息不等于有对应完整源代码，也不建立复制／重用许可。
`walnutpi.isp`公开Python、`py_isp.c`和`AWIspApi.c`，但后者依赖的ISP管理／设备／events实现
没有在该小仓库形成完整源码闭包；准备包目录也没有提供算法库的对应完整实现。
因此“VIN/ISP内核源码可见”和“ISP算法／调参运行库以二进制交付”可以同时成立。

## BSP中确实存在的部分源码／二进制混合路径

不能从NPU／VIN有源进一步推导“BSP没有二进制算法”。Avaota树内还核对出下列无后缀ar库：

| 路径／资产 | 实际内容与构建关系 | 对当前T527的范围 |
| --- | --- | --- |
| `bsp/drivers/vfe/lib/libisp_32`／`libisp_64` | ARM／AArch64预编译ISP算法对象；VFE Makefile复制成`.a`再链接模块 | 老VFE路径；当前A1／Walnut T527选择VIN而非VFE |
| VFE `lib_mipicsi2_v1`／`v2` | ARM对象归档，部分旧SoC条件使用 | 不据此判T527当前CSI必须用二进制 |
| `libhdmi_sun50iw1`／`libhdmi_sun8iw11` | 旧DISP2 HDMI路径有实际预编译对象与链接规则 | 特定旧SoC；A1关闭DISP2，不能概括为T527 HDMI只有ko |
| rt-media `libmath` | ARM对象归档，有模块链接路径 | 默认关闭；数学库预编译也不单独证明其上游无源 |

这些属于“源码包装／控制代码＋预编译对象”的真实例子，而不是最终ko文件缺失源的同一个分类。
Avaota NPU／VIN／VE／GPU子树在独立main与5.15集成分支Git身份相同，
不以无关分支存在的源码冒充当前集成路径。完整可编译性仍待实际构建。

## 其他二进制与误判

AvaotaOS准备目录内存在Mesa／EGL／GBM、FFmpeg、GStreamer与CedarX类ELF。
Mesa／glibc／FFmpeg等有公开上游，预装二进制不能直接被称作无源码闭源驱动；
具体供应方补丁／版本／完整重建输入仍要核对。`libvideoengine.so`等运行库又需独立审查。
存在于准备目录也不证明每张镜像都会安装，必须继续查实际hook／包选择。

Walnut BSP另发现无扩展名的`codec/lib/libve`，是含三个ELF32 RISC-V对象的ar归档，
不是AArch64 `.ko`。这说明只按后缀扫描不够；其特定架构／codec链接路径不能扩大成
“T527 ARM64内核驱动都缺源码”，是否影响目标配置须沿条件构建核对。
ramfs中的glibc等通用库、Wi-Fi固件和boot固件分别分类，不能充作只有ko的驱动数量。

## 对6.18迁移的意义

1. NPU／ISP内核侧具备源码适配基础，可针对6.18 API、DMA／IOMMU／memory、DT和PM改造，
   不需要仅因旧镜像里有ko就从逆向内核驱动开始。
2. NPU／ISP用户库仍可能绑定旧驱动的ioctl、结构布局、mmap／buffer分配与固件协议。
   有源码的内核层可尝试保持兼容ABI，但必须查依赖和实际验证，不能承诺旧so一定可直接用。
3. ISP画质、3A和传感器tuning不是CSI能出帧就自动完整；NPU执行网络也不代表模型转换链已开源。
4. 本工程先实现[uConsole基本系统与6.18路线](t527-linux-6-18-migration.md)，NPU／ISP作为独立功能验收。

## 证据与未确认项

NPU／ISP三个包和额外GPU包只下载、列出、解包和读ELF/ar；未安装／运行库或维护脚本。
文件长度／SHA-256与固定HTTPS Git源码中的包索引一致，未验证APT Release签名；这是文件身份核对，不是新发布信任方案。
包的`Architecture: all`不能作跨架构保证，因为包内实际有AArch64 ELF。
Debian12的Walnut包表选择这些库，所查Ubuntu24包表未选择，不声称所有镜像预装。

未确认：到货CM2实际加载模块／运行库版本、全部厂商包的源代码发布情况、用户库完整许可／重建输入，
以及这些驱动在6.18的编译与功能结果。源码可见性、许可、可编译性和实机工作四项分别判断。
