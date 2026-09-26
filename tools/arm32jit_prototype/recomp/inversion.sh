#!/bin/sh
# one engine at SCHED_FIFO 25 on core 1, MPC-like RR-20 load on the same core: who misses?
cd /tmp/mnmm; : > inversion.out
for m in "DDRW" "SWAVE SAW" "GND SIN"; do
 for duty in 10 20 30 40 50; do
  ./busyload 22 $duty 20 1 > bg.tmp & B=$!
  r=$(MNM_DSP_INTERP=1 MNM_CPUS=1 ./bench-multi os.syx --engines 1 20 "$m" 25 2>&1 | grep -E "slowest worker" | tr '\n' ' ')
  wait $B
  echo "$m | otherload=$duty% | engine: $r | $(cat bg.tmp)" >> inversion.out
 done
done
echo DONE >> inversion.out
