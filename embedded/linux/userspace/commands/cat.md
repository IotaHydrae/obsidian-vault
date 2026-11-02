# Linux cat：查看与拼接文件

> `cat` 按顺序输出文件内容；重定向可以合并或追加文件，`>` 会覆盖目标文件。

```bash
cat file.txt
cat a.txt b.txt > combined.txt
cat extra.txt >> combined.txt
cat -n file.txt
cat -b file.txt
cat -s file.txt
cat -ET file.txt
```

| 选项 | 用途 |
| --- | --- |
| `-n` | 给所有行编号 |
| `-b` | 只给非空行编号 |
| `-s` | 将连续空行压缩为一行 |
| `-E` / `-T` | 显示行尾 `$` / 将 Tab 显示为 `^I` |
| `-A` | GNU cat 的 `-vET` 组合，辅助检查非打印字符 |

不指定文件时读取标准输入，终端中可用 Ctrl+D 结束。交互创建文件可使用 `cat > newfile.txt`，追加则使用 `>>`。

长文件优先用 `less file.txt`，过滤或计数可直接用 `grep pattern file.txt`、`wc -l file.txt`。不要直接向终端输出未知二进制数据；拼接目标也不能是输入文件之一，否则重定向可能先截断输入。
