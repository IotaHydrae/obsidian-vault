# Linux platform 驱动注册与注销

> platform 驱动必须配对注册和注销；`platform_driver_probe()` 具有一次性探测等特殊约束，不能直接代替普通注册流程。

```c
ret = platform_driver_probe(&mypdrv, my_pdrv_probe);
```

## 生命周期

常规驱动使用 `platform_driver_register()` 注册，退出时通过 `platform_driver_unregister()` 注销；也可使用适用的模块辅助宏。需要使用 `platform_driver_probe()` 时，先核对目标内核对一次性探测、热插拔和延迟探测的限制，并处理注册失败。
