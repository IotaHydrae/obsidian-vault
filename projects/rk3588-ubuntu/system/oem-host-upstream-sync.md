# oem-host 构建时如何获取 GitLab 最新代码

> 镜像构建从 GitLab 获取 `main` 最新提交，再生成运行快照；不再依赖手工拷贝本地上位机目录，`.git` 不进入镜像。

## TL;DR

- 默认仓库 `git@code.example.com:team/image-project.git`，默认分支 `main`；构建账户须已有 SSH 读取权限。
- Git 缓存在构建机 `build/oem-host.git`，导出的运行快照在 `overlay/opt/oem-host/`；`SOURCE` 记录实际完整提交。
- `sync-to-board.sh` 排除此生成快照，保留构建端版本；上位机由构建端独立阶段获取。
- 独立 `oem-host` 阶段完成源码获取、依赖安装与字节码预编译；新提交只重做该阶段及 `board` / `image` 阶段，不重做 `rootfs` 阶段。
- 镜像侧 unit、env、sudoers、tmpfiles 独立维护；运行账户、socket 与权限见 [运行契约](oem-host-integration.md)。

## 构建方法与身份

正常构建无需先同步上位机；可用环境变量覆盖仓库或分支：

```sh
sudo ./build.sh --board=lubancat-5-v2
sudo ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --stage=oem-host
sudo LSP_OEM_HOST_BRANCH=main ./scripts/build-lsp-image.sh --board=lubancat-5-v2 --from=rootfs
```

`LSP_OEM_HOST_REPO` 覆盖仓库 URL，`LSP_OEM_HOST_BRANCH` 覆盖分支，默认分别为上述仓库和 `main`。
Git、OpenSSH 客户端、rsync、tar 与 flock（util-linux）是构建机依赖，不为此加入镜像运行依赖。
默认 SSH 使用 `-F /dev/null -o BatchMode=yes -o ConnectTimeout=5`，不交互索取密码。
通过 `sudo` 构建时，SSH 传输以 `SUDO_USER` 身份运行，读取该账户已有密钥与 known_hosts；
直接以 root 构建则使用 root 身份。显式 `GIT_SSH_COMMAND` 优先，由调用者负责其身份与参数。
不复制私钥到工程或镜像，也不改本地 `~/example-project/oem-host` 工作树。

默认与 `--from=apps` / `--from=base` 均按 `apps → base → rootfs → oem-host → board → image` 执行，阶段参数使用职责名称。
`--from=rootfs` 按 `apps → rootfs → oem-host → board → image` 执行；`--from=oem-host` 按 `oem-host → board → image` 执行。
`--stage=oem-host` 要求 `rootfs` 阶段已完成，单独完成获取源码、依赖安装、字节码预编译、配置与权限收尾。
每次进入 `oem-host` 流程都先获取最新提交，再按 `build/.stage-oem-host.done` 比较仓库、分支与完整 SHA；
来源不变可复用安装，来源变化重新安装并使 `board` / `image` 阶段失效，不重做 `rootfs` 阶段。
上位机阶段成功才写完成标记；重跑 `rootfs` 阶段会撤回它，后续须重新集成。
单跑 `rootfs` 阶段不获取或预编译上位机；单跑 `board` / `image` 阶段或 `--from=board` / `--from=image` 不联网，
启用上位机时要求完成状态与 rootfs 的 `SOURCE` 一致，不能跳过集成直接打包。
`board` 阶段在恢复 rootfs 后校验，`image` 阶段在拷贝或解包到镜像根分区后校验，保留 tar 归档回退能力。
启用 `LSP_ROOTFS_TAR=Y` 时，上位机阶段刷新 rootfs 归档，避免 `board` 阶段恢复旧归档丢失上位机。
单跑 `apps`、`base` / `board` / `image` 阶段、dry-run 或 `LSP_OEM_HOST=0` 也不获取远端代码。
`LSP_OEM_HOST=0` 跳过集成，不承诺清除已有 rootfs 中的上位机。

每次实际获取都必须成功，不把本地缓存当作“最新”兜底。缓存只复用 Git 对象，
镜像使用本次取到提交的 `git archive` 导出内容；自动生成的快照仍被 Git 忽略。
默认跟随最新分支不等于版本锁定，成品应以 `/opt/oem-host/SOURCE` 的完整 SHA 追溯。

## 手动同步与排除边界

工具也支持独立获取远端，或保留原来的本地源码同步用途：

```sh
sudo tools/sync-oem-host.sh --repo=git@code.example.com:team/image-project.git --branch=main -n
sudo tools/sync-oem-host.sh --repo=git@code.example.com:team/image-project.git --branch=main
tools/sync-oem-host.sh --src=<上位机源目录> -n
tools/sync-oem-host.sh --src=<上位机源目录>
```

