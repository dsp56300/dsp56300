#!/bin/sh
# prof.sh <tag>: build the recompiled mnm-bench with -g, profile it on the Force, attribute samples to inlined source
set -e
cd /tmp/claude-1000/-home-sam/1c041322-d8fa-4956-ae22-b5802f74ad7e/scratchpad
docker run --rm --user $(id -u) -v "$PWD":/s mnm-armhf-builder sh -c 'ninja -C /s/b-rc-arm-g mnm-bench >/dev/null 2>&1; arm-linux-gnueabihf-strip --strip-debug -o /s/bench-gs /s/b-rc-arm-g/mnm-bench'
scp -q bench-gs root@192.168.1.44:/tmp/mnmrc/
ssh root@192.168.1.44 'cd /tmp/mnmrc && export LD_LIBRARY_PATH=/tmp/mnmrc/usr/lib/arm-linux-gnueabihf:/tmp/mnmrc/lib/arm-linux-gnueabihf && MNM_DSP_INTERP=1 taskset -c 3 ./usr/bin/perf record -F 8000 -o pg.data ./bench-gs os.syx 2 3 >/dev/null 2>&1; ./usr/bin/perf script -i pg.data -F ip,sym 2>/dev/null | awk "{print \$1}" | sort | uniq -c | sort -rn' > pg-ips.txt
awk '{printf "0x%x %s\n", strtonum("0x"$2)-0x400000, $1}' pg-ips.txt | awk '$1 !~ /^0xb/' > pg-addr.txt
cut -d' ' -f1 pg-addr.txt > pg-addrs-only.txt
cp b-rc-arm-g/mnm-bench bench-g
docker run --rm --user $(id -u) -v "$PWD":/s mnm-armhf-builder sh -c 'arm-linux-gnueabihf-addr2line -e /s/bench-g -i -f -C -a @/s/pg-addrs-only.txt > /s/pg-lines.txt'
python3 profagg.py
