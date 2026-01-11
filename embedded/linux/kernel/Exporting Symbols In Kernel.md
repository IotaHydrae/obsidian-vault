
int __i2c_first_dynamic_bus_num;  
EXPORT_SYMBOL_GPL(__i2c_first_dynamic_bus_num);

有头文件声明了 __i2c_first_dynamic_bus_num 为 extern， 如果不加 EXPORT_SYMBOL_GPL 的话，在其他内核代码中引用这个变量会发生什么？

在 Linux 内核开发中，如果不使用 EXPORT_SYMBOL_GPL（或 EXPORT_SYMBOL）来导出变量，尽管在头文件中有 extern 声明，其他内核代码对该变量的访问能力会取决于**引用该变量的代码是如何编译的**。

主要分为以下两种情况：

### 1. 如果引用代码被编译进内核镜像 (Built-in)

如果引用该变量的代码（例如另一个驱动或子系统）是直接编译进 vmlinux（即在 make menuconfig 中选为 [*] 而不是 [M]）：

- **结果：可以正常工作。**
    
- **原因：** 在内核编译的最后阶段，所有的 .o 文件会被链接成一个巨大的二进制文件。此时，标准的 C 语言链接规则生效。只要 __i2c_first_dynamic_bus_num 在某个 .o 中定义了，链接器（ld）就能找到它的地址并完成重定位。
    
- **结论：** 对于内建代码，EXPORT_SYMBOL 不是必须的。
    

### 2. 如果引用代码被编译为内核模块 (.ko)

如果引用该变量的代码被编译为一个可加载的内核模块（在 make menuconfig 中选为 [M]）：

- **结果：模块无法加载，提示“Unknown symbol”。**
    
- **具体过程：**
    
    1. **编译阶段 (Compile)：** 正常通过。因为有 extern 声明，编译器相信这个变量在别处存在。
        
    2. **链接阶段 (Modpost)：** 在模块链接生成 .ko 的过程中，内核的脚本会检查该模块引用的所有外部符号。如果发现该符号没有被 EXPORT_SYMBOL 导出，通常会报一个 Warning（在某些内核配置下可能是 Error）。
        
    3. **运行阶段 (Runtime)：** 当你尝试执行 insmod 或 modprobe 加载该模块时，内核的模块加载器（Module Loader）会尝试解析模块中的符号。由于 __i2c_first_dynamic_bus_num 没有在全局内核符号表（可通过 /proc/kallsyms 查看）中导出，加载器会报错：
        
        codeBash
        
        ```
        insmod: ERROR: could not insert module my_driver.ko: Unknown symbol in module
        ```
        
        并在 dmesg 中看到类似：
        
        codeText
        
        ```
        my_driver: Unknown symbol __i2c_first_dynamic_bus_num (err -2)
        ```
        

### 3. EXPORT_SYMBOL_GPL 的特殊性

你提到的代码使用了 EXPORT_SYMBOL_GPL：

- **版权限制：** 如果你导出了这个变量，但引用它的模块声明的 MODULE_LICENSE 不是 GPL 兼容的（例如是 Proprietary），那么即便你加了 EXPORT_SYMBOL_GPL，非 GPL 模块在加载时依然会报错，无法访问该变量。
    
- **目的：** 这通常是为了保护内核核心接口，只允许开源驱动调用。
    

### 总结

|   |   |   |
|---|---|---|
|引用者类型|是否加 EXPORT_SYMBOL|结果|
|**Built-in (内建)**|否|**正常工作** (由 ld 链接)|
|**Module (模块)**|否|**加载失败** (Unknown symbol)|
|**Module (模块)**|是|**正常工作**|
|**Non-GPL Module**|是 (仅 _GPL)|**加载失败** (Permission denied)|

**为什么 __i2c_first_dynamic_bus_num 需要导出？**  
因为 I2C 设备号的分配可能被各种第三方驱动模块（如某个特定的传感器驱动）引用，这些驱动通常编译为 .ko 模块，因此内核开发者必须使用 EXPORT_SYMBOL_GPL 显式地将该变量暴露给模块层。