# 登出后 IPC 对象消失：logind 与进程生命周期

> 用户会话结束、服务进程退出和 IPC 名称删除是不同生命周期；先证明触发链，再改变清理策略。

## TL;DR

- 区分 POSIX/System V IPC、名称与已打开映射，删除名字不必然立即撤销现有映射。
- 检查合并配置与 logind 运行值，两者可能因服务尚未重载而不同。
- linger 影响用户 manager 生命周期，不等价于全局 RemoveIPC 策略。
- 本仓库没有预置全局 RemoveIPC=no，也没有迁入外部工程的业务修复脚本。

## 最小诊断

```sh
systemd-analyze cat-config systemd/logind.conf
loginctl show-user <USER> -p Linger -p State
loginctl list-sessions
ipcs -m
ipcs -s
```

这些命令查看配置与对象，部分信息受权限限制。检查用户 UID、服务所属用户和实际会话，
不要假定 SSH 登录账户与后台服务账户相同。
读取 `/proc/PID/maps`、文件描述符与 `/dev/shm` 可关联对象，但路径列表可能包含私密信息，分享前脱敏。

## 有效对照

在隔离测试账户中创建无业务影响的 canary，记录对象身份、权限与 owner。
分别检查登出前、最后会话退出后、后台服务重启后的状态，保持服务版本与输入不变。
确认观测链能识别已知存在/被删除的对象，不能只因目录为空就推断清理根因。
全局策略改变可能扩大其他用户 IPC 的保留时间，必须按实际服务生命周期设计。

## 修复层次

若服务应独立于交互会话，优先检查其 system/user unit 所有权与启动方式。
linger、RemoveIPC 和应用主动 unlink 各自处理不同问题，不能互相替代。
已有进程仍持有被删除对象时，新进程可能创建另一个同名对象，导致双方使用不同资源；
改变策略不自动恢复这类分裂，恢复动作需由应用契约决定。

## 依据与边界

参考 [logind.conf](https://www.freedesktop.org/software/systemd/man/latest/logind.conf.html)
及目标 systemd/IPC 实现。具体 UID 范围、系统用户例外和发行版默认值按目标版本核对。
本文仅提炼通用故障模型，没有本次设备实测或默认策略变更。
