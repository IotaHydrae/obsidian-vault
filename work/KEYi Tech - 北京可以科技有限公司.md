
主要负责 Loona 机器人 BSP 开发。

## 1. 负责 Loona 机器人显示框架 LVGL 移植适配，硬件2D加速。

基于 LVGL v8 版本移植，适配了SoC的 2D 加速功能，支持fb/drm。编写了ARGB8888旋转算法，使用libyuv完成rgb到yuv的转换。

## 2. 负责 Loona 机器人的 Yocto SDK 工程的搭建



## 3. 负责 BSP 的 CI 自动化编译打包流程开发。



## 4. 驱动电源管理，低功耗休眠以及测试工具的开发。

为 linux 上的外设驱动添加 PM 电源管理接口，通过 power/state di

## 5. OTA 功能的开发

在 SoC 厂提供的 OTA 方案上修改，使用 recovery 模式进行升级

## 6. Secure Boot 的开发与测试


