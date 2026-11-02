# Linux platform 设备与驱动的匹配顺序

> platform 总线可通过 `driver_override`、设备树、ACPI、platform ID 表和名称匹配驱动；具体逻辑以目标内核版本为准。

## 匹配顺序

1. 设置了 `driver_override` 时，只比较其值和驱动名，直接返回比较结果。
2. 尝试设备树 OF 匹配。
3. 尝试 ACPI 匹配。
4. 驱动提供 `id_table` 时查找匹配的 `platform_device_id`。
5. 没有使用以上分支时，回退到设备名和驱动名比较。

## 注意

名称相同不是 OF 或 ACPI 匹配的必要条件；查看目标版本的 `platform_match()` 确认匹配顺序和返回行为。
