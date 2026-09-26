# Static-recompilation gate test

Translates Stage 1's sine-table loop body (P:$100091-$10009a) ahead of time into C++ that calls the
interpreter's own handlers with constant opcodes, compiled into `dsp.cpp`'s translation unit so GCC
inlines and constant-folds them. See `docs/ARM32_JIT.md`, "Static recompilation".

1. Build `mnm_recomp.cpp` (copy into schwung-monomodule's `tools/bench/`, add
   `add_executable(mnm-recomp tools/bench/mnm_recomp.cpp)` +
   `target_link_libraries(mnm-recomp PRIVATE mnmcore ${CMAKE_DL_LIBS})`) for x86, then run
   `mnm-recomp os.syx --gen > gen.txt`. This lists the handlers the interpreter resolved, as offsets.
2. `nm -C <that x86 mnm-recomp> > nm.txt; python3 recomp_gen.py gen.txt nm.txt > dsp56k_recomp.inl`
3. Build for armhf with `-DCMAKE_CXX_FLAGS=-I<dir containing dsp56k_recomp.inl>`. dsp.cpp includes the
   file via `__has_include`. Run on the device: `MNM_DSP_INTERP=1 ./mnm-recomp os.syx 2000000`.
