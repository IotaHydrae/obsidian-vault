# awk：按字段筛选与汇总文本

> `awk` 逐行读取文本、按字段处理并输出结果，适合检查日志和简单表格；复杂 CSV、JSON 应使用对应格式的解析器。

## TL;DR

- `awk '条件 {动作}' 文件` 按记录执行；`$0` 是整行，`$1` 是第一字段，`NF` 是字段数。
- 默认按空白分字段；`-F` 可指定简单分隔符，带引号和换行的 CSV 不能可靠地用 `-F,` 通用解析。
- `BEGIN` 在读取前执行，`END` 在所有输入后执行；不要把同一个文件同时作为输入和 shell 重定向输出。
- Linux 发行版可能使用不同 awk 实现；脚本优先使用 POSIX 语法，扩展特性需核对实际实现。

## 常用例子

```sh
# 打印非空记录的第一、第三字段
awk 'NF >= 3 { print $1, $3 }' input.txt

# 筛选包含 error 的行，并显示输入行号
awk '/error/ { print NR, $0 }' input.log

# 对第二列数值求和并统计记录数
awk 'NF >= 2 { sum += $2; count++ } END { print count, sum }' input.txt
```

`NR` 是所有输入的累计行号，`FNR` 在每个输入文件开始时重置。数值转换会依 awk 规则处理文本；对格式不可信的数据应先验证字段，而不是把所有转换结果当成有效测量值。

## 简单分隔符

```sh
# 对没有引号、转义或嵌入换行的简单逗号分隔数据
awk -F, 'NR > 1 && NF >= 3 { print $1, $3 }' measurements.txt
```

这不是通用 CSV 解析示例；需要处理引号中的逗号、双引号转义或多行字段时，应选用 CSV 解析器。`input.txt` 等名称只是示例；shell 管道中的变量需要正确引用，避免将不可信内容拼进 awk 程序。

## 参考

- [POSIX awk](https://pubs.opengroup.org/onlinepubs/9799919799/utilities/awk.html)：语法、变量与可移植行为。
