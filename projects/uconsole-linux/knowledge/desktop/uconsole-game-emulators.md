# uConsole：模拟器选型与内置按键／外接手柄配置

## TL;DR

- 个人使用首批候选为 RetroArch＋mGBA／FCEUmm／Snes9x／PCSX-ReARMed／FinalBurn Neo，另选 PPSSPP 和 DOSBox Pure；商用另筛许可。
- 内置方向键与ABXY可能来自键盘、Joystick两路逻辑设备；先实测逐键绑定，USB／蓝牙手柄另存配置。
- 这是2026-11-01的选型调查，尚未安装、配好或上板测帧率；不代表镜像已预装，也不承诺所有游戏满速。

## 选什么软件

RetroArch是前端和运行框架，主机模拟由各个libretro核心提供；统一输入、存档、快捷键是选择它的主要理由。
第一批先覆盖适合方向键／四个面键的游戏，再根据实际核心板、游戏和控制器扩展。
下表不是商业出货白名单；Snes9x／FinalBurn Neo有商业限制，默认不进入待售设备镜像。
产品模式、源码义务及ROM／BIOS权利见[商用分发记录](uconsole-emulator-distribution.md)。

| 游戏平台 | 候选软件／核心 | 选择理由和前提 |
| --- | --- | --- |
| GB／GBC／GBA | RetroArch＋mGBA | 一套覆盖三代掌机；先验证方向、ABXY、肩键和存档 |
| FC／NES | RetroArch＋FCEUmm | 作为较简单的画面／声音／输入基线，不是硬件满速承诺 |
| SFC／SNES | RetroArch＋Snes9x | 四个面键与常用手柄布局匹配；肩键按游戏要求补齐；商业预装默认排除 |
| PS1 | RetroArch＋PCSX-ReARMed | ARM目标的候选；优先按核心文档使用匹配的真实BIOS，HLE兼容性另核对，输入类型按游戏选择 |
| 街机／Neo Geo／CPS等 | RetroArch＋FinalBurn Neo | 按该版本DAT核对匹配的ROM集合；单个文件名不足以确认兼容；商业预装默认排除 |
| PSP | 独立PPSSPP | 专门设置PSP的输入和画面；首轮从原生分辨率、关闭额外画质增强开始 |
| 老DOS游戏 | RetroArch＋DOSBox Pure | 可利用键盘／轨迹球，也支持手柄映射、屏幕键盘和即时存档 |

PSP中依赖模拟摇杆／肩键的游戏优先外接手柄。DOSBox Pure本身也是libretro核心，不能写成独立DOSBox安装包。
不同发行版提供的核心、版本和包名可能不同；所有二进制必须匹配目标ARM64用户空间和核心接口。
这里没有检查Ubuntu／Arch软件源的实际包可用性，不给未经验证的一键安装命令。
游戏内容、BIOS、核心配置与模拟器程序分别管理，不写入通用镜像的构建输入。

## 内置ABXY为什么要先看事件

**固定源码事实：** ClockworkPi官方提交`53a05efca806ca024aab4fbfd28a680688c2541c`
的键盘固件声明Consumer、Keyboard、Joystick、Mouse复合HID。
同一物理键盘可能被系统拆成多个逻辑输入设备，并非多只独立USB硬件。

- 默认方向键发送键盘箭头，Fn层改为PageUp／PageDown／Home／End。
- ABXY和Start／Select可随背部switch 2的PD2状态发送键盘或Joystick按钮；不是Fn切游戏模式。
- Fn只在固件内切换层，不假定主机能收到独立Fn事件，不能直接当通用模拟器hotkey。
- `keyboard.h`按钮编号注释与实际`keymaps.ino`调用有不一致，配置以实际事件为准，不复制注释编号。

这些事实只对应所查固件，不能证明用户当前刷的是同一版。首次记录固件／开关状态后，
用输入事件工具和模拟器绑定界面分别确认方向、ABXY、Start／Select，按物理位置而非猜测字母绑定。
不预先选择整只Joystick并假定方向轴存在，也不因有Joystick报告就认为所有模拟器会自动合并输入。

## RetroArch配置分层

1. **物理设备→RetroPad。** 在`Settings → Input`下确认input／joypad backend、Port 1设备与按键。
   若菜单版本提供`RetroPad Binds → Port 1 Controls`，逐项完成绑定；不同版本的菜单层级可能变化。
   键盘方向＋Joystick按钮映射到同一玩家是一种候选方式，以所选backend同时接受为验收，不能声称全部自动支持。
2. **RetroPad→目标主机。** 在游戏运行后的`Quick Menu → Controls`调整主机按键，按核心／内容保存remap。
   remap与物理手柄绑定是两层，不靠改每个核心来修系统没有识别设备的问题。
3. **保存不同控制器。** USB和蓝牙手柄先在Linux正常识别，再选玩家端口／Device Index、检查自动配置并按需保存。
   保留内置按键与外接手柄的独立配置，重连后检查设备顺序，不硬编码`/dev/input/eventN`。
   `Save Controller Profile`／手柄自动配置与键盘绑定分开保存，核心remap又是另一层；混合输入需分别确认重启后仍生效。
