#!/bin/bash
sudo python3 ../external/pmu-tools/ocperf.py record -e raw_syscalls:sys_enter -e raw_syscalls:sys_exit ./data/trace.data -- ./build/src/RUN_PEBS
