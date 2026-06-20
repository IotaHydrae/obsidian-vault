
| 部件  | 规格                            |
| --- | ----------------------------- |
| 开发板 | Purple Pi R1 IDO-SBC2D06）     |
| CPU | SSD201 2 x Cortex-A7 @ 1.2GHz |
| RAM | SIP 64MB DDR2                 |
| ROM | On board 128MB SPI NAND Flash |

## 固件烧录

首先给板子配网，将网线连接至靠近USB-A口的网口(eth0)，然后执行

```bash
udhcpc -i eth0
```

如果网络连接正常，可以观察到如下日志

```bash
udhcpc: started, v1.31.1
udhcpc: sending discover
udhcpc: sending select for 192.168.50.243
udhcpc: lease of 192.168.50.243 obtained, lease time 86400
deleting routers
adding dns 192.168.50.1
```

### kernel

单独更新 kernel 分区，可以参考如下步骤，首先在PC端将编译好的 kernel 镜像拷贝至板子中

```
scp images/kernel root@192.168.50.243:~/
```

回到板子一侧，更新分区并重启

```bash
dd if=/root/kernel of=/dev/mtdblock10 bs=1M && reboot
```

## 屏幕配置

先说一下硬件部分，开发板提供了一个 40 Pin 的RGB LCD接口，引脚定义与市面上大部分RGB屏幕相同，但是板子没有提供屏幕背光驱动电路，所以你需要自己画一个带背光驱动的转接板，或者你可以直接买一个带背光驱动的屏幕模组。

在编写本文时，我使用的屏幕是一块 4.0 寸的 RGB接口的屏幕，就是86盒上用的那种，分辨率 480x480，型号为 D395C930UV0。

板子提供的SDK中默认的屏幕配置是一块分辨率为 720x1280 的 MIPI 屏。

再来聊聊软件部分，

### 修改引脚复用模式

```bash
vim kernel/arch/arm/boot/dts/infinity2m-ssc011a-s01a-padmux-rgb565-rmii-doublenet.dtsi
```


## 参考文档

[Purple Pi R1 烧录流程]([Purple Pi R1 烧录流程](https://industio.yuque.com/mdtih8/lgnqq1/yezo0g3ragnuar79?singleDoc#ePYxu))
[PurPle-Pi-R1接口调试教程手册]([PurPle-Pi-R1接口调试教程手册](https://industio.yuque.com/mdtih8/lgnqq1/nlecxxl09gxt9wdz?singleDoc#jVrW8))