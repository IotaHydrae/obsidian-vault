# lsp-set-rule：运行期绑定现场设备（相机 / LCD / 手套）

> 现场绑定走 `/usr/local/sbin/lsp-set-rule`：**规则行的形状固定在脚本里**（`emit_line`），
> 调用方只能填值 —— 这样上位机不必拼 udev 语法，也不会写出互相冲突的规则。

## TL;DR

- 一个型号一行、互不干扰：不同型号 VID/PID 不同 ⇒ 规则不会同时命中同一台设备。
- **灵猴相机支持两种绑定且互斥**：按 USB 路径（`KERNELS`）**或**按 SN；配置了路径就去掉 SN 规则，
  反之亦然。**老款相机**（VID `0abd`）VID/PID/SN 全都一样 ⇒ 只能按路径绑定，且用
  `ENV{ID_V4L_CAPABILITIES}=="*:capture:*"` 认"视频流那个节点"。
- ⚠️ **VID:PID 是必选参数**，不要省略（省略后老款相机就无从区分）。
- **灵猴相机的 SN 已固化**成 `EXAMPLE_ID_06`（胸前）/`EXAMPLE_ID_07`（左上）/`SN0003`（右上）；LCD 与左右手套的
  出厂规则是**全占位符**（等上位机绑定）。
- 工具新生成的相机 / LCD / 手套规则统一 **`MODE="0666"`，不设置 `GROUP`**，与上位机回读契约一致。
  既有现场绑定不批量改写；`restore` 仍恢复镜像原件。
- 写完必须自己跑 `udevadm control --reload-rules` + `udevadm trigger`（与上位机原来的做法一致）。
- ⛔ **udev 规则"不再设置某一项"时，`change` 事件不会把它还原** —— 必须显式写默认值。
- ⛔ 测试完要把规则恢复原样（工程机上试产件全是 `EXAMPLE_ID_06`，按 SN 那一支要用真机唯一 SN 验）。

**需求**（用户 2026-09-23 明确）：现场要把相机 / LCD / 左右手套绑到稳定的 `/dev/` 名字上；
**相机的两种绑定方式都要支持** —— 生产机灵猴相机 SN 唯一，按 SN 绑；试产/测试机 SN 全是
`EXAMPLE_ID_06`，只能按 RK3588 的 USB 路径（DT 节点名）绑；**老款相机 `0abd:8050` 的 VID/PID 与 SN
全一样，只能按 USB 路径绑**（这条要保留）。新生成规则权限用 **0666**，**没有解绑、只有换绑**。

**做法**：调用方只交"**哪个型号 + 用哪种绑定 + 值**"（⛔ **VID:PID 必选**，用户 2026-09-23 明确），
规则文本一个字都传不进来；**没被点名的型号那一行原样保留**（含注释），所以可以一个型号一个型号地配。

| 角色 | 参数 | 写进哪个文件 |
| --- | --- | --- |
| `camera-left` / `camera-right` / `camera-chest` | `<VID:PID> port=<DT 节点名>` **或** `<VID:PID> serial=<序列号>`（**同一型号**二选一） | `99-{left-hand,right-hand,chest}-camera.rules` |
| `lcd-voice-bridge` | `<VID:PID> lcd=` / `glove_left=` / `glove_right=`（没给的沿用当前文件里的值） | `99-lcd_voice_bridge.rules` |
| `lcd` / `glove_left` / `glove_right`（**单设备角色**） | `<VID:PID> serial=<序列号>`（也认 `<角色名>=<序列号>`） | 同上，但**只改自己那一行**，另外两行一个字不动 |

> **两级角色**：`lcd-voice-bridge` 是**文件级**（一次重写三行，没给值的沿用文件里的当前值）——
> 上位机走的还是它；`lcd` / `glove_left` / `glove_right` 是**单设备级**（用户 2026-10-02 要求：
> "角色缺少 glove_left glove_right"）—— 现场只想换左手套时不碰 LCD 与右手套。
> `show <单设备角色>` 只列那一行；`restore <单设备角色>` 也只把那一行退回镜像原件
> （文件级 `restore` 是整份退回）。回归见 `tests/lsp-set-rule-roles.sh`。

