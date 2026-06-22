# Linux head 命令详解

`head` 用于**查看文件开头内容**，默认打印前10行。

## 1. 基础语法

```bash
head [选项] 文件名
```

## 2. 常用参数
### -n 行数：指定显示前N行

```bash
# 显示前5行
head -n 5 test.txt
# 简写（直接数字）
head -5 test.txt
```

### -c 字节数：显示前N个字符/字节

```bash
# 显示文件前100字节
head -c 100 test.txt
# 带单位：K/M/G
head -c 2K test.txt
```

### -q 静默，不输出文件名（多文件时）
### -v 始终打印文件名标题

## 3. 多文件查看

```bash
# 同时看两个文件开头，默认会输出文件名
head file1.txt file2.txt
# 不显示文件名
head -q file1.txt file2.txt
```

## 4. 配合管道 | 使用（高频场景）

```bash
# 查看 ls 输出前10行
ls -l /etc | head

# 查看日志前20行
cat /var/log/syslog | head -20

# 筛选进程前5个
ps aux | head -5
```

## 5. 实用示例
1. 查看配置文件前15行
```bash
head -n 15 /etc/nginx/nginx.conf
```
2. 只看文件前200个字符
```bash
head -c 200 app.log
```
3. 结合 tail 截取中间一段（第11~20行）
```bash
head -20 test.txt | tail -10
```

## 补充对比
- `head`：文件头部
- `tail`：文件尾部
- `less`：分页浏览完整文件

需要我给你整理一份 `head + tail + grep` 日志排查常用组合命令吗？