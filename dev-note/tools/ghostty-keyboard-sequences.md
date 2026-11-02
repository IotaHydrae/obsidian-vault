# Shift+Enter 显示乱码：先捕获按键序列，再绑定目标程序

> 终端编码按键，shell、复用器或 TUI 解释字节；绑定应放在实际需要换行的那一层。

## TL;DR

- `;2;13~` 可能是扩展按键序列未被完整解释的尾部，只凭显示内容还不能确认来源。
- 先捕获当前路径的原始字节，再核对目标程序和键图。
- 全局把 Shift+Enter 改成普通回车，可能把原本的换行操作变成提交。

## 捕获实际输入

在要排查的终端中运行；按一次目标组合键后退出并恢复终端设置：

```python
import os
import select
import sys
import termios
import tty

fd = sys.stdin.fileno()
original = termios.tcgetattr(fd)
try:
    tty.setraw(fd)
    print("Press one key combination", end="\r\n", flush=True)
    data = os.read(fd, 1)
    while select.select([fd], [], [], 0.2)[0]:
        data += os.read(fd, 64)
finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, original)
print(data.hex(" "), repr(data))
```

采集窗口只用于短按键序列，慢速分片需调整。直连、SSH、tmux 和应用启用键盘协议后的环境可能不同；独立脚本验证的是该会话状态，TUI 运行时的协议还需用其诊断功能或相应捕获手段确认。

例如 `ESC[27;2;13~` 可表示 xterm 风格的修改键回车；另一类协议可能使用 `ESC[13;2u`。含义和是否启用由当前协议决定，不能看到类似尾部就假定所有环境相同。

## Zsh 中插入换行的例子

在确认实际收到第一种序列、且目标是 ZLE 编辑缓冲区后，可使用专门 widget：

```zsh
insert-newline() { LBUFFER+=$'\n'; }
zle -N insert-newline
bindkey -M emacs $'\e[27;2;13~' insert-newline
```

键图示例是 `emacs`；使用 vi 模式时按实际键图配置。先在临时 `zsh -f` 会话验证，再持久保存；恢复时移除对应绑定或还原备份。

若目标是某个 TUI 的输入框，应检查它支持的协议及快捷键。shader 作用于显示，不负责解释按键；两个现象在时间上相邻也不能证明因果。

参考：[Ghostty 修改键兼容讨论](https://github.com/ghostty-org/ghostty/discussions/3151)、[Ghostty 按键绑定](https://ghostty.org/docs/config/keybind)、[Zsh ZLE](https://zsh.sourceforge.io/Doc/Release/Zsh-Line-Editor.html)。
