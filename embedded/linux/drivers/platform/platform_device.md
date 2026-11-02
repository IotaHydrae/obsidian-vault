# Linux platform_device 与驱动匹配

> platform 设备通过 platform 总线匹配驱动；设备名与驱动名相同只是可能的回退方式，不是所有设备的必要条件。

## 设备信息

`struct platform_device` 包含名称、设备对象和资源数组；具体字段以目标内核版本为准。

## 匹配

[platform_match](platform_match.md) 涉及 `driver_override`、设备树、ACPI、ID 表和名称回退。设备树或 ACPI 匹配成功时，不要求 `platform_device.name` 与驱动名相同。
