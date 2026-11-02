# 在 Debian / Ubuntu 上安装 pyenv

> 安装 Python 构建依赖后，用 pyenv 安装项目所需版本，再配置版本选择范围。

## 安装依赖

```zsh
sudo apt update; sudo apt install make build-essential libssl-dev zlib1g-dev \
libbz2-dev libreadline-dev libsqlite3-dev curl git \
libncursesw5-dev xz-utils tk-dev libxml2-dev libxmlsec1-dev libffi-dev liblzma-dev
```

## 安装 pyenv

```zsh
curl -fsSL https://pyenv.run | bash
```

这是远程安装脚本；运行前检查其来源和内容，并按 pyenv 的说明配置当前 shell。

## 安装 Python

```zsh
pyenv install --list
pyenv install VERSION
```

将 `VERSION` 替换为所需版本。不同发行版的依赖包名称可能不同。
