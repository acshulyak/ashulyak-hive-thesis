#Simple L1i simulator

This c++ script run trace_gen traces through a simple L1i simulator.

##Dependencies
- zlib

##Compile
```
g++ -X3 -std=c++11 -lz simple_l1i.cpp -o uarch_indep_metrics
```

##Run
NOTE: Traces generated by the trace_gen pintool must be gzipped first.
```
./simple_l1i <trace-file>.gz <result-output-file>
```

To run the script on all gzipped trace files in a folder execute
```
./run_all_files.sh <directory-location> ./uarch_indep_metrics
```

##Output
- Total Instructions
- Total Requests
- Total Misses
- Total Replacements

##Options
- N1L prefetching can be enabled in the code by assigning N1L to the prefetch_type variable as opposed to NONE. prefetch_type assignment can be found on line 82.
- The cache configuration can be changed by altering the parameters in the cache_config struct initialized on line 68.