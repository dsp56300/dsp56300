#!/bin/sh
# devbench.sh <runs> <bin>...   interleaved, pinned to cpu 3, 2 s per machine; prints "<bin> <avg load %>" per run
cd /tmp/mnmrc
runs=$1; shift
for r in $(seq $runs); do
  for b in "$@"; do
    l=$(MNM_DSP_INTERP=1 taskset -c 3 ./$b os.syx 2 3 2>/dev/null | tail -n 1 | awk '{print $4}')
    echo "$b $l $(cat /sys/class/thermal/thermal_zone0/temp)"
  done
done