```
sudo lsp-set-rule list                                            # 现在绑的是什么
sudo lsp-set-rule set camera-left 0abd:8050 port=fc800000.usb      # 老款：只能按口
sudo lsp-set-rule set camera-left 0c45:636b port=fc800000.usb      # 灵猴：按口（试产机）
sudo lsp-set-rule set camera-left 0c45:636b serial=EXAMPLE_ID_07          # 灵猴：按 SN（换件后 SN 变了才用；会先问一句）
sudo lsp-set-rule set camera-chest 0c45:636b port=fc880000.usb
sudo lsp-set-rule set lcd-voice-bridge 303a:1001 lcd=02:00:00:00:00:03
sudo lsp-set-rule set --dry-run …                                  # 只看 diff
sudo lsp-set-rule restore camera-left                              # 回到镜像里装的那份
```

- **一个型号一行、互不干扰**：两个型号的 VID/PID 不同 ⇒ 各自的规则不会同时命中同一台设备，
  所以"老款行 + 灵猴行"并存是对的，不需要 `old=` 之类的东西。**同一型号**的按口/按 SN 才互斥
  （用户 2026-09-23 明确："配置了路径就去掉 SN 规则，配置 SN 就去掉路径规则"）—— `set` 会把该型号
  现有的行**换掉**，切过去之后另一种绑定的行不在文件里。⛔ 为什么必须互斥：试产件三台灵猴的 SN
  都是 `EXAMPLE_ID_06`，同一型号两行同时存在时**三台会全挂同一个 /dev 名字**（工程机实测，见下）。
- **规则行的形状在脚本里**（`emit_line`），调用方只能填值：按口用 `KERNELS` +
  `ENV{ID_VENDOR_ID}`/`ENV{ID_MODEL_ID}`，按 SN 用 `ATTRS{idVendor}`/`ATTRS{idProduct}` +
  `ENV{ID_SERIAL_SHORT}`。多个父链条件可以共存，但必须匹配同一祖先（见 `udev(7)`）；
  DWC3 控制器的 `KERNELS` 与 USB 设备的 VID/PID `ATTRS` 不在同一层，所以按口用 `ENV`。
  `port=` 与 `serial=` 同给会被拒。
  ⇒ `RUN+=` / `PROGRAM==` / `IMPORT{program}=` 这类"让 udev 执行程序"的写法**注入不进来**；
  文件名来自内置目录（`role_spec`），**不接受路径**；值按整个参数校验
  （`^[0-9A-Za-z][0-9A-Za-z:._-]{0,63}$`），拒绝换行、控制字符和引号。
  `VID:PID` 同样必须整体匹配四位十六进制加冒号；不能用逐行 `grep` 判断单个参数，
  否则一行合法值就会放过夹带的额外规则。隔离回归验证拒绝时退出 2、全部规则不变、
  不调用 udev，并以合法 64 字符序列号作阳性对照。
- ⚠️ 老款（VID `0abd`）那行用 `ENV{ID_V4L_CAPABILITIES}=="*:capture:*"` 认"视频流那个节点"
  （不是 `ATTR{index}`）—— 这是原规则文件里的实测写法，脚本按 VID 区分保留。
  ⏳ **已知未实测的差别**：左手老款那行原来写的是 `ENV{ID_PATH}=="platform-fc800000.usb-usb-0:1:1.0"`，
  本工具按口写的是 `KERNELS=="<DT 节点名>"`（与右手、与灵猴那行一致）。**要等老款相机插上实测一次**
  才算验过（工程机上现在只有灵猴）。
- 📌 顺带实测过的两条（供以后判断）：**规则里可以不要 VID/PID**（只按 `KERNELS` +
  `ENV{ID_V4L_CAPABILITIES}=="*:capture:*"` 或 `ATTR{index}=="0"`，实测只命中视频流那个节点）；
  但**不能只按口**——去掉 `index`/能力判据后，同一台相机的两个节点（`video1` 与 `video2`）
  会**都挂上同一个名字**。用户选择保留 VID/PID。
