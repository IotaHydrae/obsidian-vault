# 串口命令与后台抓取

> `tools/serial-exec` 使用一个串口连接，同时记录启动日志和执行命令；命令完成以唯一结束标记及远端退出码为准。

## TL;DR

- 默认 `/dev/ttyUSB0`、1500000、8N1。开发机在 `dialout` 组时不用 sudo。
- `capture start` 后台运行并立即返回；`run` 自动复用该连接，`status` / `stop` 在命令执行期间仍可用。
- `--json` 提供机器可读结果。超时返回 4，`exit_code=null`，不把安静或命令回显当成成功。
- 输出目录必须不存在，避免覆盖旧记录；状态放在按用户隔离的 0700 目录。

## 常用命令

```bash
tools/serial-exec list
tools/serial-exec run -c 'systemd-analyze time' --json
# 未登录时；环境变量由调用者提供，不把口令放在命令行。
tools/serial-exec run --login board --password-env LSP_SERIAL_PASSWORD -c 'uname -r' --json
tools/serial-exec run --sudo --password-env LSP_SERIAL_PASSWORD -c 'systemctl --failed' --json

tools/serial-exec capture start --output /tmp/boot-record --duration 90 --json
tools/serial-exec capture status --json
tools/serial-exec run -c 'uptime' --json
tools/serial-exec capture stop --json
```

`--duration 0`（默认）一直记录到停止；正数表示秒数。开始返回 `running` 时串口已经打开。
`--port` / `--baud` / `--state-dir` 可放在子命令前后；同一会话必须使用相同波特率。
仍兼容 `--list`、省略 `run` 的 `-c`、`--login USER:PASS`；旧 `--settle` 接受但不再参与完成判定。
`--help` / `--version` 不创建虚拟环境、状态或打开串口；首次实际使用自动安装 pyserial 到 `.venv`，
可用 `LSP_SERIAL_VENV` 指定已有环境。

## 命令结果与恢复

```json
{"status":"error","tool_exit_code":1,"results":[{"command":"exit 7","exit_code":7,"output":"","truncated":false,"elapsed_s":0.01}],"synchronized":true}
```

| 工具退出码 | 含义 |
| --- | --- |
| 0 | 操作成功；所有远端命令退出码为 0 |
| 1 | 远端命令非零；准确值在 `results[].exit_code` |
| 2 | 参数无效 |
| 3 | 环境/串口/登录错误或会话忙 |
| 4 | 等待截止时间到达；不能确认远端命令完成 |

每条 `-c` 在独立子 shell 内运行，`cd` / `export` 不跨命令保留，`exit` 不会退出登录 shell。
`--sudo` 使用 root shell，在唯一 sudo 提示出现后才发送密码；兼容免密/已缓存凭据的情况。
密码来自 `--password-env` 或兼容的 `--login USER:PASS`，不拼进命令、结果或状态文件。
多条命令遇到非零/超时立即停止。`--timeout` 默认 10 秒，包含登录、shell 同步和该条命令。
输出最多保留约 2 Mi 字符，截断明确返回 `truncated=true`；抓取文件保留完整原始字节。
`output` 是两个标记间收到的文本，会去首尾换行；串口上的并发内核/服务日志也可能混入，不能视作独立 stdout。

超时不会自动杀掉板子上的进程。抓取会话标记 `synchronized=false`，后续 `run` 拒绝发送新命令；
确认可以中断前一条命令后，用 `run --recover -c '…'` 发送 Ctrl-C 并重新同步。
直接运行也先探测 shell 响应，再发送实际命令。重启会断开命令完成协议，建议后台抓日志，
通过 SSH 或现场按钮触发重启。

抓取期间不要再用 minicom/tio/socat 打开同一串口；工具内部用每端口锁、pyserial exclusive
及 Linux `TIOCEXCL` 管理独占。只有一个 RX 线程读串口，并发 `run` 会报忙。
停止、自动到期、SIGTERM 或串口断开都会关闭日志和连接；`status` 保留最后状态，不按旧 PID 杀进程。

## 日志与时间口径

| 文件 | 内容 |
| --- | --- |
| `serial.raw` | 收到的全部原始字节，含 ANSI；不主动记录发送数据 |
| `serial.log` | `[elapsed seconds]` 加去 ANSI 文本；未完成行标记 `[partial]` |
| `serial.jsonl` | 每行的主机 monotonic 纳秒、UTC、相对秒、文本和 `complete` |

时间戳取自**主机收到该行最后一个字节的读取批次**；同一批次多行可以有相同时间戳。
它包含 UART、USB 和主机调度延迟，不是板子写入时刻，也不是墙钟跳变后的板子时间。
UTF-8 跨读取批次解码，末尾未换行文本在停止时落盘。原始串口回显可能含敏感信息，分享前检查日志。
直接运行可用 `--log FILE` 追加 RX；复用抓取时记录在该会话的 `serial.raw`。

## 验证

```bash
.venv/bin/python tests/serial-exec.py
```

PTY 回归覆盖慢输出、退出码、命令隔离、超时恢复、并发控制、日志完整性、自动停止、独占和释放；
无需 root 或开发板。`--keep` 保留测试日志。真实串口验证：后台记录时 `systemd-analyze time` /
HTTP 检查返回完整输出，`exit 7` 返回远端 7，停止后可立即重新打开。
