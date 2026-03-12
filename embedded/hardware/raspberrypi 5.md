## CPU

### events per second: 10598.98

```
❯ sysbench cpu --threads=$(nproc) run
sysbench 1.0.20 (using system LuaJIT 2.1.1723681758)

Running the test with following options:
Number of threads: 4
Initializing random number generator from current time


Prime numbers limit: 10000

Initializing worker threads...

Threads started!

CPU speed:
    events per second: 10598.98

General statistics:
    total time:                          10.0003s
    total number of events:              106005

Latency (ms):
         min:                                    0.37
         avg:                                    0.38
         max:                                    4.64
         95th percentile:                        0.38
         sum:                                39981.45

Threads fairness:
    events (avg/stddev):           26501.2500/70.65
    execution time (avg/stddev):   9.9954/0.00
```

## Memory

### 3677.94 MiB/sec

```zsh
❯ sysbench memory run
sysbench 1.0.20 (using system LuaJIT 2.1.1723681758)

Running the test with following options:
Number of threads: 1
Initializing random number generator from current time


Running memory speed test with the following options:
  block size: 1KiB
  total size: 102400MiB
  operation: write
  scope: global

Initializing worker threads...

Threads started!

Total operations: 37666358 (3766214.63 per second)

36783.55 MiB transferred (3677.94 MiB/sec)


General statistics:
    total time:                          10.0000s
    total number of events:              37666358

Latency (ms):
         min:                                    0.00
         avg:                                    0.00
         max:                                    0.04
         95th percentile:                        0.00
         sum:                                 4911.87

Threads fairness:
    events (avg/stddev):           37666358.0000/0.00
    execution time (avg/stddev):   4.9119/0.00
```