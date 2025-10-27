## 1. 生成 SSH 公钥（在你的电脑上）

在你的 **本地电脑** 打开终端（Windows 可用 PowerShell 或 Git Bash；Linux/macOS 直接终端）：

```bash
ssh-keygen -t ed25519 -C "your_email@example.com"
```

## 2. 将公钥复制到树莓派

### 2.1 使用命令

### 2.2 手动拷贝

在你的windows机器上查看公钥内容，并复制

```bash
cat ~/.ssh/id_ed25519.pub
```