### 通过 sysfs 调整  framebuffer 的控制台光标闪烁行为

定位到`/sys/class/graphics/fbcon`
有如下文件
```shell
# cd /sys/class/graphics/fbcon && ls
cursor_blink  rotate        subsystem/
power/         rotate_all    uevent
```

默认情况下光标闪烁，文件cursor_blink值为1，向其中写0关闭光标闪烁
```shell
# On
# echo 1 > /sys/class/graphics/fbcon/cursor_blink
# Off
# echo 0 > /sys/class/graphics/fbcon/cursor_blink
```
