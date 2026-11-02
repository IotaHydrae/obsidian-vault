# Chromium 首次启动的两个弹窗、书签栏三个默认收藏（怎么关掉的）

> 本文是 [chromium-rkmpp.md](chromium-rkmpp.md) 的**首启默认项篇**：kiosk 第一次启动会弹的两个窗口、
> 书签栏里那三个默认收藏，分别是怎么在镜像里关掉/去掉的（含为什么必须这么做）。
> 浏览器本身的硬解能力、开关、集成与实测见主篇。
> ⚠️ 正文**原样搬自主篇的 §5.1~§5.2**（节号保留）。

## TL;DR

- 首启两个弹窗（默认浏览器询问 / "恢复页面"之类）**已在镜像里关掉**（§5.1）——
  kiosk 场景下弹窗会盖住现场页面。
- 书签栏三个默认收藏**已在镜像里去掉**（§5.2）；改法与验证命令都在本节。


### 5.1 首次启动的两个弹窗（2026-09-14，已在镜像里关掉）

新刷的机器第一次开 Chromium 会**连着弹两个必须点确认的窗**，把基于 web 的上位机挡在门外
（用户 2026-09-14 报的）。两个弹窗原因不同、修法也不同，**各关各的，少一个都会卡**：

| 弹窗 | 谁弹的 | 根因 | 关法 |
| --- | --- | --- | --- |
| **Chromium Additional Terms of Service**（正文是 "This Space Intentionally Blank / In official builds this space will show the terms of service"） | Chromium | `chromium 151.0.7922.169` 的回归：Debian [#1145071](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1145071)，官方在 `151.0.7922.173` 用 `ungoogled/disable-tos-dialog.patch` 修掉；我们这份包还是 `.169` | 构建脚本给包自带的 `/etc/chromium/master_preferences` 补 `distribution.require_eula: false`（Debian 报告者给的绕法） |
| **Choose password for new keyring** | gnome-keyring | autologin（`board` 无密码）→ **没有登录钥匙环**；Chromium 通过 libsecret 找 `org.freedesktop.secrets`，没有 collection 就请求新建 → gnome-keyring 弹窗要密码。不填**每次开机都会再弹** | `overlay/etc/chromium.d/99-lsp-flags` 里加 `--password-store=basic`：用 Chromium 自己的口令库，完全不碰 libsecret |

**判据：看 profile 状态，不用盯屏幕。** 干净 `--user-data-dir` 起来后看 `<dir>/Local State` 的
`browser.first_run_finished`（首次启动流程走完才会置上；被弹窗挡住就一直不置位）：

| 条件 | `first_run_finished` | `EULA Accepted` 哨兵 | 结论 |
| --- | --- | --- | --- |
| 什么都不加（基线，已复现） | **不置位**（Local State 才 801 B，连 `First Run` 都没有） | 无 | 卡在 ToS 弹窗 |
| 只加 `require_eula=false` | `true` | 无（弹窗根本没出现，自然不用点） | ToS 已关 |
| 两个都加 | `true` | 无 | 两个都关 ✓ |

**D-Bus 证据**（开发板**真实会话环境**抓的，`dbus-monitor --session`）：

| 条件 | `org.freedesktop.secrets` 调用 | 结果 |
| --- | --- | --- |
| 不加 `--password-store=basic` | `ReadAlias` → **`CreateCollection`** → `/org/freedesktop/secrets/prompt/p4` | 弹窗，首次启动卡住 |
| 加 `--password-store=basic` | **0 条**（日志里剩的 4 行只是 dbus-monitor 打印的名字列表） | 正常完成首次启动 |

⚠️ **怎么才能"在真实会话里"测**：从 ssh 起浏览器时，这个会话的 D-Bus **不在**
`/run/user/1000/bus`，而在 `/tmp/dbus-XXXX`；要把它从 `gnome-shell` 进程里取出来再导出，
否则浏览器根本不在会话里、密钥环路径压根不会被触发（我第一轮就是这么被误导的，A/B 两边都是 0）：

```bash
SH=$(pgrep -x gnome-shell | head -1)
export DBUS_SESSION_BUS_ADDRESS=$(tr '\0' '\n' < /proc/$SH/environ | sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p')
export XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 XDG_CURRENT_DESKTOP=ubuntu:GNOME
```

**别用这些"想当然"的办法**：

- `--no-first-run`：**不管 ToS**（那个由 master prefs 的 `require_eula` 决定，实测加上它也没用）。
- 整份覆盖 `/etc/chromium/master_preferences`：包里那份还有 `skip_first_run_ui`、关 signin/
  safebrowsing、DuckDuckGo 搜索源一整套，覆盖会丢。构建脚本的做法是**只补一个键**：
  幂等 + grep 守卫 + 插完校验 JSON 合法（坏了就地回滚）。
- 改包自带的 `/etc/chromium.d/default-flags` 加 flag：那文件带 RKMPP 硬解开关，覆盖就变**软解**。
  新开关一律写自己的 `99-lsp-flags`（包装脚本 source `/etc/chromium.d/*`，每个文件都是往
  `CHROMIUM_FLAGS` 追加，顺序无关）。
- 删/停 gnome-keyring：会影响别的东西，而且 Ubuntu 对同类问题
  （[bug #2073356](https://bugs.launchpad.net/ubuntu/+source/chromium-browser/+bug/2073356)）
  标的是 **Won't Fix**（属 gnome-keyring + 无密码会话），只能在浏览器侧关。

**维护提示**：以后把 Chromium 升到 `151.0.7922.173` 或更新（Debian 已带
`disable-tos-dialog.patch`）后，`require_eula` 那段补丁可以删；留着也无害（幂等）。
升级时如果包里的 `master_preferences` 结构变了，构建脚本会打
`[warn] ★ require_eula 没写进去…已回滚`，别忽略。

`tools/verify-image.sh` 的「Chromium（RKMPP 硬件加速）」分组里有 3 项对应检查：
开关文件在不在、`password-store=basic` 在不在、`require_eula=false` 在不在。

### 5.2 书签栏里的三个默认收藏（2026-09-15，已在镜像里去掉）

新 profile 的书签栏会凭空多出 **Debian.org / Latest News / Help**（用户 2026-09-15 报的）。
它们**不是** Chromium 上游的默认书签，而是 **Debian 包的**：

| 谁 | 在哪 | 作用 |
| --- | --- | --- |
| 书签内容 | `/usr/share/chromium/initial_bookmarks.html`（Netscape 书签格式，三条 `<A HREF=…>`）| 建 profile 时导入的源文件 |
| 触发开关 | `/etc/chromium/master_preferences` 的 `distribution.import_bookmarks_from_file` | 指向上面那个文件；**`import_bookmarks` 本来就是 `false`，真正起作用的是这个键** |

**关法与实测**（开发板真实会话 + 干净 `--user-data-dir`，看 `<dir>/Default/Bookmarks`）：

| 配置 | 结果 |
| --- | --- |
| 原始（HTML 有 3 条 + master_preferences 引用它） | `bookmark_bar` 下正好 `Debian.org` / `Latest News` / `Help` 三条 |
| 只删 `import_bookmarks_from_file`（HTML 原样） | 不再生成 `Default/Bookmarks`（0 条）|
| 再清空 HTML（两处都改） | 同上（0 条）|

构建脚本两处都做（双保险，换包/升级也漏不掉），都在 chromium 补丁块里：

1. 用 `python3` 把这一个键从 `master_preferences` 里删掉 —— **必须用 JSON 而不是 `sed` 删行**
   （纯文本删行会留尾逗号或少逗号，JSON 一坏 Chromium 会整份忽略，连 `skip_first_run_ui`、
   关 signin 那些一起失效）。改完校验 JSON，坏了就从 `.lsp-bak` 回滚。
2. 把 `initial_bookmarks.html` 换成一个合法但没有任何书签的文档（只留 `DL` 骨架）+ 一行注释。

**已经建好的 profile 不会自己变**（导入只在建 profile 时发生）。要清掉现成机器上的三条：
关掉 Chromium，删掉 `~/.config/chromium/Default/Bookmarks` 里对应三个节点后启动；
或者干脆在书签栏上右键删除（只影响这一台）。
`tools/verify-image.sh` 有 2 项检查：HTML 里没有 `HREF=`、`master_preferences` 不再引用它。
