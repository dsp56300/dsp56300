# gearmulator study (Osirus / Vavra / Xenia / Nord Lead 2x on the Force)

Results: `docs/ARM32_JIT.md`, "Gearmulator synths on the Force".

- `gearmulator-glue.patch`: apply to `dsp56300/gearmulator` (main @ 9710c1f) after replacing
  `source/cpu/dsp56300` with this fork. Drops the JIT-only calls (`supportBranchAtLoopEnd`, audio workgroup).
- `gm_probe.cpp` + `CMakeLists.txt`: copy to `source/gmstudy/`, add `add_subdirectory(gmstudy)` to `source/CMakeLists.txt`.
  `gm_probe_{virus,mq,xt,n2x} <romdir> [seconds]` runs a scripted workload, prints the output hash, executed DSP
  instructions per audio second and wall/real-time; `GM_HOT=<n>` lists the hottest DSP addresses.
- Build flags: `-DDSP56K_NO_JIT_RUNTIME -DDSP56K_EXEC_STATS -DDSP56K_INTERP_DEFAULT -DDSP56K_INTERP_CYCLES`.
- ROMs are never committed. Note the user's microQ 2.23 dump was byte-swapped (swap each byte pair).
