# `/etc/profile.d/` 的 ROS 2 Bash 守卫

`/etc/profile` 也会被 dash 等 POSIX shell 读取；ROS 的 `setup.bash` 只供 Bash 使用。
因此登录环境通过完整的 `if` 条件判断 shell、已有 ROS 环境和目标文件，再加载：

```sh
if [ -n "${BASH_VERSION:-}" ] && [ -z "${ROS_DISTRO:-}" ] && [ -f /opt/ros/jazzy/setup.bash ]; then
    . /opt/ros/jazzy/setup.bash
fi
```

不要把单独的失败检测放在 `sh -e` 的执行路径上；非 Bash 应跳过加载并正常返回。
已设置 `ROS_DISTRO` 时不重复 source，文件缺失时也不加载。
实现来自 `overlay/etc/profile.d/ros2.sh`；`tools/verify-image.sh` 的「ROS 2 环境与 USB 主机工具」组核对守卫与环境文件。

该守卫是通用登录环境契约，与已被 mask 的 `usbdevice.service` 无关；
固定 USB 主机策略见 [usb-gadget.md](../../usb/usb-gadget.md)。
