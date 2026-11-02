# 终端产品：模拟器预装、游戏内容导入与商用分发

## TL;DR

- 产品模式候选：经过许可审核的模拟器／核心＋设备按键配置＋用户导入内容；演示游戏只选自有或取得相应商业再分发权利的内容。
- 个人使用推荐不等于商业预装白名单：Snes9x、FinalBurn Neo默认排除，其他软件也要核对精确版本、依赖及分发义务。
- 免费下载、购买ROM、源码可见或标注“软件赠送”都不能单独证明具备随设备商业分发权；ROM／BIOS与程序分别审核。

## 本工程记录的产品模式

2026-11-01，用户提出未来制作并出售自己的终端设备。本篇将该方向记为候选产品设计，
不表示已确定出货版本、取得额外商业授权或完成出货及全部销售地区的法律审核。
[模拟器与输入调查](uconsole-game-emulators.md)继续记录个人使用候选和实机配置方法；
实际商业包只从审核通过的软件／核心／资源集合生成，不能整份照搬个人推荐。

| 层次 | 计划提供 | 发布前要确定 |
| --- | --- | --- |
| 模拟程序 | RetroArch或独立模拟器，加选定核心 | 精确提交／版本、许可证、依赖、修改、构建与源码交付方式 |
| 设备配置 | 内置Keyboard／Joystick、USB／蓝牙手柄、画面与快捷键配置 | 来自真实固件和backend的逐项验证；配置／图标等素材自身权利 |
| 游戏入口 | 用户选择并导入本地文件、设置内容／存档目录 | 文件识别、权限与清晰的使用说明；不把用户导入功能等同游戏授权 |
| 出厂演示 | 自有原创或明确允许对应商业再分发的内容 | 代码、ROM、图像、音乐、文字、商标与展示素材的权利范围 |
| 系统交付 | 程序、许可说明、对应源码／必要构建材料和需要的安装信息 | 按每份实际许可证履行，不以链接上游首页代替全部义务 |

默认产品内容集合不含未经授权的第三方ROM或主机BIOS，也不捆绑替用户自动下载这些内容的来源。
授权明确的游戏可以预装；“ROM一律不能提供”同样不是正确的产品规则。

## 模拟器许可初筛

下面是选型阶段的初筛，**没有任何一项被本文自动批准为商用出货组件**。
Libretro汇总表用于定位，发布时仍核对选定源码、打包版本、子目录／第三方依赖和实际二进制。

| 候选 | 初筛许可／限制 | 产品处理方式 |
| --- | --- | --- |
| RetroArch | 主许可告知为 GPL-3.0-or-later | 商业分发允许，但需满足对应源码、许可及适用安装信息等义务 |
| mGBA | MPL 2.0 | 纳入候选；核对MPL覆盖文件及其修改的源码提供条件、通知和依赖 |
| FCEUmm／PCSX-ReARMed | 主源码告知为 GPL-2.0-or-later | 纳入候选；以精确核心版本／修改及全部依赖复核，不把前端许可当核心许可 |
| PPSSPP／DOSBox Pure | 主源码告知为 GPL-2.0-or-later | 纳入候选；独立核对所交付版本与附带库／资源，不笼统写整个软件栈同一许可 |
| Snes9x | 自定义非商业限制 | 默认排除；如需随产品提供，要先解决所需权利人的另行授权 |
| FinalBurn Neo | 自定义非商业／不得商业获利限制 | 默认排除；不能仅写“免费赠送”就绕过限制 |

GPL允许收费／商业分发，义务按实际适用版本执行。交付二进制时，
采用该许可证允许的源码随附、书面offer或网络提供方式；保留对应源码、修改、必要构建脚本和许可通知。
不能用与出货版本不匹配的上游仓库地址，代替完整对应源码。
提供源码不意味着声称本项目已审查过每项链接组合或所有独立产品组件的许可兼容性。

