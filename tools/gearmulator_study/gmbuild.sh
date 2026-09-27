#!/bin/sh
# usage: gmbuild.sh <builddir> <extra cxx flags> <targets...>   (x86 image)
B=$1; F=$2; shift 2
cd /tmp/claude-1000/-home-sam/914a40e2-1c90-484f-964e-e7d331854be4/scratchpad
docker run --rm -v $PWD:/w -w /w mnm-x86-builder sh -c "cmake -S gearmulator -B $B -DCMAKE_BUILD_TYPE=Release -Dgearmulator_BUILD_JUCEPLUGIN=OFF -Dgearmulator_BUILD_JUCEPLUGIN_CLAP=OFF -DCMAKE_CXX_FLAGS='$F' > $B.cfg.log 2>&1; cmake --build $B -j14 --target $* -- -k > $B.build.log 2>&1; grep -E 'error' $B.build.log | sort -u | head -20"
