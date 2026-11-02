# 从电脑操作 Termux：USB SSH、scrcpy 与中文输入

> SSH 把电脑终端连接到手机命令行，scrcpy 传输 Android 画面与输入事件；两条路径的权限、输入法和会话生命周期分别处理。

## TL;DR

- 长文本、中文命令和 CLI 会话优先验证 SSH；需要 Android 界面时再用投屏。
- ADB 转发连接手机已有监听服务，不会自动启动 Termux 或 SSH。
- 投屏显示模式不等于实测帧率；键盘事件也不等于 Unicode 文本输入。

## 文件访问由 Android 权限决定

Termux home 是手机真实文件系统中的应用目录。共享存储授权后可通过 `~/storage/shared`、`~/storage/downloads` 等链接访问；其他应用私有目录仍受沙箱限制。

在手机 Termux 中运行 `termux-setup-storage` 并完成系统授权后，验证读取和可清理的临时写入；目录链接存在不等于已拥有所有访问权限。代码、可执行文件和包环境优先放 Termux home，共享存储的执行及元数据语义不同。

Root、ADB shell、Shizuku 和普通 Termux 是不同身份。电脑 ADB 的 `su` 成功，不代表 Termux 获得同样授权；`ssh` 登录也不会自动提权。终端 Agent 的实际权限来自其进程和可调用工具，提示词不是权限边界。Agent 的“不保存本次会话”选项也不等于删除已有登录凭据或禁止云端记录，应按各自配置与服务约定判断。

## USB SSH 的最小链路

前提：Termux 已安装 OpenSSH，已配置自己的公钥认证，电脑已授权 ADB。以下使用示例端口，手机用户名在 Termux 运行 `whoami` 查询。若已有 SSH 服务，先核对端口和监听配置，避免重复启动。

手机 Termux：

```sh
sshd -p 8022 -o ListenAddress=127.0.0.1 -o PasswordAuthentication=no
```

电脑：

```sh
adb devices
adb forward tcp:28022 tcp:8022
TERMUX_USER='u0_a123'  # 替换为当前手机实际用户名
ssh -p 28022 -o HostKeyAlias=termux-usb-phone "$TERMUX_USER@127.0.0.1"
```

多设备时给 ADB 命令加 `-s <serial>`；同时连接多台手机时，用不同本地端口和主机别名。第一次校验主机密钥，密钥变化时核查设备与服务，不通过关闭校验解决。

`HostKeyAlias` 用来为该设备识别主机密钥。转发命令成功不代表 SSH 认证成功；还要验证远端身份、home、中文往返和终端程序交互。USB 重连后若转发丢失需重建，手机服务被回收后则须先恢复服务。

电脑执行 `adb forward --remove tcp:28022` 只移除该转发，不会停止手机上的 SSH；按实际需要管理监听服务。Termux 退到后台、退出 SSH 与停止远端进程也不是同一件事，长任务应另验证会话保活及系统回收。

## scrcpy 的帧率与输入

```text
Android 画面 → 视频编码 → ADB／USB → 电脑解码显示
电脑键鼠     → 输入控制通道         → Android
```

压缩视频使 USB 投屏具备较高流畅度，但不能据此断言每次都使用同一硬件编码器。内容不变时不必产生新帧，面板 90 Hz 或虚拟显示 60 Hz 都不是实际传输帧率。

```sh
scrcpy --print-fps
scrcpy --keyboard=uhid
```

第一条观察实际捕获帧率；第二条是单独启动另一种输入模式的示例，使用前确认本机版本支持。默认 SDK 注入的字符范围有限；UHID 模拟物理键盘，需要设备支持及 Android 键盘布局／输入法配合，也不保证电脑输入法的组合文本直接传入。

分别测试英文、中文输入、快捷键和粘贴。CLI 的中文通过 SSH 成功，不等于 scrcpy 中文也通过；退出终端程序回到 shell 后再运行管理命令。

`phone-ssh`、`phone-mirror`、`pi-phone` 是个人环境中的包装命令，不是 Termux 或 scrcpy 标准命令。迁移时先理解底层服务、端口和参数，避免直接依赖不存在的脚本。

## 参考

- [Termux：OpenSSH 软件包](https://github.com/termux/termux-packages/tree/master/packages/openssh)
- [Termux：文件系统布局](https://github.com/termux/termux-packages/wiki/Termux-file-system-layout)
- [ADB：端口转发](https://developer.android.com/tools/adb#forwardports)
- [scrcpy：视频与帧率](https://github.com/Genymobile/scrcpy/blob/master/doc/video.md)
- [scrcpy：键盘模式](https://github.com/Genymobile/scrcpy/blob/master/doc/keyboard.md)