若GPLv3软件的终端分发符合其User Product等条件，还要核对Section 6安装信息要求和例外，
包括修改版本安装／运行所需的程序、方法或授权信息；还须按条款核对分发者或第三方是否保留安装修改代码的能力。
不能把“硬件由我们出售”当作自动豁免，
也不能简化为所有硬件／固件都必须无条件开放。按实际交付、更新／签名能力和条款判断。
MPL的源码范围、分发方式和通知要求又不同，不用GPL规则替代MPL审核。

初筛已核对以下固定源码中的主许可告知；它们不是未来出货版本的自动选择：

| 项目 | 核查提交／入口 |
| --- | --- |
| RetroArch | [3bf1482](https://github.com/libretro/RetroArch/tree/3bf1482c16aaae84c7bca763e5cb087aae7bc3a1)，retroarch.c及COPYING |
| mGBA | [52de910](https://github.com/mgba-emu/mgba/tree/52de910807c0178d6cfb088aa350952569e8f08f)，README及LICENSE |
| FCEUmm | [ac0d00a](https://github.com/libretro/libretro-fceumm/tree/ac0d00a7fc01166f84c7bac01d1084ebfd2b865e)，fceu.c／ppu.c |
| PCSX-ReARMed | [7b0e8f8](https://github.com/libretro/pcsx_rearmed/tree/7b0e8f8fadfbce3d170d195b9d9eecaabfa08c8e)，libretro.c／psxcommon.h |
| PPSSPP | [fb93fad](https://github.com/hrydgard/ppsspp/tree/fb93fad41c6fc7c368c313e191e34ccf2f303d3b)，Core.cpp；LICENSE另含PSPSDK告知 |
| DOSBox Pure | [Codeberg d9c1356](https://codeberg.org/schelling/dosbox-pure/tree/d9c1356a6376fc35bd9a900b46c4666b3f4500e6)，README／dosbox.cpp；原GitHub仓库现为迁移说明 |
| Snes9x核心／FBNeo | [Snes9x fae2fea](https://github.com/libretro/snes9x/tree/fae2fea08f74180759ef540ee94259213f503480)、[FBNeo 0a2dddc](https://github.com/finalburnneo/FBNeo/tree/0a2dddc937b4e67b9ae57b1b055c165d52efc43e)，各自许可文件 |

mGBA提供另行商业许可渠道，不代表遵守MPL的商业分发也必须购买另一份许可。
源文件的主许可告知不覆盖自动审查全部第三方文件、build选项、核心组合、Qt／FFmpeg和资源。

## ROM／BIOS／游戏素材分开判断

- 原创内容只有在代码、图片、音乐等权利也齐备时，才可作为自有演示包。
- 开源或公共领域内容须核对实际许可对象、素材例外及销售地区适用性；代码开源不自动覆盖游戏全部素材。
- 购买数字游戏或ROM通常取得的是对应使用权，不能仅凭付款推断可以向每台设备复制出售。
- 免费试玩／作者允许个人下载，不等于允许改包、商业再分发或用于产品宣传。
- BIOS、固件、封面、截图、角色图、音乐、商标和打包器附带素材分别确认，不从模拟器许可推导授权。

上面是产品设计与权利记录方法，不是对某款游戏或全部销售地区的授权结论。
美国版权局资料用于说明复制／分发等权利的区别；实际量产按销售地区、合同和具体版本复核。

## 此前收集的个人使用内容来源

下列作者／项目来源可用于选型和个人体验，**均未自动列入商业预装内容白名单**。
记录的是2026-11-01可见的下载方式，不固定价格，也不保证后续页面／许可不变。

| 平台／内容 | 原始来源 | 本次确认与授权边界 |
| --- | --- | --- |
| GB／GBC，Tobu Tobu Girl Deluxe | [作者itch.io](https://tangramgames.itch.io/tobu-tobu-girl-deluxe) | 作者源码明确代码MIT，图像／文字／音效／音乐CC BY 4.0；有公开商用许可依据，待匹配ROM版本并履行通知／署名 |
| GBA，Goodboy Galaxy Demo | [作者试玩页](https://goodboygalaxy.itch.io/goodboy-galaxy-demo) | 可下载GBA ROM试玩；“GBA (roms only)”是下载项，不等于商业出厂授权 |
| GBA，Goodboy Galaxy完整版 | [作者购买页](https://goodboygalaxy.itch.io/goodboy-galaxy-gba) | 作者直接出售ROM；购买与随设备复制／分发权分开 |
| NES，Micro Mages | [作者购买页](https://morphcatgames.itch.io/micromages) | 下载包含NES／Second Quest ROM，作者注明Steam版也附ROM；不推导批量预装许可 |
| 经典PC冒险游戏 | [ScummVM官方游戏页](https://www.scummvm.org/games/) | 部分作品获准免费下载；逐个游戏审核再分发条件，使用ScummVM而非自动视作主机ROM |

Tobu Tobu Girl Deluxe的[固定作者README](https://github.com/SimonLarsen/tobutobugirl-dx/blob/4985d4d282aa43229f250ffa3b0d86998ebd55d3/README.md)
明确代码MIT与上述素材CC BY 4.0；后者允许商业共享／改作，但需署名、许可链接、修改说明等。
因此它可列为许可明确的演示候选，不需要凭空要求另签商业合同；
仍须匹配具体ROM版本、完成MIT通知与CC BY义务，且不表示作者为本终端背书。
其他来源本次未建立量产预装权限，ScummVM免费列表也需逐游戏审核。

第一轮功能测试候选为Tobu Tobu Girl Deluxe和Goodboy Galaxy Demo；这里只记录建议，
没有下载／安装游戏，也没有把试玩内容加入镜像或自动构建流程。
保留作者、游戏版本、文件摘要、许可／授权文本、覆盖资源、地区／期限／商业范围与分发义务，
核对并履行适用公开许可或另行授权后，再决定能否加入出厂演示包。

## 发布时应保留的工程材料

候选转入批准集合时，保存模拟器／核心／依赖和内容的精确身份、许可文件、审核结论与修改记录，
将出货二进制、配置、对应源码／构建材料关联。source offer等采用方式按适用条款决定，不统一写固定期限。
实机输入、显示、音频、存档、恢复、更新与内容导入按[模拟器验收方法](uconsole-game-emulators.md)另测。
没有测过的板型、游戏、图形backend和手柄组合不得写成出厂支持。
本次没有安装软件、修改镜像包列表、签订额外商业协议、批准出厂内容集合或开始生产发布。

## 原始依据

- [Libretro许可证索引](https://docs.libretro.com/development/licenses/)：前端和核心许可分别列出，汇总不替代具体版本。
- [Snes9x核心LICENSE](https://github.com/libretro/snes9x/blob/fae2fea08f74180759ef540ee94259213f503480/LICENSE)、[FBNeo的license.txt](https://github.com/finalburnneo/FBNeo/blob/0a2dddc937b4e67b9ae57b1b055c165d52efc43e/src/license.txt)：商业限制按文件核对。
- [GPLv3正文](https://www.gnu.org/licenses/gpl-3.0.html)、[GPLv2正文](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)、[GNU GPL FAQ](https://www.gnu.org/licenses/gpl-faq.html)。
- [MPL 2.0 FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/)：商业使用、二进制分发与源码范围。
- [CC BY 4.0正文](https://creativecommons.org/licenses/by/4.0/legalcode.en)：商业共享／改作及署名等条件。
- [美国版权局说明](https://www.copyright.gov/what-is-copyright/)：复制、分发等权利；不是其他地区的法律结论。

网页／许可证快照与摘要在忽略的`image/work/uconsole-emulator-commercial-20261101/`，
输入源码／模拟器手册证据另见`image/work/uconsole-emulator-research-20261101/`。
