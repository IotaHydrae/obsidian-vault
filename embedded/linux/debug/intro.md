
|层级|目标|工具|
|---|---|---|
|系统级|CPU、内存、IO瓶颈|top, vmstat, iostat, sar|
|进程级|哪个进程慢|top, htop, pidstat, perf|
|函数级|哪个函数慢|perf, ftrace, eBPF|
|内核级|驱动 / syscall 慢|ftrace, kprobe, bcc, eBPF|
