# Linux Component Framework

> Component framework 用于协调多个设备组件与聚合驱动的绑定和解绑；组件注册不等于聚合设备已完成绑定。

```c
static const struct component_ops tcon_ops = {
    .bind = tcon_bind,
    .unbind = tcon_unbind,
};

/* probe / remove 中配对： */
return component_add(&pdev->dev, &tcon_ops);
/* component_del(&pdev->dev, &tcon_ops); */
```

聚合驱动负责匹配组件，并通过 `component_bind_all()` / `component_unbind_all()` 协调绑定。部分绑定失败时需要按框架约定清理已绑定组件。组件、聚合驱动及其数据的生命周期必须保持一致；回调签名与资源管理以目标内核版本为准。
