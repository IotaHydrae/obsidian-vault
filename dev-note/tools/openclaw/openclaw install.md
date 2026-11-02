# 使用 nvm 安装 OpenClaw

> 先通过 nvm 准备符合要求的 Node.js 环境，再运行 OpenClaw 安装脚本。

## 安装 Node.js

```bash
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.4/install.sh | bash
source ~/.zshrc

nvm install node
```

示例加载的是 `~/.zshrc`，其他 shell 需使用对应的初始化文件。

## 安装 OpenClaw

```bash
curl -fsSL https://openclaw.ai/install.sh | bash
```

以上命令会执行远程脚本，运行前检查来源及内容；需要复现安装时固定 Node.js 和 OpenClaw 版本。

## 诊断

```bash
openclaw doctor --fix
```

该命令包含修复操作；运行前先查看工具说明和可能的配置变更。