- ⚠️ **LCD / 左右手套的出厂规则也是"全占位"**（2026-09-28 用户要求）：镜像里
  `overlay/etc/udev/rules.d/99-lcd_voice_bridge.rules` 的**三行都是**
  `ENV{ID_SERIAL_SHORT}=="XX:XX:XX:XX:XX:XX"` ⇒ 出厂**不生成** `/dev/lcd_voice_bridge`、
  `/dev/glove_left|right`；三块 ESP32-S3 的真值（`MAC − 1`，唯一）都由上位机现场
  `lsp-set-rule set lcd-voice-bridge 303a:1001 lcd=<SN>`（以及 `glove_left=` / `glove_right=`）写进去。
  用户原话："LCD 的 udev rules 也改成占位符"、"上位机要改这个逻辑" ⇒ **上位机的 LCD 配置逻辑
  由上位机自己改**（原来是从 `/dev/lcd_voice_bridge` 读序列号，占位之后绑定前没有这个节点）。
  离线判据：`tools/verify-image.sh` 的 `3c-5` 分组（三行必须全是占位 SN，且规则行里不含
  `ID_PATH` / 真 MAC）。
- 写完自己跑 `udevadm control --reload-rules` + `udevadm trigger`（与上位机原来的做法一致），
  内容没变且声明的节点存在时不写盘、不 reload；缺节点则重新触发 udev。
  `--dry-run` 始终只预览，即使缺节点也不 reload/trigger。规则用同目录临时文件生成、原子发布，
  准备或发布失败保留原规则；审计 `/var/log/lsp-set-rule.log`（0640 root:adm）。
- ⚠️ **灵猴相机的 SN 已固化**（2026-09-28 用户要求）：镜像里三路灵猴的规则直接写死 `EXAMPLE_ID_06`（胸）/
  `EXAMPLE_ID_07`（左）/ `SN0003`（右），量产流程**不需要**再动相机规则。所以**只有灵猴（VID `0c45`）**
  改绑定时会先问一句"灵猴相机的绑定是固化好的 …… 确定要修改吗？"—— `serial=` 与 `port=` **都算
  "切换绑定"**，答非 `y` 即取消（rc=2、文件一个字节不改）。**其他型号（老款 `0abd:8050` 等）不问**
  （用户 2026-09-28："只有灵猴相机切换绑定才会提示，其他的暂时不提示"）。四种情况不问：值与现有
  规则一致（走"没有变化"分支）、`--dry-run`、非灵猴型号、**非交互调用**（stdin 不是 tty）——
  最后这条是给**上位机**留的：它按设备真实 SN 写，在这儿 `read` 会把整条配置流程挂住，所以只在
  stderr 提示一句照旧写。要显式跳过用 `--yes` 或 `LSP_SET_RULE_ASSUME_YES=1`。
  回归：`sudo bash tests/lsp-set-rule-confirm.sh`（开发板实测 `通过 30 / 失败 0`）。
- 板上实测（开发板 `<HOST_07>`，2026-09-23，新版 `VID:PID` 必选口径）：
  ① `set camera-left 0abd:8050 port=fc800000.usb` → 只换老款那行（变成 `KERNELS` 形式），
  灵猴那行**一字不动**；② `set camera-left 0c45:636b port=fc800000.usb` → 灵猴那行换成按口；
  ③ `set camera-left 0c45:636b serial=EXAMPLE_ID_06` → 灵猴那行**被换掉**（按口那行不再存在），
  老款行仍原样 —— 同一型号互斥、不同型号共存；④ `set lcd-voice-bridge 303a:1001 lcd=11:22:…`
  → 只换 LCD 那条，左右手套两条序列号（`02:00:00:00:00:01`/`:A0`）**原样保留**。
  被拒用例（全 rc=2）：缺 VID:PID、`VID:PID` 写错（`0c45636b`）、`port=` 与 `serial=` 同给、
  相机用 `lcd=`、tty 用 `port=`、值带 `../`、未知角色、没给任何 `名=值`。
- ⛔ 踩过的坑（已修）：判定"这一行属于哪个型号"时我第一版写成 `ID_VENDOR_ID=="0abd"`，
  而实际文本是 `ENV{ID_VENDOR_ID}=="0abd"`（中间还有 `}`）⇒ **旧行一条都没被替换**，
  文件里越写越多行。现在只匹配 `键` + `=="值"` 两段（`line_is_model`），板上复测过。
