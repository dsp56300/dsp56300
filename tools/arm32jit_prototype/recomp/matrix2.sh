#!/bin/sh
# engines (one per core, cores 1,2,3) x background duty (RR 20, like MPC's AudioWorkers) x engine priority
cd /tmp/mnmm; : > matrix2.out
for m in "DDRW" "SWAVE SAW"; do
 for duty in 0 25 50; do
  for prio in 15 25; do
   for n in 1 2 3; do
    cpus=$(echo 1,2,3 | cut -d, -f1-$n)
    if [ $duty -gt 0 ]; then ./busyload 16 $duty 20 1,2,3 & B=$!; fi
    r=$(MNM_DSP_INTERP=1 MNM_CPUS=$cpus ./bench-multi os.syx --engines $n 8 "$m" $prio 2>&1 | grep -E "slowest worker" | tr '\n' ' ')
    [ $duty -gt 0 ] && { kill $B 2>/dev/null; wait $B 2>/dev/null; }
    echo "$m | bg=$duty% | prio=$prio | n=$n | $r" >> matrix2.out
   done
  done
 done
done
echo DONE >> matrix2.out
