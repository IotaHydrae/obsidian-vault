# labwc ScreenCast：验证真实 PipeWire 画面

> ScreenCast 成功需要把门户请求、返回节点和独立受控窗口的像素串成同一次证据。

## TL;DR

- 串联 CreateSession/SelectSources/Start response、真实 FD/节点和已知红/蓝窗口画面。
- 原始 buffer 的旋转由 `SPA_META_VideoTransform` 描述；先应用元数据，再与横屏窗口比较。
- 节点、任意变化或帧数不等于正确画面或屏幕 FPS；测试先确认桌面未锁。

## 方法：用已知窗口验证整条取帧路径

适用于 Ubuntu CM4、labwc/wlroots 0.20.2、wlr portal 与 PipeWire。

1. 在未锁定的普通用户会话中，用独立 GTK DrawingArea 显示带标题的已知红/蓝窗口。
2. 独立 Gio 客户端依次完成 CreateSession、SelectSources、Start，记录各次 response。
3. 使用门户返回的真实 FD 连接对应 PipeWire 节点，核对同一次请求的节点身份。
4. 保存红/蓝两种对应帧，按 SPA transform 解释像素方向，审阅受控标题和内容。
5. 关闭自有 request/session、节点、窗口与进程，保留正常门户服务和用户配置。

判据是请求、节点与已知窗口内容一致。若只得到锁屏画面、任意变化或其他窗口，
只能记录实际采集对象，不能把它计为目标窗口验证成功。

## 原因：buffer 布局与桌面方向属于不同层次

PipeWire 交付的原始像素可以保留面板方向，由变换元数据描述逻辑方向；
直接打开未变换的原图不足以判断桌面旋转是否正确。
协商帧率、客户端收帧计数和屏幕刷新率也有不同语义，没有性能规格时只报告观察值。

## CM4 设备观察（2026-10-24）

同一 CM4 启动中分别测试 Noctalia 和 Waybar：三个门户请求均 response=0，
真实 FD/节点一致；保存的红/蓝帧均含受控标题窗口。

| 外壳 | 观察窗口 | 客户端收到的帧数 |
| --- | --- | ---: |
| Noctalia | 8 秒 | 75 |
| Waybar | 8 秒 | 64 |

计数仅描述本轮采样，不推导性能合格线。逻辑区域为 853×480；
RGBx buffer 为 720×1280、stride 2880，附 `SPA_META_VideoTransform=3`。
应用该变换后与逻辑横屏一致，协商 fps=0/1 不等于屏幕刷新率。

首轮自动锁屏后虽取到变化的锁屏帧，但目标窗口不可见，未计为成功；
解锁后才完成已知窗口验证。没有为测试修改生产门户配置或锁屏策略。

## 证据与未验证范围

证据位于 `~/.cache/uconsole-desktop-20261024/functional/screencast/` 及相邻 `waybar/`，
包含独立探针、请求记录、红/蓝 PNG 与清理记录。
浏览器/OBS、远程会议、共享音频、多输出和其他核心板尚未验证。
Screenshot 走外部 grim/slurp，必须[单独验证](screenshot-portal.md)，不能由本结果代替。
