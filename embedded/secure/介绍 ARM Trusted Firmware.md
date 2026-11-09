# ARM Trusted Firmware（TF-A）详解

ARM Trusted Firmware-A（TF-A） 是 ARM 为 Cortex-A 系列处理器设计的一套开源安全启动与运行时固件框架。

它位于 BootROM、U-Boot、Linux 等软件之间，负责建立 ARM TrustZone 安全执行环境，并提供处理器启动、异常级别切换、电源管理、安全服务等底层功能。

如果你正在研究 RK3506、RK3588 这类 ARM SoC 的 Linux 启动流程，那么 TF-A 是非常值得理解的一层。

先建立一个核心认识：

> TF-A 不只是一个 Bootloader，而是一套分阶段运行的固件体系。它既参与 Linux 启动，也可能在 Linux 启动之后继续驻留，提供安全世界的运行时服务。

## 1. TF-A 在整个 ARM 启动链中的位置

先看一个典型的 ARMv8-A SoC 启动流程：

[Linux-arm(1)ATF启动流程_linux atf-CSDN博客](https://images.openai.com/static-rsc-4/VnlHiRYS1ulN7NlpIlCIfWLmAWORY0Jm25APhXSX-uqZnA4S6pPg7q8dVC31PpAjT87hNaVffsL_Ut0Jy6dDgkUVI0urzR7WRn5yxQXivvcyRmhYFR6ZnYNlDHayXgtfDlFSbrgprzCrWjtdzJXe9XeFL1rRfutwhQxhX3YNsek?purpose=inline)

[blog.csdn.net](https://blog.csdn.net/Once_day/article/details/145330809?utm_source=chatgpt.com)

```
         ARM SoC 上电
              │
              ▼
        BootROM（硬件固化）
              │
              ▼
       BL1（可选，TF-A）
              │
              ▼
       BL2（TF-A）
              │
       ┌──────┴─────────┐
       │                │
       ▼                ▼
  BL31（TF-A）       BL32（可选）
  EL3 Runtime       OP-TEE OS
       │                │
       └──────┬─────────┘
              │
              ▼
       BL33（U-Boot）
              │
              ▼
          Linux Kernel
              │
              ▼
           Rootfs
```

这里最重要的是理解 BL1、BL2、BL31、BL32、BL33 这些名称。

它们不是五个必须同时存在的独立程序，而是 ARM Trusted Firmware 体系中不同启动阶段或不同安全执行环境的固件映像。

## 2. BL1、BL2、BL31、BL32、BL33 分别做什么？

|阶段|运行级别|主要职责|
|---|---|---|
|BL1|EL3|最初的可信启动，基础 CPU 初始化|
|BL2|Secure EL1|加载、验证后续固件，初始化安全内存|
|BL31|EL3|常驻 Secure Monitor，提供运行时服务|
|BL32|Secure EL1|可选的 Trusted OS，例如 OP-TEE|
|BL33|Non-secure EL2/EL1|普通世界 Bootloader，例如 U-Boot|

这里有几个值得注意的地方。

### BL1：Trusted Bootstrap

BL1 是最早的 TF-A 启动阶段。

主要工作：

- 初始化必要的 CPU 架构状态。
    
- 设置异常向量。
    
- 判断冷启动还是热启动。
    
- 初始化必要的平台硬件。
    
- 加载并验证 BL2。
    
- 将控制权交给 BL2。
    

但并不是所有 SoC 都有 TF-A BL1。

例如，某些 SoC 的 BootROM 已经完成了 BL1 所需要的一部分工作，直接将控制权交给 TF-A BL2。

### BL2：Trusted Boot Firmware

BL2 可以理解为一个负责加载后续固件的可信 Bootloader。

它主要负责：

1. 初始化 DRAM。
    
2. 配置 TrustZone 内存隔离。
    
3. 加载 BL31。
    
4. 加载可选的 BL32（例如 OP-TEE）。
    
5. 加载 BL33（例如 U-Boot）。
    
6. 根据平台安全启动配置验证固件。
    

这里需要特别注意：

BL2 并不是简单地把几个二进制文件从 Flash 搬到 RAM。

它还负责建立后续安全执行环境所需的内存布局、加载参数以及安全边界。

### BL31：EL3 Runtime Firmware

这是我认为理解 TF-A 最重要的部分。

BL31 会一直驻留在 EL3，直到 Linux 启动之后仍然可能继续工作。

它主要负责：

- Secure Monitor。
    
- PSCI 电源管理接口。
    
- CPU Core 启动、关闭及休眠。
    
- Secure World 与 Normal World 之间的切换。
    
- 处理来自低异常级别的 SMC 调用。
    
- 管理部分安全中断及运行时服务。
    

例如，Linux 执行：

```
psci_cpu_on(cpu, entry_point);
```

底层会通过 SMC 指令请求 EL3 的 TF-A 执行 CPU 启动操作。

因此，Linux 并不需要直接操作所有 CPU 电源控制寄存器，而是通过 PSCI 标准接口请求固件完成。

这也是为什么 Linux 内核中有 PSCI 驱动，而 TF-A 中有 PSCI 实现。

官方设计文档对 BL31 的职责有详细说明：

GitHub+1

### BL32：Secure Payload

BL32 是可选的安全世界固件。

最常见的例子就是 OP-TEE。

它运行在 Secure World，负责处理需要隔离保护的应用和服务。

例如：

- 密钥管理。
    
- 安全存储。
    
- 加密和解密。
    
- 安全启动相关服务。
    
- DRM 相关安全操作。
    
- 安全认证。
    

注意，BL32 不一定就是 OP-TEE，也可以是其他 Trusted OS 或安全分区管理相关固件。

### BL33：Normal World Bootloader

BL33 就是普通世界的 Bootloader。

常见的就是：

- U-Boot
    
- EDK2 / UEFI
    
- 其他平台 Bootloader
    

它通常运行在 EL2 或 EL1，随后负责加载 Linux Kernel。

## 3. 为什么 BL31 这么特殊？理解 ARM Exception Level

要理解 TF-A，必须先理解 ARMv8-A 的 Exception Level。

ARMv8-A 提供了 EL0 到 EL3 四个异常级别。

[ARM64 Execution States - ElseWhere](https://images.openai.com/static-rsc-4/GvJHApr_Au9EX5QFtboR8HcsIgHs8h5RUw8IDa7x5wIPsQvk7FX4gUyj3QbmQd4rT77z12e4n1IVW90Z_xslSt17DiJXjh-QyFHi200oMqVVbunm9X1yWwzW07gWpeS7JndPgwTDUW6ZohlxU9mjoVVfxeY0lQ5wzBZLdQNGzp8?purpose=inline)

[duetorun.com](https://duetorun.com/blog/20230602/a64-execution-state/?utm_source=chatgpt.com)

|Exception Level|典型用途|
|---|---|
|EL0|普通用户程序|
|EL1|Linux Kernel|
|EL2|Hypervisor，例如 KVM|
|EL3|Secure Monitor / TF-A|

其中：

- EL0：用户空间。
    
- EL1：操作系统内核。
    
- EL2：虚拟化层。
    
- EL3：最高权限的固件执行层。
    

但是这里还有一个非常重要的概念：

Exception Level 和 Security State 是两个不同的维度。

ARM TrustZone 把执行环境分成：

- Secure World
    
- Normal World
    

例如：

```
              ARM Cortex-A
                   │
          ┌────────┴────────┐
          │                 │
      Secure World      Normal World
          │                 │
         EL3               EL2
          │                 │
         EL1               EL1
          │                 │
         EL0               EL0
          │                 │
        OP-TEE             Linux
```

这只是概念示意，并非所有 EL 都能在两个 Security State 中自由使用。

EL3 是 Secure Monitor 所在的最高异常级别，负责管理两个世界之间的切换。

例如：

```
Linux Kernel (EL1, Non-secure)
             │
             │ SMC instruction
             ▼
       TF-A BL31 (EL3)
             │
             │ 切换到 Secure World
             ▼
        OP-TEE (EL1, Secure)
             │
             │ 执行安全操作
             ▼
       TF-A BL31 (EL3)
             │
             │ 返回 Normal World
             ▼
        Linux Kernel
```

SMC（Secure Monitor Call）就是两个世界之间请求安全固件服务的重要入口。

## 4. TF-A 和 OP-TEE 是什么关系？

这是一个很容易混淆的地方。

它们不是竞争关系，而是协作关系。

## Secure World

OP-TEE（BL32）

Trusted OS · Secure EL1

安全应用、密钥管理、安全存储

Secure Monitor Call / 返回

TF-A（BL31）

Secure Monitor · EL3

世界切换、PSCI、电源管理、运行时服务

## Normal World

Linux Kernel

Non-secure EL1

普通操作系统

U-Boot（BL33）

Non-secure EL2 / EL1

加载 Linux

可以这样理解：

- TF-A BL31 是安全世界与普通世界之间的底层管理者。
    
- OP-TEE 是运行在安全世界中的操作系统。
    
- Linux 是运行在普通世界中的操作系统。
    

例如，Linux 用户空间的程序想使用安全存储：

```
Linux Application
       │
       ▼
OP-TEE Client API
       │
       ▼
Linux OP-TEE Driver
       │
       ▼
      SMC
       │
       ▼
TF-A BL31
       │
       ▼
OP-TEE BL32
       │
       ▼
Trusted Application
       │
       ▼
Secure Storage
```

这里 OP-TEE Client API、Linux 驱动和 OP-TEE OS 是不同层次的组件。

也就是说，没有 OP-TEE，TF-A 依然可以正常工作；没有 OP-TEE，BL31 依然可以提供 PSCI 服务。

## 5. TF-A 的几个重要技术

如果你以后准备阅读 TF-A 源码，建议重点关注下面几个部分。

### 5.1 PSCI

PSCI 全称 Power State Coordination Interface。

它是 ARM 定义的电源管理接口。

主要功能：

|接口|功能|
|---|---|
|`CPU_ON`|启动其他 CPU Core|
|`CPU_OFF`|关闭当前 CPU|
|`CPU_SUSPEND`|CPU 进入低功耗状态|
|`SYSTEM_RESET`|系统复位|
|`SYSTEM_OFF`|系统关机|

例如 Linux 启动 CPU1：

```
Linux Kernel
     │
     ▼
PSCI CPU_ON
     │
     ▼
SMC
     │
     ▼
TF-A BL31
     │
     ▼
SoC Power Controller
     │
     ▼
CPU1 Power ON
```

你可以把 PSCI 理解为 Linux 与底层固件之间的一套标准电源管理协议。

### 5.2 SMC

SMC 是 ARM 提供的一条 CPU 指令：

```
smc #0
```

它可以触发 Secure Monitor Call，让处理器进入 EL3 异常处理流程。

Linux 可以通过 SMC 请求：

- PSCI 服务。
    
- OP-TEE 服务。
    
- 平台特定的安全服务。
    

并不是所有 SMC 都直接由 BL31 自己处理，有些请求会由 BL31 分发给其他服务或安全世界。

### 5.3 FIP

FIP 全称 Firmware Image Package。

这是 TF-A 提供的一种固件打包格式。

它可以将多个固件映像打包在一起，例如：

```
fip.bin
  │
  ├── BL31
  ├── BL32
  ├── BL33
  ├── FW_CONFIG
  ├── TB_FW_CONFIG
  └── TRUSTED_BOOT_FW_CERT
```

BL2 可以从 FIP 中读取需要的映像。

需要注意：

FIP 是一种打包格式，不是所有 ARM SoC 都使用 FIP。具体固件存储格式取决于 SoC 厂商的启动架构。

Trusted Firmware-A 2.15.0 documentation+1

## 6. TF-A 源码应该怎么看？

官方仓库：

[ARM-software/arm-trusted-firmware](https://github.com/ARM-software/arm-trusted-firmware?utm_source=chatgpt.com)

官方文档：

[Trusted Firmware-A Documentation](https://tf-a.docs.trustedfirmware.org/?utm_source=chatgpt.com)

主要目录：

```
arm-trusted-firmware/
│
├── bl1/                  # BL1
│
├── bl2/                  # BL2
│
├── bl31/                 # EL3 Runtime Firmware
│
├── bl32/                 # Secure Payload 相关框架
│
├── common/               # 公共代码
│
├── drivers/              # 通用驱动
│
├── include/              # 公共头文件
│
├── lib/                  # 通用库
│
├── plat/                 # SoC / 开发板平台适配
│   ├── arm/
│   ├── allwinner/
│   └── ...
│
├── services/             # PSCI、SMC 等运行时服务
│
└── tools/
    ├── fiptool/          # FIP 打包工具
    └── cert_create/      # 安全启动证书工具
```

我建议重点阅读：

|源码位置|重点|
|---|---|
|`bl1/`|最早期启动过程|
|`bl2/`|固件加载与启动参数|
|`bl31/`|EL3、SMC、运行时服务|
|`services/std_svc/psci/`|PSCI 实现|
|`plat/`|SoC 平台适配|
|`tools/fiptool/`|固件打包|

尤其是 `bl31/` 和 `services/std_svc/psci/`，对于理解 Linux 与底层 ARM 固件的交互非常有价值。

## 7. 放到 Rockchip SoC 上应该怎么理解？

结合你正在研究的 RK3506、RK3588，我觉得这是最有实际价值的部分。

Rockchip 平台的启动链通常不是简单地从 TF-A BL1 开始，而是会有厂商自己的 BootROM、DDR 初始化代码和 U-Boot SPL 等组件。

一个概念性的启动流程：

```
        Rockchip BootROM
               │
               ▼
       TPL / DDR Init
       （依平台而定）
               │
               ▼
          U-Boot SPL
               │
        ┌──────┴──────┐
        │             │
        ▼             ▼
      BL31          BL32
      TF-A          OP-TEE
        │             │
        └──────┬──────┘
               │
               ▼
          U-Boot BL33
               │
               ▼
          Linux Kernel
               │
               ▼
             Rootfs
```

这里有一个重要区别：

U-Boot SPL 不一定就是 TF-A BL1。

在一些平台上，SPL 可以承担类似 BL2 的 Trusted Boot Firmware 职责；也有平台使用非 TF-A 的启动固件直接加载 BL31。

所以判断一个固件属于哪个阶段，不能只看它的文件名，要看它的执行职责和交接接口。

在 Rockchip 的固件目录或打包配置中，你可能会看到：

|文件或组件|可能对应的职责|
|---|---|
|`idbloader.img`|Rockchip 前期启动固件|
|`u-boot-spl.bin`|SPL / 前期 Bootloader|
|`bl31.elf` / `bl31.bin`|TF-A BL31|
|`tee.bin`|OP-TEE BL32|
|`u-boot.itb`|U-Boot 等后续映像的打包文件|
|`trust.img`|Rockchip 特定的可信固件打包形式|

具体对应关系会随 Rockchip SoC、SDK 版本和启动配置变化。

### 8. 我建议你建立的知识体系

学习 ARM 启动过程时，不要把所有东西都归类成 Bootloader。建议分成四个层次：

第一层：硬件启动

BootROM / DDR

- BootROM
    
- SRAM
    
- DDR 初始化
    
- SoC 时钟及基础硬件
    

第二层：可信固件启动

TF-A

- BL1 / BL2
    
- BL31
    
- BL32
    
- Secure Boot
    

第三层：普通世界启动

U-Boot

- SPL
    
- U-Boot proper
    
- Device Tree
    
- Linux Kernel
    

第四层：操作系统

Linux

- EL1
    
- PSCI
    
- OP-TEE Client
    
- 驱动与用户空间
    

我推荐接下来按照这个顺序深入：

1. ARM TrustZone：理解 Secure World 和 Normal World 的硬件隔离机制。
    
2. ARM Exception Levels：深入 EL0～EL3、SCR_EL3、SMC。
    
3. TF-A BL31：理解 EL3 Runtime、PSCI 和 CPU 上下电。
    
4. OP-TEE：理解 Linux 如何通过 SMC 与 Trusted OS 通信。
    
5. Secure Boot：理解从 BootROM 到 Linux 的 Chain of Trust。
    

最后推荐两个官方资料：

- [TF-A 官方文档](https://tf-a.docs.trustedfirmware.org/?utm_source=chatgpt.com)
    
- [OP-TEE OS 源码](https://github.com/OP-TEE/optee_os?utm_source=chatgpt.com)
    

我的建议：先深入 BL31，而不是从 BL1 开始逐行读源码。 因为 BL31 是 Linux 启动之后仍然会与之交互的固件层。把 PSCI、SMC 和 EL3 理解透彻之后，你就能真正理解 Linux 内核下面还有哪些软件在控制 ARM SoC。