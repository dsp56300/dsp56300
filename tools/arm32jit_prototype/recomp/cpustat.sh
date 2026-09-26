#!/bin/sh
# cpustat.sh <seconds>: per-CPU busy % over the interval, from /proc/stat
s=$1
a=$(grep '^cpu[0-9]' /proc/stat); sleep $s; b=$(grep '^cpu[0-9]' /proc/stat)
echo "$a
--
$b" | awk 'BEGIN{p=0} /^--/{p=1;next} {n=$1; t=0; for(i=2;i<=NF;i++)t+=$i; idle=$5+$6; if(!p){T0[n]=t;I0[n]=idle}else{dt=t-T0[n]; di=idle-I0[n]; printf "%s %.0f%% busy\n", n, 100*(dt-di)/dt}}'
