# 参考工程的通用经验与工具适配

## TL;DR

- 从用户指定的参考工程审阅技能、主题笔记、构建阶段与工具；按本仓库接口重写并脱敏。
- 迁入 7 个技能、5 份参考材料、17 篇通用知识，以及 3 个实际工具；菜单保存也采用原子发布。
- 已有 32 篇知识按同一写作规范复核，旧实测保留环境、数据、证据与未知项。
- 参考工程保持只读。迁入机制使用本地夹具验证；后续 CM5IO 构建与实机结果另见开发状态。

## 选择方法

先列出参考工程的技能、笔记、scripts 和 tools，再按 rootfs 构建、产物身份、
验证、启动测量、设备诊断五条线选择相关实现。正文与工具只迁入能解释用途、
满足本库契约且可脱离原硬件的方法。此清单说明实际阅读与适配范围，不表示
参考工程全部笔记和业务代码均已逐行审核。

本仓库已有 Python 后端、严格内核回执与镜像检查器；复用时接入这些入口。
来自另一工程的 shell 总脚本不直接成为第二套构建系统。源码级复用与参考机制后
重新实现在下表分别说明。

## 技能与配套资料

完整导航在 skills/README.md（工程源码：`skills/README.md`）。

| 技能 | 保留的可复用内容 | 本库适配 |
| --- | --- | --- |
| project-onboarding | 入口、执行链、输入输出与状态模型 | 三类板型、两个 rootfs 后端、handoff 和子模块 pin |
| developer-code-quality | 局部推理、资源所有权、失败语义、自查 | 脚本、驱动、配置与项目验证边界 |
| developer-testing | oracle、对照、测量与结果层次 | 现有测试入口，硬件观察与离线测试分离 |
| developer-knowledge | 结论优先、压缩、证据与维护 | 明确 TL;DR，所有知识篇严格少于 300 行 |
| knowledge-base-review | 摸底、分批、事实核实、拆篇引用 | 审计工具与仓库索引；附审计/拆分参考 |
| embedded-linux-boot-optimizer | 时钟、关键路径、受控实验和恢复 | GPIO UART 与已有 CM4 实验边界；附诊断参考 |
| engineering-embedded-linux-driver-engineer | API、binding、生命周期、并发与审查 | Pi 上游/独立补丁/板级边界；附方法、示例与准确性参考 |

以上共 5 份配套 reference。删除源 metadata 的身份、路径与现场依赖。
复制或改写的通用技能保留相应许可证；公开驱动技能的 MIT 版权通知保留。

## 构建脚本与工具的取舍

| 参考来源与机制 | 落地位置 | 复用方式与范围 |
| --- | --- | --- |
| 配置库：读取数据、校验枚举、0600 临时文件、原子替换 | build_menu.py（工程源码：`tools/build_menu.py`） | 采用原子保存；Python JSON/TOML 校验，保存失败保留原选择 |
| 分阶段镜像脚本：输入身份、失败失效、成功标记 | [阶段复用](../knowledge/build/rootfs-stages-and-failure-recovery.md)、[输入身份](../knowledge/build/input-identity-and-cache.md) | 提炼设计；保留本库独立工作目录，不增加未经验证的通用续跑 |
| chroot 挂载库：只查目标子树、反向卸载、失败保留 | [rootfs 隔离](../knowledge/build/rootfs-isolation.md) | 对照本库 unshare、mountinfo 和导出预检；没有复制另一套挂载入口 |
| 应用包库：源码/配方摘要、LFS 指针、架构和版本 | [包复用](../knowledge/build/userspace-package-reuse.md)、[APT 信任](../knowledge/build/apt-cache-and-trust.md) | 复核现有 desktop_packages/receipt 契约，保留独立 ABI 与板级边界 |
| compress-image：独立压缩、rawsize、可选摘要 | compress_image.py（工程源码：`tools/compress_image.py`） | 按本库重写；zstd/Zip64、显式输入、不覆盖、失败清理 |
| notes-audit：过滤示例、首屏摘要、长度、索引与链接 | knowledge_audit.py（工程源码：`tools/knowledge_audit.py`） | 按本库重写，接入 `make knowledge-check`；不迁源项目过时模式 |
| 串口工具的 Transcript：增量 UTF-8、raw/text/JSONL、时间戳 | serial_capture.py（工程源码：`tools/serial_capture.py`） | 直接脱敏适配该类，新写接收入口；不迁 daemon、RPC 或命令执行 |
| 镜像校验脚本：按文件、包状态、权限、启用关系验收 | image_inspect.py（工程源码：`tools/image_inspect.py`） | 复用检查方法并扩展现有检查器，增加 CM5IO 禁止项与 UART 登录要求 |
| 启动采样与 USB 映射工具 | [启动 IO](../knowledge/boot/boot-io-attribution.md)、[USB 映射](../knowledge/usb/device-mapping-method.md) | 保留观测方法，删除固定 MMC、端口/序列号与业务进程表 |
| compile_commands 工具 | 驱动技能参考 | 保留 `.cmd`、构建 cwd 与生成头文件的关系；本轮未增加需要实际构建树的新命令 |
| APT 缓存导入导出、镜像日期、桌面状态库 | [APT 信任](../knowledge/build/apt-cache-and-trust.md)、[阶段复用](../knowledge/build/rootfs-stages-and-failure-recovery.md) | 保留数据与身份检查原则；本库已有 spec/profile/receipt，不增加重复状态格式 |
| U-Boot/ATF、SoC 打包布局、板级修复、设备同步/写盘 | 对应后端责任边界 | 原平台布局和部署代码不适用于本库 Pi 固件链，未迁入 |

工具参数、依赖和退出码见 tools/README.md（工程源码：`tools/README.md`）。
稳定机器接口继续由 Makefile.agent 定义；这些人工 CLI 不新增 JSON 状态契约。

## 新增知识的实际范围

17 篇是按独立问题组织的迁入知识，不包含原有 32 篇和索引：

- 构建：输入身份、rootfs 隔离、验证分层、工具契约、失败恢复、APT 信任、ARM64 执行路径、压缩发布。
- rootfs：文件元数据与 capability、服务就绪与网络、精简镜像、logind IPC 生命周期。
- 启动：时钟域与可用终点、IO 争抢与归因。
- 设备树：源码、编译件、启动选择与运行树的验证链。
- 存储：克隆介质的 UUID/PARTUUID 冲突。
- USB：sysfs/udev 与物理口映射。

各篇写明当前已实现行为和未来设计的区别，链接当前源码或规范。
本库原有文章重排结论、方法、机制和证据，清理过期范围与调查流水，保留历史实验的
关键数值、boot ID、哈希、归档路径与否定结论。文章索引见[知识库](../knowledge/README.md)。

## 脱敏与验证边界

移除源工程的内网/主机/账户、凭据、设备序列号、私有仓库/服务、产品代码、
专用引脚/寄存器/分区布局与硬件性能数字；通用例子用占位符。
原有本库证据保留必要可定位路径；用户明确要求的 `pi` / `1` 是 CM5IO 的公开开发默认值。

结构审计覆盖 TL;DR、严格行数、重复标题、索引与本地链接，人工复核覆盖来源与适用范围。
压缩的夹具回读验证无损字节，串口只测 Transcript 与参数行为；硬件串口仍未验证。
后续真实镜像构建、CM5 IO Board 首次启动及有限超频结果，连同未完成的收尾，
统一记录在[开发状态](status.md)，不由迁入工具的夹具结果替代。
