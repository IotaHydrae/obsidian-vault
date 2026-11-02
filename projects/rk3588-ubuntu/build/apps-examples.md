# 三个 app 的实例细节（`lsp-mcap-view` / `lsp-rk-monitor` / `lsp-usb-map`）

> 目录约定、Makefile 契约、依赖预检、体积规则都在 [apps.md](apps.md)；这里只放**每个 app 自己的东西**
> （需求、依赖、坑、界面约定、回归）。

## 1. `lsp-mcap-view`（终端版 MCAP 预览器）

- **需求**：现场经常**只能 ssh** 进设备，图形端（Lichtblick/Foxglove）用不上；而且 Foxglove 3.x
  闭源且必须登录 ⇒ 自己写一个只读 bag 的终端工具。
- **依赖**：`mcap`（读容器 + 拿 schema 文本）、`ratatui`/`crossterm`（界面）、`memmap2`（大文件不整读进内存）。
  **不依赖 ROS 运行时**：`.msg` 文本从 MCAP 里解析，CDR 按"小端 + 各自尺寸对齐 + 4 字节封装头"自己走。
- ⚠️ **两个必须记住的坑**（都踩过）：
  1. MCAP 里**每个类型一条 Schema 记录**（`PoseStamped` 那条只写 `Pose pose`，`geometry_msgs/Pose`
     在另一条记录里）⇒ 必须把**全文件** schema 合并成一个 registry；只看当前 channel 那份会缺依赖；
  2. `.msg` 里常用**短类型名**（`Pose`、`Header`）⇒ 按"同包优先 + 后缀匹配"解析成全名，
     并给 `pkg/T` 与 `pkg/msg/T` 两种写法都建别名。
- **构建**：`make build` 会自动找 cargo（`sudo` 下 PATH 会丢，所以回退到 `~/.cargo/bin` 并导出
  `RUSTUP_HOME`/`CARGO_HOME`）；x86 上交叉（`--target aarch64-unknown-linux-gnu` + `aarch64-linux-gnu-gcc`），
  板子上原生。产物 ~1 MB（`lto` + `strip` + `panic=abort`）。
- **用法**：`lsp-mcap <bag>` 直接进 TUI；`topics` / `fields <topic>` / `get <path>` 给脚本用；
  message path 支持 `x[0]` 与 `x[:]`（全部通道，CLI 输出 `t,channel,v` 长表）。
- **当 `.mcap` 的默认打开程序**：`files/` 里带
  `usr/share/mime/packages/lsp-mcap.xml`（`application/x-mcap`，glob `*.mcap` + 魔数 `\x89MCAP0`）、
  `usr/share/applications/lsp-mcap.desktop`、`usr/local/bin/lsp-mcap-open`（挑终端跑 TUI，退出后等回车）；
  `make install` 还会在 rootfs 里跑 `update-mime-database`/`update-desktop-database` 并写 `/etc/xdg/mimeapps.list`。
  镜像在安装 deb 后用 overlay 的 `NoDisplay=true` 桌面项覆盖包内文件，隐藏应用启动图标，保留 `.mcap` 双击关联与 CLI。
  ⚠️ 判定关联是否生效用 `gio info -a standard::content-type <file>`（`xdg-mime query filetype` 不走 GIO 探测，会误报）。
- **画线**：稀疏时点对点连线；密集（点数 > 2×画布列数）时按列取 min/max、再**首尾相连成折线** ——
  包络不丢尖峰、线段数只有 O(列数)。⛔ 别回到"每列一条孤立竖线"（那会看起来断线）；
  `cargo test` 的 3 个几何单测（连续性 / ≤2×列数 / NaN 真断口）就是钉这个的。

## 2. `lsp-rk-monitor`（RK3588 硬件占用率监控）

- **需求**：替代 `tools/lsp-rk-monitor.sh`（438 行 bash，TUI 靠"单次写整帧 + 每行 `\033[K`"手工拼 ANSI）。
  重写后：**左栏**实时条形读数，**右栏** scope 风格滚动曲线（5×5 网格 + 左侧刻度 + 参考线 + braille 画线，
  配色/画法与 `lsp-mcap` 同一套）。⛔ **脚本保留**（没编 Rust / 没装二进制时的兜底），**脚本不进镜像**。
- **依赖**只要 `ratatui` + `crossterm`：**不引 clap**（参数集小，手写解析更好核对）、**不引 chrono**
  （帧时间戳就是 `date +%H:%M:%S`）。产物 **725,904 B**（`lto` + `strip` + `panic=abort`，动态链接）。
- **与脚本的兼容性是硬要求**：同参数（`-i/-n/--once/--json/--top/--no-color`）、**JSON 键名与数值口径逐字一致**
  ⇒ 两份输出能直接 diff。板上实测（root）：`cpu_mhz`/`gpu_mhz`/`ddr_mhz`/`mem_total_kb`/`cache_kb`/`swap_*`/
  `npu_load`/`rga_load`/`mpp_cores`/`thermal` 格式**完全相同**；`cpu_pct`/`gpu_pct`/`ddr_pct`/`*_irq`/`mpp_tasks`
  是"两次采样做差"，两份进程采样窗口不重合 ⇒ 数值本来就会差几个点。
- **采样模型**：`Sampler` 持上一帧的累计量（`/proc/stat`、`/proc/interrupts`、`task_count`、`/proc/<pid>/stat`）
  做差；**第一次 `sample()` 只建立基线** ⇒ `--once`/`--json` 也要先等一个 interval 才有速率（与脚本一致）。
