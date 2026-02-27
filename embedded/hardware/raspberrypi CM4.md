## sysbench

### CPU

arm_freq = 1.5GHz

```text
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