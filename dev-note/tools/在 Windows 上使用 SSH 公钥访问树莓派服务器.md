# 从 Windows 使用 SSH 公钥登录树莓派

> 把 Windows 客户端的公钥追加到树莓派用户的 `authorized_keys`，再用 `ssh -v` 检查认证方式。

## 前提

- Windows 和树莓派均可使用 SSH；示例用户为 `pi`。
- 以下 `PI_IP` 是占位符，替换为目标主机地址。首次复制公钥需要已有的登录方式。
- 只复制 `.pub` 公钥文件，不要传输私钥。

## 配置公钥

在 Windows 的 PowerShell 或 Git Bash 中生成密钥；如果已有密钥，避免覆盖。复制公钥前，确认树莓派上没有需要保留的 `~/id_ed25519.pub`，否则应改用其他目标文件名：

```bash
ssh-keygen -t ed25519 -C "raspberry-pi-access"
scp ~/.ssh/id_ed25519.pub pi@PI_IP:~/id_ed25519.pub
```

登录树莓派，在目标用户下执行：

```bash
mkdir -p ~/.ssh
chmod 700 ~/.ssh
cat ~/id_ed25519.pub >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
```

确认追加成功后，可删除远端用于传输的 `~/id_ed25519.pub` 临时文件。仅在确认它不是需要保留的原文件时删除。

## 客户端配置与验证

Windows 用户的 `~/.ssh/config` 示例：

```sshconfig
Host raspberry-pi
  HostName PI_IP
  User pi
  IdentityFile ~/.ssh/id_ed25519
```

将 `PI_IP` 替换为实际地址，然后执行：

```bash
ssh -v raspberry-pi
```

检查调试输出是否显示使用 `publickey` 完成认证；不要在调试日志中公开私钥或其他敏感信息。