- **解析层全是纯函数 + 单测**（`make test` → 17 个），单测输入是**板子上抄回来的原始行**（`/proc/stat`、
  `/proc/interrupts`、`/proc/mpp_service/load`、debugfs 的 rga/npu 输出、`meminfo`、带括号的 `/proc/<pid>/stat`）。
  改解析先跑 `make test`。
- **TUI**（界面文案**全英文**）：**左边是外设列表**（鼠标左键点选、`1`..`9` `0` 直选、`Tab`/`↑↓` 上下移），
  右边画被选中外设的曲线；默认 CPU，**每个核一张独立小图**（2 列 × 4 行，百分比固定 `0~100%` 量程、
  标题 `名字 now max`）。单通道模块（GPU/DDR…）画成带完整坐标轴的大图；`all` 行是所有模块的进度条文本。
  其余键：`a` 总览、`+`/`-` 缩放时间窗（**`+` = 放大 = 窗口变短**，与 `lsp-mcap` 的 `zoom_x` 同向）、
  `s` 散点/连线、`t` 顺带采进程占用、`space` 暂停、`r` 清历史、`q`/`Esc` 退出（Ctrl-C 退出码 130）。
  `LSP_MONITOR_ASCII=1` → 用 Block 画点**并且不画网格**。
- ⛔ **量程：百分比固定 `0~100%`，其余单位才自适应**（`axis_bounds()`）。2026-09-20 之前百分比按窗口自适应
  ⇒ 峰值必然贴顶、空闲核看着像满载；别改回去（`pct_axis_is_always_0_100` 单测 +
  `tests/lsp-rk-monitor-tui.sh` 的 0/25/50/75/100 刻度断言）。
- ⛔ **网格布局的 rect 一定要裁到区域内**：8 张小图塞进 9 行时行高被抬到最小 3 行，第 4 张起就画到区域外，
  **ratatui 会 panic**（`index outside of buffer`，板上 40x12/60x20 实测）。
- ⛔ **`echo 密码 | sudo -S lsp-rk-monitor` 起 TUI 后按键进不去**：Ubuntu 24.04 的 sudo 默认
  `Defaults use_pty`（`/etc/sudoers:15`），sudo 会给命令新开一个 pty，而它自己的 stdin 是那根管道
  ⇒ 键盘不会转发，表现是"画面在刷、按键只被 tty 回显、app 毫无反应"（2026-09-20 踩了）。
  要 root 就用 `ssh -t <板子> sudo lsp-rk-monitor`。
- 接口地图、语义与更多坑：`notes/hardware/rk3588-hw-monitor.md`。

## 3. `lsp-usb-map`（USB 设备 → 控制器 + 物理口）

- **上游是 `tools/lsp-usb-map.sh`**（bash，约 400 行）：**慢且只认本板** —— 为了按"显示宽度"对齐，
  每个单元格都要起 `printf|wc -c` + `printf|tr|wc -c` 四个进程，一次运行 200+ 次 fork（工程机实测 2.02 s）；
  而且在本机（x86）上满屏 "UNKNOWN / 不在本板已知控制器表里"。
- **Rust 版做了两件事**：
  ① **通用**：通用层只用 sysfs/udev（控制器标识、总线号、端口链、速率代次、驱动、`/dev` 节点、`ID_PATH`），
     **只有 device tree 认出是 LubanCat 时**才把端口翻译成丝印口名（`J16A(蓝色下层)`、`Hub.P3(…)`），
     认不出就退回 `Port <端口链>` 且不报错 ⇒ 本机、自研板、开发板同一份二进制；
  ② **快 + 有界面**：udev 属性优先读 `/run/udev/data/c<主>:<次>`（不 fork udevadm）、宽度用 `chars()`
     累加（CJK 记 2 列）、TUI 用 ratatui（↑↓ / Enter 详情 / `/` 过滤 / 自动刷新）。工程机实测 **16 ms**
     （shell 版 0.95 s）。
- **与脚本的兼容性**：同参数（`--once/--json/-v/<filter>`）、**JSON 键名是脚本那套的超集**
  （脚本的键都在，另加 `nodes`/`bus`/`port_path`/`host`/`host_mapped`）；退出码同样 0 / 1 / 2
  （2 = 按板卡跑且发现设备落在映射表外）。**脚本保留作兜底**，不进镜像。
- **解析层是纯函数 + 单测**（`make test` → 11 个）：输入是两台真机抄回来的字段
  （工程机的 `usbdrd3_1/fc400000.usb` 与 x86 的 `pci0000:00/0000:00:14.0`），覆盖设备名判定 / 自然序 /
  platform 截断 / 板级映射与通用回退 / 速率代次 / 厂名去重。
- **回归** `tests/lsp-usb-map.sh`（**PC 与板子通用**）：查"字段齐全 / port_path 与设备名一致 /
  `nodes` 里是完整 `/dev` 路径且真实存在 / 过滤是子集 / 缩进的父设备在同列表 / `--board=auto` 不报错"。
  实测：本机（x86）通过 **5/0**；工程机（LubanCat-5 V2）通过 **6/0**。
- 板级映射依据（口名映射表）：`notes/usb/topology-lubancat-5-v2.md`；排查方法：`notes/usb/device-mapping-method.md`。
