# 常用 Skills

> 按任务选择技能和配套工具，优先在需要的项目中启用；技能、CLI 和 MCP 服务的安装范围分别确认。

## TL;DR

| 项目 | 主要用途 | 形态 |
| --- | --- | --- |
| [brooks-lint](https://github.com/hyhmrright/brooks-lint) | 代码评审、架构审查、技术债与测试质量 | 代码质量技能集 |
| [universal-modder](https://github.com/rehan-remade/universal-modder) | 从游戏结构分析到 Mod 素材、脚本和打包 | Mod 工作流技能集与工具链 |
| [REA](https://github.com/morluto/rea) | 原生程序、JavaScript、.NET 和 Web 逆向分析 | MCP 服务、CLI 与配套技能 |
| [frontend-design](https://github.com/anthropics/skills/tree/main/skills/frontend-design) | 前端界面的视觉方向、布局、字体与交互细节 | Anthropic 官方设计技能 |

资料核对：2026-11-02。命令为上游提供的使用入口；安装选项、依赖与支持范围以对应仓库为准。

## brooks-lint：代码质量与可维护性

围绕代码复杂度、可读性、架构边界与测试价值组织评审，适合改动审查、重构前评估和技术债梳理。

常用入口：`brooks-review` 检查改动，`brooks-audit` 查看架构，`brooks-debt` 排序技术债，`brooks-test` 检查测试质量，`brooks-health` 查看综合质量概况。`brooks-sweep` 会按发现推进修改，使用前确定变更范围。

项目级安装示例，在目标项目根目录执行，需要 Node.js 20+：

```sh
npx brooks-lint install agents --project
```

`agents` 目标对应 `.agents/skills/`。保留技能目录与 `_shared/` 的相对结构，避免只复制单个入口而丢失共享规则。其他客户端的安装目标见 [官方安装说明](https://github.com/hyhmrright/brooks-lint#installation)。

## universal-modder：游戏 Mod 工作流

覆盖游戏目录与资源格式识别、Mod 框架选择、素材生成、脚本编写、安装位置和打包。适合需要同时处理代码与美术资源的 Mod 项目；具体引擎仍需对应框架和资源工具。

仅添加技能的入口：

```sh
npx skills add https://github.com/rehan-remade/universal-modder
```

在安装交互中选择需要的技能与项目范围。可按任务选择 `mod-any-game`、`game-recon`、`reverse-engineering`、`fal-assets`、`asset-pipeline` 等入口，无需为了单一任务引入整套工作流。

完整工具链还包含 `um` CLI 和 fal 素材服务。CLI 环境需要 Git、Python 3.10+ 和 FFmpeg，`uv` 为推荐工具；3D 模型转多方向精灵图还需要 Blender，fal 素材生成需要相应凭据。仅装技能不等于这些工具与认证已配置。安装层次和依赖见 [仓库说明](https://github.com/rehan-remade/universal-modder#install)。

## REA：逆向分析工作流与 MCP 工具

将反汇编、反编译、脚本和分析结果交给 Agent 使用。配套技能名为 `reverse-engineer-anything`，可用于梳理程序结构、追踪调用与数据流、整理证据和可复用分析脚本。

原生反汇编／反编译需要可用的 Hopper、Ghidra、IDA 或 Kuna 后端；部分文件元数据、JavaScript、.NET 等静态分析可使用内置能力，不一定需要原生反汇编器。Web 分析另需相应浏览器环境。

先查看安装计划：

```sh
npx rea-agents setup --dry-run
```

正式 `setup` 会注册 MCP，并按目标客户端配置工作流／技能；README 的默认安装会涉及个人配置和个人技能目录。需要项目级使用时，按 [安装范围说明](https://github.com/morluto/rea/blob/main/docs/installation.md#limit-rea-to-one-project-or-agent-profile) 选择适用的客户端配置方式，不将默认 setup 视为仅影响当前项目。

REA 更适合需要实际分析工具的任务；仅收藏技能文本无法代替后端、MCP 接入和依赖检查。具体入口见 [安装与排障文档](https://github.com/morluto/rea/blob/main/docs/installation.md)。

## frontend-design：前端视觉设计

Anthropic 官方技能，适合创建页面、组件和应用界面，或重做已有界面的视觉方向。重点是让配色、字体、布局与产品主题和用户任务相符，形成明确的设计选择。

工作方式是先提出精简设计计划，再对照需求复核并实现；实现时兼顾响应式、键盘焦点、减少动态效果偏好和可访问性，并在环境支持时通过截图自查。已有品牌规范或明确设计要求时，遵循具体需求。

在目标前端项目中只选择这个技能：

```sh
npx skills add https://github.com/anthropics/skills --skill frontend-design
```

按安装器提示选择客户端并确认项目范围。技能正文见 [官方 SKILL.md](https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md)。

## 安装范围与依赖

`npx skills add` 所用的 [Skills CLI](https://github.com/vercel-labs/skills) 当前要求 Node.js 22.20+，默认安装到项目，添加 `-g` 才是全局范围。它与 `brooks-lint` 自带安装器、REA setup 的作用范围不同，应按对应入口核对。
