#!/usr/bin/env python3
# bench.py <runs> <bin>...  -- copies nothing; runs devbench.sh on the Force and prints median/min/max per binary
import subprocess, sys, statistics, collections
runs, bins = sys.argv[1], sys.argv[2:]
out = subprocess.run(['ssh', 'root@192.168.1.44', 'sh /tmp/mnmrc/devbench.sh ' + runs + ' ' + ' '.join(bins)],
                     capture_output=True, text=True).stdout
r = collections.defaultdict(list); temps = []
for l in out.split('\n'):
    f = l.split()
    if len(f) == 3 and f[1].endswith('%'): r[f[0]].append(float(f[1][:-1])); temps.append(int(f[2]) / 1000)
for b in bins:
    v = r[b]
    print(f'{b:14s} median {statistics.median(v):6.1f}%  min {min(v):6.1f}  max {max(v):6.1f}  (n={len(v)})' if v else f'{b}: no data')
if temps: print(f'SoC temp {min(temps):.1f}-{max(temps):.1f} C')
