# 使用二进制和 systemd 安装 Gitea

> 在 Linux amd64 上安装 Gitea 二进制，使用 `git` 系统用户运行 systemd 服务，将数据和配置分别放在 `/var/lib/gitea`、`/etc/gitea`。

## TL;DR

- 以下以 1.25.5 为例；安装前确认目标版本、系统架构、下载来源和文件校验。
- 创建专用用户和目录，将二进制放到 `/usr/local/bin/gitea`，再配置 systemd。
- 安装完成后收紧配置文件权限；数据库、反向代理和监听端口需要按实际环境配置。

## 下载与准备

下载对应版本和架构的二进制：

```bash
wget -O gitea https://dl.gitea.com/gitea/1.25.5/gitea-1.25.5-linux-amd64
chmod +x gitea
```

核对二进制适用于本机，并在运行前按发布方提供的方法验证文件。以下目录和用户操作需要管理员权限：

```bash
sudo adduser --system --shell /bin/bash --gecos "Git Version Control" \
  --group --disabled-password --home /home/git git
sudo mkdir -p /var/lib/gitea/{custom,data,log} /etc/gitea
sudo chown -R git:git /var/lib/gitea/
sudo chmod -R 750 /var/lib/gitea/
sudo chown root:git /etc/gitea
sudo chmod 770 /etc/gitea
sudo cp gitea /usr/local/bin/gitea
```

## systemd 服务

将以下核心配置写入 `/etc/systemd/system/gitea.service`：

```ini
[Unit]
Description=Gitea
After=network.target

[Service]
Type=simple
User=git
Group=git
WorkingDirectory=/var/lib/gitea/
ExecStart=/usr/local/bin/gitea web --config /etc/gitea/app.ini
Restart=always
RestartSec=2s
Environment=USER=git HOME=/home/git GITEA_WORK_DIR=/var/lib/gitea

[Install]
WantedBy=multi-user.target
```

数据库依赖、socket 激活、低端口权限及 Git LFS 等按实际部署另行配置。

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now gitea
sudo systemctl status gitea
```

## 安装后权限

Web 安装阶段暂时开放 `/etc/gitea` 的组写权限，完成初始化后收紧：

```bash
sudo chmod 750 /etc/gitea
sudo chmod 640 /etc/gitea/app.ini
```

执行前确认配置文件已生成，并核对服务账户对配置和数据目录的访问权限。