4. **快捷键。** 用可被主机识别的按键作Hotkey Enable，再绑定菜单、存档、读档和退出，检查不会与游戏操作冲突。
   键盘直通游戏时使用Game Focus等合适方式避免热键截获；先确认进入和退出方式。

如果混合Keyboard／Joystick绑定不顺利，可验证固件已有的全键盘输出状态，或先使用独立外接手柄。
不为选型调查刷固件。手柄自动配置、蓝牙重连与热键可用性都要在真实backend／桌面会话中测试。
蓝牙已配对不等于HID输入可用；蓝牙和USB可表现为不同设备名／ID，分别记录。

## 上板验收方法

| 项目 | 明确判据 | 不够的替代证据 |
| --- | --- | --- |
| 输入 | 方向、四面键、Start／Select、所需肩键／摇杆逐项正确，无重复／卡键；组合键实测 | 游戏菜单能打开、单个USB HID已枚举 |
| 外接手柄 | USB／蓝牙各自按键、玩家端口、重连和内置／外接切换正确 | 配对或自动配置提示成功 |
| 显示 | 记录物理输出、模式、比例／缩放、渲染backend与实际可见画面 | 远程截图或renderer字符串 |
| 声音 | 实际扬声器／耳机输出，检查爆音、断续与同步 | 音频设备存在 |
| 存档 | 使用已知进度，保存、退出后重新打开并读回；普通存档和state分开 | 写操作返回成功 |
| 性能 | 固定板型、频率、散热、模拟器／核心、具体游戏场景、分辨率与效果，记录帧率／音频／温度 | 一次FPS样本、同SoC其他板宣传或仅GPU名称 |

首轮只选择少量明确场景；不要把不同游戏、PSP倍数、滤镜和核心配置混合后比较硬件。
CM3／CM4／CM5／CM2以及原生显示／虚拟屏的性能分别记录；T527使用何种GPU／显示栈尚有独立前提。
界面可见、画面正常与实际GPU执行也分开，渲染验收可参考[无屏桌面记录](cm5-headless-desktop.md)。

## 测试内容与下载来源

已找到作者直接提供的Tobu Tobu Girl Deluxe GB ROM、Goodboy Galaxy GBA试玩／完整版、
Micro Mages NES ROM，以及ScummVM官方免费游戏入口，详见[内容来源表](uconsole-emulator-distribution.md#此前收集的个人使用内容来源)。
它们是个人体验与功能验证候选，不因免费／可购买而自动获得随终端商业复制分发的权利。
ROM、BIOS、游戏素材与模拟器程序分别审核；出厂演示只从明确有相应商业权利的内容集合选择。

## 镜像集成边界与未知项

建议以后作为可选组件管理RetroArch与选定核心，PPSSPP／DOSBox Pure按需选择；
这是候选设计，不修改当前image component、系统配置档或默认包列表。
面向出售的终端再按[商业预装模式](uconsole-emulator-distribution.md)选取审核通过的软件／核心与授权内容，
提供对应许可／源码等交付材料和用户本地内容导入入口；个人推荐集合不能直接整包出货。
未来内置输入profile必须来自实机事件，区分固件／开关／backend，不将一台设备的编号推广为通用配置。
本次没有安装、启动模拟器、下载游戏／BIOS、配对手柄或测试板子；尚无帧率、延迟、音质和耗电结果。

## 官方来源

- [RetroArch输入、RetroPad、remap与hotkey](https://docs.libretro.com/guides/input-and-controls/)
- [mGBA](https://docs.libretro.com/library/mgba/)、[FCEUmm](https://docs.libretro.com/library/fceumm/)、[Snes9x](https://docs.libretro.com/library/snes9x/)
- [PCSX-ReARMed与BIOS要求](https://docs.libretro.com/library/pcsx_rearmed/)、[FinalBurn Neo与ROM集合](https://docs.libretro.com/library/fbneo/)
- [DOSBox Pure](https://docs.libretro.com/library/dosbox_pure/)、[当前Codeberg源码入口](https://codeberg.org/schelling/dosbox-pure)：源码仓库已迁移，不把GitHub迁移说明当实际实现版本。
- [PPSSPP硬件和手柄支持](https://www.ppsspp.org/docs/getting-started/recommended-hardware/)
- [uConsole固定固件keymaps](https://github.com/clockworkpi/uConsole/blob/53a05efca806ca024aab4fbfd28a680688c2541c/Code/uconsole_keyboard/keymaps.ino)：86–112、204–238、300–311、562–624行。
- [复合HID声明](https://github.com/clockworkpi/uConsole/blob/53a05efca806ca024aab4fbfd28a680688c2541c/Code/uconsole_keyboard/uconsole_keyboard.ino)：14–35行。

官方手册为2026-11-01快照，菜单和支持范围随版本变化；不是这几项软件最新release的身份记录。
网页快照／摘要与固件源码在忽略的`image/work/uconsole-emulator-research-20261101/`，不作为镜像输入。
