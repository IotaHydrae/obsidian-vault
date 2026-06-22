# Linux cat 命令完整教程

`cat` 全称 concatenate，核心作用：**读取文件、打印内容、合并文件、新建文件**。

## 一、基础语法
```bash
cat [选项] 文件名
```

## 二、最常用用法
### 1. 查看文件内容
```bash
cat test.txt
```

### 2. 一次性查看多个文件
```bash
cat a.txt b.txt c.txt
```

### 3. 创建新文件（写入内容）
```bash
cat > newfile.txt
# 输入文字，按 Ctrl+D 保存退出
```
追加内容（不覆盖）：
```bash
cat >> newfile.txt
```

### 4. 文件合并
把 a、b 内容写入 all.txt
```bash
cat a.txt b.txt > all.txt
```

### 5. 标准输入直接输出（无文件时）
```bash
cat
# 输入文字回车，Ctrl+D 结束回显
```

## 三、高频参数（options）
| 参数 | 作用 |
|------|------|
| `-n` | 显示所有行号（包括空行） |
| `-b` | 仅给非空行编号 |
| `-s` | 压缩连续多行空白为一行 |
| `-E` | 每行末尾显示 `$` |
| `-T` | Tab 制表符显示为 `^I` |
| `-A` | 等价 `-vET`，显示所有隐藏符号 |

示例：
```bash
cat -n test.txt    # 带全部行号
cat -b test.txt    # 不空行编号
cat -s test.txt    # 合并空行
cat -ET test.txt   # 显示$和Tab符号
```

## 四、管道搭配使用
1. 分页查看长文件（配合 less/more）
```bash
cat log.txt | less
cat log.txt | more
```
2. 过滤关键字（grep）
```bash
cat nginx.log | grep error
```
3. 统计行数
```bash
cat test.txt | wc -l
```

## 五、常见小技巧
1. 清空文件内容（不删文件）
```bash
cat /dev/null > test.txt
```
2. 复制文件（等价 cp）
```bash
cat source.txt > dest.txt
```
3. 反向查看文件不用 cat：用 `tac`
```bash
tac test.txt
```

## 六、注意事项
- `cat > file` 会**覆盖**原有内容；`>>` 才是追加
- 超大日志文件不建议直接 `cat`，会刷屏，优先用 `less`
- 二进制文件慎用 cat，会输出乱码破坏终端显示

需要我给你整理一份可直接复制的 cat 常用命令速查表吗？