- 上位机 `app.rules.script_has_spec()` 回读 helper 输出时要求 **0666 且没有 GROUP**；
  `tests/sbin-config.py --oem-host-root=<上位机源码目录>` 调用实际函数核对 tty 与相机 SN 模板，
  并用旧权限、错误 SN / 型号作反证。按口模板单独核对权限与身份；上位机此函数不接受 `port`。
- ⛔ **udev 规则"不再设置某项"时，`change` 触发不会把它还原**（2026-09-23 工程机实测）：
  删掉那条临时 `GROUP="plugdev"` 规则后 `udevadm trigger`（默认 `action=change`）只把
  `MODE` 还原成 666，**属组仍留在 `plugdev`**；要恢复成默认的 `dialout` 得用
  `udevadm trigger --action=add --subsystem-match=tty`。需要恢复属组时须显式还原或重新触发 add，
  不能仅删除 GROUP 后依赖 change。
- **工程机实测（`<HOST_09>`，三台灵猴相机真的插着，2026-09-23，测试后已把规则恢复原样）**：
  · **改绑定之后 `/dev` 节点还在，并且会正确改指**：`camera-left` 从 `fc800000.usb` 改成
    `fc880000.usb`（胸前口）后 `/dev/camera_left → video5`（ID_PATH `platform-fc880000.usb-…`），
    改回来又变回 `video1`（`platform-fc800000.usb-…`）。
  · ⛔ **`udevadm trigger` 只是把事件排进队列就返回** —— 不等它 settle 就去查 `/dev`，会看到
    **旧链接或悬空链接**（第一版实测：`/dev/camera_left` 还指着 `video1`、`udevadm info` 连
    `ID_PATH` 都查不到，下一轮又跳到别的相机）。已改成 `trigger` 之后跑
    `udevadm settle --timeout=10`。
  · ⛔ **"内容没变就不 reload"会让丢掉的节点永远修不回来**：手工 `rm /dev/camera_left` 后再用
    同样的参数调用，旧逻辑直接"没有变化"返回、节点再也不出现。现在会检测该文件声明的符号链接
    是否都在，缺了就**照旧 reload + trigger**（实测能修回来）。
  · 工具现在每次 `set`/`restore` 都会把该文件声明的符号链接**当前指向**打出来
    （`/dev/camera_left → video5`），"节点还在不在"一眼可见；规则没匹配到任何设备时打
    `（当前没有设备匹配这条规则：设备没插 / 口或 SN 不对）`（实测：把口改成一个不存在的
    `fc000000.usb`，`/dev/camera_left` 会被 udev 正确删掉）。
- ⛔ **"每个相机两套 rules"不能同时塞进同一个文件**（工程机实测，试产件）：
  左手的文件里同时放"按 SN `EXAMPLE_ID_06`"和"按口 `fc800000.usb`"两行后，**三台相机（`video1`
  `fc800000`、`video3` xhci、`video5` `fc880000`）全都挂上了 `/dev/camera_left`** ——
  因为三台的 `ID_SERIAL_SHORT` 都是 `EXAMPLE_ID_06`，SN 那条**同时命中三台**；而 `/dev/camera_left`
  只可能指向其中一台（实测 `-> video3`，即最后被处理的那台，顺序还不保证）。
  ⇒ **试产/测试机只能按口**；**生产机（SN 唯一）两行同时放才安全**（两行命中同一台设备、
  同一个名字，不冲突）。所以现在的实现是"每个相机角色两套模板、`serial=` 与 `port=` 二选一"。
- ⛔ 其余两个踩过的坑（都已修）：`current_slot_value` 的 sed 少写一个等号
  （`ENV{ID_SERIAL_SHORT}="` 应为 `}=="`）会静默取不到值；相机目标的槽位要同时列 `SERIAL PORT`，
  否则 `port=` 会被判成"不支持"。
- 验收口径：**按 SN 那一支要用生产机唯一的 SN 测**（现有试产件全是 `EXAMPLE_ID_06`，用它们测就是上面
  第三条那个"三台抢一个名字"的场景）。
