# Linux head：查看文件开头

> `head` 默认输出前 10 行；`-n` 指定行数，`-c` 指定字节数。

```bash
head -n 5 file.txt
head -c 100 file.txt
head -q file1.txt file2.txt
head -n 20 file.txt | tail -n 10
```

- 最后一条命令取第 11–20 行；文件不足 20 行时输出按实际内容确定。
- 多文件输入默认带文件名标题，`-q` 隐藏，`-v` 强制显示。
- `-c` 按字节截取，可能切断 UTF-8 字符；不是“字符数”。
- GNU head 的大小后缀中 `K` 表示 1024、`KB` 表示 1000；其他实现需查本机说明。

日志尾部使用 `tail`，分页阅读使用 `less`。
