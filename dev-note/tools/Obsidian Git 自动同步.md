# Obsidian Git 自动同步

> 打开此 Vault 后，Git 插件每 5 分钟提交并同步笔记；启动时及每 5 分钟拉取远端更新。

## 当前配置

| 项目 | 设置 |
| --- | --- |
| 验证环境 | Linux、Obsidian 1.14.5、Git 插件 2.42.0 |
| 远端与分支 | `origin/master`，使用现有 GitHub SSH 连接 |
| Auto commit-and-sync interval | 5 分钟 |
| Auto pull interval | 5 分钟 |
| Pull on startup | 开启 |
| Pull / Push on commit-and-sync | 均开启 |
| Sync method | Merge |
| 自动提交信息 | `vault sync: {{date}}` |

没有文件变化时不会新增空提交。自动任务仅在 Obsidian 运行且此 Vault 已打开时执行，关闭应用后不再同步。

## 手动同步与检查

1. 打开命令面板，执行 `Git: Commit-and-sync`，完成提交、拉取与推送。
2. 只获取其他设备的更新时执行 `Git: Pull`。
3. 查看状态栏和 Git 源代码管理面板，确认没有冲突或待推送提交。

终端可辅助检查：

```bash
git status --short
git rev-list --left-right --count HEAD...origin/master
```

第二条命令的 `0 0` 表示本地与本地记录的远端分支一致；判断服务器最新状态前仍需成功获取远端。切换设备时先完成拉取再编辑，同一文件并发修改产生冲突时需要处理后再同步。

## 本机配置与其他设备

`.obsidian/` 被仓库忽略，插件程序、启用状态和自动同步设置不会通过此仓库分发；其他设备需分别安装并配置。SSH 凭据与 Git 提交身份也由各设备管理，不放入笔记。

本机 `.serena/` 通过 `.git/info/exclude` 忽略。自动提交使用 Git 的忽略规则，不再重复配置插件的 auto-commit glob 排除项。

参考：[Obsidian Git](https://github.com/Vinzent03/obsidian-git)、[插件文档](https://publish.obsidian.md/git-docs/)。
