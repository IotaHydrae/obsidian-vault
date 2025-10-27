## 1. 生成 SSH 公钥（在你的电脑上）

在你的 **本地电脑** 打开终端（Windows 可用 PowerShell 或 Git Bash；Linux/macOS 直接终端）：

```bash
ssh-keygen -t ed25519 -C "your_email@example.com"
```

## 2. 将公钥复制到树莓派

### 2.1 使用命令

### 2.2 手动拷贝

在你的windows机器上将公钥文件复制到树莓派上

```bash
scp ~/.ssh/id_ed25519.pub pi@192.168.50.100
```

登录到树莓派，执行如下命令

```bash
cat ~/.ssh/id_ed25519.pub >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
```

### 3. 修改 Windows 上的 ssh config文件
```c
Host 192.168.50.100
  HostName 192.168.50.100
  User pi
  IdentityFile ~/.ssh/id_ed25519
```