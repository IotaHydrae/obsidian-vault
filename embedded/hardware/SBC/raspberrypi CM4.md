## sysbench

### CPU

#### 1.5GHz

events per second:  5855.53

```bash
pi@cm4-pi:~ $ sysbench cpu --threads=$(nproc) run
sysbench 1.0.20 (using system LuaJIT 2.1.1723681758)

Running the test with following options:
Number of threads: 4
Initializing random number generator from current time


Prime numbers limit: 10000

Initializing worker threads...

Threads started!

CPU speed:
    events per second:  5855.53

General statistics:
    total time:                          10.0006s
    total number of events:              58582

Latency (ms):
         min:                                    0.68
         avg:                                    0.68
         max:                                    1.42
         95th percentile:                        0.68
         sum:                                39978.13

Threads fairness:
    events (avg/stddev):           14645.5000/3.84
    execution time (avg/stddev):   9.9945/0.00
```

#### 2.0 GHz

```bash
pi@cm4-pi:~ $ sysbench cpu --threads=$(nproc) run
sysbench 1.0.20 (using system LuaJIT 2.1.1723681758)

Running the test with following options:
Number of threads: 4
Initializing random number generator from current time


Prime numbers limit: 10000

Initializing worker threads...

Threads started!

CPU speed:
    events per second:  7803.38

General statistics:
    total time:                          10.0005s
    total number of events:              78062

Latency (ms):
         min:                                    0.51
         avg:                                    0.51
         max:                                   12.56
         95th percentile:                        0.51
         sum:                                39979.49

Threads fairness:
    events (avg/stddev):           19515.5000/26.46
    execution time (avg/stddev):   9.9949/0.00
```

### Memory

1967.24 MiB/sec

```bash
pi@cm4-pi:~ $ sysbench memory run
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

Total operations: 20152878 (2014457.12 per second)

19680.54 MiB transferred (1967.24 MiB/sec)


General statistics:
    total time:                          10.0001s
    total number of events:              20152878

Latency (ms):
         min:                                    0.00
         avg:                                    0.00
         max:                                    0.24
         95th percentile:                        0.00
         sum:                                 4806.83

Threads fairness:
    events (avg/stddev):           20152878.0000/0.00
    execution time (avg/stddev):   4.8068/0.00
```