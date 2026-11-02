# Linux platform 总线与驱动注册

> platform 驱动注册时将底层 `device_driver` 关联到 `platform_bus_type`，再交给驱动核心注册。

## 关键路径

```c
drv->driver.owner = owner;
drv->driver.bus = &platform_bus_type;
return driver_register(&drv->driver);
```

`platform_bus_type` 提供 `match`、`probe`、`remove`、电源管理等回调。匹配逻辑见 [platform_match](platform_match.md)。

## 边界

结构体字段、回调签名及注册实现以目标内核版本为准。
