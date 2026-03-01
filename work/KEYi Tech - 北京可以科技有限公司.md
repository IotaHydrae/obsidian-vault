
主要负责 Loona 机器人 BSP 开发。

## 1. 负责 Loona 机器人显示框架 LVGL 移植适配，硬件2D加速。

基于 LVGL v8 版本移植，适配了SoC的 2D 加速功能，支持fb/drm。编写了ARGB8888旋转算法，使用libyuv完成rgb到yuv的转换。

## 2. 负责 Loona 机器人的 Yocto SDK 工程的搭建

基于官方提供的一个最小yocto工程二次开发，该yocto工程只用于编译文件系统，在原来的基础上添加了更多的layer，recipe以满足 app 需求，因为yocto版本较低，app 需要的某些依赖在高版本的yocto中才加入，所以在适配高版本的recipe，layer等比较困难，好在都完成了。

在yocto工程开发完成之后，后续的文件系统改动全部放到yocto工程中，包括用户 app 软件的编译、打包等。

## 3. 负责 BSP 的 CI 自动化编译打包流程开发。

使用 gitea + drone 完成，在编译服务器上配置好编译固件需要用到的docker容器，gitea上发布release自动触发编译流程，编译完成后自动上传到私有网盘中，方便测试、下载。

## 主板 I2C 驱动的开发

###  PowerMCU

主板上有一颗单片机用于控制充电，电源开关
### 电量计

### 加密芯片

### Tof

## 电机控制MCU 的 SPI 驱动开发

根据定义好的协议，

## 4. 驱动电源管理，低功耗休眠以及测试工具的开发。

为 linux 上的外设驱动添加 PM 电源管理接口，通过 power/state 发起调用流程。添加PM接口的驱动包括但不限于：Display，电机，MCU等。

编写的压力测试工具yong

## 5. OTA 功能的开发

在 SoC 厂提供的 OTA 方案上修改，使用 recovery 模式进行升级，APP检测到recovery模式会自动下载新的OTA包的app文件，升级逻辑由APP开发人员编写。

## 6. Secure Boot 的开发与测试

通过自定义的 RSA 密钥，对 SPL 程序进行加密，芯片的boot0程序会识别 SPL 中的加密信息，并与芯片内部写入FUSE 的密钥特征进行比对，如果不符，则停止引导过程。

### 7.  HomeAssist 的接入

根据 HomeAssist 的 MQTT 协议标准，编写订阅发布逻辑，注册设备，提供设备状态等数据。

