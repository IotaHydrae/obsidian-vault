## sysbench

### CPU

```bash
debian@BeagleBone:~$ sysbench cpu --threads=$(nproc) run
sysbench 1.0.20 (using system LuaJIT 2.1.1700206165)

Running the test with following options:
Number of threads: 1
Initializing random number generator from current time


Prime numbers limit: 10000

Initializing worker threads...

Threads started!

CPU speed:
    events per second:    39.61

General statistics:
    total time:                          10.0108s
    total number of events:              397

Latency (ms):
         min:                                   25.00
         avg:                                   25.20
         max:                                   28.90
         95th percentile:                       25.74
         sum:                                10003.40

Threads fairness:
    events (avg/stddev):           397.0000/0.00
    execution time (avg/stddev):   10.0034/0.00
```

### Memory

```bash
debian@BeagleBone:~$ sysbench memory run
sysbench 1.0.20 (using system LuaJIT 2.1.1700206165)

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

Total operations: 1513293 (151155.32 per second)

1477.83 MiB transferred (147.61 MiB/sec)


General statistics:
    total time:                          10.0003s
    total number of events:              1513293

Latency (ms):
         min:                                    0.00
         avg:                                    0.00
         max:                                    2.73
         95th percentile:                        0.00
         sum:                                 3954.38

Threads fairness:
    events (avg/stddev):           1513293.0000/0.00
    execution time (avg/stddev):   3.9544/0.00
```