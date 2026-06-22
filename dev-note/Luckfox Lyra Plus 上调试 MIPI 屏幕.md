
编译更新内核

```bash
./build.sh kernel
adb push kernel-6.1/zboot.img /root
```

如果设备使用的是 SPI-NAND