`--repo` 与显式 `--src` 互斥；未给 `--repo` 时本地源仍默认 `~/example-project/oem-host`。
缓存与快照由实际构建账户管理；已有 sudo 构建生成的文件时，手动同步也使用 sudo。
生成代码快照不含凭据，根目录固定 0755，避免导出临时目录的 0700 被保留到快照。
远程 dry-run 只显示获取计划，不联网，也不创建缓存或目标；本地 dry-run 预览实际文件差异。
实际同步用 rsync 校验内容并删除退役文件，同大小 / 同时间戳的内容修改也会同步，软链保持为软链。
快照包含代码、素材、依赖清单、README、第三方许可与新增运行资产，不采用固定白名单。

远程 `SOURCE` 记录仓库 URL、分支、完整 SHA、`clean` 和 UTC 同步时间。
本地模式记录源路径、完整 SHA、`clean` / `dirty` 和时间，支持 `.git` 为文件的 worktree；
无 Git 元数据时明确记 `unversioned`。`dirty` 包括非忽略的未跟踪文件，提交号不能独自复现这类快照。

`.git` 目录或文件不进入快照。顶层 `.gitignore`、tests、docs、scripts、旧部署清单、
环境文件、logs、data、node_modules、部署备份、编辑器 / agent 配置被排除。
Python 虚拟环境、字节码和开发缓存跨层排除；业务内的 `app/scripts`、`app/docs`、`app/tests` 保留。
过滤只限制发送端，因此旧快照中已排除的路径也会被删除。
`deploy/` 只保留 `image-reset/` 运行树及 `collection-service/` 的 worker、`.path`、`.service` 三个指定文件；
后者的 README 和其他脚本、其余部署脚本、sudoers 与旧单元排除。
上游 `image-reset/expected-runtime.json` 与 `expected-system.json` 也排除，
由 `oem-host` 阶段针对已安装应用和本镜像生成新清单；
安装时以本镜像 `overlay/home/board/.lsp/config/device.yaml` 生成目标 rootfs 内的同名恢复原件，
要求已安装配置与 overlay 逐字节一致；更新 manifest 中该项摘要与 `image-overlay` 来源标记，
以及 `SOURCE.json` 已有的原件 manifest 摘要。上游 Git 快照和本机上位机工作树均不回写。
其余五份现场 rules 原件保持上游内容，目标路径、manifest 摘要与镜像实际规则均须严格一致；
manifest 必须恰含这六项，缺项、额外项或 rules 不一致均拒绝集成。上游清单生成器只负责
runtime / system 两份摘要，不能生成恢复原件；上述 `device.yaml` 适配由镜像安装阶段完成。
重置通过临时 systemd 服务执行；采集准备安装外部三文件并只 enable 请求 `.path`，不发起任务。
这些构建检查不代替真实重置验收，详见 [运行契约](oem-host-integration.md#工位复位与数据盘挂载)。
浏览器始终由 `lsp-kiosk.service` 启动，两套 kiosk 启动入口不能并行安装。

只有 rsync 成功才原子替换 `SOURCE`；传输失败保留错误码，不报同步成功。
文件树不是跨文件事务，失败时目标可能已部分更新，应修复后重新同步，不能以旧 `SOURCE` 证明完成。
源与目标相同或互为子目录时拒绝同步；本地同步期间不要修改源工作树。
构建不会更新运行系统，现场部署见 [overlay-apply.md](../build/overlay-apply.md)。

## 上游变更时核对什么

| 上游变化 | 镜像侧核对 |
| --- | --- |
| requirements 或解释器 | 系统 Python、wheel 缓存、`rclpy` 导入，重新运行 `oem-host` 阶段 |
| 监听与页面地址 | socket、env、kiosk URL、Chromium allowlist |
| 运行账户与写路径 | kiosk 身份、board 共享组、setgid、umask、ReadWritePaths |
| 新 root 操作 | 固定入口与参数约束，见 [sudo-policy.md](kiosk/sudo-policy.md) |
| 页面皮肤 | 着陆页底色、`LSP_KIOSK_BACKGROUND` 与实际页面背景对齐 |
| 日志、素材、部署树 | tmpfiles 排除项、运行资产同步边界、外设路径与自启契约 |

## 验证依据

`python3 tests/sync-oem-host.py [--keep]` 使用临时工程、真实 rsync / Git 和本地远端仓库，
验证默认 `main`、更新 / 切换分支、来源 SHA、删除退役内容、获取失败阻断和远程预览零联网 / 零写入，
并保留本地模式对新运行资产、嵌套目录、许可证、软链、同大小同时间戳修改、Git / worktree 与脏状态的覆盖。
只有 rsync 传输错误用命令桩注入；`--revision=<ref>` 可对旧脚本做反证。
这些隔离回归不访问真实 GitLab，不代替完整镜像构建或上位机功能验收。

`python3 tests/oem-host-build-stage.py [--keep]` 另验证独立阶段与失败清理：上游旧日期原件不覆盖
本镜像 device.yaml，生成后的六项清单与来源摘要自洽，五份 rules 任意一份不一致都拒绝集成。

- `tools/sync-oem-host.sh`：远端获取、本地同步、排除清单与 `SOURCE` 生成。
- `scripts/build-lsp-image.sh`：阶段缓存、依赖、字节码、目录权限和单元集成。
- `tools/verify-image.sh`：镜像来源与 `.git` 排除、静态配置及依赖配套体检。
